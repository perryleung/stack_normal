#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>
#include <memory>
#include <arpa/inet.h>
#include "driver.h"
#include "schedule.h"
#include "clientsocket.h"

#define CURRENT_PROTOCOL 64
using std::string;
using std::weak_ptr;

namespace simWOSS_channel {
#if (PHY_PID | CURRENT_PROTOCOL) == PHY_PID

using msg::MsgSendDataReq;
using msg::MsgSendAckReq;
using msg::MsgSendDataRsp;
using msg::MsgSendAckRsp;
using msg::MsgRecvDataNtf;
using msg::MsgTimeOut;
using msg::MsgPosReq;
using msg::MsgPosRsp;
using msg::MsgPosNtf;


using pkt::Packet;
using msg::Position;

//定义事件
struct EvRecvPosNtfRsp : hsm::Event<EvRecvPosNtfRsp>{};
struct EvChannelRsp : hsm::Event<EvChannelRsp>{};

enum PacketType {
    PACKET_TYPE_DATA = 1,
    PACKET_TYPE_POSITION_REQ,
    PACKET_TYPE_POSITION_RSP,
    PACKET_TYPE_ENERGY_REQ,
    PACKET_TYPE_ENERGY_RSP,
    PACKET_TYPE_STATE_REQ,
    PACKET_TYPE_STATE_RSP,
    PACKET_TYPE_DATA_RSP,
    PACKET_TYPE_POSITION_NTF,
    PACKET_TYPE_POSITION_NTF_RSP
};

typedef enum{
    Data,
    Ack
}DataOrAck;


struct Top;
struct Idle;
struct WaitPosNtfRsp;
struct WaitRsp;

struct SimulateWOSSChannelHeader
{
    uint8_t type;
    uint8_t errorFlag;
};

#pragma pack(1)

    struct PositionForTra
    {
        char type;
        uint16_t x;
        uint16_t y;
        uint16_t z;
    };

#pragma pack()

class SimulateWOSSChannel : public drv::Driver<SimulateWOSSChannel, PHY_LAYER, CURRENT_PROTOCOL>
{
public:
    SimulateWOSSChannel() { DRIVER_CONSTRUCT(Top); }
    void Init()
    {
        LOG(INFO) << "SimulateWOSSChannel Init : Map the opened fd of socket and DAP";
        string server_addr = Config::Instance()["simulatechannel"]["address"].as<string>();
        string server_port = Config::Instance()["simulatechannel"]["port"].as<string>();

        channelWPtr_ = ClientSocket::NewClient(server_addr, server_port);

        if (!channelWPtr_.expired()) { // 引用计数非零
            GetDap().Map(GetFd());
            // SendPosReq();
        }
    }

    // void SendDownData(const Ptr<MsgSendDataReq> &);
    // void SendDownAck(const Ptr<MsgSendAckReq> &);

    template<typename ReqType, DataOrAck MsgFlag>
    void SendDown(const Ptr<ReqType> &);

    void SendUpRsp(const Ptr<EvChannelRsp> &);

    void SendPosReq();
    void RespondPosReq(const Ptr<MsgPosReq> &);

    void SendUp(const Ptr<MsgRecvDataNtf> &msg)
    {
        SendNtf(msg, MAC_LAYER, MAC_PID);
    }

    void HandleDataNtf(const Ptr<IOData> &);
    void HandlePosRsp(const Ptr<IOData> &);
    void HandlePosNtf(const Ptr<MsgPosNtf> &);

    void Notify()
    {
        LOG(INFO) << "Simulate Channel receive data";

        IODataPtr data = ReadQueue::Front();

        SimulateWOSSChannelHeader *p = (SimulateWOSSChannelHeader *)data->data;

        switch (p->type) {
            case PACKET_TYPE_DATA:
                HandleDataNtf(data);
                break;
            case PACKET_TYPE_POSITION_RSP:
                HandlePosRsp(data);
                break;
            case PACKET_TYPE_DATA_RSP:
                {
                    LOG(INFO) << "RX DATA RSP";
                    Ptr<EvChannelRsp> event(new EvChannelRsp);
                    GetHsm().ProcessEvent(event);
                    break;
                }
            case PACKET_TYPE_POSITION_NTF_RSP:
                {
                    LOG(INFO) << "RX POS NTF RSP";
                    Ptr<EvRecvPosNtfRsp> event(new EvRecvPosNtfRsp);
                    GetHsm().ProcessEvent(event);
                    break;
                }
            default:
                break;
        }

        ReadQueue::Pop();
        free(data->data);
    }

