#include "hsm.h"
#include "message.h"
#include "module.h"
#include "sap.h"
#include "schedule.h"
#include "packet.h"
#include "trace.h"
#include "app_datapackage.h"
#include <stdlib.h>
#include <vector>
#include <ev.h>
#include "libev_tool.h"
#include "client.h"
#include "routetable.h"

#define CURRENT_PID 2

extern client* UIClient;
extern client* TraceClient;

extern RouteTable globalRouteTable;

namespace srt{
#if NET_PID == CURRENT_PID

using msg::MsgSendDataReq;
using msg::MsgSendDataRsp;
using msg::MsgRecvDataNtf;

struct ComeBackToIdle : hsm::Event<ComeBackToIdle>{};

struct Top;
struct Idle;
struct WaitRsp;

struct SrtHeader
{
        uint16_t serialNum;
	uint8_t srcID;
	uint8_t nextID;
	uint8_t destID;
};

class Srt : public mod::Module<Srt, NET_LAYER, CURRENT_PID>
{
private:
    RouteTable full;
public:
	Srt()
	{
		GetSap().SetOwner(this);
		GetHsm().Initialize<Top>(this);
	}
	
	void Init()
	{
		uint8_t nodeID = (uint8_t)Config::Instance()["Srt"]["nodeID"].as<int>();
		full.setNodeID(nodeID);
		YAML::Node entry1 = Config::Instance()["Srt"]["entry1"];
		YAML::Node entry2 = Config::Instance()["Srt"]["entry2"];
		YAML::Node entry3 = Config::Instance()["Srt"]["entry3"];
		YAML::Node entry4 = Config::Instance()["Srt"]["entry4"];
		YAML::Node entry5 = Config::Instance()["Srt"]["entry5"];
		YAML::Node entry6 = Config::Instance()["Srt"]["entry6"];
		YAML::Node entry7 = Config::Instance()["Srt"]["entry7"];

		vector<YAML::Node> vt;
		vt.push_back(entry1);
		vt.push_back(entry2);
		vt.push_back(entry3);
		vt.push_back(entry4);
		vt.push_back(entry5);
		vt.push_back(entry6);
		vt.push_back(entry7);

		addEntry(full, vt);
		full.print();
		globalRouteTable = full;
		LOG(INFO) << "Srt configuration is complete";

		toApp();
	}
	
	bool IsForwarder(const Ptr<MsgRecvDataNtf> &);
	bool handleHead(SrtHeader *, uint8_t);
	bool handleHead(SrtHeader *);
	void SendDown1(const Ptr<MsgSendDataReq> &);
	void SendDown2(const Ptr<MsgRecvDataNtf> &);
	bool CanSend(const Ptr<MsgSendDataReq> &);
private:
	void addEntry(RouteTable &rt, vector<YAML::Node> &vt);
	void toApp();	
        uint16_t serialNumCount = 0;
};
struct Top : hsm::State<Top, hsm::StateMachine, Idle>
{
};

struct Idle : hsm::State<Idle, Top>
{	
	typedef hsm_vector<MsgSendDataReq, MsgRecvDataNtf, ComeBackToIdle> reactions;

	HSM_TRANSIT(MsgSendDataReq, WaitRsp, &Srt::SendDown1, &Srt::CanSend);
	HSM_TRANSIT(MsgRecvDataNtf, WaitRsp, &Srt::SendDown2, &Srt::IsForwarder);
	HSM_DEFER(ComeBackToIdle);
};

struct WaitRsp : hsm::State<WaitRsp, Top>
{
	typedef hsm_vector<MsgSendDataRsp, MsgSendDataReq, MsgRecvDataNtf, ComeBackToIdle> reactions;

