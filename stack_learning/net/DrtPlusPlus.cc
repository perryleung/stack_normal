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
#include "client.h"

#define CURRENT_PID 6

extern client* UIClient;

extern RouteTable globalRouteTable;

namespace drtPlusPlus{
#if NET_PID == CURRENT_PID

//声明事件
using msg::MsgSendDataReq;
using msg::MsgSendDataRsp;
using msg::MsgRecvDataNtf;
using msg::MsgTimeOut;

//定义事件
struct MsgRecvData : hsm::Event<MsgRecvData>
{
    pkt::Packet packet;
};
struct MsgRecvHello : hsm::Event<MsgRecvHello>
{
    pkt::Packet packet;
};
struct MsgRecvHelloRsp : hsm::Event<MsgRecvHelloRsp>
{
	pkt::Packet packet;
};
struct MsgRecvUpdate : hsm::Event<MsgRecvUpdate>
{
	pkt::Packet packet;
};
//时间到了，发送HelloRsp
struct MsgTO1 : hsm::Event<MsgTO1>
{
};
//时间到了，发送Update
struct MsgTO2 : hsm::Event<MsgTO2>
{
};
//时间到了，暂停发包，收尾
struct MsgTO3 : hsm::Event<MsgTO3>
{
};
//时间到了，发送hello包
struct MsgTO4 : hsm::Event<MsgTO4>
{
};

//定义宏,定时器类型
const uint16_t TimeForHelloRsp = 1;
const uint16_t TimeForUpdate = 2;
const uint16_t TimeForPause =3;
const uint16_t TimeForHello = 4;

//定义协议包头
struct DrtPlusPlusHeader
{
    uint8_t type;
    uint8_t srcID;
	uint8_t lastID;
    uint8_t nextID;
    uint8_t destID;
};

//定义宏，包类型
const uint8_t HELLO = 1;
const uint8_t HELLO_RSP = 2;
const uint8_t UPDATE = 3;
const uint8_t DATA = 4;

//定义宏，广播地址
const uint8_t BROADCAST = 255;

//定义宏，包头中无用字段
const uint8_t USELESS = 0;

//声明状态
struct Top;
struct Init;
struct Idle;
struct WaitTopo;
struct EmptyWindow;
struct WaitRsp;

//定义DrtPlus协议
class DrtPlusPlus : public mod::Module<DrtPlusPlus, NET_LAYER, CURRENT_PID>
{
	private:
		RouteTable full;//总路由表
		RouteTable incre;//更新路由表
		uint8_t helloPktSeq;//hello包序号
		uint8_t recvHelloPktSeq;
	public:
		DrtPlusPlus()
		{

            GetSap().SetOwner(this);
            GetHsm().Initialize<Top>(this);
            GetTap().SetOwner(this);
		}
		void Init()
		{
			uint8_t nodeID = 
					(uint8_t)Config::Instance()["drtPlus"]["nodeID"].as<int>();
			full.setNodeID(nodeID);
			incre.setNodeID(nodeID);
			Entry entry={nodeID, nodeID, 0 , 255};
			full.addEntry(entry);
			helloPktSeq = 0;
			srand(full.getNodeID());
			SetTimer(10, TimeForHello);
		}
		void sendHello(const Ptr<MsgTO4> &msg);
		void sendDown1(const Ptr<MsgSendDataReq> &msg);
		void sendDown2(const Ptr<MsgRecvData> &msg);
		void doNothing(const Ptr<MsgSendDataRsp> &msg){}
		void parseNtf(const Ptr<MsgRecvDataNtf> &msg);
		void parseTO(const Ptr<MsgTimeOut> &msg);
		void setTimerForRsp(const Ptr<MsgRecvHello> &msg);
		void doNothing(const Ptr<MsgRecvHelloRsp> &msg){}
		void updateRouteTable1(const Ptr<MsgRecvUpdate> &msg);
		bool isForwarder(const Ptr<MsgRecvData> &msg);
		void sendHelloRsp(const Ptr<MsgTO1> &msg);
		void doNothing(const Ptr<MsgTO2> &msg){}
		void pause(const Ptr<MsgTO3> &msg);
		void updateRouteTable2(const Ptr<MsgRecvHelloRsp> &msg);
		void updateRouteTable2(const Ptr<MsgRecvUpdate> &msg);
		void sendUpdate(const Ptr<MsgTO2> &msg);
	private:
		bool handleHead(DrtPlusPlusHeader *header);
		bool handleHead(DrtPlusPlusHeader *header, uint8_t dest);
		void RT2packet(RouteTable &rt, pkt::Packet &pkt);
		void packet2RT(RouteTable &rt, pkt::Packet &pkt);
		void toApp();
		int timeRand();
	static void IdleCB(EV_P_ ev_idle *w, int revents){
        ev_idle_stop(EV_A_ w);  // 需要stop，否则会循环
        free(w);
    }
};

//define the state
struct Top : hsm::State<Top, hsm::StateMachine, Init>
{
};
struct Init : hsm::State<Init, Top>
{
	typedef hsm_vector<MsgSendDataReq, MsgRecvDataNtf, MsgTimeOut, MsgTO4> reactions;
	//什么都不做，直到定时器事件到达
	HSM_WORK(MsgTimeOut, &DrtPlusPlus::parseTO);
	HSM_TRANSIT(MsgTO4, WaitTopo, &DrtPlusPlus::sendHello);
	HSM_DEFER(MsgSendDataReq);
	HSM_DEFER(MsgRecvDataNtf);
};
struct Idle : hsm::State<Idle, Top>
{
	typedef hsm_vector<MsgSendDataReq, MsgSendDataRsp, MsgRecvDataNtf, MsgTimeOut,
			MsgRecvHello, MsgRecvHelloRsp, MsgRecvUpdate, MsgRecvData,
			MsgTO1, MsgTO2, MsgTO3> reactions;
	HSM_TRANSIT(MsgSendDataReq, WaitRsp, &DrtPlusPlus::sendDown1);//useful
	HSM_WORK(MsgSendDataRsp, &DrtPlusPlus::doNothing);//impossible
	HSM_WORK(MsgRecvDataNtf, &DrtPlusPlus::parseNtf);//useful
	HSM_WORK(MsgTimeOut, &DrtPlusPlus::parseTO);//useful
	HSM_WORK(MsgRecvHello, &DrtPlusPlus::setTimerForRsp);//useful
	HSM_WORK(MsgRecvHelloRsp, &DrtPlusPlus::doNothing);//do nothing
	HSM_TRANSIT(MsgRecvUpdate, WaitTopo, &DrtPlusPlus::updateRouteTable1);//useful
	HSM_TRANSIT(MsgRecvData, WaitRsp, &DrtPlusPlus::sendDown2, &DrtPlusPlus::isForwarder);
	//useful
	HSM_WORK(MsgTO1, &DrtPlusPlus::sendHelloRsp);//useful
	HSM_WORK(MsgTO2, &DrtPlusPlus::doNothing);//impossible
	HSM_TRANSIT(MsgTO3, EmptyWindow, &DrtPlusPlus::pause);
};
struct WaitTopo : hsm::State<WaitTopo, Top>
{
	typedef hsm_vector<MsgSendDataReq, MsgSendDataRsp, MsgRecvDataNtf, MsgTimeOut,
			MsgRecvHello, MsgRecvHelloRsp, MsgRecvUpdate, MsgRecvData,
			MsgTO1, MsgTO2, MsgTO3> reactions;
	HSM_DEFER(MsgSendDataReq);
	HSM_WORK(MsgSendDataRsp, &DrtPlusPlus::doNothing);//impossible
	HSM_WORK(MsgRecvDataNtf, &DrtPlusPlus::parseNtf);//useful
	HSM_WORK(MsgTimeOut, &DrtPlusPlus::parseTO);//useful
	HSM_DEFER(MsgRecvHello);
	HSM_WORK(MsgRecvHelloRsp, &DrtPlusPlus::updateRouteTable2);//useful
	HSM_WORK(MsgRecvUpdate, &DrtPlusPlus::updateRouteTable2);//useful
	HSM_DEFER(MsgRecvData);
	HSM_DEFER(MsgTO1);
	HSM_TRANSIT(MsgTO2, Idle, &DrtPlusPlus::sendUpdate);//useful
	HSM_DEFER(MsgTO3);
};

struct EmptyWindow : hsm::State<EmptyWindow, Top>
{
    typedef hsm_vector<MsgSendDataReq, MsgSendDataRsp, MsgRecvDataNtf, MsgTimeOut,
			MsgRecvHello, MsgRecvHelloRsp, MsgRecvUpdate, MsgRecvData,
			MsgTO1, MsgTO2, MsgTO4> reactions;
	HSM_DEFER(MsgSendDataReq);
	HSM_WORK(MsgSendDataRsp, &DrtPlusPlus::doNothing);//impossible
	HSM_WORK(MsgRecvDataNtf, &DrtPlusPlus::parseNtf);//useful
	HSM_WORK(MsgTimeOut, &DrtPlusPlus::parseTO);//useful
	HSM_WORK(MsgRecvHello, &DrtPlusPlus::setTimerForRsp);//useful
	HSM_WORK(MsgRecvHelloRsp, &DrtPlusPlus::doNothing);//do nothing
	HSM_TRANSIT(MsgRecvUpdate, WaitTopo, &DrtPlusPlus::updateRouteTable1);//useful
	HSM_TRANSIT(MsgRecvData, WaitRsp, &DrtPlusPlus::sendDown2, &DrtPlusPlus::isForwarder);
	//useful
	HSM_WORK(MsgTO1, &DrtPlusPlus::sendHelloRsp);//useful
	HSM_WORK(MsgTO2, &DrtPlusPlus::doNothing);//impossible
	HSM_TRANSIT(MsgTO4, WaitTopo, &DrtPlusPlus::sendHello);
};

struct WaitRsp : hsm::State<WaitRsp, Top>
{
	typedef hsm_vector<MsgSendDataReq, MsgSendDataRsp, MsgRecvDataNtf, MsgTimeOut
			> reactions;
	HSM_DEFER(MsgSendDataReq);
	HSM_TRANSIT(MsgSendDataRsp, Idle);//useful
	HSM_DEFER(MsgRecvDataNtf);
	HSM_DEFER(MsgTimeOut);
};

void DrtPlusPlus::pause(const Ptr<MsgTO3> &msg)
{
    SetTimer(4*60, TimeForHello);
}

void DrtPlusPlus::sendHello(const Ptr<MsgTO4> &msg)
{
    LOG(INFO)<<"sendHello()";
	//make a packet
	pkt::Packet pkt(0);
	DrtPlusPlusHeader *header = pkt.Header<DrtPlusPlusHeader>();
	header->type = HELLO;
	header->srcID = full.getNodeID();
	header->lastID = helloPktSeq++;
	header->nextID = USELESS;
	header->destID = BROADCAST;
	pkt.Move<DrtPlusPlusHeader>();
	Trace::Instance().Log(NET_LAYER, NET_PID, -1, -1, (int)header->srcID, -1, -1, (int)header->destID, (int)header->lastID, -1, -1, -1, "hello", "send");

	//make a msg
	Ptr<MsgSendDataReq> m(new MsgSendDataReq);
	m->packet = pkt;
	m->address = BROADCAST;

	//send down the msg
	SendReq(m, MAC_LAYER, MAC_PID);

	//让其马上发送，而非io触发	
    struct ev_idle *idle_watcher = new struct ev_idle;
    ev_idle_init(idle_watcher, IdleCB);
    ev_idle_start(LibevTool::Instance().GetLoop(), idle_watcher);
	SetTimer(rand()%5+15, TimeForUpdate);
	SetTimer(30*60, TimeForPause);
}
void DrtPlusPlus::sendDown1(const Ptr<MsgSendDataReq> &msg)
{
    LOG(INFO)<<"sendDown1()";
	//不懂
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
	//填充头部
	DrtPlusPlusHeader *header = msg->packet.Header<DrtPlusPlusHeader>();
	uint8_t dest = (uint8_t)msg->address;
	if(handleHead(header, dest))
	{
		//打印log
		Trace::Instance().Log(NET_LAYER, NET_PID, msg->packet, (int)header->srcID,  (int)header->lastID,  (int)header->nextID,  (int)header->destID, -1, -1, -1, -1, "data", "send"); 
		//更新msg，往下发
		msg->packet.Move<DrtPlusPlusHeader>();
		msg->address = header->nextID;
		SendReq(msg, MAC_LAYER, MAC_PID);
		//回上层Rsp
		Ptr<MsgSendDataRsp> m(new MsgSendDataRsp);
		SendRsp(m, TRA_LAYER, TRA_PID);
	}else{
		LOG(INFO)<<"can not find next nodeID";
	}
}
void DrtPlusPlus::sendDown2(const Ptr<MsgRecvData> &msg)
{
    LOG(INFO)<<"sendDown2()";
	//不懂
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
	//处理头
	DrtPlusPlusHeader *header = msg->packet.Header<DrtPlusPlusHeader>();
	if(handleHead(header))
	{
		//打印log
		Trace::Instance().Log(NET_LAYER, NET_PID, msg->packet, (int)header->srcID,  (int)header->lastID,  (int)header->nextID,  (int)header->destID, -1, -1, -1, -1, "data", "forward");
		//构造MsgSendDataReq，往下发
		msg->packet.ChangeDir();
		Ptr<MsgSendDataReq> m(new MsgSendDataReq);
		m->packet = msg->packet;
		m->address = header->nextID;
		SendReq(m, MAC_LAYER, MAC_PID);
	}else{
		LOG(INFO)<<"can not find next nodeID";
	}
}
void DrtPlusPlus::parseNtf(const Ptr<MsgRecvDataNtf> &msg)
{
    LOG(INFO)<<"parseNtf()";
	DrtPlusPlusHeader *header=msg->packet.Header<DrtPlusPlusHeader>();
	if(header->type==DATA)
	{
        LOG(INFO)<<"header->type==DATA";
		//Trace::Instance().Log(NET_LAYER, NET_PID, msg->packet, (int)header->srcID, (int)header->lastID, (int)header->nextID , (int)header->destID, -1, -1, -1, -1, "data", "recv");
		Ptr<MsgRecvData> m(new MsgRecvData);
		m->packet=msg->packet;
		GetHsm().ProcessEvent(m);
	}else if(header->type==HELLO){
        LOG(INFO)<<"header->type==HELLO";  
		Trace::Instance().Log(NET_LAYER, NET_PID, -1, -1, (int)header->srcID, -1, -1, (int)header->destID, (int)header->lastID, -1, -1, -1, "hello", "recv");
		Ptr<MsgRecvHello> m(new MsgRecvHello);
		m->packet=msg->packet;
		GetHsm().ProcessEvent(m);
	}else if(header->type==HELLO_RSP){
        LOG(INFO)<<"header->type==HELLO_RSP";  
		Trace::Instance().Log(NET_LAYER, NET_PID, -1, -1, (int)header->srcID, -1, -1, (int)header->destID, -1, -1, -1, -1, "helloRsp", "recv");
		Ptr<MsgRecvHelloRsp> m(new MsgRecvHelloRsp);
		m->packet=msg->packet;
		GetHsm().ProcessEvent(m);
	}else if(header->type==UPDATE){
        LOG(INFO)<<"header->type==UPDATE";  
		Trace::Instance().Log(NET_LAYER, NET_PID, -1, -1, (int)header->srcID, -1, -1, (int)header->destID, -1, -1, -1, -1, "update", "recv");
		Ptr<MsgRecvUpdate> m(new MsgRecvUpdate);
		m->packet=msg->packet;
		GetHsm().ProcessEvent(m);	
	}
}
void DrtPlusPlus::parseTO(const Ptr<MsgTimeOut> &msg)
{
    LOG(INFO)<<"parseTO()";
    LOG(INFO)<<msg->msgId;
    if(msg->msgId == TimeForHelloRsp){
        LOG(INFO)<<"TO1";
        Ptr<MsgTO1> m(new MsgTO1);
        GetHsm().ProcessEvent(m);
    }else if(msg->msgId == TimeForUpdate){
        LOG(INFO)<<"TO2";
        Ptr<MsgTO2> m(new MsgTO2);
        GetHsm().ProcessEvent(m);
    }else if(msg->msgId == TimeForPause){
        LOG(INFO)<<"TO3";
        Ptr<MsgTO3> m(new MsgTO3);
        GetHsm().ProcessEvent(m);
    }else if(msg->msgId == TimeForHello){
        LOG(INFO)<<"TO4";
        Ptr<MsgTO4> m(new MsgTO4);
        GetHsm().ProcessEvent(m);
    }
}
void DrtPlusPlus::setTimerForRsp(const Ptr<MsgRecvHello> &msg)
{
    LOG(INFO)<<"setTimerForRsp()";
	DrtPlusPlusHeader *header = msg->packet.Header<DrtPlusPlusHeader>();
	recvHelloPktSeq = header->lastID;
	//保存hello包中的信息到路由表中
	RouteTable recv;
	recv.setNodeID(header->srcID);
	Entry entry = {header->srcID, header->srcID, 0, header->lastID};
	recv.addEntry(entry);
	full.handleRouteTable(recv, incre);
	globalRouteTable = full;
	if(incre.getEntry().size()>0){
		full.print();
		incre.print();
		toApp();
	}
	//设置定时器1
    /*
    int seed=full.getNodeID();
    srand(seed);
    int times=rand()%5+1;
    */
    SetTimer(rand()%5+1, TimeForHelloRsp);
}
void DrtPlusPlus::updateRouteTable1(const Ptr<MsgRecvUpdate> &msg)
{
    LOG(INFO)<<"updateRouteTable1()";
	RouteTable recv;
	packet2RT(recv, msg->packet);
	full.handleRouteTable(recv, incre);
	globalRouteTable = full;
	SetTimer(rand()%5+8, TimeForUpdate);
}
bool DrtPlusPlus::isForwarder(const Ptr<MsgRecvData> &msg)
{
    LOG(INFO)<<"isForwarder()";
	DrtPlusPlusHeader *header = msg->packet.Header<DrtPlusPlusHeader>();
	//检查路由表中是否有该包的源节点，若没有，就加入路由表
	if(full.findNextNode(header->srcID) == 0){
		Entry entry = {header->srcID, header->lastID, 255, 0};
		full.addEntry(entry);
		toApp();
	}
	//检查路由表中是否有该包的上一跳节点，若没有，就加入路由表
	if(full.findNextNode(header->lastID) == 0){
		Entry entry = {header->lastID, header->lastID, 0, 0};
		full.addEntry(entry);
		toApp();
	}
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
		msg->packet.Move<DrtPlusPlusHeader>();
		//把MsgRecvData从新包装成MsgRecvDataNtf
		Ptr<MsgRecvDataNtf> m(new MsgRecvDataNtf);
		m->packet=msg->packet;
		Trace::Instance().Log(NET_LAYER, NET_PID, msg->packet, (int)header->srcID, (int)header->lastID, (int)header->nextID , (int)header->destID, -1, -1, -1, -1, "data", "sendUp");
        SendNtf(m, TRA_LAYER, TRA_PID);
		return false;
	}else if(header->nextID ==full.getNodeID())
	{
		return true;
	}else{
		Trace::Instance().Log(NET_LAYER, NET_PID, msg->packet, (int)header->srcID, (int)header->lastID, (int)header->nextID , (int)header->destID, -1, -1, -1, -1, "data", "discard");
		return false;
	}
}
void DrtPlusPlus::sendHelloRsp(const Ptr<MsgTO1> &msg)
{
    LOG(INFO)<<"sendHelloRsp()";
	//填充包头
	pkt::Packet pkt(50);
	RT2packet(full, pkt);
	DrtPlusPlusHeader *header=pkt.Header<DrtPlusPlusHeader>();
	header->type = HELLO_RSP;
	header->srcID = full.getNodeID();
	header->lastID = USELESS;
	header->nextID = USELESS;
	header->destID = BROADCAST;
	pkt.Move<DrtPlusPlusHeader>();
	Trace::Instance().Log(NET_LAYER, NET_PID, -1, -1, (int)header->srcID, -1, -1, (int)header->destID, -1, -1, -1, -1, "helloRsp", "send");
	//发送包
	Ptr<MsgSendDataReq> m(new MsgSendDataReq);
    m->packet=pkt;
    m->address = BROADCAST;
    SendReq(m, MAC_LAYER, MAC_PID);
}
void DrtPlusPlus::updateRouteTable2(const Ptr<MsgRecvHelloRsp> &msg)
{
    LOG(INFO)<<"updateRouteTable2()";
	RouteTable recv;
	packet2RT(recv, msg->packet);
    full.handleRouteTable(recv, incre);
    globalRouteTable = full;
}
void DrtPlusPlus::updateRouteTable2(const Ptr<MsgRecvUpdate> &msg)
{
    LOG(INFO)<<"updateRouteTable2()";
	RouteTable recv;
	packet2RT(recv, msg->packet);
    full.handleRouteTable(recv, incre);
    globalRouteTable = full;
}
void DrtPlusPlus::sendUpdate(const Ptr<MsgTO2> &msg)
{
    LOG(INFO)<<"sendUpdate()";
	if(incre.getEntry().size()>0)
	{
	    LOG(INFO)<<"incre.getEntry().size()>0...";
		pkt::Packet pkt(50);
		RT2packet(incre, pkt);
		DrtPlusPlusHeader *header=pkt.Header<DrtPlusPlusHeader>();	
		header->type=UPDATE;
		header->srcID = full.getNodeID();
		header->lastID = USELESS;
		header->nextID = USELESS;
		header->destID = BROADCAST;
		pkt.Move<DrtPlusPlusHeader>();

		Trace::Instance().Log(NET_LAYER, NET_PID, -1, -1, (int)header->srcID, -1, -1, (int)header->destID, -1, -1, -1, -1, "update", "send");
		
		Ptr<MsgSendDataReq> m(new MsgSendDataReq);
		m->packet=pkt;
		m->address = BROADCAST;
		SendReq(m, MAC_LAYER, MAC_PID);
		full.print();
		incre.print();
		toApp();
		incre.clearEntry();
	}
}
bool DrtPlusPlus::handleHead(DrtPlusPlusHeader *header)
{
    LOG(INFO)<<"handleHead()";
	if(full.findNextNode(header->destID) == 0)
	{
		return false;
	}
	header->lastID = full.getNodeID();
	header->nextID = full.findNextNode(header->destID);
	return true;
}
bool DrtPlusPlus::handleHead(DrtPlusPlusHeader *header, uint8_t dest)
{
    LOG(INFO)<<"handleHead()";
	if(full.findNextNode(dest) == 0)
	{
		return false;
	}
	header->type = DATA;
	header->srcID = full.getNodeID();
	header->lastID = full.getNodeID();
	header->nextID = full.findNextNode(dest);
	header->destID = dest;
	return true;
}
void DrtPlusPlus::RT2packet(RouteTable &rt, pkt::Packet &pkt)
{
    LOG(INFO)<<"RT2packet()";
	char routeArr[50];
	routeArr[0]=rt.getNodeID();
	int i=1;
    vector<Entry>::iterator iter=rt.getEntry().begin();
    while(iter!=rt.getEntry().end())
    {   
        routeArr[i++]=char(iter->destNode);
        routeArr[i++]=char(iter->nextNode);
        routeArr[i++]=char(iter->metric);
		if(iter->destNode==full.getNodeID()){
        	routeArr[i++]=char(recvHelloPktSeq);
		}else{
			routeArr[i++]=char(iter->seqNum);
		}
		
        iter++;
    }
    routeArr[i]=char(0);
    memcpy(pkt.Raw(), routeArr, i+1);
}
void DrtPlusPlus::packet2RT(RouteTable &rt, pkt::Packet &pkt)
{
    LOG(INFO)<<"packet2RT()";
	pkt.Move<DrtPlusPlusHeader>();//很重要，一定要移
	char *ptr=pkt.Raw();
	rt.setNodeID(*ptr++);
	while(*ptr!=0)
	{
		Entry e;
		e.destNode=uint8_t(*ptr++);
		e.nextNode=uint8_t(*ptr++);
		e.metric=uint8_t(*ptr++);
		e.seqNum=uint8_t(*ptr++);
		rt.addEntry(e);
	}
}
void DrtPlusPlus::toApp()
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
int DrtPlusPlus::timeRand()
{
    
}
	MODULE_INIT(DrtPlusPlus)
	PROTOCOL_HEADER(DrtPlusPlusHeader)
#endif
}
