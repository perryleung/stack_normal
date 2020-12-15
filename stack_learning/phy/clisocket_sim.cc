#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "driver.h"
#include "schedule.h"
#include "clientsocket.h"

#include <boost/regex.hpp>
#include <cassert>
#include <string>
#include <cstring>
#include <string>
#include "dap.h"
#include "driver.h"
// #include "event.h"
#include "hsm.h"
#include "message.h"
#include "packet.h"
#include "schedule.h"
#include <stdlib.h>
#include "client.h"
#include "app_datapackage.h"

#define CURRENT_PID 4
#define PER_MSG_SIZE 1032

extern client* UIClient;

namespace clisocket_sim{
#if (PHY_PID | CURRENT_PID) == PHY_PID

using msg::MsgSendDataReq;
using msg::MsgSendAckReq;
using msg::MsgSendDataRsp;
using msg::MsgSendAckRsp;
using msg::MsgRecvDataNtf;
using msg::MsgTimeOut;
// using msg::MsgPosReq;
// using msg::MsgPosRsp;

using pkt::Packet;
// using msg::Position;
using std::string;


        // 定义状态事件，可在函数中编程触发的事件
    template <typename T>
    struct EvBase : hsm::Event<T>   // 继承event事件并作一些扩展
    {
        //DATA_INDEX data_index;
        boost::cmatch result;
    };
/*
    struct EvRxData : EvBase<EvRxData>
    {
        size_t msg_len;
        char* msg;
    };*/
    struct EvRxErr : EvBase<EvRxErr>
    {

    };
    struct EvRxConNtf : EvBase<EvRxConNtf>
    {

    };
    struct EvTxSetting : EvBase<EvTxSetting>
    {

    };
    struct EvRxSettingRsp : EvBase<EvRxSettingRsp>
    {

    };
    struct EvRxSettingAllRsp : EvBase<EvRxSettingAllRsp>
    {

    };
    struct EvDataRsp : EvBase<EvDataRsp>
    {

    };
    struct EvRxSendingRsp : EvBase<EvRxSendingRsp>
    {

    };


    struct Top;
    struct Idle;
    
    struct SendSettingReq;
    struct WaitSettingRsp;
    struct WaitConNtf;
    struct WaitDataRsp;
    struct WaitSendingData;

    typedef enum{
        Data, 
        Ack
    }DataOrAck;

    class ClisocketSim;

    class MyParser{
    public:
        void Parse(const char *);
        inline void SetOwner(ClisocketSim *ptr){owner_ = ptr;};
        inline ClisocketSim* Owner() const { return owner_;};

    private:
        ClisocketSim *owner_;

    };


class ClisocketSim : public drv::Driver<ClisocketSim, PHY_LAYER, CURRENT_PID>
{
public:
    ClisocketSim() {
        DRIVER_CONSTRUCT(Top);
        // GetSap().SetOwner(this);
        // GetDap().SetOwner(this);
        // GetHsm().Initialize<Top>(this);
        GetParser().SetOwner(this);
    }

    void Init()
    {
        TimeTxSettingREQ = 0;
        sampleRate = Config::Instance()["clientsocket"]["samplerate"].as<int>();//80000;
        gain = Config::Instance()["clientsocket"]["gain"].as<double>() / 2.5 * 65535; //0.1 /2.5 * 65535;
        cyclePrefixMs = Config::Instance()["clientsocket"]["cycleprefixms"].as<short>(); //20;//
        correlateThreadShort = Config::Instance()["clientsocket"]["correlatethreadshort"].as<double>() * 65535; //0.1 * 65535;
        transmitAmp = Config::Instance()["clientsocket"]["gain"].as<double>();//0.1;
        if (transmitAmp > 1.0) {
            transmitAmp = 1.0;
        }
        if (transmitAmp < 0.0) {
            transmitAmp = 0.0;
        }
        flagtosend = false;
        msgLenMax = 42;         //
        
        std::string server_addr = Config::Instance()["clientsocket"]["address"].as<std::string>();//"192.168.2.101";
        std::string server_port = Config::Instance()["clientsocket"]["port"].as<std::string>();//"80";
        LOG(INFO) << "ClientsocketSimulate Init and the ip is " << server_addr << " the port is " << server_port;

        clientsocketWPtr_ = ClientSocket::NewClient(server_addr, server_port);

        if (!clientsocketWPtr_.expired()) {
            LOG(INFO) << "Map the opened fd of socket and DAP";
            GetDap().Map(GetFd()); //建立fd与dap的键值关系
        }
    }

