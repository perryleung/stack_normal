//修改了原QPSK中的上传函数--3.3
//CTS,RTS,DATA在fama_ntr中都视为data类，在此不需要添加flag到类型了--3.7
//传下来的数据是上面类型由MsgSendDataReq,MsgSendAckReq决定，因为这两个事件发生到状态转移会分别将flag置data或ACK
//readcycle这个参数目前设置为30S，SendRecvCom这个函数会回复MAC MsgSendDataRsp
#include <string>
#include <queue>
#include <stdint.h>
#include <math.h>
#include "driver.h"
#include "schedule.h"
#include "clientsocket.h"
#include "hsm.h"
#include "message.h"
#include "packet.h"



#define CURRENT_PID 128

namespace qpskclient{
//#if (PHY_PID | CURRENT_PID) == PHY_PID

  using msg::MsgSendDataReq;
  using msg::MsgSendAckReq;
  using msg::MsgSendDataRsp;
  using msg::MsgSendAckRsp;
  using msg::MsgRecvDataNtf;
  using msg::MsgTimeOut;
  using msg::EvStartRecvData;

  using std::string;
  using std::queue;
  using pkt::Packet;

  struct EvRxConNtf : hsm::Event<EvRxConNtf>{};
  struct EvRxRecvEnableNtf : hsm::Event<EvRxRecvEnableNtf>{};
  struct EvRxRecvDisableNtf : hsm::Event<EvRxRecvDisableNtf>{};
  struct EvRxSendEnableNtf : hsm::Event<EvRxSendEnableNtf>{};
  struct EvTranPktContinue : hsm::Event<EvTranPktContinue>{};
  struct EvTranPktStop : hsm::Event<EvTranPktStop>{};
  struct EvStopRecvData : hsm::Event<EvStopRecvData>{};
  struct EvFinishSendData : hsm::Event<EvFinishSendData>{};
  
  struct EvStartRecvData1 : hsm::Event<EvStartRecvData1>{};//这里修改成EvStartRecvData1是因为会和msg::EvStartRecvData冲突,现在
  struct EvFinishRecvData : hsm::Event<EvFinishRecvData>{};
  
  



  struct Top;
  struct WaitConNtf;
  struct WaitRecvEnable;
  struct Idle;
  struct WaitRecvDisable;
  struct WaitSendEnable;
  struct SendDataState;
  struct TimeOutHandle;
  
  struct RecvDataState;
  // struct WaitDevStopRecvData;


  typedef enum{
    Data,
    Ack
  }DataOrAck;

  class QpskClient;

  class MyParser{
  public:
    void Parse(const char *);
    void HandleRecvData(const char *);
    inline void SetOwner(QpskClient *ptr){owner_ = ptr;};
    inline QpskClient* Owner() const { return owner_;};
    MyParser() : recvData(""){};
	void HandleDataNtf(const char*);

  private:
    QpskClient *owner_;
    string recvData;
  };


  class QpskClient : public drv::Driver<QpskClient, PHY_LAYER, CURRENT_PID>
  {
  public:
    QpskClient(){
      DRIVER_CONSTRUCT(Top);
      GetParser().SetOwner(this);
    }

