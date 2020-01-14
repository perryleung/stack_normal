#include "driver.h"
#include "schedule.h"
#include "clientsocket.h"
#include "hsm.h"
#include "message.h"
#include "packet.h"
#include <stdlib.h>
#include <string>
#include <queue>

#include "client.h"
#include "app_datapackage.h"

#define CURRENT_PID 32
#define PER_MSG_SIZE 1032

extern client* UIClient;

namespace fsknewclient {
#if (PHY_PID | CURRENT_PID) == PHY_PID

    using msg::MsgSendDataReq;
    using msg::MsgSendAckReq;
    using msg::MsgSendDataRsp;
    using msg::MsgSendAckRsp;
    using msg::MsgRecvDataNtf;
    // using msg::MsgPosReq;
    // using msg::MsgPosRsp;
    using msg::MsgTimeOut;


    using pkt::Packet;
    using std::string;


    // 定义状态事件，可在函数中编程触发的事件
    // template <typename T>
    // struct EvBase : hsm::Event<T>   // 继承event事件并作一些扩展
    // {
    // };
    struct EvRxErr : hsm::Event<EvRxErr>{};
    struct EvRxConNtf : hsm::Event<EvRxConNtf>{};
    struct EvTxSetting : hsm::Event<EvTxSetting>{};
    struct EvRxSettingRsp : hsm::Event<EvRxSettingRsp>{};
    struct EvRxSettingAllRsp : hsm::Event<EvRxSettingAllRsp>{};
    struct EvDataRsp : hsm::Event<EvDataRsp>{};
    struct EvRxSendingRsp : hsm::Event<EvRxSendingRsp>{};
    struct EvStopRecvRsp : hsm::Event<EvStopRecvRsp>{};
    struct EvTranPktContinue : hsm::Event<EvTranPktContinue>{};
    struct EvTranPktStop : hsm::Event<EvTranPktStop>{};
    struct EvStopRecvData : hsm::Event<EvStopRecvData>{};
    struct EvFinishSendData : hsm::Event<EvFinishSendData>{};
    struct EvStartRecvData : hsm::Event<EvStartRecvData>{};
    struct EvFinishRecvData : hsm::Event<EvFinishRecvData>{};
    struct EvRxSendEnableNtf : hsm::Event<EvRxSendEnableNtf>{};

    struct Top;
    struct Idle;
    struct SendSettingReq;
    struct WaitSettingRsp;
    struct WaitConNtf;
    struct WaitDataRsp;
    struct WaitSendingData;
    struct TimeOutHandle;
    struct StopRecv;
    struct WaitSendEnable;
    struct SendDataState;
    struct RecvDataState;

    typedef enum{
        Data, 
        Ack
    }DataOrAck;

    class FskNewClient;

    class MyParser{
    public:
        void setUpError(uint16_t UpError){
        upError = UpError;}
        void Parse(const char *);
        inline void SetOwner(FskNewClient *ptr){owner_ = ptr;};
        inline FskNewClient* Owner() const { return owner_;};
        void HandleDataNtf(const char*);
        MyParser():recvData("") {
        }

    private:
        uint16_t upError; 
        FskNewClient *owner_;
        string recvData;

    };


class FskNewClient : public drv::Driver<FskNewClient, PHY_LAYER, CURRENT_PID>
{
public:
    FskNewClient() {
        DRIVER_CONSTRUCT(Top);
        // GetSap().SetOwner(this);
        // GetDap().SetOwner(this);
        // GetHsm().Initialize<Top>(this);
        GetParser().SetOwner(this);
    }

