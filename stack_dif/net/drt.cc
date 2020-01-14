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

#define CURRENT_PID 4

using std::vector;
using std::endl;
using std::cout;

//extern client* UIClient;
//extern int sockfd;
extern client* UIClient;
//extern bool package_write(char* p_buffer, uint8_t datalength, uint8_t Package_type, uint8_t DestinationID, uint8_t SourceID);
extern RouteTable globalRouteTable;

namespace drt{
#if NET_PID == CURRENT_PID

using msg::MsgSendDataReq;
using msg::MsgSendDataRsp;
using msg::MsgRecvDataNtf;
using msg::MsgTimeOut;
using msg::MsgSendAckNtf;

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

/*
struct DrtInfoHeader
{
    uint8_t type;
};
*/
struct DrtDataHeader
{
    uint8_t type;
    uint8_t srcNode;
    uint8_t nextNode;
    uint8_t destNode;
};
const uint8_t HELLO=1;
const uint8_t HELLO_RSP=2;
const uint8_t UPDATE=3;
const uint8_t DATA=4;

struct Top;
struct WaitInit;
struct TopoInit;
struct Idle;
struct WaitUpdate;
struct WaitRsp;

class Drt : public mod::Module<Drt, NET_LAYER, CURRENT_PID>
{
    private:
        RouteTable full;
		RouteTable incre;
    public:
        Drt()
        {
            GetSap().SetOwner(this);
            GetHsm().Initialize<Top>(this);
            GetTap().SetOwner(this);
        }
        void Init()
        {
            uint8_t nodeID=uint8_t(atoi(Config::Instance()["drt"]["nodeID"].as<std::string>().c_str()));
            full.setNodeID(nodeID);
            incre.setNodeID(nodeID);
            Entry entry={nodeID, nodeID, 0, 1};
		    full.addEntry(entry);
            SetTimer(10);
            LOG(INFO)<<"Drt Init";
        }
        void handleIt(const Ptr<MsgRecvDataNtf> &);
        void sendDown1(const Ptr<MsgSendDataReq> &);
        void sendDown2(const Ptr<MsgRecvData> &);
		void updateRouteTable1(const Ptr<MsgRecvHelloRsp> &);
		void updateRouteTable2(const Ptr<MsgRecvUpdate> &);
		void updateRouteTable3(const Ptr<MsgRecvUpdate> &);
		void sendUpdate1(const Ptr<MsgTimeOut> &);
		void sendUpdate2(const Ptr<MsgTimeOut> &);
		bool isForwarder(const Ptr<MsgRecvData> &);
		void setTimerForRsp(const Ptr<MsgRecvHello> &);
		void sendHelloRsp(const Ptr<MsgTimeOut> &);
        void sendHello(const Ptr<MsgTimeOut> &);
        void sendAckNtfFalse(const Ptr<MsgRecvHello> &);
        void sendAckNtfFalse(const Ptr<MsgRecvHelloRsp> &);
        void sendAckNtfFalse(const Ptr<MsgRecvUpdate> &);
        void sendAckNtfFalse(const Ptr<MsgRecvData> &);
    private:
        void RT2packet(RouteTable &, pkt::Packet &);
		void packet2RT(RouteTable &, pkt::Packet &);
        void sendAckNtf(bool);
		bool handleHead(DrtDataHeader *);
		bool handleHead(DrtDataHeader *, uint8_t);
        void toApp();
    static void IdleCB(EV_P_ ev_idle *w, int revents){
        ev_idle_stop(EV_A_ w);  // 需要stop，否则会循环
        free(w);
    }
};

struct Top : hsm::State<Top, hsm::StateMachine, WaitInit>
{
};
struct WaitInit : hsm::State<WaitInit, Top>
{
    typedef hsm_vector<MsgTimeOut> reactions;

    HSM_TRANSIT(MsgTimeOut, TopoInit, &Drt::sendHello);	
};
struct TopoInit : hsm::State<TopoInit, Top>
{
    typedef hsm_vector<MsgTimeOut, MsgSendDataReq, MsgRecvDataNtf, MsgRecvHello, MsgRecvHelloRsp, MsgRecvUpdate, MsgRecvData> reactions;

    HSM_TRANSIT(MsgTimeOut, Idle, &Drt::sendUpdate1);
    HSM_DEFER(MsgSendDataReq);
    HSM_WORK(MsgRecvDataNtf, &Drt::handleIt);
    HSM_WORK(MsgRecvHello, &Drt::sendAckNtfFalse);
	HSM_WORK(MsgRecvHelloRsp, &Drt::updateRouteTable1);
	HSM_WORK(MsgRecvUpdate, &Drt::sendAckNtfFalse);
	HSM_WORK(MsgRecvData, &Drt::sendAckNtfFalse);
};
struct Idle : hsm::State<Idle, Top>
{
    typedef hsm_vector<MsgSendDataReq, MsgRecvDataNtf, MsgRecvHello, MsgRecvHelloRsp, MsgRecvUpdate, MsgRecvData, MsgTimeOut> reactions;