    inline ClientSocketPtr GetChannel()
    {
        if (channelWPtr_.expired()) {
            LOG(DEBUG) << "the Simulate channel is disconnected";
            return NULL;
        }
        return channelWPtr_.lock();
        // return channel_;
    }

    inline int GetFd()
    {
        ClientSocketPtr temp = GetChannel();
        if (temp == NULL) {
            return 2;           // stderr
        }
        return temp->GetFd();
    }

    inline bool IsConnected()
    {
        if (GetChannel() == NULL) {
            return false;
        }
        return true;
    }

    inline void Write(){
        ClientSocketPtr temp = GetChannel();
        if (temp == NULL) {
            return;
        }
        temp->Write();
    }

private:
    Position position_;
    bool has_req_for_pos_;
    // ClientSocket channel_;
    weak_ptr<ClientSocket> channelWPtr_;
    DataOrAck flag;

};

struct Top : hsm::State<Top, hsm::StateMachine, Idle>
{
    typedef hsm_vector<MsgSendDataReq, MsgRecvDataNtf, MsgPosReq, MsgPosNtf> reactions;

    HSM_DEFER(MsgSendDataReq);
    HSM_DEFER(MsgSendAckReq);
    HSM_WORK(MsgRecvDataNtf, &SimulateWOSSChannel::SendUp);
    HSM_WORK(MsgPosReq, &SimulateWOSSChannel::RespondPosReq);
    HSM_DEFER(MsgPosNtf);

};

struct Idle : hsm::State<Idle, Top>
{
    typedef hsm_vector<MsgSendDataReq, MsgSendAckReq, MsgPosNtf> reactions;

    HSM_TRANSIT(MsgPosNtf, WaitPosNtfRsp, &SimulateWOSSChannel::HandlePosNtf);
    // HSM_WORK(MsgSendDataReq, (&SimulateWOSSChannel::SendDown<MsgSendDataReq, MsgSendDataRsp>));
    // HSM_WORK(MsgSendAckReq, (&SimulateWOSSChannel::SendDown<MsgSendAckReq, MsgSendAckRsp>));
    HSM_TRANSIT(MsgSendDataReq, WaitRsp, (&SimulateWOSSChannel::SendDown<MsgSendDataReq, Data>));
    HSM_TRANSIT(MsgSendAckReq, WaitRsp, (&SimulateWOSSChannel::SendDown<MsgSendAckReq, Ack>));

    // HSM_TRANSIT(MsgSendDataReq, Idle, &SimulateWOSSChannel::template SendDown<MsgSendDataReq, MsgSendDataRsp>);
    // HSM_TRANSIT(MsgSendAckReq, Idle, &SimulateWOSSChannel::template SendDown<MsgSendAckReq, MsgSendAckRsp>);

};

struct WaitPosNtfRsp : hsm::State<WaitPosNtfRsp, Top>
{
    typedef hsm_vector<EvRecvPosNtfRsp> reactions;
    HSM_TRANSIT(EvRecvPosNtfRsp, Idle);
};

struct WaitRsp : hsm::State<WaitRsp, Top>
{
    typedef hsm_vector<EvChannelRsp> reactions;
    HSM_TRANSIT(EvChannelRsp, Idle, &SimulateWOSSChannel::SendUpRsp);
};

struct Busy : hsm::State<Busy, Top>
{
};

struct Tx : hsm::State<Tx, Top>
{
};
struct Rx : hsm::State<Tx, Top>
{
};
struct Sleep : hsm::State<Tx, Top>
{
};


template<typename ReqType, DataOrAck MsgFlag>
void SimulateWOSSChannel::SendDown(const Ptr<ReqType> &req){

    if (!IsConnected()) {
        return;
    }

    flag = MsgFlag;
    SimulateWOSSChannelHeader *h = req->packet.template Header<SimulateWOSSChannelHeader>();
    h->type = PACKET_TYPE_DATA;
    req->packet.template Move<SimulateWOSSChannelHeader>();

    auto pkt = req->packet;
    char* msg = new char[pkt.Size()];
    memcpy(msg, pkt.Raw(), pkt.Size());

    IODataPtr p(new IOData);
    p->fd = GetFd();
    p->len = pkt.Size();
    p->data = msg;

    WriteQueue::Push(p);

    // GetChannel()->Write();
    Write();

    // Ptr<RspType> rsp(new RspType);
    // SendRsp(rsp, MAC_LAYER, MAC_PID);
}