    void Init() {
      gain = (uint16_t)(Config::Instance()["qpskclient"]["rxgain"].as<double>() / 2.5 * 65535);
      correlateThreadShort = (uint16_t)(Config::Instance()["qpskclient"]["threshold"].as<double>() * 65535);
      sampleRate = Config::Instance()["qpskclient"]["samplerate"].as<uint32_t>();
      dataSize = Config::Instance()["qpskclient"]["datasize"].as<uint16_t>();
      // frameNum = static_cast<int>(ceil((static_cast<double>(dataSize)) / (static_cast<double>(dataPerFrame))));
      frameNum = dataSize / static_cast<uint16_t>(dataPerFrame);
      if(dataSize % static_cast<uint16_t>(dataPerFrame) != 0){
	++frameNum;
      }
      qpskNumChannel = Config::Instance()["qpskclient"]["qpsknumchannel"].as<uint16_t>();
      transmitAmp = Config::Instance()["qpskclient"]["txgain"].as<double>() * 32767;

      sendBytesNum = frameNum * dataPerFrame; // 真实数据加上补零数据
      zeroBytesNum = sendBytesNum - dataSize; // 帧补零数,这里是补零到N帧数据
      //ceil（）是向上取整
      sendPackageNum = static_cast<int>(ceil((static_cast<double>(sendBytesNum)) / (static_cast<double>(dataPerBag))));
      readCycle = Config::Instance()["qpskclient"]["readcycle"].as<uint16_t>();

      CleanErrorPacket();

      string server_addr = Config::Instance()["qpskclient"]["address"].as<string>();//"192.168.2.101";
      string server_port = Config::Instance()["qpskclient"]["port"].as<string>();//"80";
      LOG(INFO) << "QpskClientSimulate Init and the ip is " << server_addr << " the port is " << server_port;

      clientsocketWPtr_ = ClientSocket::NewClient(server_addr, server_port);

      if (!clientsocketWPtr_.expired()) {
          LOG(INFO) << "Map the opened fd of socket and DAP";
          GetDap().Map(GetFd()); //建立fd与dap的键值关系,里面调用了register函数
      }
    }

    inline ClientSocketPtr GetClientsocket()
    {
        if (clientsocketWPtr_.expired()) {
            LOG(DEBUG) << "the Communication machine is disconnected";
            return NULL;
        }

        return clientsocketWPtr_.lock();
      // return clientsocket_;
    }

    inline int GetFd()
    {
        ClientSocketPtr temp = GetClientsocket();
        if (temp == NULL) {
            return 2;
        }
        return temp->GetFd();

      // return GetClientsocket().GetFd();
    }

    inline bool IsConnected()
    {
      if (GetClientsocket() == NULL) {
            return false;
        }
        return true;
    }

    inline void Write(){
        ClientSocketPtr temp = GetClientsocket();
        if (temp == NULL) {
            return;
        }
        temp->Write();
    }

    inline void SetErrorPacket() {
      isErrorPacket = true;
    }

    inline void CleanErrorPacket() {
      isErrorPacket = false;
    }

    inline bool GetErrorPacket() {
      return isErrorPacket;
    }

    inline void CleanErrorPacket(const Ptr<EvFinishRecvData> &) {
      isErrorPacket = false;
    }

    inline MyParser &GetParser() {return parser_;} // 返回引用比直接返回对象需要更少开销
    inline uint16_t GetFrameNum() { return frameNum;}
	inline int GetRecordSendPackageNum() {return recordSendPackageNum;}
	inline int GetSendPackageNum() {return sendPackageNum;}

    void Notify()
    {
        LOG(INFO) << "QPSKClientsimulate receive data";

        IODataPtr data = ReadQueue::Front();

        parser_.Parse(data->data);
        free(data->data);       // malloc in readcb

        ReadQueue::Pop();
    }

    template<typename EventType, bool isSendRsp>
    void SendRecvCom(const Ptr<EventType> &);

    template<typename MsgType, DataOrAck MsgFlag>
    void SendRecvDisableCom(const Ptr<MsgType> &);

    void SendRecvDisableCom2(const Ptr<MsgTimeOut> &);

    void SendSendEnableCom(const Ptr<EvRxRecvDisableNtf> &);

    template<typename EventType>
    void SendDownData(const Ptr<EventType> &);

    void SendDevStopRecvDataCom(const Ptr<EvTranPktStop> &);
    