    HSM_TRANSIT(MsgSendDataReq, WaitRsp, &Drt::sendDown1);
    HSM_WORK(MsgRecvDataNtf, &Drt::handleIt);
    HSM_WORK(MsgRecvHello, &Drt::setTimerForRsp);
    HSM_WORK(MsgTimeOut, &Drt::sendHelloRsp);
    HSM_WORK(MsgRecvHelloRsp, &Drt::sendAckNtfFalse);
    HSM_TRANSIT(MsgRecvUpdate, WaitUpdate, &Drt::updateRouteTable2);
	HSM_TRANSIT(MsgRecvData, WaitRsp, &Drt::sendDown2, &Drt::isForwarder);
	
	
};
struct WaitUpdate : hsm::State<WaitUpdate, Top>
{	
    typedef hsm_vector<MsgTimeOut, MsgSendDataReq, MsgRecvDataNtf, MsgRecvHello, MsgRecvHelloRsp, MsgRecvUpdate, MsgRecvData> reactions;
    
    HSM_TRANSIT(MsgTimeOut, Idle, &Drt::sendUpdate2);
	HSM_DEFER(MsgSendDataReq);
	HSM_WORK(MsgRecvDataNtf, &Drt::handleIt);
	HSM_DEFER(MsgRecvHello);
	HSM_WORK(MsgRecvHelloRsp, &Drt::sendAckNtfFalse);
	HSM_WORK(MsgRecvUpdate, &Drt::updateRouteTable3);
	HSM_DEFER(MsgRecvData);
		
};
struct WaitRsp : hsm::State<WaitRsp, Top>
{
    typedef hsm_vector<MsgSendDataReq, MsgRecvDataNtf, MsgSendDataRsp> reactions;

