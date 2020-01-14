#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <pthread.h>
#include "app_datapackage.h"
#include <iostream>

#include "hsm.h"
#include "message.h"
#include "module.h"
#include "sap.h"
#include "schedule.h"
#include "client.h"

#define CURRENT_PID 1
#define MAX_TIMEOUT_COUNT 8

#define BROADCAST_ADDERSS 255

#define PACKET_TYPE_DATA 0
#define PACKET_TYPE_ACK 1

using namespace std;
union semun {
	int val;
};

// extern int sockfd;
extern client* UIClient;

using namespace std;
namespace udp {
#if TRA_PID == CURRENT_PID

/*
 * declare Events
 */
using msg::MsgSendDataReq;
using msg::MsgSendDataRsp;
using msg::MsgRecvDataNtf;
using msg::MsgPosNtf;

using pkt::Packet;

/*
 * declare States
 */
struct Top;
struct Idle;
struct WaitRsp;

/*
 * declare protocol class
 */

struct UdpHeader
{
    // uint16_t srcport;
    // uint16_t dstport;
    // uint16_t length;
    // uint16_t checksum;
};

class Udp : public mod::Module<Udp, TRA_LAYER, CURRENT_PID>
{
public:
    Udp() { MODULE_CONSTRUCT(Top); }
    void Init() {
        int pos_x = Config::Instance()["udp"]["pos_x"].as<int>();
        int pos_y = Config::Instance()["udp"]["pos_y"].as<int>();
        int pos_z = Config::Instance()["udp"]["pos_z"].as<int>();
         msg::Position tempPos(pos_x, pos_y, pos_z);
         Ptr<MsgPosNtf> m(new MsgPosNtf);
         m->ntfPos = tempPos;
         SendNtf(m, PHY_LAYER, PHY_PID);
        LOG(INFO) << "Udp Init, Node pos:["<<pos_x<<","<<pos_y<<','<<pos_z<<']'<<endl;
    }
    void SendDown(const Ptr<MsgSendDataReq> &);
    // void SendDown(const Ptr< IOData > &);
    void HandleReceiveData(const Ptr<MsgRecvDataNtf> &);

    void GetRsp(const Ptr<MsgSendDataRsp> &);

    uint16_t SocketCheckSum(uint16_t *, int);

private:
    int port_;
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

    HSM_TRANSIT(MsgSendDataReq, WaitRsp, &Udp::SendDown);
    HSM_WORK(MsgRecvDataNtf, &Udp::HandleReceiveData);
};

struct WaitRsp : hsm::State<WaitRsp, Top>
{
    typedef hsm_vector<MsgSendDataReq, MsgSendDataRsp, MsgRecvDataNtf>
        reactions;

