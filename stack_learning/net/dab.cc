#include <cmath>
#include "hsm.h"
#include "message.h"
#include "sap.h"
#include "module.h"
#include "schedule.h"

#define UPPER_LAYER 4
#define UPPER_PROTOCOL 1
#define CURRENT_LAYER 3
#define CURRENT_PROTOCOL 2
#define LOWER_LAYER 2
#define LOWER_PROTOCOL 2
#define HELLO_INTERVAL 600
#define HELLO_TYPE 1
#define REQUEST_TYPE 2
#define REPLY_TYPE 3
#define DATA_TYPE 4

namespace dab {
#if NET_PID == CURRENT_PID

using msg::MsgRecvDataNtf; 
using msg::MsgSendDataReq;
using msg::MsgSendAckNtf;
using msg::MsgTimeOut;

struct MsgRecvData : hsm::Event< MsgRecvData >
{
        pkt::Packet packet;
}

struct MsgRecvHello : hsm::Event< MsgRecvHello >
{
	pkt::Packet packet;
}

struct MsgRecvRequest : hsm::Event< MsgRecvRequest >
{
        pkt::Packet packet;
}

struct MsgRecvReply : hsm::Event< MsgRecvReply >
{
        pkt::Packet packet;
}

struct DoubleValue
{
	uint8_t left;
	uint8_t right;
}

struct HeaderBase 
{
	uint8_t type;
}

struct DataHeader : HeaderBase
{
        DoubleValue sender_hop_id;
        uint8_t next_node_id;
        uint8_t packet_seq_number;   
}

struct HelloHeader : HeaderBase
{
        uint8_t sink_id;
        DoubleValue hop_id;
        uint8_t max_hop_count;
}

struct RequestHeader : HeaderBase
{
        uint8_t node_id;
}

struct ReplyHeader : HeaderBase
{
        uint8_t node_id;
        DoubleVable hop_id;
}

struct Top
struct SinkIdle
struct NodeIdle
struct WaitReply
struct ChooseNextNode

class Dab : public mod::Module<Dab, CURRENT_LATER, CURRENT_PROTOCOL>
{
public:
        Dab()
        {
                GetSap().SetOwner(this);
                GetTap().SetOwner(this);
                GetHsm().Initialize<Top>(this);
        }

	static void Init() { LOG(INFO) << "Dab Init";}
        void HandleRecvPacket(const Ptr<MsgRecvDataNtf> &);
        bool IsSink(void);
        void SendHello(void);
	void SendRequest(const Ptr<MsgRecvData> &);
        void SendReply(void);
        void SendUp(const Ptr<MsgRecvData> &);
        void UpdateHopID(const Ptr<MsgRecvHello> &);
        bool IsForward(const Ptr<MsgRecvData> &);
        void SaveReply(const Ptr<MsgRecvReply> &);
        void CompareHopID(const Ptr<MsgRecvReply> &);
        void UpdateHeadSendDown(void);

private:
	Ptr<MsgRecvDataNtf> handling_msg_;
	
	uint8_t is_sink_;
        uint8_t node_id_;
        DoubleValue hop_id_;
        uint8_t neighbor_node_id_;
        DoubleValue neighbor_hop_id_;
};

struct Top : hsm::State<Top, hsm::StateMachine, Init>
{
    	typedef hsm_vector<MsgRecvDataNtf> reactions;

        HSM_WORK(MsgRecvDataNtf, &Dab::HandleRecvPacket);
}

struct Init : hsm::State<Init, Top>
{
        typedef hsm_vector<MsgInit> reactions;

        HSM_TRANSIT_TRANSIT(MsgInit, SinkIdle, &Dab::IsSink, NodeIdle)
}

struct SinkIdle : hsm::State<SinkIdle, Top>
{
        typedef hsm_vector<MsgTimeOut, MsgRecvRequest, MsgRecvData> reactions;

        HSM_WORK(MsgTimeOut, &Dab::SendHello);
        HSM_WORK(MsgRecvRequest, &Dab::SendReply);
        HSM_WORK(MsgRecvData, &Dab::SendUp);
}

struct NodeIdle : hsm::State<NodeIdle, Top>
{
        typedef hsm_vector<MsgSendData, MsgRecvHello, MsgRecvRequest> reactions;
   
        HSM_TRANSIT(MsgSendData, WaitReply, &Dab::SendRequest);
        HSM_WORK(MsgRecvHello, &Dab::UpdateHopID);
        HSM_WORK(MsgRecvRequest, &Dab::SendReply);
        HSM_TRANSIT(MsgRecvData, WaitReply, &Dab::SendRequest, &Dab::IsForward);
}

struct WaitReply : hsm::State<WaitReply, Top>
{
        typedef hsm_vector<MsgSendData, MsgRecvHello, MsgRecvRequest, MsgRecvData, MsgTimeOut, MsgRecvReply> reactions;

        HSM_DEFER(MsgSendData);
        HSM_WORK(MsgRecvHello, &Dab::UpdateHopID);
        HSM_WORK(MsgRecvRequest, &Dab::SendReply);
        HSM_DEFER(MsgRecvData);
        HSM_WORK(MsgTimeOut, &Dab::SendRequest, &Dab::LessThanThreshold);
        HSM_TRANSIT(MsgRecvReply, ChooseNextNode, &Dab::SaveReply);
}

struct ChooseNextNode : hsm::State<ChooseNextNode, Top>
{
        typedef hsm_vector<MsgSendData, MsgRecvHello, MsgRecvData, MsgRecvRequest, MsgRecvReply, MsgTimeOut>
 