    HSM_DEFER(MsgSendDataReq);
    HSM_DEFER(MsgRecvDataNtf);
    HSM_TRANSIT(MsgSendDataRsp, Idle);
};
void Drt::setTimerForRsp(const Ptr<MsgRecvHello> &msg)
{
    int seed=full.getNodeID();
    srand(seed);
    int times=rand()%5+1;
    SetTimer(times);
    LOG(INFO)<<"setTimerForRsp(times:"<<times<<")";
}
void Drt::handleIt(const Ptr<MsgRecvDataNtf> &msg)
{
    cout<<"handleIt"<<endl;
	DrtDataHeader *header=msg->packet.Header<DrtDataHeader>();
	if(header->type==DATA)
	{
	    LOG(INFO)<<"header->type==DATA";
		Ptr<MsgRecvData> m(new MsgRecvData);
		m->packet=msg->packet;
		GetHsm().ProcessEvent(m);
	}else if(header->type==HELLO){
	    sendAckNtf(false);
		//Trace::Instance().Log(NET_LAYER,NET_PID,msg->packet,"hello","recv");
	    LOG(INFO)<<"header->type==HELLO";
		Ptr<MsgRecvHello> m(new MsgRecvHello);
		m->packet=msg->packet;
		GetHsm().ProcessEvent(m);
	}else if(header->type==HELLO_RSP){
	    sendAckNtf(false);
		//Trace::Instance().Log(NET_LAYER,NET_PID,msg->packet,"helloRsp","recv");
	    LOG(INFO)<<"header->type==HELLO_RSP";
		Ptr<MsgRecvHelloRsp> m(new MsgRecvHelloRsp);
		m->packet=msg->packet;
		GetHsm().ProcessEvent(m);
	}else if(header->type==UPDATE){
	    sendAckNtf(false);
		//Trace::Instance().Log(NET_LAYER,NET_PID,msg->packet,"update","recv");
	    LOG(INFO)<<"header->type==UPDATE";
		Ptr<MsgRecvUpdate> m(new MsgRecvUpdate);
		m->packet=msg->packet;
		GetHsm().ProcessEvent(m);	
	}
}
void Drt::sendDown1(const Ptr<MsgSendDataReq> &msg)
{
    cout<<"sendDown1"<<endl;
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

	DrtDataHeader *header=msg->packet.Header<DrtDataHeader>();
	uint8_t dest=uint8_t(msg->address);
	//Trace::Instance().Log(NET_LAYER,NET_PID,msg->packet,"data","send");
	if(handleHead(header, dest))
	{
	    msg->packet.Move<DrtDataHeader>();
		msg->address = header->nextNode;

        SendReq(msg, MAC_LAYER, MAC_PID);
	    Ptr<MsgSendDataRsp> m(new MsgSendDataRsp);
        SendRsp(m, TRA_LAYER, TRA_PID);
	}
}
void Drt::sendDown2(const Ptr<MsgRecvData> &msg)
{
    cout<<"sendDown2"<<endl;
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


	DrtDataHeader *header=msg->packet.Header<DrtDataHeader>();
	//Trace::Instance().Log(NET_LAYER,NET_PID,msg->packet,"data","forward");
	if(handleHead(header))
	{
	    msg->packet.ChangeDir();
	    Ptr<MsgSendDataReq> m(new MsgSendDataReq);
	    m->packet=msg->packet;
		m->address = header->nextNode;
        SendReq(m, MAC_LAYER, MAC_PID);
	}
}
void Drt::updateRouteTable1(const Ptr<MsgRecvHelloRsp> &msg)
{
    LOG(INFO)<<"updateRouteTable1";
	RouteTable recv;
	packet2RT(recv, msg->packet);
    full.handleRouteTable(recv, incre);
    globalRouteTable = full;
}
void Drt::updateRouteTable2(const Ptr<MsgRecvUpdate> &msg)
{
    LOG(INFO)<<"updateRouteTable2";
	SetTimer(8);
	incre.clearEntry();
	RouteTable recv;
	packet2RT(recv, msg->packet);
    full.handleRouteTable(recv, incre);
    globalRouteTable = full;
}
void Drt::updateRouteTable3(const Ptr<MsgRecvUpdate> &msg)
{
    LOG(INFO)<<"updateRouteTable3";
	RouteTable recv;
	packet2RT(recv, msg->packet);
    full.handleRouteTable(recv, incre);
    globalRouteTable = full;

}
void Drt::sendUpdate1(const Ptr<MsgTimeOut> &msg)
{
    LOG(INFO)<<"sendUpdate1";
    
	Entry entry={full.getNodeID(), full.getNodeID(), 0, 1};
	incre.addEntry(entry);
	toApp();
	full.print();
	incre.print();
}
void Drt::sendUpdate2(const Ptr<MsgTimeOut> &msg)
{
    LOG(INFO)<<"sendUpdate2";
	if(incre.getEntry().size()>0)
	{
		toApp();
	}
	full.print();
	incre.print();
}
void Drt::sendHello(const Ptr<MsgTimeOut> &msg)
{
	//Trace::Instance().Log(NET_LAYER,NET_PID,full.getNodeID(),255,65535,255,"hello","send");
    LOG(INFO)<<"sendHello";
	pkt::Packet pkt(sizeof(TestPackage));
	DrtDataHeader *header=pkt.Header<DrtDataHeader>();
	header->type=HELLO;
	pkt.Move<DrtDataHeader>();

	Ptr<MsgSendDataReq> m(new MsgSendDataReq);
    m->packet=pkt;
	m->address=255;
    m->isData = false;
    SendReq(m, MAC_LAYER, MAC_PID);
    // 让其马上发送，而非io触发
    struct ev_idle *idle_watcher = new struct ev_idle;
    ev_idle_init(idle_watcher, IdleCB);
    ev_idle_start(LibevTool::Instance().GetLoop(), idle_watcher);
	SetTimer(15);
}
void Drt::sendHelloRsp(const Ptr<MsgTimeOut> &msg)
{
	//Trace::Instance().Log(NET_LAYER,NET_PID,full.getNodeID(),255,65535,255,"helloRsp","send");
    LOG(INFO)<<"sendHelloRsp";
    
	pkt::Packet pkt(sizeof(TestPackage));
	RT2packet(full, pkt);
	DrtDataHeader *header=pkt.Header<DrtDataHeader>();
	header->type=HELLO_RSP;
	pkt.Move<DrtDataHeader>();

	Ptr<MsgSendDataReq> m(new MsgSendDataReq);
    m->packet=pkt;
	m->address=255;
    m->isData = false;
    SendReq(m, MAC_LAYER, MAC_PID);
}
bool Drt::isForwarder(const Ptr<MsgRecvData> &msg)
{
	DrtDataHeader *header=msg->packet.Header<DrtDataHeader>();
	//Trace::Instance().Log(NET_LAYER,NET_PID,msg->packet,"data","recv");
	if(header->nextNode==full.getNodeID()&&header->destNode==full.getNodeID())
	{
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

		sendAckNtf(true);
		msg->packet.Move<DrtDataHeader>();
		Ptr<MsgRecvDataNtf> m(new MsgRecvDataNtf);
		m->packet=msg->packet;
        SendNtf(m, TRA_LAYER, TRA_PID);
		cout<<"send up"<<endl;
		return false;
	}else if(header->nextNode==full.getNodeID())
	{
	    cout<<"forward"<<endl;
		sendAckNtf(true);
		return true;
	}
	sendAckNtf(false);
	return false;
}
void Drt::RT2packet(RouteTable &r, pkt::Packet &p)
{
    LOG(INFO)<<"RT2packet";
	char routeArr[50];
	routeArr[0]=r.getNodeID();
	int i=1;
    vector<Entry>::iterator iter=r.getEntry().begin();
    while(iter!=r.getEntry().end())
    {
        routeArr[i++]=char(iter->destNode);
        routeArr[i++]=char(iter->nextNode);
        routeArr[i++]=char(iter->metric);
        routeArr[i++]=char(iter->seqNum);
        iter++;
    }
    routeArr[i]=char(0);
    memcpy(p.Raw(), routeArr, i+1);
}
void Drt::packet2RT(RouteTable &r, pkt::Packet &p)
{
    LOG(INFO)<<"packet2RT";
	p.Move<DrtDataHeader>();
	char *ptr=p.Raw();
	r.getNodeID()=*ptr++;
	while(*ptr!=0)
	{
		Entry e;
		e.destNode=uint8_t(*ptr++);
		e.nextNode=uint8_t(*ptr++);
		e.metric=uint8_t(*ptr++);
		e.seqNum=uint8_t(*ptr++);
		r.addEntry(e);
	}
}  
void Drt::sendAckNtf(bool send)
{
    #if MAC_PID == 2
        if(send){
            LOG(INFO)<<"sendAckNtf--true";
        }else{
            LOG(INFO)<<"sendAckNtf--false";
        }   
	    Ptr<MsgSendAckNtf> m(new MsgSendAckNtf);
	    m->send=send;
	    SendNtf(m, MAC_LAYER, MAC_PID);
	#endif
}
bool Drt::handleHead(DrtDataHeader *hdr)
{
    if(full.findNextNode(hdr->destNode)==0)
        return false;
	hdr->nextNode=full.findNextNode(hdr->destNode);
	cout<<"nextNode: "<<(int)hdr->nextNode
	    <<"srcNode: "<<(int)hdr->srcNode
	    <<"destNode: "<<(int)hdr->destNode<<endl;
	return true;
}
bool Drt::handleHead(DrtDataHeader *hdr, uint8_t dest)
{
    hdr->type=DATA;
	hdr->srcNode=full.getNodeID();
	if(full.findNextNode(dest)==0)
	    return false;
	hdr->nextNode=full.findNextNode(dest);
	hdr->destNode=dest;
	cout<<"nextNode: "<<(int)hdr->nextNode
	    <<"srcNode: "<<(int)hdr->srcNode
	    <<"destNode: "<<(int)hdr->destNode<<endl;
	return true;
}
void Drt::sendAckNtfFalse(const Ptr<MsgRecvHello> &msg)
{
    sendAckNtf(false);
}
void Drt::sendAckNtfFalse(const Ptr<MsgRecvHelloRsp> &msg)
{
    sendAckNtf(false);
}
void Drt::sendAckNtfFalse(const Ptr<MsgRecvUpdate> &msg)
{
    sendAckNtf(false);
}
void Drt::sendAckNtfFalse(const Ptr<MsgRecvData> &msg)
{
    sendAckNtf(false);
}
void Drt::toApp()
{
    pkt::Packet pkt(sizeof(TestPackage));
    RT2packet(incre, pkt);
    DrtDataHeader *header=pkt.Header<DrtDataHeader>();
	//Trace::Instance().Log(NET_LAYER,NET_PID,full.getNodeID(),255,65535,255,"update","send");
    header->type=UPDATE;
    pkt.Move<DrtDataHeader>();

    Ptr<MsgSendDataReq> m(new MsgSendDataReq);
    m->packet=pkt;
	m->address=255;
    m->isData = false;
    SendReq(m, MAC_LAYER, MAC_PID);
    
    char onlineStatus[2][20];
    full.toArray(onlineStatus);
    cout<<(int)onlineStatus[0][0]<<" "<<(int)onlineStatus[0][1]<<" "<<" "<<(int)onlineStatus[0][2]<<endl;
    cout<<(int)onlineStatus[1][0]<<" "<<(int)onlineStatus[1][1]<<" "<<" "<<(int)onlineStatus[1][2]<<endl;
    int DestinationID = 0, SourceID = 0, input_length = 20;

    if (UIClient != NULL) {
        // struct App_DataPackage appBuff = {3, 24, 0, 0, 0, {"miss me?"}};
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

        //package_write(sendBuff,input_length, appBuff.Package_type, appBuff.DestinationID,  appBuff.SourceID);
        // write(UIClient->_socketfd, &sendBuff, sizeof(appBuff));  
        // LibevTool::Instance().Writen(pkt->fd, pkt->data, pkt->len);
    }
}
    MODULE_INIT(Drt)
    PROTOCOL_HEADER(DrtDataHeader)
#endif
}
