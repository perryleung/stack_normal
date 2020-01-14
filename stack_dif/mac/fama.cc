#include <string>
#include "sap.h"
#include "hsm.h"
#include "module.h"
#include "message.h"
#include "schedule.h"

#define CURRENT_PROTOCOL 3

#define MAX_TIMEOUT_COUNT 8

#define BROADCAST_ADDERSS 255

#define PACKET_TYPE_DATA 0
#define PACKET_TYPE_RTS  1
#define PACKET_TYPE_CTS  2

namespace fama {
#if MAC_PID == CURRENT_PROTOCOL

using msg::MsgSendDataReq;
using msg::MsgSendDataRsp;
using msg::MsgRecvDataNtf;
//using msg::MsgPhyIdReq;
//using msg::MsgPhyIdRsp;

//using msg::NodeId;
using pkt::Packet;
//using msg::MsgTimeOut;
template <typename T>
struct EvBase : hsm::Event<T> {};
struct EvSendDataRsp : EvBase<EvSendDataRsp> {};
struct EvSendRTSRsp : EvBase<EvSendRTSRsp> {};
struct EvSendCTSRsp : EvBase<EvSendCTSRsp> {};

struct Top;
struct Idle;
struct WaitRsp;
struct ProcessRsp;
struct WaitCTS;
struct WaitData;

class Fama;

enum 
{
    SEND_DATA_REQ,
    SEND_RTS_REQ,
    SEND_CTS_REQ
};

size_t g_reqtype = SEND_DATA_REQ;

struct FamaHeader
{
        uint8_t type;
        uint8_t src;
        uint8_t dst;
};

class Fama : public mod::Module<Fama, MAC_LAYER, CURRENT_PROTOCOL>
{
public:
        Fama()  
        {
                MODULE_CONSTRUCT( Top );
        }

         void Init()
        {
				YAML::Node table = Config::Instance()["routetable"]["table"];
				mid_ = table[0].as<char>();
				next_mid_ = table[1].as<char>();
                LOG(INFO) << "Fama Init";
				LOG(INFO) << "mid_ ->" << mid_;
        }

        void SendRTSDown(const Ptr< MsgSendDataReq > &);
        void SendCTSDown(const Ptr< MsgRecvDataNtf > &);
//        void ReSendDown();
        size_t  GetType(const Ptr< MsgRecvDataNtf > &);
        bool IsRTS(const Ptr< MsgRecvDataNtf > &);
        bool IsCTS(const Ptr< MsgRecvDataNtf > &);
        bool IsData(const Ptr< MsgRecvDataNtf > &);
        void SendDown(const Ptr< MsgSendDataReq > &);
        void SendUp(const Ptr< MsgRecvDataNtf > &);
        void HandleRsp(const Ptr< MsgSendDataRsp> &);
        void HandleRecvData(const Ptr< MsgRecvDataNtf > &);
        void HandleRecvRTS(const Ptr< MsgRecvDataNtf > &);
        void HandleRecvCTS(const Ptr< MsgRecvDataNtf > &);
        void LogDataRsp(const Ptr< EvSendDataRsp > &);
        void LogRTSRsp(const Ptr< EvSendRTSRsp > &);
        void LogCTSRsp(const Ptr< EvSendCTSRsp > &);
//        inline bool HandleTimeout()
//        {
//                if (timeout_count_ < MAX_TIMEOUT_COUNT) {
//                        ReSendDown();
//                        return true;
//                } else {
//                        return false;
//                }
//        }
//        inline Ptr< MsgSendDataReq >& GetLastSendDataReq()
//        {
//                return last_data_;
//        }

private:
        inline Ptr< MsgSendDataReq >& GetLastSendDataReq()
        {
                return last_data_;
        }

        inline void SaveLastSendDataReq(const Ptr< MsgSendDataReq > & last)
        {
                last_data_ = last;
        }

//        inline void IncreaseTimeoutCount()
//        {
//                ++timeout_count_;
//        }
//
//        inline void ClearTimeoutCount()
//        {
//                timeout_count_ = 0;
//        }

private:
        Ptr< MsgSendDataReq > last_data_;

//        int last_timer_;
//        int timeout_count_;

        size_t mid_;
		size_t next_mid_;
};
struct Top : hsm::State< Top, hsm::StateMachine, Idle > 
{
        typedef hsm_vector< MsgSendDataReq > reactions;

