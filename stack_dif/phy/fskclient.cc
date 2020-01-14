#include "driver.h"
#include "schedule.h"
#include "clientsocket.h"
#include "hsm.h"
#include "message.h"
#include "packet.h"
#include <stdlib.h>
#include <string>

#include "client.h"
#include "app_datapackage.h"

#define CURRENT_PID 16
#define PER_MSG_SIZE 1032

extern client* UIClient;

namespace fskclient{
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


    struct Top;
    struct Idle;
    struct SendSettingReq;
    struct WaitSettingRsp;
    struct WaitConNtf;
    struct WaitDataRsp;
    struct WaitSendingData;
    struct TimeOutHandle;

    typedef enum{
        Data, 
        Ack
    }DataOrAck;

    class FskClient;

    class MyParser{
    public:
        void Parse(const char *);
        inline void SetOwner(FskClient *ptr){owner_ = ptr;};
        inline FskClient* Owner() const { return owner_;};

    private:
        FskClient *owner_;

    };


class FskClient : public drv::Driver<FskClient, PHY_LAYER, CURRENT_PID>
{
public:
    FskClient() {
        DRIVER_CONSTRUCT(Top);
        // GetSap().SetOwner(this);
        // GetDap().SetOwner(this);
        // GetHsm().Initialize<Top>(this);
        GetParser().SetOwner(this);
    }

    void Init()
    {
        TimeTxSettingREQ = 0;
        sampleRate = Config::Instance()["fskclient"]["samplerate"].as<int>();//80000;
        gain = Config::Instance()["fskclient"]["gain"].as<double>() / 2.5 * 65535; //0.1 /2.5 * 65535;
        cyclePrefixMs = Config::Instance()["fskclient"]["cycleprefixms"].as<short>(); //20;//
        correlateThreadShort = Config::Instance()["fskclient"]["correlatethreadshort"].as<double>() * 65535; //0.1 * 65535;
        transmitAmp = Config::Instance()["fskclient"]["transmitAmp"].as<double>();//0.1;
        if (transmitAmp > 1.0) {
            transmitAmp = 1.0;
        }
        if (transmitAmp < 0.0) {
            transmitAmp = 0.0;
        }
        flagtosend = false;
        msgLenMax = 42;         //

        readCycle = Config::Instance()["fskclient"]["readcycle"].as<uint16_t>();


        
        string server_addr = Config::Instance()["fskclient"]["address"].as<std::string>();//"192.168.2.101";
        string server_port = Config::Instance()["fskclient"]["port"].as<std::string>();//"80";
        LOG(INFO) << "FskClientSimulate Init and the ip is " << server_addr << " the port is " << server_port;

        clientsocketWPtr_ = ClientSocket::NewClient(server_addr, server_port);

        if (!clientsocketWPtr_.expired()) {
            LOG(INFO) << "Map the opened fd of socket and DAP";
            GetDap().Map(GetFd()); //建立fd与dap的键值关系
        }
    }

    void SendDownSetting1(const Ptr<EvRxConNtf> &);
    void SendDownSetting2(const Ptr<EvRxSettingRsp> &);
    template<typename MsgType, DataOrAck MsgFlag>
    void SendEnable(const Ptr<MsgType> &);
    void SendEnable2(const Ptr<MsgTimeOut> &);
    void SendDownMsg(const Ptr<EvRxSendingRsp> &);
    template<typename EventType, bool isSendRsp>
    void RecvEnable(const Ptr<EventType> &);

    void Notify()
    {
        LOG(INFO) << "FSKClientsimulate receive data";

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
    bool flagtosend;
    
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
    int msgLenMax;
    DataOrAck flag;
    IODataPtr sendingPkt;
    uint16_t readCycle;
};

    struct Top : hsm::State<Top, hsm::StateMachine, WaitConNtf>
    {
        typedef hsm_vector<EvRxErr, MsgSendDataReq, MsgSendAckReq, MsgTimeOut> reactions;
        HSM_DEFER(MsgSendAckReq);
        HSM_DEFER(MsgSendDataReq);  //
        HSM_DISCARD(EvRxErr);
        //HSM_WORK(MsgTimeOut, (&FskClient::RecvEnable<MsgTimeOut, false>));
        HSM_DEFER(MsgTimeOut);
    };

    struct WaitConNtf : hsm::State<WaitConNtf, Top>{
        typedef hsm_vector<EvRxConNtf, EvTxSetting> reactions;
        //带一个输出的状态跳转，估计是先执行输出函数再进行状态跳转，因此下面需要延迟事件，等到跳转到相应状态才派发事件，因为该事件直接触发SendSettingReq状态跳到下一个状态
        HSM_TRANSIT(EvRxConNtf, SendSettingReq, &FskClient::SendDownSetting1);
        HSM_DEFER(EvTxSetting); // 先延迟一下，等离开状态才派发
    };