    /**
     * @brief: 收到监听到载波的信息，将其上传给ＭＡＣ层，并转移到接收状态
     */
    void Send_HFM_Signal(const Ptr<EvStartRecvData> &m);
  private:
    // ClientSocket clientsocket_;
    weak_ptr<ClientSocket> clientsocketWPtr_;
    MyParser parser_;
    const int dataPerBag = 1024; // 直接初始化，是C11的特性，允许non-static变量在类内初始化
    const int lengthPerBag = 1032;
    const int dataPerFrame = 239;
    uint16_t gain;
    uint16_t frameNum;          // 多帧数难以实现，会有冲突;即使不考虑冲突，也需要考虑在数据包后面加上结尾操作符，或者通信机提供收到的帧数目
    uint16_t qpskNumChannel;
    uint16_t correlateThreadShort;
    uint32_t sampleRate;
    float transmitAmp;
    uint16_t dataSize;
    int sendBytesNum; // 真实数据加上补零数据
    int zeroBytesNum; // 帧补零数,这里是补零到N帧数据
    int sendPackageNum;
    int recordSendPackageNum;
    uint16_t readCycle;

    DataOrAck flag;
    queue<IODataPtr> sendingPktQueue;

    bool isErrorPacket;
  };

  struct Top : hsm::State<Top, hsm::StateMachine, WaitConNtf>
  {
    typedef hsm_vector<MsgSendDataReq, MsgSendAckReq, MsgTimeOut> reactions;
    HSM_DEFER(MsgSendAckReq);
    HSM_DEFER(MsgSendDataReq);  //
    HSM_DEFER(MsgTimeOut);
  };

  struct WaitConNtf : hsm::State<WaitConNtf, Top>
  {
    typedef hsm_vector<EvRxConNtf> reactions;
    // HSM_DEFER(MsgSendDataReq);
    HSM_TRANSIT(EvRxConNtf, WaitRecvEnable, &QpskClient::SendRecvCom<EvRxConNtf, false>);
  };

  struct WaitRecvEnable : hsm::State<WaitRecvEnable, Top>
  {
    typedef hsm_vector<EvRxRecvEnableNtf> reactions;
    // HSM_DEFER(MsgSendDataReq);
    // HSM_DEFER(MsgSendAckReq);
    HSM_TRANSIT(EvRxRecvEnableNtf, Idle);
  };

  struct Idle : hsm::State<Idle, Top>
  {
    typedef hsm_vector<MsgSendDataReq, MsgSendAckReq, MsgTimeOut, EvStartRecvData1,EvStartRecvData> reactions; // , EvRxRecvDisableNtf

    // HSM_TRANSIT(EvRxRecvDisableNtf, WaitRecvEnable, &QpskClient::SendRecvCom<EvRxRecvDisableNtf, false>);
    HSM_TRANSIT(MsgSendDataReq, WaitRecvDisable, &QpskClient::SendRecvDisableCom<MsgSendDataReq, Data>);
    HSM_TRANSIT(MsgSendAckReq, WaitRecvDisable, &QpskClient::SendRecvDisableCom<MsgSendAckReq, Ack>);
    HSM_TRANSIT(MsgTimeOut, TimeOutHandle, &QpskClient::SendRecvDisableCom2);
    HSM_TRANSIT(EvStartRecvData1, RecvDataState);
    HSM_TRANSIT(EvStartRecvData, RecvDataState,&QpskClient::Send_HFM_Signal);
  };
  
  struct RecvDataState : hsm::State<RecvDataState, Top>
  {
    typedef hsm_vector<EvStartRecvData1, EvFinishRecvData> reactions;
    HSM_TRANSIT(EvFinishRecvData, Idle, &QpskClient::CleanErrorPacket);
    HSM_DISCARD(EvStartRecvData1);
  };

  struct TimeOutHandle : hsm::State<TimeOutHandle, Top>
  {
    typedef hsm_vector<EvRxRecvDisableNtf> reactions;
    HSM_TRANSIT(EvRxRecvDisableNtf, WaitRecvEnable, (&QpskClient::SendRecvCom<EvRxRecvDisableNtf, false>));
  };