    HSM_DEFER(MsgSendDataReq);
    HSM_DEFER(MsgRecvDataNtf);
    // TODO: Timeout...
    HSM_TRANSIT(MsgSendDataRsp, Idle, &Udp::GetRsp);
};

/*
 * define function in protocol
 */

void Udp::SendDown(const Ptr<MsgSendDataReq> &req)
{
    // Packet pkt = req->packet;      // do not allocate memory to pkt,
    // which will be released twice.
    // first time is SendDown() of udp.cc,
    // second is read_stdin() of demo.cc.
    // UdpHeader *header = pkt.Header< UdpHeader >();
    if(UIClient != NULL)
    {
    struct App_DataPackage recvPackage;
    memcpy(&recvPackage, req->packet.Data<UdpHeader>(), sizeof(recvPackage));
    recvPackage.Package_type = 6;
    recvPackage.data_buffer[0] = TRA_LAYER;

        char* sendBuff = new char[sizeof(recvPackage)];
        memcpy(sendBuff, &recvPackage, sizeof(recvPackage));
        IODataPtr pkt(new IOData);

        pkt->fd   = UIClient->_socketfd;
        pkt->data = sendBuff;
        pkt->len  = sizeof(recvPackage);
        UIWriteQueue::Push(pkt);
        cliwrite(UIClient);
    }
    UdpHeader *header = req->packet.Header<UdpHeader>();

    // fill header
    // header->srcport  = 0;
    // header->dstport  = 3000;
    // header->length   = req->packet.Size();
    // header->checksum = 0;
    // header->checksum = SocketCheckSum((uint16_t *)header, (int)header->length);

    // if you need to resend req, use new newreq
    // Ptr< MsgSendDataReq > newreq(new MsgSendDataReq);
    // newreq = req;
    // SendReq(newreq, LOWER_LAYER, LOWER_PROTOCOL);
	
    req->packet.Move<UdpHeader>();

    SendReq(req, NET_LAYER, NET_PID);

    LOG(INFO) << "Udp send msg from console";
}

void Udp::HandleReceiveData(const Ptr<MsgRecvDataNtf> &ntf)
{
    // struct IOData
    // {
    //     int fd;
    //     void* data;
    //     size_t len;
    // };
	
	
   
    UdpHeader *header = ntf->packet.Header<UdpHeader>();
    //LOG(INFO) << "Udp Receive " << ntf->packet.Data<UdpHeader>();

 
    if(UIClient != NULL)
    {
        uint8_t type = (uint8_t)ntf->packet.realRaw()[0];

        if (type == 100 || type == 101){
            LOG(INFO)<<"Udp recv Fountain packet";

            int raw_len = ntf->packet.Raw_Len() - 1;
            LOG(INFO)<<"Fountain chunk size:"<<raw_len;

            char* sendBuff = new char[raw_len];
            memcpy(sendBuff, ntf->packet.realRaw(), raw_len);

            IODataPtr pkt(new IOData);
            pkt->fd   = UIClient->_socketfd;
            pkt->data = sendBuff;
            pkt->len  = raw_len;
            UIWriteQueue::Push(pkt);
            cliwrite(UIClient);
        } else if(!(type == 5 || type == 7 || type == 8)){
            LOG(INFO)<<"Udp recv text packet";
            struct App_DataPackage recvPackage;
            memcpy(&recvPackage, ntf->packet.Data<UdpHeader>(), sizeof(recvPackage));
            char* sendBuff = new char[sizeof(recvPackage)];
            memcpy(sendBuff, &recvPackage, sizeof(recvPackage));
            IODataPtr pkt(new IOData);

            pkt->fd   = UIClient->_socketfd;
            pkt->data = sendBuff;
            pkt->len  = sizeof(recvPackage);
            
            UIWriteQueue::Push(pkt);
            cliwrite(UIClient);
        } else {
            LOG(INFO)<<"Udp recv image packet";
            // struct  ImagePackage recvPackage;
   /*          uint8_t *pi = (uint8_t*)ntf->packet.realRaw(); */
			// cout<<endl;
			// for (unsigned i = 0; i < sizeof(ImageHeader); ++i) { 
				// cout<<hex<<(int)*(pi + i);
			// } 
			// cout<<dec<<endl;

            struct ImageHeader recvPackage;
            memcpy(&recvPackage, ntf->packet.realRaw(), sizeof(recvPackage));
            char* sendBuff = new char[sizeof(recvPackage)];
            memcpy(sendBuff, &recvPackage, sizeof(recvPackage));
            IODataPtr pkt(new IOData);

            pkt->fd   = UIClient->_socketfd;
            pkt->data = sendBuff;
            pkt->len  = sizeof(recvPackage);
            
            UIWriteQueue::Push(pkt);
            cliwrite(UIClient);
        }
    }else{
        struct TestPackage recvPackage;
        memcpy(&recvPackage, ntf->packet.Data<UdpHeader>(), sizeof(recvPackage));
        LOG(INFO) << "udp recv data: " << recvPackage.data_buffer;
    }

    if(UIClient != NULL)
    {
        struct App_DataPackage recvPackage;
        memcpy(&recvPackage, ntf->packet.Data<UdpHeader>(), sizeof(recvPackage));
        recvPackage.Package_type = 6;
        recvPackage.data_buffer[0] = TRA_LAYER;

        char* sendBuff = new char[sizeof(recvPackage)];
        memcpy(sendBuff, &recvPackage, sizeof(recvPackage));
        IODataPtr pkt(new IOData);

        pkt->fd   = UIClient->_socketfd;
        pkt->data = sendBuff;
        pkt->len  = sizeof(recvPackage);

        UIWriteQueue::Push(pkt);
        cliwrite(UIClient);
    }

    // if (!SocketCheckSum((uint16_t *)header, (int)header->length)) {
    //     LOG(INFO) << "Udp checksum successed";
    // } else {
    //     LOG(INFO) << "Udp checksum failed";
    // }
    

	
}

/**
 * Only checksum for udp header and user data
 * @param uint16_t *ptr
 * @param int nbytes
 * @return uint16_t ~sum
 */
uint16_t Udp::SocketCheckSum(uint16_t *ptr, int nbytes)
{
    LOG(INFO) << "Udp verify checkSum";

    uint16_t sum = 0;
    while (nbytes > 1) {
        sum += *ptr++;
        nbytes -= 2;
    }

    if (nbytes == 1) {
        // note: sizeof(uint8_t) = sizeof(unsigned char *) = 1,
        sum += *(uint8_t *)ptr;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum = sum + (sum >> 16);

    // when checksum is right, sum = 1, return 0
    return ~sum;
}

void Udp::GetRsp(const Ptr<MsgSendDataRsp> &rsp)
{
    LOG(INFO) << "Udp Receive Response from lower layer";
}

    MODULE_INIT(Udp)
    PROTOCOL_HEADER(UdpHeader)
#endif

}  // end namespace udp
