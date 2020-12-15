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

#define CURRENT_PROTOCOL 1
using std::string;
using std::weak_ptr;

namespace sim_channel {
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
using msg::EvStartRecvData;

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
    PACKET_TYPE_POSITION_NTF_RSP,
    PACKET_START_RECV
};

typedef enum{
    Data,
    Ack
}DataOrAck;


struct Top;
struct Idle;
struct WaitPosNtfRsp;
struct WaitRsp;

struct SimulateChannelHeader
{
    uint8_t type;
};

#pragma pack(1)//让结构体到数据保持1一字节对齐

    struct PositionForTra
    {
        char type;
        uint16_t x;
        uint16_t y;
        uint16_t z;
    };

#pragma pack()

class SimulateChannel : public drv::Driver<SimulateChannel, PHY_LAYER, CURRENT_PROTOCOL>
{
public:
    SimulateChannel() { DRIVER_CONSTRUCT(Top); }//相比于其他层结构多了一个DAP
    void Init()
    {
        LOG(INFO) << "SimulateChannel Init : Map the opened fd of socket and DAP";
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

    /**
     * @brief 应该是线程中调用来读取串口的函数，具体怎么调用还不是很清楚
    **/
    void Notify()
    {
        LOG(INFO) << "Simulate Channel receive data";

        IODataPtr data = ReadQueue::Front();

        SimulateChannelHeader *p = (SimulateChannelHeader *)data->data;//获取物理层字节头

        switch (p->type) {
            //--------------------为载波监听添加新功能---------------------------
            case PACKET_START_RECV:
                 {
                     StartRecv++;
                     Ptr<EvStartRecvData> p(new EvStartRecvData);
                     SendNtf(p,MAC_LAYER,MAC_PID);
                     LOG(INFO)<<"监听到载波";
                     if(StartRecv>=2)//同时存在多个开始接收，说明发生碰撞
                     {
                        BadData=true;   
                     }
                     break;
                 }
            //-----------------------------------------------
            case PACKET_TYPE_DATA:
                HandleDataNtf(data);
                break;
            case PACKET_TYPE_POSITION_RSP:
                HandlePosRsp(data);
                break;
            case PACKET_TYPE_DATA_RSP://信道发送完数据给当前层发送回复，使其可以转移到INIT状态
                {
                    LOG(INFO) << "RX DATA RSP";
                    Ptr<EvChannelRsp> event(new EvChannelRsp);
                    GetHsm().ProcessEvent(event);
                    break;
                }
            case PACKET_TYPE_POSITION_NTF_RSP://收到位置信息并设置，回复当前层使其返回INIT状态
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

    /**
    *@brief 启动服务器的定时器
    **/
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
    int StartRecv=0;//用于统计当前开始接收包到次数，进而判断是否出现接收端冲突
    bool BadData=false;//用于判断是否是接收端冲突到包
};

struct Top : hsm::State<Top, hsm::StateMachine, Idle>
{
    typedef hsm_vector<MsgSendDataReq, MsgRecvDataNtf, MsgPosReq, MsgPosNtf> reactions;

    HSM_DEFER(MsgSendDataReq);
    HSM_DEFER(MsgSendAckReq);
    HSM_WORK(MsgRecvDataNtf, &SimulateChannel::SendUp);
    HSM_WORK(MsgPosReq, &SimulateChannel::RespondPosReq);
    HSM_DEFER(MsgPosNtf);

};

struct Idle : hsm::State<Idle, Top>
{
    typedef hsm_vector<MsgSendDataReq, MsgSendAckReq, MsgPosNtf> reactions;

    HSM_TRANSIT(MsgPosNtf, WaitPosNtfRsp, &SimulateChannel::HandlePosNtf);
    // HSM_WORK(MsgSendDataReq, (&SimulateChannel::SendDown<MsgSendDataReq, MsgSendDataRsp>));
    // HSM_WORK(MsgSendAckReq, (&SimulateChannel::SendDown<MsgSendAckReq, MsgSendAckRsp>));
    HSM_TRANSIT(MsgSendDataReq, WaitRsp, (&SimulateChannel::SendDown<MsgSendDataReq, Data>));
    HSM_TRANSIT(MsgSendAckReq, WaitRsp, (&SimulateChannel::SendDown<MsgSendAckReq, Ack>));

    // HSM_TRANSIT(MsgSendDataReq, Idle, &SimulateChannel::template SendDown<MsgSendDataReq, MsgSendDataRsp>);
    // HSM_TRANSIT(MsgSendAckReq, Idle, &SimulateChannel::template SendDown<MsgSendAckReq, MsgSendAckRsp>);

};

struct WaitPosNtfRsp : hsm::State<WaitPosNtfRsp, Top>
{
    typedef hsm_vector<EvRecvPosNtfRsp> reactions;
    HSM_TRANSIT(EvRecvPosNtfRsp, Idle);
};

struct WaitRsp : hsm::State<WaitRsp, Top>
{
    typedef hsm_vector<EvChannelRsp> reactions;
    HSM_TRANSIT(EvChannelRsp, Idle, &SimulateChannel::SendUpRsp);
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

/**
*@brief 发送数据给信道，New_aloha只会下发MsgSendDataReq,是DATA还是ACK通过New_aloha头的数据位判断
**/
template<typename ReqType, DataOrAck MsgFlag>
void SimulateChannel::SendDown(const Ptr<ReqType> &req){

    if (!IsConnected()) {
        return;
    }

    flag = MsgFlag;//默认只有MsgSendDataReq,所以这里只会是Data
    SimulateChannelHeader *h = req->packet.template Header<SimulateChannelHeader>();//获取物理层头部的位置
    h->type = PACKET_TYPE_DATA;
    req->packet.template Move<SimulateChannelHeader>();

    auto pkt = req->packet;
    char* msg = new char[pkt.Size()];
    memcpy(msg, pkt.Raw(), pkt.Size());//这里到Raw（）返回来msg的地址头，因为此时head=0

    IODataPtr p(new IOData);
    p->fd = GetFd();
    p->len = pkt.Size();
    p->data = msg;

    WriteQueue::Push(p);

    // GetChannel()->Write();
    Write();
    LOG(INFO)<<"往信道中写入数据";
    // Ptr<RspType> rsp(new RspType);
    // SendRsp(rsp, MAC_LAYER, MAC_PID);
}

/**
*@brief 信道节点发送完回复RSP
**/
    void SimulateChannel::SendUpRsp(const Ptr<EvChannelRsp> &){
        if(flag == Data){
            Ptr<MsgSendDataRsp> rsp(new MsgSendDataRsp);
            SendRsp(rsp, MAC_LAYER, MAC_PID);
        }else if(flag == Ack){
            Ptr<MsgSendAckRsp> rsp(new MsgSendAckRsp);
            SendRsp(rsp, MAC_LAYER, MAC_PID);
        }
    }


/**
*@brief 请求节点位置信息，目前看来没有用到
**/
void SimulateChannel::RespondPosReq(const Ptr<MsgPosReq> & m)
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

/**
 *@brief 设置信道中节点的位置 
 **/
void SimulateChannel::HandlePosNtf(const Ptr<MsgPosNtf>& m){
    if(!IsConnected()){
        return;
    }
    LOG(INFO) << "SimulateChannel Try to set position from channel";
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

/**
 *@brief 请求节点位置信息 
 **/
void SimulateChannel::SendPosReq()
{
    if (!IsConnected()) {
        return;
    }

    LOG(INFO) << "SimulateChannel Try to get position from channel";

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

/**
 *@brief 接收到数据包，解包并发给上层,判断是否有错误并上发给上层
**/
void SimulateChannel::HandleDataNtf(const Ptr<IOData> &data)
{
    // char* msg = new char[data->len];
    // memcpy(msg, data->data, data->len);
    Packet pkt(data->data, data->len);//隐含的作用可能是将数据方向变成向上
    SimulateChannelHeader *h = pkt.template Header<SimulateChannelHeader>();
    pkt.Move<SimulateChannelHeader>(); // 解包
    Ptr<MsgRecvDataNtf> m(new MsgRecvDataNtf);
    m->packet = pkt;
    if(StartRecv>0)
    {  
        StartRecv--;
    }
    if(BadData)
    {
        if(StartRecv==0)//说明碰撞到数据已经接受完
        {
            BadData=false;
        }
        //将MsgRecvDataNtf中到reservation赋值为1,表示出错
        m->reservation=1;   
    }
    SendNtf(m, MAC_LAYER, MAC_PID);
    LOG(INFO)<<"将收到的数据上发给MAC层";
}
/**
 * @brief 收到节点位置信息到包，将其打印出来，并向信道发送回复
**/
void SimulateChannel::HandlePosRsp(const Ptr<IOData> &data)
{
    Position *p = (Position *)(data->data + sizeof(SimulateChannelHeader));

    position_.x = p->x;
    position_.y = p->y;
    position_.z = p->z;


    LOG(INFO) << "SimulateChannel position is [" << p->x << ", " << p->y << ", "
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

MODULE_INIT(SimulateChannel)
PROTOCOL_HEADER(SimulateChannelHeader)
#endif
}