   // void SendUp(const Ptr<EvRxData> &);
    void SendDownSetting1(const Ptr<EvRxConNtf> &);
    void SendDownSetting2(const Ptr<EvRxSettingRsp> &);
    // void SendDownAck(const Ptr<MsgSendAckReq> &);
    // void SendDownData(const Ptr<MsgSendDataReq> &);
    template<typename MsgType, DataOrAck MsgFlag>
    void SendEnable(const Ptr<MsgType> &);
    void SendDownMsg(const Ptr<EvRxSendingRsp> &);
    void SendDataRsp(const Ptr<EvDataRsp> &);
    void RecvEnable(const Ptr<EvRxSettingAllRsp> &);

    void Notify()
    {
        LOG(INFO) << "Clientsocketsimulate receive data";

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
};

    struct Top : hsm::State<Top, hsm::StateMachine, WaitConNtf>
    {
        typedef hsm_vector<EvRxErr, MsgSendDataReq> reactions;

        HSM_DEFER(MsgSendDataReq);  //
        HSM_DISCARD(EvRxErr);
    };

    struct WaitConNtf : hsm::State<WaitConNtf, Top>{
        typedef hsm_vector<EvRxConNtf, EvTxSetting, MsgSendDataReq> reactions;

        //带一个输出的状态跳转，估计是先执行输出函数再进行状态跳转，因此下面需要延迟事件，等到跳转到相应状态才派发事件，因为该事件直接触发SendSettingReq状态跳到下一个状态
        HSM_TRANSIT(EvRxConNtf, SendSettingReq, &ClisocketSim::SendDownSetting1);
        HSM_DEFER(EvTxSetting); // 先延迟一下，等离开状态才派发
        HSM_DEFER(MsgSendDataReq); 
    };

    struct SendSettingReq : hsm::State<SendSettingReq, Top>{
        typedef hsm_vector<EvTxSetting, EvRxSettingRsp, EvRxSettingAllRsp, MsgSendDataReq> reactions;
        HSM_DEFER(EvRxSettingRsp); // 预防设备过快返回一个RSP，因此延迟该事件
        HSM_DEFER(EvRxSettingAllRsp);
        HSM_DEFER(MsgSendDataReq); 
        HSM_TRANSIT(EvTxSetting, WaitSettingRsp);

    };

    struct WaitSettingRsp : hsm::State<WaitSettingRsp, Top>{

        typedef hsm_vector<EvTxSetting, EvRxSettingRsp, EvRxSettingAllRsp, MsgSendDataReq> reactions;

        HSM_TRANSIT(EvRxSettingRsp, SendSettingReq, &ClisocketSim::SendDownSetting2);
        HSM_DEFER(EvTxSetting);
        HSM_TRANSIT(EvRxSettingAllRsp, Idle, &ClisocketSim::RecvEnable);
        HSM_DEFER(MsgSendDataReq); // 准备派发事件
    };

    struct Idle : hsm::State<Idle, Top>
    {
        typedef hsm_vector<MsgSendDataReq, MsgSendAckReq> reactions;

        // HSM_TRANSIT(MsgSendDataReq, WaitSendingData, &ClisocketSim::SendDownData);
        // HSM_TRANSIT(MsgSendAckReq, WaitSendingData, &ClisocketSim::SendDownAck);
        HSM_TRANSIT(MsgSendDataReq, WaitSendingData, &ClisocketSim::SendEnable<MsgSendDataReq, Data>);
        HSM_TRANSIT(MsgSendAckReq, WaitSendingData, &ClisocketSim::SendEnable<MsgSendAckReq, Ack>);

    };

