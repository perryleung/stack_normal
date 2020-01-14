#include <string>
#include "hsm.h"
#include "message.h"
#include "module.h"
#include "sap.h"
#include "schedule.h"
#include "client.h"
#include "app_datapackage.h"

#define CURRENT_PROTOCOL 4
#define MAX_TIMEOUT_COUNT 3
#define BROADCAST_ADDERSS 255
#define PACKET_TYPE_DATA 0
#define PACKET_TYPE_ACK 1

extern client* UIClient;
namespace simple_aloha {

#if MAC_PID == CURRENT_PROTOCOL
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

/*
 * declare protocol class
 */

struct SimpleAlohaHeader
{
    uint8_t type;
    uint8_t src;
    uint8_t dst;
};
class SimpleAloha : public mod::Module<SimpleAloha, MAC_LAYER, CURRENT_PROTOCOL>
{
    public:
        SimpleAloha() {MODULE_CONSTRUCT(Top); }
        static void Init() { LOG(INFO) << "SimpleAloha Init"; }
        void SendDown(const Ptr<MsgSendDataReq> &);
        void SendUp(const Ptr<MsgRecvDataNtf> &);
    private:
        int sendSerialNum_ = 0;
        int recvSerialNum_ = 0; 
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

    HSM_WORK(MsgSendDataReq, &SimpleAloha::SendDown);
    HSM_WORK(MsgRecvDataNtf, &SimpleAloha::SendUp);
};
/*
 * define function in protocol
 */
void SimpleAloha::SendDown(const Ptr<MsgSendDataReq> &req)
{

    LOG(INFO) << "SimpleAloha send down data";

    //Packet pkt = req->packet;
   if(UIClient != NULL)
    {
        struct App_DataPackage recvPackage;
        memcpy(&recvPackage, req->packet.realRaw(), sizeof(recvPackage));
        recvPackage.Package_type = 6;
        recvPackage.data_buffer[0] = MAC_LAYER;

        char* sendBuff = new char[sizeof(recvPackage)];
        memcpy(sendBuff, &recvPackage, sizeof(recvPackage));
        IODataPtr pkt(new IOData);


        pkt->fd   = UIClient->_socketfd;
        pkt->data = sendBuff;
        pkt->len  = sizeof(recvPackage);
        UIWriteQueue::Push(pkt);
        cliwrite(UIClient);
    }
    SimpleAlohaHeader *header = req->packet.Header<SimpleAlohaHeader>();

    // fill header
    header->type = PACKET_TYPE_DATA;
    header->src  = 0;
    header->dst  = BROADCAST_ADDERSS;

    req->packet.Move<SimpleAlohaHeader>();

    Ptr<MsgSendDataReq> newreq(new MsgSendDataReq);

    newreq->packet = req->packet;

    LOG(INFO)<<"send down packet serial : "<<sendSerialNum_++;
    SendReq(newreq, PHY_LAYER, PHY_PID);

    Ptr<MsgSendDataRsp> p(new MsgSendDataRsp);
    SendRsp(p, NET_LAYER, NET_PID);
}

void SimpleAloha::SendUp(const Ptr<MsgRecvDataNtf> &ntf)
{
    LOG(INFO) << "SimpleAloha send up data";
    if(UIClient != NULL)
    {
        struct App_DataPackage recvPackage;
        memcpy(&recvPackage, ntf->packet.realRaw(), sizeof(recvPackage));
        recvPackage.Package_type = 6;
        recvPackage.data_buffer[0] = MAC_LAYER;

        char* sendBuff = new char[sizeof(recvPackage)];
        memcpy(sendBuff, &recvPackage, sizeof(recvPackage));
        IODataPtr pkt(new IOData);

        pkt->fd   = UIClient->_socketfd;
        pkt->data = sendBuff;
        pkt->len  = sizeof(recvPackage);

        UIWriteQueue::Push(pkt);
        cliwrite(UIClient);
    }

    LOG(INFO)<<"send up packet serial : "<<recvSerialNum_++;
    ntf->packet.Move<SimpleAlohaHeader>();
    SendNtf(ntf, NET_LAYER, NET_PID);
   }

MODULE_INIT(SimpleAloha)
PROTOCOL_HEADER(SimpleAlohaHeader)
#endif

}  // end namespace simple_aloha