  struct WaitRecvDisable : hsm::State<WaitRecvDisable, Top>
  {
    typedef hsm_vector<EvRxRecvDisableNtf> reactions;
    // HSM_DEFER(MsgSendDataReq);
    // HSM_DEFER(MsgSendAckReq);
    HSM_TRANSIT(EvRxRecvDisableNtf, WaitSendEnable, &QpskClient::SendSendEnableCom);

  };

  struct WaitSendEnable : hsm::State<WaitSendEnable, Top>
  {
    typedef hsm_vector<EvRxSendEnableNtf> reactions;
    HSM_TRANSIT(EvRxSendEnableNtf, SendDataState, (&QpskClient::SendDownData<EvRxSendEnableNtf>));
  };

  struct SendDataState : hsm::State<SendDataState, Top>
  {
    typedef hsm_vector<EvTranPktContinue, EvTranPktStop, EvStopRecvData, EvFinishSendData> reactions;
    HSM_WORK(EvTranPktContinue, &QpskClient::SendDownData<EvTranPktContinue>);
    HSM_WORK(EvTranPktStop, &QpskClient::SendDevStopRecvDataCom);
    HSM_DISCARD(EvStopRecvData);
    HSM_TRANSIT(EvFinishSendData, WaitRecvEnable, &QpskClient::SendRecvCom<EvFinishSendData, true>);

  };

  void
  MyParser::HandleDataNtf(const char* temp){
    int frameNum = (temp[2] & 0x00ff) | ((temp[3] << 8) & 0xff00);      //帧号
    int messageLen = (temp[4] & 0x00ff) | ((temp[5] << 8) & 0xff00);    //数据长度
    int turboItertime = (temp[6] & 0x00ff) | ((temp[7] << 8) & 0xff00);     //迭代次数
    // if (frameNum == 1) {
    //   recvData = "";
    // }
    if (turboItertime >= 3) {
      LOG(INFO) << "RX DEVICE ERR DATA";
      Owner()->SetErrorPacket();
    }
    recvData.append(temp + 8, messageLen);
    //收到5帧数据时，就生成一个事件往物理层传，上传的只有MsgRecvDataNtf
    if (frameNum == Owner()->GetFrameNum()) {        // 收到5帧数据
      if (Owner()->GetErrorPacket()) {  //收到错误信息，或者解不出来，可以认为发生了碰撞
        LOG(INFO) << "This is a error packet!";
      }
      //获取数据
        pkt::Packet pkt(const_cast<char *>(recvData.c_str()), recvData.size());
        Ptr<MsgRecvDataNtf> m(new msg::MsgRecvDataNtf);
        m->packet = pkt;
        if(turboItertime<=2)//迭代次数小于等于2时将reservation位置0，为fama-ntr判断接收是否出错提供依据
        //否则reservaton＝迭代次数
        {
          m->reservation = 0;
        }
        else
        {
          m->reservation = turboItertime;
        }
        Owner()->SendNtf(m, MAC_LAYER, MAC_PID);
      //数据上传后将存放获得数据的位置清0
        recvData = "";
        Ptr<EvFinishRecvData> p(new EvFinishRecvData);
        Owner()->GetHsm().ProcessEvent(p);
    }/*else{  不要这一部分，用监听到载波到时候便实现转移到接收状态
        Ptr<EvStartRecvData1> p(new EvStartRecvData1);
        Owner()->GetHsm().ProcessEvent(p);
    }*/
  }