        HSM_DEFER(MsgSendDataReq);
};

struct Idle : hsm::State< Idle, Top > 
{
        typedef hsm_vector< MsgSendDataReq, MsgRecvDataNtf > reactions;
        HSM_TRANSIT(MsgSendDataReq, WaitRsp, &Fama::SendDown);
        HSM_TRANSIT(MsgRecvDataNtf, WaitRsp, &Fama::HandleRecvRTS, &Fama::IsRTS);
};

struct ProcessRsp : hsm::State< ProcessRsp, Top > 
{
        typedef hsm_vector< EvSendRTSRsp, EvSendCTSRsp, EvSendDataRsp > reactions;

        HSM_TRANSIT(EvSendRTSRsp, WaitCTS, &Fama::LogRTSRsp);
        HSM_TRANSIT(EvSendCTSRsp, WaitData, &Fama::LogCTSRsp);
        HSM_TRANSIT(EvSendDataRsp, Idle, &Fama::LogDataRsp);
};

struct WaitRsp : hsm::State< WaitRsp, Top > 
{
        typedef hsm_vector< MsgSendDataRsp > reactions;
        HSM_TRANSIT(MsgSendDataRsp, ProcessRsp, &Fama::HandleRsp);
};

struct WaitCTS : hsm::State< WaitCTS, Top > 
{
        typedef hsm_vector< MsgRecvDataNtf > reactions;

        HSM_TRANSIT(MsgRecvDataNtf, WaitRsp, &Fama::HandleRecvCTS, &Fama::IsCTS);
};

struct WaitData : hsm::State< WaitData, Top >
{
        typedef hsm_vector< MsgRecvDataNtf  > reactions;