    struct SendSettingReq : hsm::State<SendSettingReq, Top>{
        typedef hsm_vector<EvTxSetting, EvRxSettingRsp, EvRxSettingAllRsp> reactions;
        HSM_DEFER(EvRxSettingRsp); // 预防设备过快返回一个RSP，因此延迟该事件
        HSM_DEFER(EvRxSettingAllRsp);
        HSM_TRANSIT(EvTxSetting, WaitSettingRsp);

    };

    struct WaitSettingRsp : hsm::State<WaitSettingRsp, Top>{

        typedef hsm_vector<EvTxSetting, EvRxSettingRsp, EvRxSettingAllRsp> reactions;
        HSM_TRANSIT(EvRxSettingRsp, SendSettingReq, &FskClient::SendDownSetting2);
        HSM_DEFER(EvTxSetting);
        HSM_TRANSIT(EvRxSettingAllRsp, Idle, (&FskClient::RecvEnable<EvRxSettingAllRsp, false>));
    };

    struct Idle : hsm::State<Idle, Top>
    {
        typedef hsm_vector<MsgSendDataReq, MsgSendAckReq, MsgTimeOut> reactions;
        HSM_TRANSIT(MsgSendDataReq, WaitSendingData, (&FskClient::SendEnable<MsgSendDataReq, Data>));
        HSM_TRANSIT(MsgSendAckReq, WaitSendingData, (&FskClient::SendEnable<MsgSendAckReq, Ack>));
        HSM_TRANSIT(MsgTimeOut, TimeOutHandle, &FskClient::SendEnable2);
    };
    
    struct TimeOutHandle : hsm::State<TimeOutHandle, Top>
    {
        typedef hsm_vector<EvRxSendingRsp> reactions;
        HSM_TRANSIT(EvRxSendingRsp, Idle, (&FskClient::RecvEnable<EvRxSendingRsp, false>));
    };

    struct WaitSendingData : hsm::State<WaitSendingData, Top>
    {
        typedef hsm_vector<EvRxSendingRsp> reactions;
        HSM_TRANSIT(EvRxSendingRsp, WaitDataRsp, &FskClient::SendDownMsg);
    };

    struct WaitDataRsp : hsm::State<WaitDataRsp, Top>
    {
        typedef hsm_vector<EvDataRsp> reactions;

        HSM_TRANSIT(EvDataRsp, Idle, (&FskClient::RecvEnable<EvDataRsp, true>));
    };