    void Init()
    {
        TimeTxSettingREQ = 0;
        sampleRate = Config::Instance()["fsknewclient"]["samplerate"].as<int>();//80000;
        gain = Config::Instance()["fsknewclient"]["gain"].as<double>() / 2.5 * 65535; //0.1 /2.5 * 65535;
        cyclePrefixMs = Config::Instance()["fsknewclient"]["cycleprefixms"].as<short>(); //20;//
        correlateThreadShort = Config::Instance()["fsknewclient"]["correlatethreadshort"].as<double>() * 65535; //0.1 * 65535;
        transmitAmp = Config::Instance()["fsknewclient"]["transmitAmp"].as<double>();//0.1;
        if (transmitAmp > 1.0) {
            transmitAmp = 1.0;
        }
        if (transmitAmp < 0.0) {
            transmitAmp = 0.0;
        }

        dataSize = Config::Instance()["fsknewclient"]["datasize"].as<uint16_t>();
        upError = Config::Instance()["fsknewclient"]["upError"].as<uint16_t>();
        parser_.setUpError(upError);
        flagtosend = false;
	frameNum = dataSize / dataPer2Frame;
	if(dataSize % dataPer2Frame != 0){
	    ++frameNum;
	}
        frameNum = static_cast<int>(ceil((static_cast<double>(dataSize)) / (static_cast<double>(dataPer2Frame))));
        sendBytesNum = frameNum * dataPer2Frame;
        zeroBytesNum = sendBytesNum - dataSize;
        sendPackageNum = static_cast<int>(ceil((static_cast<double>(sendBytesNum)) / (static_cast<double>(dataPerBag))));

        readCycle = Config::Instance()["fsknewclient"]["readcycle"].as<uint16_t>();

        CleanErrorPacket();
        
        string server_addr = Config::Instance()["fsknewclient"]["address"].as<std::string>();//"192.168.2.101";
        string server_port = Config::Instance()["fsknewclient"]["port"].as<std::string>();//"80";
        LOG(INFO) << "FskNewClientSimulate Init and the ip is " << server_addr << " the port is " << server_port;

        clientsocketWPtr_ = ClientSocket::NewClient(server_addr, server_port);

        if (!clientsocketWPtr_.expired()) {
            LOG(INFO) << "Map the opened fd of socket and DAP";
            GetDap().Map(GetFd()); //建立fd与dap的键值关系
        }
    }

    template<typename MsgType, DataOrAck MsgFlag>
    void SendEnable(const Ptr<MsgType> &);
    void SendEnable2(const Ptr<MsgTimeOut> &);
    template<typename EventType>
    void SendDownMsg(const Ptr<EventType> &);
    template<typename EventType, bool isSendRsp>
    void RecvEnable(const Ptr<EventType> &);
    void SendSendEnableCom(const Ptr<EvStopRecvRsp> &);
    void SendDevStopRecvDataCom(const Ptr<EvTranPktStop> &);

    void Notify()
    {
        LOG(INFO) << "FSKNewClient simulate receive data";

        IODataPtr data = ReadQueue::Front();
        
        parser_.Parse(data->data);
        free(data->data);       // malloc in readcb

        ReadQueue::Pop();
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


    inline MyParser &GetParser() {return parser_;} // 返回引用比直接返回对象需要更少开销
    inline int GetRecordSendPackageNum() {return recordSendPackageNum;}
    inline int GetRecordRecvFrameNum() {return recordRecvFrameNum;}
    inline void ClearRecordRecvFrameNum() { recordRecvFrameNum = 0;}
    inline void IncrRecordRecvFrameNum() { ++recordRecvFrameNum;}
    inline int GetSendPackageNum() {return sendPackageNum;}
    inline int GetFrameNum() {return frameNum;}

    bool flagtosend;


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

    
private:
    // ClientSocket clientsocket_;
    weak_ptr<ClientSocket> clientsocketWPtr_;
    MyParser parser_;
    int TimeTxSettingREQ;
    int sampleRate;
    unsigned short gain;
    short cyclePrefixMs;
    unsigned short correlateThreadShort;
    float transmitAmp;
    uint16_t frameNum;
    DataOrAck flag;
    // IODataPtr sendingPkt;
    uint16_t readCycle;
    uint16_t channelNum = 1;
    uint16_t upError; 
    uint16_t isHighSpeed = 0;
    uint16_t agcIsOn = 0;
    uint16_t uploadWave = 0;
    uint16_t dataSize;
    const uint16_t dataPer2Frame = 42;
    const uint16_t dataPerBag = 1024;
    const uint16_t lengthPerBag = 1032;

    int sendBytesNum; // 真实数据加上补零数据
    int zeroBytesNum; // 帧补零数,这里是补零到N帧数据
    int sendPackageNum;
    int recordSendPackageNum;
    int recordRecvFrameNum;

    queue<IODataPtr> sendingPktQueue;

    bool isErrorPacket;
};

