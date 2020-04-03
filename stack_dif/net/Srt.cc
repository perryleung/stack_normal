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

/*当静态路由网络层收到上层的MsgSendDataReq的时候，协议会先判读是否能发送。
（1. 守护条件为CanSend，这里会调用路由表RouteTable的对象full中的findNextNdoe函数。
2. 注意这里既是发不了，也会给UDP返回一个Rsp。可能是考虑到如果不返回，则UDP会一直在WaitRsp状态。
3. 个人认为开头的hdr没什么意义）*/
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
	/*当静态路由层接收到MsgRecvDataNtf，则判断守护条件IsForwarder（主要是判断是否要转发），如果满足条件则调用SendDown2，下发数据包。
守护条件中，首先判断头部下一跳路由地址和目标路由地址是否与自身相同。*/
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
			//如果相同，说明是给本节点的数据包，往UI和Trace传数据之后，就可以往上传传数据包了
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
		//如果下一跳跟本节点路由地址相同，说明这是一个需要转发的包，调用SendDown2。（如果都不是，说明这个包与自己无关，因此抛弃，但是要往Trace写入数据）
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
/*这里通过full路由表找到下一跳，找不到的话就什么也不做，结束这个函数。（由于之前在CanSend那里已经判断过，如果找不到下一跳就返回Rsp给传输层，因此不必再发。但是个人认为没有这个必要，可以直接判断能不能找到，Rsp也照样发就可以了。）如果找到的话，就初始化这个头部，返回true。*/
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

    //SendDown1：首先获得头部指针，然后交给协议中的handleHead函数处理
	//SendDown2：首先往UI写数据。然后获得头部指针
	SrtHeader *header = msg->packet.Header<SrtHeader>();
	LOG(INFO)<<"sendDown()...msg->address"<<msg->address;
	uint8_t dest = (uint8_t)msg->address;
        header->serialNum = serialNumCount;
        serialNumCount = serialNumCount + 1;
        string logStr;
	//传入头部指针和目标路由地址。如果找到了下一跳路由，就发消息给Trace，并且给MAC发数据包，给TRA发Rsp
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
	//把头部指针传入handleHead函数。（这里是为了改变下一跳地址）
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
		/*注意，这里的数据包要先改变方向，然后生成一个MsgSendDataReq事件给MAC层，地址需要重新赋值。由于指针本身就在那个位置，没有执行过Move函数，因此此处也不需要移动，只要赋值即可。（由于目前路由地址和MAC地址是一致的，因此可能address可以给网络层和MAC层使用，但个人认为需要修改。）*/
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
		//但是如果，下一跳地址是本节点，目标地址不是本节点，但本节点又找不到下一跳，则产生一个ComeBackToIdle事件，让处于WaitRsp状态的协议返回Idle。（这里像是是强行让状态返回了，感觉可以改进）
	}else{
	    Ptr<ComeBackToIdle> p(new ComeBackToIdle);
        GetHsm().ProcessEvent(p);
	}
	//（ComeBackToIdle事件的存在主要是因为：IsForwarder中判断的是，数据包的下一跳节点是否本节点，如果是的话，就马上进入WaitRsp状态，并且执行SendDown2。但是如果这个时候发现，并不能在路由表中找到目标节点，则说明是发不出去，因此可以直接返回Idle）
}
	MODULE_INIT(Srt)
	PROTOCOL_HEADER(SrtHeader)
#endif
}