    struct WaitSendingData : hsm::State<WaitSendingData, Top>
    {
        typedef hsm_vector<EvRxSendingRsp, MsgSendAckReq, MsgSendDataReq> reactions;
        HSM_TRANSIT(EvRxSendingRsp, WaitDataRsp, &ClisocketSim::SendDownMsg);
        HSM_DEFER(MsgSendAckReq);
        HSM_DEFER(MsgSendDataReq);


    };

    struct WaitDataRsp : hsm::State<WaitDataRsp, Top>
    {

        typedef hsm_vector<EvDataRsp, MsgSendAckReq, MsgSendDataReq> reactions;

        HSM_TRANSIT(EvDataRsp, Idle, &ClisocketSim::SendDataRsp);
        HSM_DEFER(MsgSendAckReq);
        HSM_DEFER(MsgSendDataReq);
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
        }else if(input[0] == (char)0x08 && input[1] == (char)0x01) {
            LOG(INFO) << "RX DEVICE DATA";
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
            Owner()->SendNtf(m, MAC_LAYER, MAC_PID);
        }else if(input[0] == (char)0x0b && input[1] == (char)0x01) {
            LOG(INFO) << "RX DEVICE ERR DATA";
            Ptr<EvRxErr> p(new EvRxErr);
            Owner()->GetHsm().ProcessEvent(p);
        }else if(input[0] == (char)0x0a && input[1] == (char)0x01) {
            LOG(INFO) << "RX DATA RSP";
            Ptr<EvDataRsp> p(new EvDataRsp);
            Owner()->GetHsm().ProcessEvent(p);
        }else{
            LOG(INFO) << "recv useless data";
        }

    }


    void
    ClisocketSim::SendDownSetting1(const Ptr<EvRxConNtf> &){

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
        // GetClientsocket().Write();
        Write();
        TimeTxSettingREQ++;


    }

    void
    ClisocketSim::SendDownSetting2(const Ptr<EvRxSettingRsp> &){
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
        // GetClientsocket().Write();
        Write();
        TimeTxSettingREQ++;

     }
     
    void
    ClisocketSim::SendDownMsg(const Ptr<EvRxSendingRsp> &msg){
        if(!IsConnected()){
            return;
        }
        LOG(INFO) << "Clisocketsim send down msg";
		
        WriteQueue::Push(sendingPkt);
        // GetClientsocket().Write();
        Write();
    }

    template<typename MsgType, DataOrAck MsgFlag>
    void ClisocketSim::SendEnable(const Ptr<MsgType> &Upmsg){
        if(!IsConnected()){
            return;
        }
        // LOG(INFO) << "Clisocketsim send down msg";
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
        // GetClientsocket().Write();
        Write();
    }


    void
    ClisocketSim::SendDataRsp(const Ptr<EvDataRsp> &pmsg){
        if(!IsConnected()){
            return;
        }
          // 允许接收数据
        LOG(INFO) << "allow receive data and send upper rsp";
        char* msg = new char[PER_MSG_SIZE];
        memset(msg, 0, PER_MSG_SIZE);
        IODataPtr p(new IOData);

        p->fd   = GetFd();
        p->data = msg;
        p->len  = PER_MSG_SIZE;

        msg[0] = (char)0x08;
        msg[1] = (char)0x00;


        WriteQueue::Push(p);
        // GetClientsocket().Write();
        Write();
        if(flag == Data){
            Ptr<MsgSendDataRsp> rsp(new MsgSendDataRsp);
            SendRsp(rsp, MAC_LAYER, MAC_PID);
        }else if(flag == Ack){
            Ptr<MsgSendAckRsp> rsp(new MsgSendAckRsp);
            SendRsp(rsp, MAC_LAYER, MAC_PID);
        }
        

        
    }
    
    void
    ClisocketSim::RecvEnable(const Ptr<EvRxSettingAllRsp> &pmsg){
        if(!IsConnected()){
            return;
        }
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
        // GetClientsocket().Write();
        Write();
    }

MODULE_INIT(ClisocketSim)
//PROTOCOL_HEADER(SimulateChannelHeader)
#endif
}
