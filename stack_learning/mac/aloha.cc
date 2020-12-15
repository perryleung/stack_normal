#include <string>
#include "hsm.h"
#include "message.h"
#include "module.h"
#include "sap.h"
#include "schedule.h"
#include "tools.h"

#define CURRENT_PID 2


#define MAX_TIMEOUT_COUNT 3
#define BROADCAST_ADDERSS 255
#define PACKET_TYPE_DATA 0
#define PACKET_TYPE_ACK 1

namespace aloha {

#if MAC_PID == CURRENT_PID

/*
 * declare Events
 */
using msg::MsgSendDataReq;
using msg::MsgSendAckReq;
using msg::MsgSendDataRsp;
using msg::MsgSendAckRsp;
using msg::MsgRecvDataNtf;
using msg::MsgTimeOut;
using msg::MsgSendAckNtf;

using pkt::Packet;

/*
 * declare States
 */
struct Top;
struct Idle;
struct WaitRsp;
struct WaitRsp1;
struct WaitAck;
struct WaitVbf;

/*
 * declare protocol class
 */

struct AlohaHeader
{
    uint8_t type;
    uint8_t src;
    uint8_t dst;
};

class Aloha : public mod::Module<Aloha, MAC_LAYER, CURRENT_PID>
{
public:
    Aloha() { MODULE_CONSTRUCT(Top); }
    static void Init() { LOG(INFO) << "Aloha Init"; }
    void SendDown(const Ptr<MsgSendDataReq> &);
    void SendUp(const Ptr<MsgRecvDataNtf> &);
    void ReSendDown();

    bool HandleReceiveData(const Ptr<MsgRecvDataNtf> &);
    void HandleSendAckReq(const Ptr<MsgSendAckNtf> &);

    void ReceiveAck(const Ptr<MsgRecvDataNtf> &) {Trace::Instance().Log(MAC_LAYER, MAC_PID, 255, 255, 65535, 255, "ack", "recv");}
    void HandleTimeouMsg(const Ptr<MsgTimeOut> &) {}
    bool IsDataPacket(const Ptr<MsgRecvDataNtf> &);
    bool IsAckPacket(const Ptr<MsgRecvDataNtf> &);
    bool IsSendAck(const Ptr<MsgSendAckNtf> &msg)
    {
        if (msg->send) {
            LOG(INFO) << "send ACK";
            return true;
        }
        LOG(INFO) << "not send ACK";
        return false;
    }

    bool HandleTimeout(const Ptr<MsgTimeOut> &)
    {
        if (timeout_count_ < MAX_TIMEOUT_COUNT) {
            ReSendDown();
            return true;
        } else {
            return false;
        }
    }
    bool IsData(const Ptr<MsgSendDataReq> &msg)
    {
        if(msg->isData){
            return true;    
        }else{
            SendDown(msg);
            return false;
        }
    }
    void HandleAckNtf(const Ptr<MsgSendAckNtf> &msg)
    {
        if(IsSendAck(msg)){
            HandleSendAckReq(msg);
        }
    }

private:
    inline Ptr<MsgSendDataReq> &GetLastSendDataReq() { return last_data_; }
    inline void SaveLastSendDataReq(const Ptr<MsgSendDataReq> &last)
    {
        last_data_ = last;
    }

    inline void IncreaseTimeoutCount() { ++timeout_count_; }
    inline void ClearTimeoutCount() { timeout_count_ = 0; }
private:
    Ptr<MsgSendDataReq> last_data_;

    int last_timer_;
    int timeout_count_;

    char mid_;
};

/*
 * define States
 */
struct Top : hsm::State<Top, hsm::StateMachine, Idle>
{
    typedef hsm_vector<MsgSendDataReq, MsgRecvDataNtf> reactions;
    HSM_DEFER(MsgSendDataReq);
    HSM_DEFER(MsgRecvDataNtf);
};

struct Idle : hsm::State<Idle, Top>
{
    typedef hsm_vector<MsgSendDataReq, MsgRecvDataNtf> reactions;

    HSM_TRANSIT_TRANSIT(MsgSendDataReq, WaitRsp, &Aloha::SendDown, &Aloha::IsData, WaitRsp1);
    HSM_TRANSIT(MsgRecvDataNtf, WaitVbf, &Aloha::SendUp, &Aloha::IsDataPacket);
};

struct WaitRsp : hsm::State<WaitRsp, Top>
{
    typedef hsm_vector<MsgSendAckRsp, MsgSendDataRsp, MsgSendDataReq, MsgRecvDataNtf> reactions;

    HSM_TRANSIT(MsgSendAckRsp, Idle);
    HSM_TRANSIT(MsgSendDataRsp, WaitAck);
    HSM_DEFER(MsgSendDataReq);
    HSM_DEFER(MsgRecvDataNtf);
};

struct WaitRsp1 : hsm::State<WaitRsp1, Top>
{
    typedef hsm_vector<MsgSendAckRsp, MsgSendDataRsp, MsgSendDataReq, MsgRecvDataNtf> reactions;