  void
  MyParser::Parse(const char *input){
    if (input[0] == (char)0x55 && input[1] == (char)0xaa) {
      LOG(INFO) << "RX DEVICE CONNECTION NTF 55aa";
      Ptr<EvRxConNtf> p(new EvRxConNtf);
      Owner()->GetHsm().ProcessEvent(p);
    }else if (input[0] == (char)0x13 && input[1] == (char)0x01) {
      LOG(INFO) << "RX DEVICE RECV ENABLE NTF 1301";
      Ptr<EvRxRecvEnableNtf> p(new EvRxRecvEnableNtf);
      Owner()->GetHsm().ProcessEvent(p);
    }else if (input[0] == (char)0x15 && input[1] == (char)0x01) {
      LOG(INFO) << "RX DATA 1501";
      HandleDataNtf(input);
    }else if (input[0] == (char)0x14 && input[1] == (char)0x01) {
      // if (!recvData.empty()) {
      //   pkt::Packet pkt(const_cast<char *>(recvData.c_str()), recvData.size());
      //   Ptr<MsgRecvDataNtf> m(new msg::MsgRecvDataNtf);
      //   m->packet = pkt;
      //   Owner()->SendNtf(m, MAC_LAYER, MAC_PID);
      //   recvData = "";
      // }
      LOG(INFO) << "DEVICE STOP RECV DATA 1401";
      Ptr<EvRxRecvDisableNtf> p(new EvRxRecvDisableNtf);
      Owner()->GetHsm().ProcessEvent(p);
    }else if (input[0] == (char)0x16 && input[1] == (char)0x01) {
      LOG(INFO) << "RX DEVICE SEND ENABLE NTF 1601";
      Ptr<EvRxSendEnableNtf> p(new EvRxSendEnableNtf);
      Owner()->GetHsm().ProcessEvent(p);
    }else if (input[0] == (char)0x17 && input[1] == (char)0x01) {
      if (Owner()->GetRecordSendPackageNum() < Owner()->GetSendPackageNum()) {
        LOG(INFO) << "RX DEVICE RECV PACKAGE NTF 1701 AND CONTINUE";
        Ptr<EvTranPktContinue> p(new EvTranPktContinue);
        Owner()->GetHsm().ProcessEvent(p);
      }else{
        LOG(INFO) << "RX DEVICE RECV PACKAGE NTF 1701 AND STOP";
        Ptr<EvTranPktStop> p(new EvTranPktStop);
        Owner()->GetHsm().ProcessEvent(p);
      }
    }else if (input[0] == (char)0x18 && input[1] == (char)0x01) {
      LOG(INFO) << "RX DEVICE STOP RECV DATA 1801";
      Ptr<EvStopRecvData> p(new EvStopRecvData);
      Owner()->GetHsm().ProcessEvent(p);
    }else if (input[0] == (char)0x19 && input[1] == (char)0x01) {
      LOG(INFO) << "RX DEVICE FINISH SEND DATA 1901";
      Ptr<EvFinishSendData> p(new EvFinishSendData);
      Owner()->GetHsm().ProcessEvent(p);
    }else if(input[0] == (char)0x0d && input[1] == (char)0x01){//接受到ＨＦＭ，表示帧听到数据
      LOG(INFO) << "帧听到HFM数据";
      Ptr<EvStartRecvData> p(new EvStartRecvData);
      Owner()->GetHsm().ProcessEvent(p);//将监听到ＨＦＭ的事件给状态机处理  
    }
  }
  template<typename EventType, bool isSendRsp>
  void QpskClient::SendRecvCom(const Ptr<EventType> &){
    if (!IsConnected()) {
      return;
    }

    LOG(INFO) << "Send the 1300 : Recv Enable Command";
    char *msg = new char[lengthPerBag];
    memset(msg, 0, lengthPerBag);

    msg[0] = (char)0x13;
    msg[1] = (char)0x00;
    msg[2] = (char)(gain & 0x00ff);
    msg[3] = (char)((gain >> 8) & 0x00ff);
    msg[4] = (char)(correlateThreadShort & 0x00ff);
    msg[5] = (char)((correlateThreadShort >> 8) & 0x00ff);
    msg[6] = (char)((sampleRate >> 16) & 0x000000ff);
    msg[7] = (char)((sampleRate >> 24) & 0x000000ff);
    msg[8] = (char)(sampleRate & 0x000000ff);
    msg[9] = (char)((sampleRate >> 8) & 0x000000ff);
    msg[10] = (char)(frameNum & 0x00ff);
    msg[11] = (char)((frameNum >> 8) & 0x00ff);
    msg[12] = (char)(qpskNumChannel & 0x00ff);
    msg[13] = (char)((qpskNumChannel >> 8) & 0x00ff);

    IODataPtr pkt(new IOData);
    pkt->fd   = GetFd();
    pkt->data = msg;
    pkt->len  = lengthPerBag;

    WriteQueue::Push(pkt);
    // GetClientsocket().Write();
    Write();

    if (isSendRsp) {
      if(flag == Data){
        Ptr<MsgSendDataRsp> rsp(new MsgSendDataRsp);
        SendRsp(rsp, MAC_LAYER, MAC_PID);
      }else if(flag == Ack){
        Ptr<MsgSendAckRsp> rsp(new MsgSendAckRsp);
        SendRsp(rsp, MAC_LAYER, MAC_PID);
      }
    }else{
      LOG(DEBUG) << "Set Timeer " << readCycle << "s";
      SetTimer(readCycle);
    }
  }