	HSM_TRANSIT(MsgSendDataRsp, Idle);
	HSM_TRANSIT(ComeBackToIdle, Idle);
	HSM_DEFER(MsgSendDataReq);
	HSM_DEFER(MsgRecvDataNtf);
};

void Srt::addEntry(RouteTable &rt, vector<YAML::Node> &vt){
	for(int i = 0; i < vt.size(); i++){
		if(vt[i][0].as<int>() != 0){
			Entry entry = 
				{(uint8_t)vt[i][0].as<int>(), (uint8_t)vt[i][1].as<int>(), (uint8_t)vt[i][2].as<int>(), 0};
			rt.addEntry(entry);
		}
	}
}

void Srt::toApp()
{
    LOG(INFO)<<"toApp()";
	char onlineStatus[2][20];
    full.toArray(onlineStatus);
    //把路由信息发送给UI
    int DestinationID = 0, SourceID = 0, input_length = 20;

    if (UIClient != NULL) {
        //struct App_DataPackage appBuff = {3, 24, 0, 0, 0, {"miss me?"}};
		struct App_DataPackage appBuff;
		appBuff.Package_type = 3;
		appBuff.data_size = 24;
		appBuff.SerialNum = 0;
        char* sendBuff = new char[sizeof(appBuff)];
        bzero(appBuff.data_buffer, sizeof(appBuff.data_buffer));
        memcpy(appBuff.data_buffer, onlineStatus, sizeof(onlineStatus));
        SourceID=int(full.getNodeID());
        DestinationID=int(full.getNodeID());
        appBuff.SourceID = SourceID;
        appBuff.DestinationID = DestinationID;
        memcpy(sendBuff, &appBuff, sizeof(appBuff));
        cout<<"package type : "<<(int)sendBuff[1]<<endl;

        IODataPtr pkt(new IOData);


        pkt->fd   = UIClient->_socketfd;
        pkt->data = sendBuff;
        pkt->len  = sizeof(appBuff);
        UIWriteQueue::Push(pkt);
        cliwrite(UIClient);
	}
}

bool Srt::CanSend(const Ptr<MsgSendDataReq> &msg)
{
    if(full.findNextNode(msg->address) == 0){
        Ptr<MsgSendDataRsp> p(new MsgSendDataRsp);
	    SendRsp(p, TRA_LAYER, TRA_PID);
        return false;
    }
    return true;
}
bool Srt::IsForwarder(const Ptr<MsgRecvDataNtf> &msg)
{
	LOG(INFO) << "Srt receive a packet from lower layer";
	
	SrtHeader *header = msg->packet.Header<SrtHeader>();
	uint8_t* hdr = (uint8_t*)msg->packet.realRaw();
	LOG(INFO)<<int(*(hdr))<<endl;
	LOG(INFO)<<int(*(hdr+1))<<endl;
	LOG(INFO)<<int(*(hdr+2))<<endl;
	if(header->nextID==full.getNodeID()&&header->destID==full.getNodeID())
		{
			//把数据送到UI
			if(UIClient != NULL)
			{
				struct App_DataPackage recvPackage;
				memcpy(&recvPackage, msg->packet.realRaw(), sizeof(recvPackage));
				recvPackage.Package_type = 6;
				recvPackage.data_buffer[0] = NET_LAYER;

				char* sendBuff = new char[sizeof(recvPackage)];
				memcpy(sendBuff, &recvPackage, sizeof(recvPackage));
				IODataPtr pkt(new IOData);


				pkt->fd   = UIClient->_socketfd;
				pkt->data = sendBuff;
				pkt->len  = sizeof(recvPackage);
				UIWriteQueue::Push(pkt);
				cliwrite(UIClient);
			}
			msg->packet.Move<SrtHeader>();
                        string logStr; 
                        if(((uint8_t*)msg->packet.realRaw())[0] == 100||((uint8_t*)msg->packet.realRaw())[0] == 101){ 
			logStr = Trace::Instance().Log(NET_LAYER, NET_PID, header->serialNum, msg->packet.realDataSize(), (int)header->srcID, -1, (int)header->nextID , (int)header->destID, -1, -1, -1, -1, "data", "sendUp");
}else{
			logStr = Trace::Instance().Log(NET_LAYER, NET_PID, msg->packet, (int)header->srcID, -1, (int)header->nextID , (int)header->destID, -1, -1, -1, -1, "data", "sendUp");
}
			if (TraceClient != NULL)
			{
				IODataPtr pkt(new IOData);

				pkt->fd = TraceClient->_socketfd;

				char* sendBuff = new char[logStr.length()];
				memcpy(sendBuff, logStr.data(), logStr.length());
				pkt->data = sendBuff;
				pkt->len = logStr.length();
				TraceWriteQueue::Push(pkt);
				cliwrite(TraceClient);
			}
		    SendNtf(msg, TRA_LAYER, TRA_PID);
			return false;
		}else if(header->nextID ==full.getNodeID())
		{
			return true;
		}else{
			string logStr = Trace::Instance().Log(NET_LAYER, NET_PID, msg->packet, (int)header->srcID, -1, (int)header->nextID , (int)header->destID, -1, -1, -1, -1, "data", "discard");
			LOG(INFO)<<"header->srcID:"<<(int)header->srcID<<" header->nextID:"<<(int)header->nextID
			<<" header->destID:"<<(int)header->destID<<endl;
			if (TraceClient != NULL)
			{
				IODataPtr pkt(new IOData);

				pkt->fd = TraceClient->_socketfd;

				char* sendBuff = new char[logStr.length()];
				memcpy(sendBuff, logStr.data(), logStr.length());
				pkt->data = sendBuff;
				pkt->len = logStr.length();
				TraceWriteQueue::Push(pkt);
				cliwrite(TraceClient);
			}
			return false;
		}
}

bool Srt::handleHead(SrtHeader *header)
{
	if(full.findNextNode(header->destID) == 0)
	{
		return false;
	}
	header->nextID = full.findNextNode(header->destID);	
}

bool Srt::handleHead(SrtHeader *header, uint8_t dest)
{
	if(full.findNextNode(dest) == 0)
	{
		return false;
	}
	header->nextID = full.findNextNode(dest);
	header->srcID = full.getNodeID();
	header->destID = dest;
	return true;
}

void Srt::SendDown1(const Ptr<MsgSendDataReq> &msg)
{
	LOG(INFO) << "Srt receive a packet from upper layer";
    
    uint8_t* hdr = (uint8_t*)msg->packet.realRaw();
	LOG(INFO)<<int(*(hdr))<<endl;
	LOG(INFO)<<int(*(hdr+1))<<endl;
	LOG(INFO)<<int(*(hdr+2))<<endl;
    
	if(UIClient != NULL)
    {
        struct App_DataPackage recvPackage;
        memcpy(&recvPackage, msg->packet.realRaw(), sizeof(recvPackage));
        recvPackage.Package_type = 6;
        recvPackage.data_buffer[0] = NET_LAYER;
        LOG(INFO)<<"drt send sourceID  = "<<(int)recvPackage.SourceID;

        char* sendBuff = new char[sizeof(recvPackage)];
        memcpy(sendBuff, &recvPackage, sizeof(recvPackage));
        IODataPtr pkt(new IOData);


        pkt->fd   = UIClient->_socketfd;
        pkt->data = sendBuff;
        pkt->len  = sizeof(recvPackage);
        UIWriteQueue::Push(pkt);
        cliwrite(UIClient);
    }
    
	SrtHeader *header = msg->packet.Header<SrtHeader>();
	LOG(INFO)<<"sendDown()...msg->address"<<msg->address;
	uint8_t dest = (uint8_t)msg->address;
        header->serialNum = serialNumCount;
        serialNumCount = serialNumCount + 1;
        string logStr;
	if(handleHead(header, dest)){
                        if(((uint8_t*)msg->packet.realRaw())[0] == 100 ||((uint8_t*)msg->packet.realRaw())[0] == 101){ 
			logStr = Trace::Instance().Log(NET_LAYER, NET_PID, header->serialNum, msg->packet.realDataSize(), (int)header->srcID, -1, (int)header->nextID , (int)header->destID, -1, -1, -1, -1, "data", "send");
}else{
		logStr = Trace::Instance().Log(NET_LAYER, NET_PID, msg->packet, (int)header->srcID, -1,  (int)header->nextID,  (int)header->destID, -1, -1, -1, -1, "data", "send");
}
		if (TraceClient != NULL)
		{
			IODataPtr pkt(new IOData);

			pkt->fd = TraceClient->_socketfd;

			char* sendBuff = new char[logStr.length()];
			memcpy(sendBuff, logStr.data(), logStr.length());
			pkt->data = sendBuff;
			pkt->len = logStr.length();
			TraceWriteQueue::Push(pkt);
			cliwrite(TraceClient);
		}
		msg->packet.Move<SrtHeader>();
		msg->address = header->nextID;
		SendReq(msg, MAC_LAYER, MAC_PID);
		Ptr<MsgSendDataRsp> p(new MsgSendDataRsp);
	    SendRsp(p, TRA_LAYER, TRA_PID);
	}
}

void Srt::SendDown2(const Ptr<MsgRecvDataNtf> &msg)
{
	if(UIClient != NULL)
    {
        struct App_DataPackage recvPackage;
        memcpy(&recvPackage, msg->packet.realRaw(), sizeof(recvPackage));
        recvPackage.Package_type = 6;
        recvPackage.data_buffer[0] = NET_LAYER;
        LOG(INFO)<<"drt send sourceID  = "<<(int)recvPackage.SourceID;

        char* sendBuff = new char[sizeof(recvPackage)];
        memcpy(sendBuff, &recvPackage, sizeof(recvPackage));
        IODataPtr pkt(new IOData);


        pkt->fd   = UIClient->_socketfd;
	    pkt->data = sendBuff;
        pkt->len  = sizeof(recvPackage);
        UIWriteQueue::Push(pkt);
        cliwrite(UIClient);
    }

	SrtHeader *header = msg->packet.Header<SrtHeader>();
        string logStr; 
	if(handleHead(header)){
                        if(((uint8_t*)msg->packet.realRaw())[0] == 100 ||((uint8_t*)msg->packet.realRaw())[0] == 101){ 
			logStr = Trace::Instance().Log(NET_LAYER, NET_PID, header->serialNum, msg->packet.realDataSize(), (int)header->srcID, -1, (int)header->nextID , (int)header->destID, -1, -1, -1, -1, "data", "forward");
}else{
		logStr = Trace::Instance().Log(NET_LAYER, NET_PID,msg->packet, (int)header->srcID, -1,  (int)header->nextID,  (int)header->destID, -1, -1, -1, -1, "data", "forward");
}
		if (TraceClient != NULL)
		{
			IODataPtr pkt(new IOData);

			pkt->fd = TraceClient->_socketfd;

			char* sendBuff = new char[logStr.length()];
			memcpy(sendBuff, logStr.data(), logStr.length());
			pkt->data = sendBuff;
			pkt->len = logStr.length();
			TraceWriteQueue::Push(pkt);
			cliwrite(TraceClient);
		}
		msg->packet.ChangeDir();
		
		Ptr<MsgSendDataReq> m(new MsgSendDataReq);
		m->packet = msg->packet;
		m->address = header->nextID;
        if(msg->needCRC){
            LOG(INFO)<<"Forwarder SIGNIFICANT packet";
            m->needCRC = true;
        } else {
            LOG(INFO)<<"Forwarder NORMAL packet";
            m->needCRC = false;
        }
		SendReq(m, MAC_LAYER, MAC_PID);
	}else{
	    Ptr<ComeBackToIdle> p(new ComeBackToIdle);
        GetHsm().ProcessEvent(p);
	}
}
	MODULE_INIT(Srt)
	PROTOCOL_HEADER(SrtHeader)
#endif
}