        HSM_TRANSIT(MsgRecvDataNtf, Idle, &Fama::HandleRecvData, &Fama::IsData);
};

void Fama::HandleRsp(const Ptr< MsgSendDataRsp> &)
{
    switch(g_reqtype) {
        case SEND_DATA_REQ:
            {
                Ptr< EvSendDataRsp > datarsp(new EvSendDataRsp);
                GetHsm().ProcessEvent(datarsp);
                break;
            }
        case SEND_RTS_REQ:
            {
                Ptr  < EvSendRTSRsp > rtsrsp(new EvSendRTSRsp);
                GetHsm().ProcessEvent(rtsrsp);
                break;
            }
        case SEND_CTS_REQ:
            {
                Ptr<   EvSendCTSRsp > ctsrsp(new EvSendCTSRsp);
                GetHsm().ProcessEvent(ctsrsp);
                break;
            }
        default:
            LOG(INFO) << "HandleRsp Wrong!!!";
            break;  
    }
}

void Fama::SendDown(const Ptr< MsgSendDataReq > &req)
{
        LOG(INFO) << "MAC ID :" << mid_;
        LOG(INFO) << "Fama receive data from upper layer";
        LOG(INFO) << "Fama transit state: Ready --> WaitRsp";

        SendRTSDown(req);

}

void Fama::HandleRecvRTS(const Ptr< MsgRecvDataNtf > &msg) 
{
        LOG(INFO) << "MAC ID :" << mid_;
        SendCTSDown(msg);
    
        LOG(INFO) << "Fama receive a RTS";
        LOG(INFO) << " Fama transit state: Idle --> WaitRsp";
}

void Fama::HandleRecvCTS(const Ptr< MsgRecvDataNtf > &msg)
{
        SendReq(GetLastSendDataReq(), PHY_LAYER, PHY_PID);
        //修改全局变量g_reqtype
        g_reqtype = SEND_DATA_REQ;
        LOG(INFO) << "Fama receive a CTS";
        LOG(INFO) << "Fama transit state: WaitCTS --> WaitRsp";
        Ptr< MsgSendDataRsp > newrsp(new MsgSendDataRsp);
        SendRsp(newrsp,  NET_LAYER, NET_PID);
}

void Fama::LogRTSRsp(const Ptr< EvSendRTSRsp > &)
{
        LOG(INFO) << "Fama receive RESPONSE from device, send RTS successed";
        LOG(INFO) << "Fama transit stat e: WaitRsp --> WaitCTS";
}

void Fama::LogCTSRsp(const Ptr< EvSendCTSRsp > &)
{
        LOG(INFO) << "Fama receive RESPONSE from device, send CTS successed";
        LOG(INFO) << "Fama transit state: WaitRsp --> WaitData";
}

void Fama::LogDataRsp(const Ptr< EvSendDataRsp > &)
{
        LOG(INFO) << "Fama receive RESPONSE from device, send DATA successed";
        LOG(INFO) << "Fama transit state: WaitRsp --> Idle";
}

void Fama::HandleRecvData(const Ptr< MsgRecvDataNtf > & msg)
{
        Ptr< MsgSendDataRsp > newrsp(new MsgSendDataRsp);
        SendRsp(newrsp, NET_LAYER, NET_PID);
        SendUp(msg);
        LOG(INFO) << "Fama transit state : WaitData --> Idle";
}

void Fama::SendRTSDown(const Ptr< MsgSendDataReq > &req)
{
        
        Packet newpkt(0);
        Packet pkt = req->packet;

        FamaHeader *header = pkt.Header< FamaHeader >(); 
        FamaHeader *h = newpkt.Header< FamaHeader >();
        
        // fill data header
        header->type = PACKET_TYPE_DATA;
        header->src  = mid_;
        header->dst  = next_mid_;
        
        // fill RTS header
        h->type = PACKET_TYPE_RTS;
        h->src  = mid_;
        h->dst  = header->dst;

        pkt.Move<FamaHeader>(); 
        Ptr< MsgSendDataReq > datareq(new MsgSendDataReq);
        datareq->packet = pkt;
        SaveLastSendDataReq(datareq);
        
        newpkt.Move<FamaHeader>();
        
        Ptr< MsgSendDataReq > rtsreq(new MsgSendDataReq);
        rtsreq->packet = newpkt;
        SendReq(rtsreq, PHY_LAYER, PHY_PID);
        //修改全局变量g_reqtype
        g_reqtype = SEND_RTS_REQ;
        LOG(INFO) << "fama send a RTS";
}

void Fama::SendCTSDown(const Ptr< MsgRecvDataNtf > &ntf)
{
        
        Packet newpkt(0);
        Packet pkt = ntf->packet;
        FamaHeader *header = pkt.Header< FamaHeader >(); 
        FamaHeader *h = newpkt.Header< FamaHeader >();
        
        // fill CTS header
        h->type = PACKET_TYPE_CTS;
        h->src  = mid_;
        h->dst  = header->src;

        pkt.Move<FamaHeader>(); 
        newpkt.Move<FamaHeader>();
        

        Ptr< MsgSendDataReq > ctsreq(new MsgSendDataReq);
        ctsreq->packet = newpkt;
        SendReq(ctsreq, PHY_LAYER, PHY_PID);

        g_reqtype = SEND_CTS_REQ;
         LOG(INFO) << "fama send a CTS";
}


void Fama::SendUp(const Ptr< MsgRecvDataNtf > &ntf)
{
        ntf->packet.Move<FamaHeader>();
        // send to upper layer
        SendNtf(ntf, NET_LAYER, NET_PID);
}

size_t Fama::GetType(const Ptr< MsgRecvDataNtf > &ntf)
{ 
        Packet pkt = ntf->packet;
        FamaHeader *header = pkt.Header< FamaHeader >(); 
        size_t type = header->type;
        size_t dst  = header->dst;
        if (type == PACKET_TYPE_DATA) {
                LOG(INFO) << "Fama receive Data";
                if (dst == mid_)
                {
                    LOG(INFO) << "Fama receive a Right Data!";
                    return PACKET_TYPE_DATA;
                }
        }
        else if (type == PACKET_TYPE_RTS) {
                LOG(INFO) << "Fama receive RTS";
                if (dst == mid_)
                {
                    LOG(INFO) << "Fama receive a Right RTS!";
                    return PACKET_TYPE_RTS;
                 }
        }
        else if (type == PACKET_TYPE_CTS) {
                LOG(INFO) << "Fama receive CTS";
                if (dst == mid_)
                {
                    LOG(INFO) << "Fama receive a Right CTS!";
                    return PACKET_TYPE_CTS;
                }
        } 
//错误处理
        return false;
}

bool Fama::IsData(const Ptr< MsgRecvDataNtf > &ntf)
{
    LOG(INFO) << "PACKET_TYPE_DATA";
    return (GetType(ntf) == PACKET_TYPE_DATA);
}

bool Fama::IsRTS(const Ptr< MsgRecvDataNtf > &ntf)
{
    LOG(INFO) << "PACKET_TYPE_RTS";
    return (GetType(ntf) == PACKET_TYPE_RTS);
}

bool Fama::IsCTS(const Ptr< MsgRecvDataNtf > &ntf)
{
    LOG(INFO) << "PACKET_TYPE_CTS";
    return (GetType(ntf) == PACKET_TYPE_CTS);
}

MODULE_INIT(Fama)
PROTOCOL_HEADER(FamaHeader)
#endif

} // end namespace fama