    struct Top : hsm::State<Top, hsm::StateMachine, WaitConNtf>
    {
        typedef hsm_vector<EvRxErr, MsgSendDataReq, MsgSendAckReq, MsgTimeOut> reactions;
        HSM_DEFER(MsgSendAckReq);
        HSM_DEFER(MsgSendDataReq);  //
        HSM_DISCARD(EvRxErr);
        //HSM_WORK(MsgTimeOut, (&FskNewClient::RecvEnable<MsgTimeOut, false>));
        HSM_DEFER(MsgTimeOut);
    };

    struct WaitConNtf : hsm::State<WaitConNtf, Top>{
        typedef hsm_vector<EvRxConNtf> reactions;
        //带一个输出的状态跳转，估计是先执行输出函数再进行状态跳转，因此下面需要延迟事件，等到跳转到相应状态才派发事件，因为该事件直接触发SendSettingReq状态跳到下一个状态
        HSM_TRANSIT(EvRxConNtf, Idle, (&FskNewClient::RecvEnable<EvRxConNtf, false>));
        // HSM_DEFER(EvTxSetting); // 先延迟一下，等离开状态才派发
    };

    struct Idle : hsm::State<Idle, Top>
    {
        typedef hsm_vector<MsgSendDataReq, MsgSendAckReq, MsgTimeOut, EvStartRecvData> reactions;
        HSM_TRANSIT(MsgSendDataReq, StopRecv, (&FskNewClient::SendEnable<MsgSendDataReq, Data>));
        HSM_TRANSIT(MsgSendAckReq, StopRecv, (&FskNewClient::SendEnable<MsgSendAckReq, Ack>));
        HSM_TRANSIT(MsgTimeOut, TimeOutHandle, &FskNewClient::SendEnable2);
        HSM_TRANSIT(EvStartRecvData, RecvDataState);
    };

    struct RecvDataState : hsm::State<RecvDataState, Top>
    {
        typedef hsm_vector<EvStartRecvData, EvFinishRecvData> reactions;
        HSM_TRANSIT(EvFinishRecvData, Idle, &FskNewClient::CleanErrorPacket);
        HSM_DISCARD(EvStartRecvData);
    };
    
    struct TimeOutHandle : hsm::State<TimeOutHandle, Top>
    {
        typedef hsm_vector<EvStopRecvRsp> reactions;
        HSM_TRANSIT(EvStopRecvRsp, Idle, (&FskNewClient::RecvEnable<EvStopRecvRsp, false>));
    };

    struct StopRecv : hsm::State<StopRecv, Top>
    {
        typedef hsm_vector<EvStopRecvRsp> reactions;
        HSM_TRANSIT(EvStopRecvRsp, WaitSendEnable, &FskNewClient::SendSendEnableCom);

    };

    struct WaitSendEnable : hsm::State<WaitSendEnable, Top>
    {
        typedef hsm_vector<EvRxSendEnableNtf> reactions;
        HSM_TRANSIT(EvRxSendEnableNtf, SendDataState, (&FskNewClient::SendDownMsg<EvRxSendEnableNtf>));

    };

    struct SendDataState : hsm::State<SendDataState, Top>{
        typedef hsm_vector<EvTranPktContinue, EvTranPktStop, EvStopRecvData, EvFinishSendData> reactions;
        HSM_WORK(EvTranPktContinue, (&FskNewClient::SendDownMsg<EvTranPktContinue>));
        HSM_WORK(EvTranPktStop, (&FskNewClient::SendDevStopRecvDataCom));
        HSM_DISCARD(EvStopRecvData);
        HSM_TRANSIT(EvFinishSendData, Idle, (&FskNewClient::RecvEnable<EvFinishSendData, true>));

    };