    void SimulateWOSSChannel::SendUpRsp(const Ptr<EvChannelRsp> &){
        if(flag == Data){
            Ptr<MsgSendDataRsp> rsp(new MsgSendDataRsp);
            SendRsp(rsp, MAC_LAYER, MAC_PID);
        }else if(flag == Ack){
            Ptr<MsgSendAckRsp> rsp(new MsgSendAckRsp);
            SendRsp(rsp, MAC_LAYER, MAC_PID);
        }
    }



void SimulateWOSSChannel::RespondPosReq(const Ptr<MsgPosReq> & m)
{
    // 有位置数据就直接往上发，没有位置数据就向信道发送请求包，当有响应后再往上发
    if (position_.x == 0 || position_.y == 0 || position_.z == 0) {
        SendPosReq();
        has_req_for_pos_ = true;
    } else {
        Ptr<MsgPosRsp> rsp(new MsgPosRsp);

        rsp->rspPos.x = position_.x;
        rsp->rspPos.y = position_.y;
        rsp->rspPos.z = position_.z;

        SendRsp(rsp, GetLastReqLayer(), GetLastReqPid());
    }
}

void SimulateWOSSChannel::HandlePosNtf(const Ptr<MsgPosNtf>& m){
    if(!IsConnected()){
        return;
    }
    LOG(INFO) << "SimulateWOSSChannel Try to set position from channel";
    PositionForTra tempPos;
    tempPos.type = PACKET_TYPE_POSITION_NTF;
    tempPos.x = htons((m->ntfPos).x);
    tempPos.y = htons((m->ntfPos).y);
    tempPos.z = htons((m->ntfPos).z);


    IODataPtr p(new IOData);
    char* data = new char[sizeof(PositionForTra)];

    memcpy(data, &(tempPos), sizeof(PositionForTra));

    p->fd = GetFd();
    p->data = data;
    p->len = sizeof(PositionForTra);

    WriteQueue::Push(p);
    Write();
}

void SimulateWOSSChannel::SendPosReq()
{
    if (!IsConnected()) {
        return;
    }

    LOG(INFO) << "SimulateWOSSChannel Try to get position from channel";

    IODataPtr p(new IOData);

    char* data = new char[1];
    data[0] = PACKET_TYPE_POSITION_REQ;

    p->fd   = GetFd();
    p->data = data;
    p->len  = 1;

    WriteQueue::Push(p);

    // GetChannel()->Write();
    Write();
}

void SimulateWOSSChannel::HandleDataNtf(const Ptr<IOData> &data)
{
    // char* msg = new char[data->len];
    // memcpy(msg, data->data, data->len);
    Packet pkt(data->data, data->len);

    SimulateWOSSChannelHeader *h = pkt.template Header<SimulateWOSSChannelHeader>();
    // 误码处理
    if(h->errorFlag==0)
    { 
        LOG(INFO) << "packet with no error";
    }else if(h->errorFlag==1)
    {
        LOG(INFO) << "packet with error";
    }else
    {
        LOG(INFO) << "unknown errorflag";
    }

    pkt.Move<SimulateWOSSChannelHeader>(); // 解包

    Ptr<MsgRecvDataNtf> m(new MsgRecvDataNtf);

    m->packet = pkt;

    SendNtf(m, MAC_LAYER, MAC_PID);
}

// 修改了仿真层的首部，加了error位，因此此函数可能需要修改。但是仿真信道并没有找到相应部分，可能是未完成的部分(如果需要使用则要修改)
void SimulateWOSSChannel::HandlePosRsp(const Ptr<IOData> &data)
{
    Position *p = (Position *)(data->data + sizeof(SimulateWOSSChannelHeader));

    position_.x = p->x;
    position_.y = p->y;
    position_.z = p->z;


    LOG(INFO) << "SimulateWOSSChannel position is [" << p->x << ", " << p->y << ", "
              << p->z << "]";

    if (has_req_for_pos_) {
        Ptr<MsgPosRsp> rsp(new MsgPosRsp);

        rsp->rspPos.x = position_.x;
        rsp->rspPos.y = position_.y;
        rsp->rspPos.z = position_.z;

        SendRsp(rsp, GetLastReqLayer(), GetLastReqPid());

        has_req_for_pos_ = false;
    }
}

MODULE_INIT(SimulateWOSSChannel)
PROTOCOL_HEADER(SimulateWOSSChannelHeader)
#endif
}