    void MyParser::Parse(const char *input){

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
            Ptr<EvRxSendingRsp> p(new EvRxSendingRsp);
            Owner()->GetHsm().ProcessEvent(p);
        }else if((input[0] == (char)0x08 && input[1] == (char)0x01) || (input[0] == (char)0x0b && input[1] == (char)0x01)) {
            LOG(INFO) << "RX DEVICE DATA";
            int CRCerror = (input[6] & 0x00ff) | ((input[7] << 8) & 0xff00);
            if ((input[0] == (char)0x0b && input[1] == (char)0x01) || CRCerror != 0) {
                LOG(INFO) << "RX DEVICE ERR DATA";
                // Ptr<EvRxErr> p(new EvRxErr);
                Trace::Instance().Log(PHY_LAYER, PHY_PID, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, "data", "CRCerror");
                // Owner()->GetHsm().ProcessEvent(p);
            }

            int msgLen = (input[4] & 0x00ff) | ((input[5] << 8) & 0xff00);
            msgLen++;
            char *msg = new char[msgLen];
            memset((void*)msg, 0, msgLen);
            memcpy(msg, input+8, msgLen);

		/*	if(UIClient != NULL)
			{
				struct App_DataPackage recvPackage;
				memcpy(&recvPackage, msg, sizeof(recvPackage));
				recvPackage.Package_type = 6;
				recvPackage.data_buffer[0] = PHY_LAYER;

				char* sendBuff = new char[sizeof(recvPackage)];
				memcpy(sendBuff, &recvPackage, sizeof(recvPackage));
				IODataPtr pkt(new IOData);


				pkt->fd   = UIClient->_socketfd;
				pkt->data = sendBuff;
				pkt->len  = sizeof(recvPackage);
				UIWriteQueue::Push(pkt);
				cliwrite(UIClient);
			}*/


            pkt::Packet pkt(msg, msgLen);
            Ptr<MsgRecvDataNtf> m(new msg::MsgRecvDataNtf);
            m->packet = pkt;
            delete [] msg;
            Trace::Instance().Log(PHY_LAYER, PHY_PID, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, "data", "recv");
            Owner()->SendNtf(m, MAC_LAYER, MAC_PID);
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
    FskClient::SendDownSetting1(const Ptr<EvRxConNtf> &){
        if(!IsConnected()){
            return;
        }
        // 触发EvTxSetting事件
        LOG(INFO) << "TX SETTING REQ";
        Ptr<EvTxSetting> p(new EvTxSetting);
        GetHsm().ProcessEvent(p);

        LOG(INFO) << "Try to send setting information one";
        char *msg = new char[PER_MSG_SIZE];
        memset(msg, 0, PER_MSG_SIZE);
        msg[0] = (char)0x06;
        msg[1] = (char)0x00;
        msg[2] = (char)(gain & 0x00ff);
        msg[3] = (char)((gain >> 8) & 0x00ff);
        msg[4] = (char)((sampleRate >> 16) & 0x000000ff);
        msg[5] = (char)((sampleRate >> 24) & 0x000000ff);
        msg[6] = (char)((sampleRate >> 0) & 0x000000ff);
        msg[7] = (char)((sampleRate >> 8) & 0x000000ff);

        IODataPtr pkt(new IOData);
        pkt->fd   = GetFd();
        pkt->data = msg;
        pkt->len  = PER_MSG_SIZE;

        WriteQueue::Push(pkt);
        Write();
        TimeTxSettingREQ++;
    }

    void
    FskClient::SendDownSetting2(const Ptr<EvRxSettingRsp> &){
        if(!IsConnected()){
            return;
        }
        LOG(INFO) << "TX SETTING REQ";
        Ptr<EvTxSetting> p(new EvTxSetting);
        GetHsm().ProcessEvent(p);

        char *msg = new char[PER_MSG_SIZE];
        memset(msg, 0, PER_MSG_SIZE);

        if (TimeTxSettingREQ == 1) {
            LOG(INFO) << "Try to send setting information two";

            msg[0] = (char)0x07;
            msg[1] = (char)0x00;
            msg[2] = (char)0x00;
            msg[3] = (char)0x00;
            msg[4] = (char)((sampleRate >> 16) & 0x000000ff);
            msg[5] = (char)((sampleRate >> 24) & 0x000000ff);
            msg[6] = (char)((sampleRate >> 0) & 0x000000ff);
            msg[7] = (char)((sampleRate >> 8) & 0x000000ff);
        }else if (TimeTxSettingREQ == 2) {
            LOG(INFO) << "Try to send setting information three";

            msg[0] = (char)0x0c;
            msg[1] = (char)0x00;
            msg[2] = (char)(cyclePrefixMs & 0x00ff);
            msg[3] = (char)((cyclePrefixMs >> 8) & 0x00ff);
            msg[4] = (char)((correlateThreadShort >> 0) & 0x00ff);
            msg[5] = (char)((correlateThreadShort >> 8) & 0x00ff);

        }

        IODataPtr pkt(new IOData);
        pkt->fd   = GetFd();
        pkt->data = msg;
        pkt->len  = PER_MSG_SIZE;

        WriteQueue::Push(pkt);
        Write();
        TimeTxSettingREQ++;

     }
     
    void
    FskClient::SendDownMsg(const Ptr<EvRxSendingRsp> &msg){
        if(!IsConnected()){
            return;
        }
        LOG(INFO) << "Fskclient send down msg";
		
        WriteQueue::Push(sendingPkt);
        Trace::Instance().Log(PHY_LAYER, PHY_PID, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, "data", "send");
        Write();
    }
    
    void FskClient::SendEnable2(const Ptr<MsgTimeOut> &)
    {   
        if(!IsConnected()){
            return;
        }
            
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
    void FskClient::SendEnable(const Ptr<MsgType> &Upmsg){
        if(!IsConnected()){
            return;
        }
        

        flag = MsgFlag;
        char *msg2 = new char[PER_MSG_SIZE];
        memset(msg2, 0, PER_MSG_SIZE);

        short transmitAmpshort = (short)(transmitAmp * 32767);
        msg2[0] = 0x0a;
        msg2[1] = 0x00;
        msg2[2] = (char)(transmitAmpshort & 0x00ff);
        msg2[3] = (char)(((transmitAmpshort & 0xff00) >> 8) & 0x00ff);
        msg2[4] = (char)(msgLenMax & 0x000000ff);
        msg2[5] = (char)(((msgLenMax & 0x0000ff00) >> 8) & 0x000000ff);
        msg2[6] = 0x00;
        msg2[7] = 0x00;

        auto uppkt = Upmsg->packet;

        memcpy(msg2+8, uppkt.Raw(), uppkt.Size());

        IODataPtr pkt2(new IOData);

        pkt2->fd   = GetFd();
        pkt2->len  = PER_MSG_SIZE;
        pkt2->data = msg2;

        sendingPkt = pkt2;

        // SendDownMsg(pkt2);        


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

    template<typename EventType, bool isSendRsp>
    void FskClient::RecvEnable(const Ptr<EventType> &){
        if(!IsConnected()){
            return;
        }

        // 允许接收数据
        LOG(INFO) << "allow receive data ";
        char* msg = new char[PER_MSG_SIZE];
        memset(msg, 0, PER_MSG_SIZE);

        IODataPtr p(new IOData);
        p->fd   = GetFd();
        p->data = msg;
        p->len  = PER_MSG_SIZE;

        msg[0] = (char)0x08;
        msg[1] = (char)0x00;

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

MODULE_INIT(FskClient)
#endif
}
