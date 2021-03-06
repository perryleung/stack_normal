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



#define CURRENT_PID 8

namespace qpskclient{
#if (PHY_PID | CURRENT_PID) == PHY_PID

  using msg::MsgSendDataReq;
  using msg::MsgSendAckReq;
  using msg::MsgSendDataRsp;
  using msg::MsgSendAckRsp;
  using msg::MsgRecvDataNtf;
  using msg::MsgTimeOut;

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
  
  struct EvStartRecvData : hsm::Event<EvStartRecvData>{};
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
/*（注意，这里的数据包指的是上位机给通信机发的，一共有1032字节的那个包。而数据帧指的是真实发出去的，一帧239字节的那个帧）
dataSize      		一个数据包的大小（1024）
 dataPerFrame  	一个数据帧的大小（239）
 frameNum    		对一个数据包，应该发帧的数目（5）
 sendBytesNum   	总的一个数据包大小（239*5=1195）
 dataPerBag      	一个数据包的大小（1024）（不是很懂为什么这么干）
 sendPackageNum 	需要发的数据包的数目（1195/1024）*/

    void Init() {
      gain = (uint16_t)(Config::Instance()["qpskclient"]["rxgain"].as<double>() / 2.5 * 65535);
      correlateThreadShort = (uint16_t)(Config::Instance()["qpskclient"]["threshold"].as<double>() * 65535);
      sampleRate = Config::Instance()["qpskclient"]["samplerate"].as<uint32_t>();
      dataSize = Config::Instance()["qpskclient"]["datasize"].as<uint16_t>();
      // frameNum = static_cast<int>(ceil((static_cast<double>(dataSize)) / (static_cast<double>(dataPerFrame))));
    /* dataSize：42 （配置文件中的数值）
 dataPer2Frame： 42 （每两帧数据能包含的数据量，事实上纯数据只有40bytes）
 frameNum：1 			需要多少个两帧。（可能是这样理解的）
 sendBytesNum：42 	发出的字节数（配置文件中的值可能不是整数倍，补零的情况）
 zeroBytesNum：0  	补零字节数
 dataperBag：1024		给通信机发的数据包大小
 sendPackageNum：1	发多少个数据包给通信机*/
      frameNum = dataSize / static_cast<uint16_t>(dataPerFrame);
      if(dataSize % static_cast<uint16_t>(dataPerFrame) != 0){
					++frameNum;
      }
      qpskNumChannel = Config::Instance()["qpskclient"]["qpsknumchannel"].as<uint16_t>();
      transmitAmp = Config::Instance()["qpskclient"]["txgain"].as<double>() * 32767;

      sendBytesNum = frameNum * dataPerFrame; // 真实数据加上补零数据
      zeroBytesNum = sendBytesNum - dataSize; // 帧补零数,这里是补零到N帧数据
      sendPackageNum = static_cast<int>(ceil((static_cast<double>(sendBytesNum)) / (static_cast<double>(dataPerBag))));
      readCycle = Config::Instance()["qpskclient"]["readcycle"].as<uint16_t>();
      

      CleanErrorPacket();

      string server_addr = Config::Instance()["qpskclient"]["address"].as<string>();//"192.168.2.101";
      string server_port = Config::Instance()["qpskclient"]["port"].as<string>();//"80";
      LOG(INFO) << "QpskClientSimulate Init and the ip is " << server_addr << " the port is " << server_port;

      clientsocketWPtr_ = ClientSocket::NewClient(server_addr, server_port);
			// 创建与通信机的TCP连接，这里ClientSocket中的Init()会构造一个对象并返回
      //此处的初始化调用connect函数来建立连接，一般只有两种情况，成功连接和一直连接不上返回错误。（假如长时间未连上，终端不会提示错误并继续往下跑）

      if (!clientsocketWPtr_.expired()) {
      	  //如果建立成功则打印以下
          LOG(INFO) << "Map the opened fd of socket and DAP";
          GetDap().Map(GetFd()); //建立fd与dap的键值关系
          //在Dap中，需要获得套接字描述符之后，在向DapMap注册
          //GetDap()和Dap对象是Driver中的
          //然后调度器可以在DapMap中通过套接字描述符fd_找到相应的Dap，就可以找到对应的协议层
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
    typedef hsm_vector<MsgSendDataReq, MsgSendAckReq, MsgTimeOut, EvStartRecvData> reactions; // , EvRxRecvDisableNtf

    // HSM_TRANSIT(EvRxRecvDisableNtf, WaitRecvEnable, &QpskClient::SendRecvCom<EvRxRecvDisableNtf, false>);
    HSM_TRANSIT(MsgSendDataReq, WaitRecvDisable, &QpskClient::SendRecvDisableCom<MsgSendDataReq, Data>);
    HSM_TRANSIT(MsgSendAckReq, WaitRecvDisable, &QpskClient::SendRecvDisableCom<MsgSendAckReq, Ack>);
    HSM_TRANSIT(MsgTimeOut, TimeOutHandle, &QpskClient::SendRecvDisableCom2);
    HSM_TRANSIT(EvStartRecvData, RecvDataState);
  };
  
  struct RecvDataState : hsm::State<RecvDataState, Top>
  {
    typedef hsm_vector<EvStartRecvData, EvFinishRecvData> reactions;
    HSM_TRANSIT(EvFinishRecvData, Idle, &QpskClient::CleanErrorPacket);
    HSM_DISCARD(EvStartRecvData);
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

//下段代码是收到有出错也往上传
//收到了5帧数据时，就生成一个事件往物理层传，上传的只有MsgRecvDataNtf事件
    if (frameNum == Owner()->GetFrameNum()) {        // 收到5帧数据
      if (Owner()->GetErrorPacket()) {
        LOG(INFO) << "This is a error packet!";
      }
      //从一个协议层发往另一个协议层的事件，都是通过指针传递的。为了实现多态，HSM通过基类指针传递，这样就带来了如何找到事件原型的问题。例如QPSK中往MAC层发MsgRecvDataNtf事件，生成了指针，传入Module基类中的SendNtf函数
        pkt::Packet pkt(const_cast<char *>(recvData.c_str()), recvData.size());
        Ptr<MsgRecvDataNtf> m(new msg::MsgRecvDataNtf);
        m->packet = pkt;
        m->reservation = turboItertime;
        Owner()->SendNtf(m, MAC_LAYER, MAC_PID);
        recvData = "";
        Ptr<EvFinishRecvData> p(new EvFinishRecvData);
        Owner()->GetHsm().ProcessEvent(p);
    }else{
        Ptr<EvStartRecvData> p(new EvStartRecvData);
        Owner()->GetHsm().ProcessEvent(p);
    }
 /*这段代码是没错了才往上传
   if (frameNum == Owner()->GetFrameNum()) {        // 收到5帧数据
      if (!Owner()->GetErrorPacket()) {
        LOG(INFO) << "This is a error packet!";
        pkt::Packet pkt(const_cast<char *>(recvData.c_str()), recvData.size());
        Ptr<MsgRecvDataNtf> m(new msg::MsgRecvDataNtf);
        m->packet = pkt;
        m->reservation = turboItertime;
        Owner()->SendNtf(m, MAC_LAYER, MAC_PID);
        }
        recvData = "";
        Ptr<EvFinishRecvData> p(new EvFinishRecvData);
        Owner()->GetHsm().ProcessEvent(p);
    }else{
        Ptr<EvStartRecvData> p(new EvStartRecvData);
        Owner()->GetHsm().ProcessEvent(p);
    }
 */   
  }


  void
  MyParser::Parse(const char *input){
    //MyParser类中的Parse（解析）函数，对这些指令解析，然后发布相应的事件
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
    }
  }

//SendRecvDisableCom在上层有数据要发送的时候调用，目的有两个：1. 把上层数据分成数据包。2. 发送指令断开连接。
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
    }else{                      // 超过2014就截断
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

//SendRecvDisableCom2只有在MsgTimeOut事件到来的时候才调用。作用只有一个，就是发指令断开连接。（事实上，QPSK的readCycle，在配置文件中设置成了65535。）
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
#endif
}