    // struct WaitSendingData : hsm::State<WaitSendingData, Top>
    // {
    //     typedef hsm_vector<EvRxSendingRsp> reactions;
    //     HSM_TRANSIT(EvRxSendingRsp, WaitDataRsp, &FskNewClient::SendDownMsg);
    // };

    struct WaitDataRsp : hsm::State<WaitDataRsp, Top>
    {
        typedef hsm_vector<EvDataRsp> reactions;
        HSM_TRANSIT(EvDataRsp, Idle, (&FskNewClient::RecvEnable<EvDataRsp, true>));
    };



    void MyParser::Parse(const char *input) {

        if (input[0] == (char)0x55 && input[1] == (char)0xaa) {
            LOG(INFO) << "RX DEVICE CONNECTION NTF";
            Ptr<EvRxConNtf> p(new EvRxConNtf);
            Owner()->GetHsm().ProcessEvent(p);
        }else if(input[0] == (char)0x06 && input[1] == (char)0x01){
            LOG(INFO) << "RX DEVICE SETTING RSP1";
            Ptr<EvRxSettingRsp> p(new EvRxSettingRsp);
            Owner()->GetHsm().ProcessEvent(p);
        }else if(input[0] == (char)0x07 && input[1] == (char)0x01){
            LOG(INFO) << "RX DEVICE SETTING RSP2";
            Ptr<EvRxSettingRsp> p(new EvRxSettingRsp);
            Owner()->GetHsm().ProcessEvent(p);
        }else if(input[0] == (char)0x0c && input[1] == (char)0x01){
            LOG(INFO) << "RX DEVICE SETTING RSP3";
            Ptr<EvRxSettingAllRsp> p(new EvRxSettingAllRsp);
            Owner()->GetHsm().ProcessEvent(p);
        }else if(input[0] == (char)0x09 && input[1] == (char)0x01) {
            LOG(INFO) << "stop the device receive message";
            Owner()->flagtosend = true;
            // Ptr<EvRxSendingRsp> p(new EvRxSendingRsp);
            Ptr<EvStopRecvRsp> p(new EvStopRecvRsp);
            Owner()->GetHsm().ProcessEvent(p);
        }else if (input[0] == (char)0x0a && input[1] == (char)0x01) {
            LOG(INFO) << "RX DEVICE SEND ENABLE NTF 0A01";
            Ptr<EvRxSendEnableNtf> p(new EvRxSendEnableNtf);
            Owner()->GetHsm().ProcessEvent(p);
        }else if (input[0] == (char)0x1b && input[1] == (char)0x01) {
            if (Owner()->GetRecordSendPackageNum() < Owner()->GetSendPackageNum()) {
                LOG(INFO) << "RX DEVICE RECV PACKAGE NTF 1B01 AND CONTINUE";
                Ptr<EvTranPktContinue> p(new EvTranPktContinue);
                Owner()->GetHsm().ProcessEvent(p);
            }else{
                LOG(INFO) << "RX DEVICE RECV PACKAGE NTF 1B01 AND STOP";
                Ptr<EvTranPktStop> p(new EvTranPktStop);
                Owner()->GetHsm().ProcessEvent(p);
            }

        }else if (input[0] == (char)0x1c && input[1] == (char)0x01) {
            LOG(INFO) << "RX DEVICE STOP RECV DATA 1c01";
            Ptr<EvStopRecvData> p(new EvStopRecvData);
            Owner()->GetHsm().ProcessEvent(p);

        }else if (input[0] == (char)0x1d && input[1] == (char)0x01) {
            LOG(INFO) << "RX DEVICE FINISH SEND DATA 1d01";
            Ptr<EvFinishSendData> p(new EvFinishSendData);
            Owner()->GetHsm().ProcessEvent(p);

        }  else if((input[0] == (char)0x08 && input[1] == (char)0x01) || (input[0] == (char)0x0b && input[1] == (char)0x01)) {
            LOG(INFO) << "RX DEVICE DATA";
            int CRCerror = (input[6] & 0x00ff) | ((input[7] << 8) & 0xff00);
            if ((input[0] == (char)0x0b && input[1] == (char)0x01) || CRCerror != 0) {
                LOG(INFO) << "RX DEVICE ERR DATA";
                // Ptr<EvRxErr> p(new EvRxErr);
                Trace::Instance().Log(PHY_LAYER, PHY_PID, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, "data", "CRCerror");
                // Owner()->GetHsm().ProcessEvent(p);
                Owner()->SetErrorPacket();
            }

            HandleDataNtf(input);

            // int msgLen = (input[4] & 0x00ff) | ((input[5] << 8) & 0xff00);
            // msgLen++;
            // char *msg = new char[msgLen];
            // memset((void*)msg, 0, msgLen);
            // memcpy(msg, input+8, msgLen);

            // pkt::Packet pkt(msg, msgLen);
            // Ptr<MsgRecvDataNtf> m(new msg::MsgRecvDataNtf);
            // m->packet = pkt;
            // delete [] msg;
            // Trace::Instance().Log(PHY_LAYER, PHY_PID, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, "data", "recv");
            // Owner()->SendNtf(m, MAC_LAYER, MAC_PID);
        }
        // else if(input[0] == (char)0x0b && input[1] == (char)0x01) {
        //     LOG(INFO) << "RX DEVICE ERR DATA";
        //     Ptr<EvRxErr> p(new EvRxErr);
        //     Trace::Instance().Log(PHY_LAYER, PHY_PID, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, "data", "CRCerror");
        //     Owner()->GetHsm().ProcessEvent(p);
        // }
        else if(input[0] == (char)0x0a && input[1] == (char)0x01) {
            LOG(INFO) << "RX DATA RSP";
            Ptr<EvDataRsp> p(new EvDataRsp);
            Owner()->GetHsm().ProcessEvent(p);
        }else if (input[0] == (char)0x0d && input[1] == (char)0x01) {
            //Trace::Instance().Log(PHY_LAYER, PHY_PID, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, "data", "CRCerrord1");
        }else{
            LOG(INFO) << "recv useless data";
        }

    }