  template<typename MsgType, DataOrAck MsgFlag>
  void QpskClient::SendRecvDisableCom(const Ptr<MsgType> &msg){

    if(!IsConnected()){
      return;
    }

    flag = MsgFlag;
    // 这里是分帧处理,注意帧长239，包长dataSize,与上位机与通信机的1032-8=1024通信长度的区别与联系
    auto pkt = msg->packet;
    char* newPkt = new char[dataSize];
    if (pkt.Size() <= dataSize) { // 不够dataSize就补零,这里是一次补零，补到dataSize,下面还有二次补零，补到N帧数据
      memset(newPkt, 0, dataSize);
      memcpy(newPkt, pkt.Raw(), pkt.Size());
    }else{                      // 超过1024就截断
      memcpy(newPkt, pkt.Raw(), dataSize);
    }

    for (int i = 0; i < sendPackageNum; i++) {
      if (i == sendPackageNum - 1) {
        char* dataMsg = new char[lengthPerBag];
        memset(dataMsg, 0, lengthPerBag);
        dataMsg[0] = 0x17;
        dataMsg[1] = 0x00;
        int leftDataLen = sendBytesNum - i * dataPerBag;
        dataMsg[2] = (char)(leftDataLen & 0x00ff);
        dataMsg[3] = (char)((leftDataLen >> 8) & 0x00ff);
        dataMsg[4] = (char)(frameNum & 0x00ff);
        dataMsg[5] = (char)((frameNum >> 8) & 0x00ff);

        memcpy(dataMsg + 8, newPkt + i * dataPerBag, leftDataLen - zeroBytesNum); // 在最后一个包中，余下的数据段减去二次补零数据段就是剩余的真实数据（真实指的是非二次补零数据的数据段）
        IODataPtr sendPkt(new IOData);
        sendPkt->fd = GetFd();
        sendPkt->len = lengthPerBag;
        sendPkt->data = dataMsg;
        sendingPktQueue.push(sendPkt);
      }else{
        char* dataMsg = new char[lengthPerBag];
        memset(dataMsg, 0, lengthPerBag);
        dataMsg[0] = 0x17;
        dataMsg[1] = 0x00;
        dataMsg[2] = (char)(dataPerBag & 0x00ff);
        dataMsg[3] = (char)((dataPerBag >> 8) & 0x00ff);
        dataMsg[4] = (char)(frameNum & 0x00ff);
        dataMsg[5] = (char)((frameNum >> 8) & 0x00ff);
        
        memcpy(dataMsg + 8, newPkt + i * dataPerBag, dataPerBag);
        IODataPtr sendPkt(new IOData);
        sendPkt->fd = GetFd();
        sendPkt->len = lengthPerBag;
        sendPkt->data = dataMsg;
        sendingPktQueue.push(sendPkt);
      }
    }
    recordSendPackageNum = 0;

    LOG(INFO) << "Send the 1400 : Recv Disable Command";
    char *comMsg = new char[lengthPerBag];
    memset(comMsg, 0, lengthPerBag);

    comMsg[0] = (char)0x14;
    comMsg[1] = (char)0x00;

    IODataPtr sendComPkt(new IOData);
    sendComPkt->fd   = GetFd();
    sendComPkt->data = comMsg;
    sendComPkt->len  = lengthPerBag;

    WriteQueue::Push(sendComPkt);
    // GetClientsocket().Write();
    Write();
  }