    HSM_TRANSIT(MsgSendAckRsp, Idle);
    HSM_TRANSIT(MsgSendDataRsp, Idle);
    HSM_DEFER(MsgSendDataReq);
    HSM_DEFER(MsgRecvDataNtf);
};

struct WaitAck : hsm::State<WaitAck, Top>
{
    typedef hsm_vector<MsgRecvDataNtf, MsgTimeOut, MsgSendDataReq, MsgSendAckNtf> reactions;

    HSM_TRANSIT_DEFER(MsgRecvDataNtf, Idle, &Aloha::ReceiveAck,
                      &Aloha::IsAckPacket);
    HSM_TRANSIT_TRANSIT(MsgTimeOut, WaitRsp, &Aloha::HandleTimeouMsg,
                        &Aloha::HandleTimeout, Idle);
    HSM_DEFER(MsgSendDataReq);
    HSM_WORK(MsgSendAckNtf, &Aloha::HandleAckNtf);
};

struct WaitVbf : hsm::State<WaitVbf, Top>
{
    typedef hsm_vector<MsgSendAckNtf, MsgSendDataReq, MsgRecvDataNtf> reactions;

    HSM_TRANSIT_TRANSIT(MsgSendAckNtf, WaitRsp, &Aloha::HandleSendAckReq,
                        &Aloha::IsSendAck, Idle);
    HSM_DEFER(MsgSendDataReq);
    HSM_DEFER(MsgRecvDataNtf);
};

/*
 * define function in protocol
 */
void Aloha::SendDown(const Ptr<MsgSendDataReq> &req)
{
	Trace::Instance().Log(MAC_LAYER, MAC_PID, req->packet, "data", "send");
    LOG(INFO) << "Aloha send down data";

    //Packet pkt = req->packet;

    AlohaHeader *header = req->packet.Header<AlohaHeader>();

    // fill header
    header->type = PACKET_TYPE_DATA;
    header->src  = mid_;
    header->dst  = BROADCAST_ADDERSS;

    req->packet.Move<AlohaHeader>();

    Ptr<MsgSendDataReq> newreq(new MsgSendDataReq);

    newreq->packet = req->packet;

    SendReq(newreq, PHY_LAYER, PHY_PID);

    last_timer_ = SetTimer(rand_num(7, 12));

    IncreaseTimeoutCount();
    SaveLastSendDataReq(req);

    Ptr<MsgSendDataRsp> p(new MsgSendDataRsp);
    SendRsp(p, NET_LAYER, NET_PID);
    ClearTimeoutCount();
}

void Aloha::SendUp(const Ptr<MsgRecvDataNtf> &ntf)
{
	Trace::Instance().Log(MAC_LAYER, MAC_PID, ntf->packet, "data", "recv");
    LOG(INFO) << "Aloha send up data";

    ntf->packet.Move<AlohaHeader>();
    SendNtf(ntf, NET_LAYER, NET_PID);
}

void Aloha::ReSendDown()
{
    
	Trace::Instance().Log(MAC_LAYER, MAC_PID, GetLastSendDataReq()->packet, "data", "resend");
    LOG(INFO) << "Aloha resend data ("<<timeout_count_+1<<")";

    SendReq(GetLastSendDataReq(), PHY_LAYER, PHY_PID);

    last_timer_ = SetTimer(rand_num(7, 12));

    IncreaseTimeoutCount();
}

void Aloha::HandleSendAckReq(const Ptr<MsgSendAckNtf> &ntf)
{
	Trace::Instance().Log(MAC_LAYER, MAC_PID, 255, 255, 65535, 255, "ack", "send");
    LOG(INFO) << "Aloha send ack";

    // make ack
    Packet pkt(0);

    AlohaHeader *h = pkt.Header<AlohaHeader>();

    h->type = PACKET_TYPE_ACK;
    h->src  = mid_;
    h->dst  = BROADCAST_ADDERSS;

    pkt.Move<AlohaHeader>();

    // send ack
    Ptr<MsgSendAckReq> m(new MsgSendAckReq);

    m->packet = pkt;

    SendReq(m, PHY_LAYER, PHY_PID);
}

bool Aloha::IsDataPacket(const Ptr<MsgRecvDataNtf> &ntf)
{
    LOG(INFO) << "Aloha receive something";
    AlohaHeader *header = ntf->packet.Header<AlohaHeader>();

    int type = header->type;

    if (type == PACKET_TYPE_DATA) {
        LOG(INFO) << "Aloha receive data";
        return true;
    } else {
        return false;
    }
}

bool Aloha::IsAckPacket(const Ptr<MsgRecvDataNtf> &ntf)
{
    AlohaHeader *header = ntf->packet.Header<AlohaHeader>();

    int type = header->type;

    if (type == PACKET_TYPE_ACK) {
        LOG(INFO) << "Aloha receive ack";
        return true;
    } else {
        if(type == PACKET_TYPE_DATA)
        SendUp(ntf);
        return false;
    }
}

MODULE_INIT(Aloha)
PROTOCOL_HEADER(AlohaHeader)

#endif

}  // end namespace aloha