    void
    MyParser::HandleDataNtf(const char* input) {

        int frameNum = (input[2] & 0x00ff) | ((input[3] << 8) & 0xff00);      //帧号
        int msgLen = (input[4] & 0x00ff) | ((input[5] << 8) & 0xff00);

        recvData.append(input+8, msgLen);
	Owner()->IncrRecordRecvFrameNum();

	LOG(INFO) << "frameNum is " << Owner()->GetRecordRecvFrameNum() << " GetFrameNum is " << Owner()->GetFrameNum();

        if (Owner()->GetRecordRecvFrameNum() == Owner()->GetFrameNum()) {
            if (Owner()->GetErrorPacket()) {
                LOG(INFO) << "This is a error packet!";
                if(upError==1){
                    LOG(INFO) << "RECV DATA AND SEND NTF ";
                    pkt::Packet pkt(const_cast<char*>(recvData.c_str()), recvData.size());
                    Ptr<MsgRecvDataNtf> m(new msg::MsgRecvDataNtf);
                    m->packet = pkt;
                    // Trace::Instance().Log(PHY_LAYER, PHY_PID, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, "data", "recv");
                    Owner()->SendNtf(m, MAC_LAYER, MAC_PID);
                }
            }else{
                LOG(INFO) << "RECV DATA AND SEND NTF ";
                pkt::Packet pkt(const_cast<char*>(recvData.c_str()), recvData.size());
                Ptr<MsgRecvDataNtf> m(new msg::MsgRecvDataNtf);
                m->packet = pkt;
                // Trace::Instance().Log(PHY_LAYER, PHY_PID, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, "data", "recv");
                Owner()->SendNtf(m, MAC_LAYER, MAC_PID);
            }
                recvData = "";
                Owner()->ClearRecordRecvFrameNum();
                Ptr<EvFinishRecvData> p(new EvFinishRecvData);
                Owner()->GetHsm().ProcessEvent(p);
        }else{
            Ptr<EvStartRecvData> p(new EvStartRecvData);
            Owner()->GetHsm().ProcessEvent(p);
        }
    }