        HSM_DEFER(MsgSendDataReq);
        HSM_WORK(MsgRecvHello, &Dab::UpdateHopID);
        HSM_DEFER(MsgRecvData);
        HSM_WORK(MsgRecvRequest, &Dab::SendReply);
        HSM_WORK(MsgRecvReply, &Dab::CompareHopID);
        HSM_TRANSIT(MsgTimeOut, NodeIdle, &Dab::UpdateHeadSendDown);
}

void Dab::HandleRecvPacket(const Ptr<MsgRecvDataNtf> &msg)
{
	msg->packet.MoveB(sizeof(DataHeader));
        DataHeader *header = msg->packet.Header<DataHeader>();
 
        switch(header)
	{
 	  case HELLO_TYPE : 
	  {
		Ptr<MsgRecvHello> m(new MsgRecvHello);
                        m->packet = msg->packet;
                        GetHsm().ProcessEvent(m);
                        break;
	  }
          case REQUEST_TYPE :
          {		Ptr<MsgRecvRequest> m(new MsgRecvRequest);
           		m->packet = msg->packet;
			GetHsm().ProcessEvent(m);
	  		break;
          }
	  case REPLY_TYPE :
	  {
			Ptr<MsgRecvReply> m(new MsgRecvReply);
			m->packet = msg->packet;
			GetHsm().ProcessEvent(m);
 			break;
	  }
	  case DATA_TYPE :
	  {
	 		Ptr<MsgRecvData> m(new MsgRecvData);
			m->packet = msg->packet;
			GetHsm().ProcessEvent(m);
			break;
	  }
	}
	
}

bool Dab::IsSink(void)
{
	if(is_sink_)
	return 1;
	else
	return 0; 
}

void Dab::SendHello(void)
{
  	pkt::Packet p(HELLOPACKET);
	HelloHeader *header = p->Header<HelloHeader>();
	header->type = HELLO_TYPE;
	header->sink_id = node_id_;
        header->hop_id.left = 9;
	header->hop_id.right = 9;
        header->max_hop_count = 9;
	
	Ptr<MsgSendDataReq> m(new MsgSendDataReq);
	m->packet = p;
	GetHsm().ProcessEvent(m);
	SendReq(m, LOWER_LAYER, LOWER_PROTOCOL);
}

void Dab::SendRequest(const Ptr<MsgRecvDataNtf> &msg)
{
	handling_msg_ = msg;
	pkt::Packet p(REQUESTPACKET);
	RequestHeader *header = p->Header<RequestHeader>();
	header->type = REQUEST_TYPE;
	header->node_id = node_id_;
	
	Ptr<MsgSendDataReq> m(new MsgSendDataReq);
	m->packet = p;
	GetHsm().ProcessEvent(m);
	SendReq(m, LOWER_LATER, LOWER_PROTOCOL);
}	

void Dab::SendReply(void)
{
	pkt::Packet p(REPLYPACKET);
	ReplyHeader *header = p->Header<ReplyHeader>();
	header->type = REPLY_TYPE;
  	header->node_id = node_id_;
	header->hop_id = hop_id_;

	Ptr<MsgSendDataReq> m(new MsgSendDataReq);
	m->packet = p;
	GetHsm().ProcessEvent(m);
	SendReq(m, LOWER_LAYER, LOWER_PROTOCOL);
}

void Dab::SendUp(const Ptr<MsgRecvData> &msg)
{
	SendNtf(msg, UPPER_LAYER, UPPER_PROTOCOL);
}

void Dab::UpdateHopID(const Ptr<MsgRecvHello> &msg)
{
	msg->packet.MoveB(sizeof(HelloHeader));
	HelloHeader *header = msg->packet.Header<HelloHeader>();
}

bool Dab::IsForward(const Ptr<MsgRecvData> &msg)
{
	msg->packet.MoveB(sizeof(DataHeader));
	DataHeader *header = msg->packet.Header<DataHeader>();
	
	if(header->next_node_id == node_id_)
	return 1;
	else
	return 0;
}

void Dab::SaveReply(const Ptr<MsgRecvReply> &msg)
{
	msg->packet.MoveB(sizeof(ReplyHeader));
	ReplyHeader *header = msg->packet.Header<ReplyHeader>();

	neighbor_node_id_ = header->node_id;
	neighbor_hop_id_ = header->hop_id;
}

void Dab::CompareHopID(const Ptr<MsgRecvReply> &msg)
{
	msg->packet.MoveB(sizeof(ReplyHeader));
	ReplyHeader *header = msg->packet.Header<ReplyHeader>();

        if(header->node_id.left <= neighbor_node_id_.left);
        {
		if(header->node_id.left < neighbor_node_id_.left)
		{
		neighbor_node_id_ = header->node_id;
		neighbor_hop_id_ = header->hop_id;
		}
		else
		{
		if(header->node_id.right < neighbor_node_id_.right);
			{
			neighbor_node_id_ = header->node_id;
			neighbor_hop_id_ = header->hop_id;
			}
		}
	}
	

}
MODULE_INIT(Dab)
PROTOCOL_HEADER(Dab)

#endif
}//end namespace dab