  void QpskClient::SendRecvDisableCom2(const Ptr<MsgTimeOut> &){
    if(!IsConnected()){
      return;
    }

    LOG(INFO) << "Send the 1400 : Recv Disable Command";
    char *comMsg = new char[lengthPerBag];
    memset(comMsg, 0, lengthPerBag);

    comMsg[0] = (char)0x14;
    comMsg[1] = (char)0x00;

    IODataPtr sendComPkt(new IOData);
    sendComPkt->fd   = GetFd();
    sendComPkt->data = comMsg;
    sendComPkt->len  = lengthPerBag;

    WriteQueue::Push(sendComPkt);
    // GetClientsocket().Write();
    Write();
  }

  void
  QpskClient::SendSendEnableCom(const Ptr<EvRxRecvDisableNtf> &){
    if (!IsConnected()) {
      return;
    }

    LOG(INFO) << "Send the 1600 : Send Enable Command";
    char *msg = new char[lengthPerBag];
    memset(msg, 0, lengthPerBag);

    msg[0] = (char)0x16;
    msg[1] = (char)0x00;
    msg[2] = (char)((uint16_t)transmitAmp & 0x00ff);     //发射幅度
    msg[3] = (char)((((uint16_t)transmitAmp & 0xff00) >> 8) & 0x00ff);
    msg[4] = (char)((sampleRate >> 16) & 0x000000ff);
    msg[5] = (char)((sampleRate >> 24) & 0x000000ff);
    msg[6] = (char)(sampleRate & 0x000000ff);
    msg[7] = (char)((sampleRate >> 8) & 0x000000ff);
    msg[8] = (char)(frameNum & 0x00ff);
    msg[9] = (char)((frameNum >> 8) & 0x00ff);

    IODataPtr pkt(new IOData);
    pkt->fd   = GetFd();
    pkt->data = msg;
    pkt->len  = lengthPerBag;

    WriteQueue::Push(pkt);
    // GetClientsocket().Write();
    Write();
  }


  template<typename EventType>
  void QpskClient::SendDownData(const Ptr<EventType> &){
    if(!IsConnected()){
      return;
    }
    LOG(INFO) << "Send down msg 1700";

    IODataPtr pkt = sendingPktQueue.front();
    sendingPktQueue.pop();
    recordSendPackageNum++;
    WriteQueue::Push(pkt);
    // GetClientsocket().Write();
    Write();
  }

  void QpskClient::Send_HFM_Signal(const Ptr<EvStartRecvData> &)
  {
    LOG(INFO)<<"上传监听到载波到信息给mac，并转移到接受数据状态";
    Ptr<EvStartRecvData> m(new EvStartRecvData);
    SendNtf(m, MAC_LAYER, MAC_PID);
  }
  void
  QpskClient::SendDevStopRecvDataCom(const Ptr<EvTranPktStop> &){
    if (!IsConnected()) {
      return;
    }

    LOG(INFO) << "Send the 1800 : Send Dev Stop Recv Data Command";
    char *msg = new char[lengthPerBag];
    memset(msg, 0, lengthPerBag);

    msg[0] = (char)0x18;
    msg[1] = (char)0x00;

    IODataPtr pkt(new IOData);
    pkt->fd   = GetFd();
    pkt->data = msg;
    pkt->len  = lengthPerBag;

    WriteQueue::Push(pkt);
    // GetClientsocket().Write();
    Write();
  }

MODULE_INIT(QpskClient)
//#endif
}