    template<typename EventType>
    void FskNewClient::SendDownMsg(const Ptr<EventType> &) {
        if(!IsConnected()){
            return;
        }
        LOG(INFO) << "fsknewclient send down msg";

        IODataPtr pkt = sendingPktQueue.front();
        sendingPktQueue.pop();
        recordSendPackageNum++;
        WriteQueue::Push(pkt);

        Write();
    }

    void
    FskNewClient::SendSendEnableCom(const Ptr<EvStopRecvRsp> &) {
        if (!IsConnected()) {
            return;
        }
        LOG(INFO) << "Send 0a00 : send enable command";

        char *msg2 = new char[PER_MSG_SIZE];
        memset(msg2, 0, PER_MSG_SIZE);

        short transmitAmpshort = (short)(transmitAmp * 32767);
        msg2[0] = 0x0a;
        msg2[1] = 0x00;
        msg2[2] = (char)(transmitAmpshort & 0x00ff);
        msg2[3] = (char)(((transmitAmpshort & 0xff00) >> 8) & 0x00ff);

        msg2[4] = (char)((sampleRate >> 16) & 0x000000ff);
        msg2[5] = (char)((sampleRate >> 24) & 0x000000ff);
        msg2[6] = (char)((sampleRate >> 0) & 0x000000ff);
        msg2[7] = (char)((sampleRate >> 8) & 0x000000ff);

        msg2[8] = (char)(((frameNum * 2) >> 0) & 0x00ff);
        msg2[9] = (char)(((frameNum * 2) >> 8) & 0x00ff);

        msg2[10] = (char)((cyclePrefixMs >> 0) & 0x00ff);
        msg2[11] = (char)((cyclePrefixMs >> 8) & 0x00ff);

        msg2[12] = (char)(isHighSpeed & 0x00ff);
        msg2[13] = (char)((isHighSpeed >> 8) & 0x00ff);

        IODataPtr pkt2(new IOData);

        pkt2->fd   = GetFd();
        pkt2->len  = PER_MSG_SIZE;
        pkt2->data = msg2;

        WriteQueue::Push(pkt2);
        // GetClientsocket().Write();
        Write();
    }
    
    void FskNewClient::SendEnable2(const Ptr<MsgTimeOut> &)
    {   
        if(!IsConnected()){
            return;
        }
        LOG(INFO) << "stop the device to recv data 0900";
        char *msg = new char[PER_MSG_SIZE];
        memset(msg, 0, PER_MSG_SIZE);
        // 禁止通信机接收数据
        msg[0] = (char)0x09;
        msg[1] = (char)0x00;

        IODataPtr pkt(new IOData);
        pkt->fd   = GetFd();
        pkt->data = msg;
        pkt->len  = PER_MSG_SIZE;

        WriteQueue::Push(pkt);
        Write();
    }

    template<typename MsgType, DataOrAck MsgFlag>
    void FskNewClient::SendEnable(const Ptr<MsgType> &Upmsg) {
        if(!IsConnected()){
            return;
        }

        flag = MsgFlag;

        auto pkt = Upmsg->packet;
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
                dataMsg[0] = 0x1b;
                dataMsg[1] = 0x00;
                int leftDataLen = sendBytesNum - i * dataPerBag;
                dataMsg[2] = (char)(leftDataLen & 0x00ff);
                dataMsg[3] = (char)((leftDataLen >> 8) & 0x00ff);

                memcpy(dataMsg + 8, newPkt + i * dataPerBag, leftDataLen - zeroBytesNum); // 在最后一个包中，余下的数据段减去二次补零数据段就是剩余的真实数据（真实指的是非二次补零数据的数据段）
                IODataPtr sendPkt(new IOData);
                sendPkt->fd = GetFd();
                sendPkt->len = lengthPerBag;
                sendPkt->data = dataMsg;
                sendingPktQueue.push(sendPkt);
            }else{
                char* dataMsg = new char[lengthPerBag];
                memset(dataMsg, 0, lengthPerBag);
                dataMsg[0] = 0x1b;
                dataMsg[1] = 0x00;
                dataMsg[2] = (char)(dataPerBag & 0x00ff);
                dataMsg[3] = (char)((dataPerBag >> 8) & 0x00ff);

                memcpy(dataMsg + 8, newPkt + i * dataPerBag, dataPerBag);
                IODataPtr sendPkt(new IOData);
                sendPkt->fd = GetFd();
                sendPkt->len = lengthPerBag;
                sendPkt->data = dataMsg;
                sendingPktQueue.push(sendPkt);
            }
        }
        recordSendPackageNum = 0;

        // SendDownMsg(pkt2);        

        char *msg = new char[PER_MSG_SIZE];
        memset(msg, 0, PER_MSG_SIZE);
        // 禁止通信机接收数据
        msg[0] = (char)0x09;
        msg[1] = (char)0x00;

        IODataPtr pkt2(new IOData);
        pkt2->fd   = GetFd();
        pkt2->data = msg;
        pkt2->len  = PER_MSG_SIZE;

        WriteQueue::Push(pkt2);
        Write();
    }

    template<typename EventType, bool isSendRsp>
    void FskNewClient::RecvEnable(const Ptr<EventType> &event) {
        if(!IsConnected()){
            return;
        }

        // 允许接收数据
        LOG(INFO) << "allow receive data send 0800";
        char* msg = new char[PER_MSG_SIZE];
        memset(msg, 0, PER_MSG_SIZE);

        IODataPtr p(new IOData);
        p->fd   = GetFd();
        p->data = msg;
        p->len  = PER_MSG_SIZE;

        msg[0] = (char)0x08;
        msg[1] = (char)0x00;
        msg[2] = (char)(gain & 0x00ff);
        msg[3] = (char)((gain >> 8) & 0x00ff);
        msg[4] = (char)((correlateThreadShort >> 0) & 0x00ff);
        msg[5] = (char)((correlateThreadShort >> 8) & 0x00ff);

        msg[6] = (char)((sampleRate >> 16) & 0x000000ff);
        msg[7] = (char)((sampleRate >> 24) & 0x000000ff);
        msg[8] = (char)((sampleRate >> 0) & 0x000000ff);
        msg[9] = (char)((sampleRate >> 8) & 0x000000ff);

        msg[10] = (char)((frameNum * 2) & 0x00ff);
        msg[11] = (char)(((frameNum * 2) >> 8) & 0x00ff);

        msg[12] = (char)(channelNum & 0x00ff);
        msg[13] = (char)((channelNum >> 8) & 0x00ff);

        msg[14] = (char)(cyclePrefixMs & 0x00ff);
        msg[15] = (char)((cyclePrefixMs >> 8) & 0x00ff);

        msg[16] = (char)(isHighSpeed & 0x00ff);
        msg[17] = (char)((isHighSpeed >> 8) & 0x00ff);

        msg[18] = (char)(agcIsOn & 0x00ff);
        msg[19] = (char)((agcIsOn >> 8) & 0x00ff);

        msg[20] = (char)(uploadWave & 0x00ff);
        msg[21] = (char)((uploadWave >> 8) & 0x00ff);

	ClearRecordRecvFrameNum();
        WriteQueue::Push(p);
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
            SetTimer(readCycle);
        }
    }

    void
    FskNewClient::SendDevStopRecvDataCom(const Ptr<EvTranPktStop> &) {
        if (!IsConnected()) {
            return;
        }

        LOG(INFO) << "Send the 1C00 : Tell Dev Stop Recv Data and Send Data Command";
        char *msg = new char[lengthPerBag];
        memset(msg, 0, lengthPerBag);

        msg[0] = (char)0x1c;
        msg[1] = (char)0x00;

        IODataPtr pkt(new IOData);
        pkt->fd   = GetFd();
        pkt->data = msg;
        pkt->len  = lengthPerBag;

        WriteQueue::Push(pkt);
        // GetClientsocket().Write();
        Write();
    }


MODULE_INIT(FskNewClient)
#endif
}
