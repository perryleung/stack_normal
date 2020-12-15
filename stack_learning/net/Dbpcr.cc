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
#include <deque>
#include "time.h"
#include "routing_layer_info_record.h"

#define CURRENT_PID 7

extern client* UIClient;
extern client* TraceClient;

namespace dbpcr{
#if NET_PID == CURRENT_PID
//声明事件
using msg::MsgSendDataReq;
using msg::MsgSendDataRsp;
using msg::MsgRecvDataNtf;
using msg::MsgTimeOut;
using msg::MsgPosNtf;
//定义事件
struct EvRecvReq : hsm::Event<EvRecvReq>
{
    pkt::Packet packet;
};
struct EvRecvRsp : hsm::Event<EvRecvRsp>
{
	pkt::Packet packet;
};
struct EvRecvTgtData : hsm::Event<EvRecvTgtData>
{
    pkt::Packet packet;
};
struct EvRecvBdctData : hsm::Event<EvRecvBdctData>
{
    pkt::Packet packet;
};
//定义定时器
//时间到了，处理Request
struct EvHandleReqTimeout : hsm::Event<EvHandleReqTimeout>
{
};
//时间到了，发送Data
struct EvWaitRspTimeout : hsm::Event<EvWaitRspTimeout>
{
};
//时间到了，处理Data
struct EvHandleDataTimeout : hsm::Event<EvHandleDataTimeout>
{
};
//时间到了，GenerateData
struct EvGenerateDataTimeout : hsm::Event<EvGenerateDataTimeout>
{
};
//定义协议包头
struct DbpcrHeader
{
    uint8_t type;            //Request: 1; Response: 2; TgtData: 3; BdctData: 4
	uint16_t x;				 //三维坐标，实测时用于判断是否能收到
	uint16_t y;
	uint16_t z;
    uint8_t senderID;        //In Reqesut: senderID;       In Response: senderID;   In TgtData: senderID;   In BdctData: senderID
	uint32_t packetSeq;       //In Reqesut: packetSeq;      In Reqsponse: packetSeq; In TgtData: PacketSeq	In BdctData: PacketSeq
    uint16_t senderDepth;     //In Reqesut: senderDepth;    In Response: destID;     In TgtData: destID		In BdctData: senderDepth
    uint16_t depthThreshold;  //In Reqesut: depthThreshold; In Response: ReceiveSNR; In TgtData: meaningless	In BdctData: depthThreshold
};
//定义宏，包类型
const uint8_t REQUEST = 1;
const uint8_t RESPONSE = 2;
const uint8_t TGTDATA = 3;
const uint8_t BDCTDATA = 4;

const uint8_t BROADCAST = 255;

const uint8_t TimerForHandleReq = 1;
const uint8_t TimerForWaitRsp = 2;
const uint8_t TimerForHandleData = 3;
const uint8_t TimerForGenerateData = 4;

const double SOUND_SPEED = 1500;
//声明状态
struct Top;
struct Idle;
struct Busy;
//定义Request缓存队列
class ReqQueItem
{
	public:
		ReqQueItem(pkt::Packet p, double t, double snr) : m_p(p), m_sendTime(t), m_receiveSNR(snr) {}
		~ReqQueItem() {}
	public:
		pkt::Packet m_p;
		double m_sendTime;
		double m_receiveSNR;
};
class ReqQueue
{
	public:
		ReqQueue() {}
		~ReqQueue() {
			ReqQueItem* tmp;
			while(!m_queue.empty()) {
				tmp = m_queue.back();
				m_queue.pop_back();
				delete tmp;
			}
			m_queue.clear();
		}
		bool empty() {return m_queue.empty();}
		int size() {return m_queue.size();}

		void pop() {m_queue.pop_front();}
		ReqQueItem* front() {return m_queue.front();}
		int insert(ReqQueItem* item);
		int purge(uint32_t ps);
	private:
		std::deque<ReqQueItem*> m_queue;
};
int ReqQueue::insert(ReqQueItem* item) {
	ReqQueItem* tmp;
	std::deque<ReqQueItem*>::iterator iter;
	
	int i = 0;
	iter = m_queue.begin();
	while (iter != m_queue.end()) {
		tmp = *iter;
		if (tmp->m_sendTime > item->m_sendTime) {
			m_queue.insert(iter, item);
			return i;
		}
		iter++;
		i++;
	}
	m_queue.push_back(item);
	return i;
}
int ReqQueue::purge(uint32_t ps) {
	std::deque<ReqQueItem*>::iterator iter;
	
	int i = 0;
	iter = m_queue.begin();
	while (iter != m_queue.end()) {
		DbpcrHeader *header = ((*iter)->m_p).Header<DbpcrHeader>();
		if (header->packetSeq == ps) {
			m_queue.erase(iter);
			return i;
		}
		iter++;
		i++;
	}
	return -1;
}
//定义Data缓存队列
class DataQueItem
{
	public:
		DataQueItem(pkt::Packet p, double t) : m_p(p), m_sendTime(t) {}
		~DataQueItem() {}
	public:
		pkt::Packet m_p;
		double m_sendTime;
};
class DataQueue
{
	public:
		DataQueue() {}
		~DataQueue() {
			DataQueItem* tmp;
			while(!m_queue.empty()) {
				tmp = m_queue.back();
				m_queue.pop_back();
				delete tmp;
			}
			m_queue.clear();
		}
		bool empty() {return m_queue.empty();}
		int size() {return m_queue.size();}

		void pop() {m_queue.pop_front();}
		DataQueItem* front() {return m_queue.front();}
		int insert(DataQueItem* item);
		int purge(uint32_t ps);
	private:
		std::deque<DataQueItem*> m_queue;
};
int DataQueue::insert(DataQueItem* item) {
	DataQueItem* tmp;
	std::deque<DataQueItem*>::iterator iter;
	
	int i = 0;
	iter = m_queue.begin();
	while (iter != m_queue.end()) {
		tmp = *iter;
		if (tmp->m_sendTime > item->m_sendTime) {
			m_queue.insert(iter, item);
			return i;
		}
		iter++;
		i++;
	}
	m_queue.push_back(item);
	return i;
}
int DataQueue::purge(uint32_t ps) {
	std::deque<DataQueItem*>::iterator iter;
	
	int i = 0;
	iter = m_queue.begin();
	while (iter != m_queue.end()) {
		DbpcrHeader *header = ((*iter)->m_p).Header<DbpcrHeader>();
		if (header->packetSeq == ps) {
			m_queue.erase(iter);
			return i;
		}
		iter++;
		i++;
	}
	return -1;
}
//定义Dbpcr协议
class Dbpcr : public mod::Module<Dbpcr, NET_LAYER, CURRENT_PID>
{
        private:
                uint16_t m_isSource;
                uint16_t m_packetnum;
		uint16_t m_timeintervalMin;
		uint16_t m_timeintervalMax;
		void startGenerate();
	public:
                void generatePacket(const Ptr<EvGenerateDataTimeout> &msg);
	private:
		uint8_t m_id;
		uint16_t m_x;
		uint16_t m_y;
		uint16_t m_z;
		uint16_t m_depth;
		uint16_t m_depthThreshold;
                uint16_t m_transmitRange;
                double m_reqDelta;
                double m_dataDelta;
                double m_waitRspTime;
                
		uint32_t m_seq; // packetSeq: 0-7:nodeID; 8-32: seq
		ReqQueue m_reqQueue;
		DataQueue m_dataQueue;
		uint32_t m_currentHandleSeq;
		int m_handleReqTimerID;
		int m_handleDataTimerID;
		int m_waitRspTimerID;
	public:
		Dbpcr()
		{

            GetSap().SetOwner(this);
            GetHsm().Initialize<Top>(this);
            GetTap().SetOwner(this);
		}
		void Init()
		{
			RoutingLayerInfoRecord::Instance().Init();
                        m_isSource = Config::Instance()["routingLayerTest"]["isSource"].as<uint16_t>();
                        if (m_isSource != 0) {
                                m_packetnum = Config::Instance()["routingLayerTest"]["packetnum"].as<uint16_t>();
                                m_timeintervalMin = Config::Instance()["routingLayerTest"]["timeinterval"][0].as<uint16_t>();
        			m_timeintervalMax = Config::Instance()["routingLayerTest"]["timeinterval"][1].as<uint16_t>();
				startGenerate();
                        }
			m_id = (uint8_t)Config::Instance()["Dbpcr"]["id"].as<int>();
			m_x = (uint16_t)Config::Instance()["Dbpcr"]["x"].as<int>();
			m_y = (uint16_t)Config::Instance()["Dbpcr"]["y"].as<int>();
			m_z = (uint16_t)Config::Instance()["Dbpcr"]["z"].as<int>();
			m_depth = (uint16_t)Config::Instance()["Dbpcr"]["z"].as<int>();
			m_depthThreshold = (uint16_t)Config::Instance()["Dbpcr"]["depthThreshold"].as<int>();
                        m_transmitRange = (uint16_t)Config::Instance()["Dbpcr"]["transmitRange"].as<int>();
                        m_reqDelta = m_transmitRange * 1.0 / (uint16_t)Config::Instance()["Dbpcr"]["reqDeltaParam"].as<int>();
                        m_dataDelta = m_transmitRange * 1.0 / (uint16_t)Config::Instance()["Dbpcr"]["dataDeltaParam"].as<int>();
                        m_waitRspTime = 10 + (2 * m_transmitRange / SOUND_SPEED) * (m_transmitRange - m_depthThreshold) / m_reqDelta;
                        m_seq = 0;
			LOG(INFO) << "id: " << (int)m_id << ", depth: " << m_depth << ", depthThreshold: " << m_depthThreshold;
                        LOG(INFO) << "(x, y, z): (" << (int)m_x << ", " << (int)m_y << ", " << (int)m_z << ")" << ", transmitRange: " << m_transmitRange;
                        LOG(INFO) << "reqDelta: " << m_reqDelta << ", dataDelta: " << m_dataDelta << ", waitRspTime: " << m_waitRspTime;
			//设置三位坐标
		    msg::Position tempPos(m_x, m_y, m_z);
		    Ptr<MsgPosNtf> m(new MsgPosNtf);
		    m->ntfPos = tempPos;
		    SendNtf(m, PHY_LAYER, PHY_PID);
		}
		//global Function
		void parseNtf(const Ptr<MsgRecvDataNtf> &msg);
		void parseTimeout(const Ptr<MsgTimeOut> &msg);
		void handleReq(const Ptr<EvRecvReq> &msg);
		void handleReqTimeout(const Ptr<EvHandleReqTimeout> &msg);
		void sendReq(uint32_t ps);
		void sendRsp(uint8_t destID, uint32_t ps);
		void sendData(uint8_t destID, uint16_t receiveSNR);
		//Idle Function
		void handleUpperData1(const Ptr<MsgSendDataReq> &msg);
		void handleRsp1(const Ptr<EvRecvRsp> &msg);
		void handleTgtData1(const Ptr<EvRecvTgtData> &msg);
		bool isTarget1(const Ptr<EvRecvTgtData> &msg);
		void handleBdctData1(const Ptr<EvRecvBdctData> &msg);
		void handleDataTimeout1(const Ptr<EvHandleDataTimeout> &msg);
		//Busy Function
		void handleUpperData2(const Ptr<MsgSendDataReq> &msg);
		bool handleRsp2(const Ptr<EvRecvRsp> &msg);
		void doNothing2_1(const Ptr<EvRecvRsp> &msg);
		void handleTgtData2(const Ptr<EvRecvTgtData> &msg);
		void handleBdctData2(const Ptr<EvRecvBdctData> &msg);
		bool handleWaitRspTimeout2(const Ptr<EvWaitRspTimeout> &msg);
		void doNothing2_2(const Ptr<EvWaitRspTimeout> &msg);
};
//定义状态
struct Top : hsm::State<Top, hsm::StateMachine, Idle>
{
};
struct Idle : hsm::State<Idle, Top>
{
	typedef hsm_vector<MsgSendDataReq, MsgRecvDataNtf, EvRecvReq, EvRecvRsp, EvRecvTgtData, EvRecvBdctData, 
						MsgTimeOut, EvHandleReqTimeout, EvHandleDataTimeout, EvGenerateDataTimeout> reactions;

	HSM_TRANSIT(MsgSendDataReq, Busy, &Dbpcr::handleUpperData1);
	HSM_WORK(MsgRecvDataNtf, &Dbpcr::parseNtf);
	HSM_WORK(EvRecvReq, &Dbpcr::handleReq);
	HSM_WORK(EvRecvRsp, &Dbpcr::handleRsp1)
	HSM_TRANSIT(EvRecvTgtData, Busy, &Dbpcr::handleTgtData1, &Dbpcr::isTarget1);
	HSM_WORK(EvRecvBdctData, &Dbpcr::handleBdctData1);
	HSM_WORK(MsgTimeOut, &Dbpcr::parseTimeout);
	HSM_WORK(EvHandleReqTimeout, &Dbpcr::handleReqTimeout);
	HSM_TRANSIT(EvHandleDataTimeout, Busy, &Dbpcr::handleDataTimeout1);
	HSM_WORK(EvGenerateDataTimeout, &Dbpcr::generatePacket);
};
struct Busy : hsm::State<Busy, Top>
{
	typedef hsm_vector<MsgSendDataReq, MsgRecvDataNtf, EvRecvReq, EvRecvRsp, EvRecvTgtData, EvRecvBdctData, 
						MsgTimeOut, EvHandleReqTimeout, EvWaitRspTimeout, EvGenerateDataTimeout> reactions;

	HSM_WORK(MsgSendDataReq, &Dbpcr::handleUpperData2);
	HSM_WORK(MsgRecvDataNtf, &Dbpcr::parseNtf);
	HSM_WORK(EvRecvReq, &Dbpcr::handleReq);
	HSM_TRANSIT(EvRecvRsp, Idle, &Dbpcr::doNothing2_1, &Dbpcr::handleRsp2)
	HSM_WORK(EvRecvTgtData, &Dbpcr::handleTgtData2);
	HSM_WORK(EvRecvBdctData, &Dbpcr::handleBdctData2);
	HSM_WORK(MsgTimeOut, &Dbpcr::parseTimeout);
	HSM_WORK(EvHandleReqTimeout, &Dbpcr::handleReqTimeout);
	HSM_TRANSIT(EvWaitRspTimeout, Idle, &Dbpcr::doNothing2_2, &Dbpcr::handleWaitRspTimeout2);
	HSM_WORK(EvGenerateDataTimeout, &Dbpcr::generatePacket);
};
//定义通用函数
void Dbpcr::parseNtf(const Ptr<MsgRecvDataNtf> &msg) {
	LOG(INFO) << "parseNtf()..........";
	DbpcrHeader *header = msg->packet.Header<DbpcrHeader>();
	uint16_t x = header->x;
	uint16_t y = header->y;
	uint16_t z = header->z;
	double dist = std::sqrt((x - m_x) * (x - m_x) + (y - m_y) * (y - m_y) + (z - m_z) * (z - m_z));
	if (dist > m_transmitRange) {
		LOG(INFO) << "Not receive(based on position)!!!!!!!!!!";
		return;
	}
	if (header->type == REQUEST)
	{
        LOG(INFO) << "header->type == REQUEST";
        RoutingLayerInfoRecord::Instance().log(to_string(m_id) + string(":") + to_string(header->packetSeq) + string(":receive:request"));
		Ptr<EvRecvReq> m(new EvRecvReq);
		m->packet = msg->packet;
		GetHsm().ProcessEvent(m);
	} else if (header->type == RESPONSE) {
        LOG(INFO) << "header->type == RESPONSE";  
        RoutingLayerInfoRecord::Instance().log(to_string(m_id) + string(":") + to_string(header->packetSeq) + string(":receive:response"));
		Ptr<EvRecvRsp> m(new EvRecvRsp);
		m->packet = msg->packet;
		GetHsm().ProcessEvent(m);
	} else if (header->type == TGTDATA) {
        LOG(INFO) << "header->type == TGTDATA";  
        RoutingLayerInfoRecord::Instance().log(to_string(m_id) + string(":") + to_string(header->packetSeq) + string(":receive:tgtdata"));
		Ptr<EvRecvTgtData> m(new EvRecvTgtData);
		m->packet = msg->packet;
		GetHsm().ProcessEvent(m);
	} else if (header->type == BDCTDATA) {
        LOG(INFO) << "header->type == BDCTDATA";  
        RoutingLayerInfoRecord::Instance().log(to_string(m_id) + string(":") + to_string(header->packetSeq) + string(":receive:bdctdata"));
		Ptr<EvRecvBdctData> m(new EvRecvBdctData);
		m->packet = msg->packet;
		GetHsm().ProcessEvent(m);
	}
}
void Dbpcr::startGenerate() {
	LOG(INFO) << "startGenerate()..........";
	SetTimer(rand_num(m_timeintervalMin, m_timeintervalMax), TimerForGenerateData);
}
void Dbpcr::generatePacket(const Ptr<EvGenerateDataTimeout> &msg) {
        LOG(INFO) << "generatePacket()..........";
	pkt::Packet pkt(0);	
	Ptr<MsgSendDataReq> m(new MsgSendDataReq);
	m->packet = pkt;
	m->address = BROADCAST;	
	SendReq(m, NET_LAYER, NET_PID);
        m_packetnum--;
        if (m_packetnum > 0) {
            LOG(INFO) << "The remaining " << m_packetnum << " packets will be sent";
            SetTimer(rand_num(m_timeintervalMin, m_timeintervalMax), TimerForGenerateData);
        } else {
            LOG(INFO) << "Test end!!!!!!!!!!!!!!!!!!!!!!";
        }
}
void Dbpcr::parseTimeout(const Ptr<MsgTimeOut> &msg) {
	LOG(INFO) << "parseTimeout()..........";
    if (msg->msgId == TimerForHandleReq) {
        LOG(INFO) << "TimeForHandleReq";
        Ptr<EvHandleReqTimeout> m(new EvHandleReqTimeout);
        GetHsm().ProcessEvent(m);
    } else if (msg->msgId == TimerForWaitRsp) {
        LOG(INFO) << "TimerForWaitRsp";
        Ptr<EvWaitRspTimeout> m(new EvWaitRspTimeout);
        GetHsm().ProcessEvent(m);
    } else if (msg->msgId == TimerForHandleData) {
        LOG(INFO) << "TimerForHandleData";
        Ptr<EvHandleDataTimeout> m(new EvHandleDataTimeout);
        GetHsm().ProcessEvent(m);
    } else if (msg->msgId == TimerForGenerateData) {
        LOG(INFO) << "TimerForGenerateData";
        Ptr<EvGenerateDataTimeout> m(new EvGenerateDataTimeout);
        GetHsm().ProcessEvent(m);        
    }
}
void Dbpcr::handleReq(const Ptr<EvRecvReq> &msg) {
	LOG(INFO) << "handleReq()..........";
	DbpcrHeader *header = msg->packet.Header<DbpcrHeader>();
	//检查Data缓存队列
	int index = m_dataQueue.purge(header->packetSeq);
	LOG(INFO) << "m_dataQueue.purge index: " << index;
	if (index == 0) {
		CancelTimer(m_handleDataTimerID);
		if (m_dataQueue.size() > 0) {
			DataQueItem* item = m_dataQueue.front();
			time_t currentTime;	
			currentTime = time(NULL);
			m_handleDataTimerID = SetTimer(item->m_sendTime - (long)currentTime, TimerForHandleData);
		}
	} else if (index == -1) {
		uint16_t senderDepth = header->senderDepth;
		uint16_t depthThreshold = header->depthThreshold;
		LOG(INFO) << "Request's senderDepth: " << senderDepth << ", m_depth: " << m_depth;
		int depthDiff = senderDepth - m_depth;
		LOG(INFO) << "depthDiff: " << depthDiff;
		if (depthDiff > depthThreshold) {
			double waitTime = (2 * m_transmitRange / SOUND_SPEED) * (m_transmitRange - depthDiff) / m_reqDelta; //计算等待时间
			LOG(INFO) << "waitTime: " << waitTime;
			time_t currentTime;	
			currentTime = time(NULL);	
			double sendTime = waitTime + (long)currentTime;
			LOG(INFO) << "sendTime: " << sendTime;
			if (m_reqQueue.size() == 0) {
				m_handleReqTimerID = SetTimer(waitTime, TimerForHandleReq);			
				m_reqQueue.insert(new ReqQueItem(msg->packet, sendTime, 0));			
			} else {
				int index = m_reqQueue.insert(new ReqQueItem(msg->packet, sendTime, 0));
				if (index == 0) {
					CancelTimer(m_handleReqTimerID);
					m_handleReqTimerID = SetTimer(waitTime, TimerForHandleReq);
				}
			}		
		}
	}
}
void Dbpcr::handleReqTimeout(const Ptr<EvHandleReqTimeout> &msg) {
	LOG(INFO) << "handleReqTimeout()..........";
	ReqQueItem* item = m_reqQueue.front();
	m_reqQueue.pop();
	LOG(INFO) << "item->sendTime: " << item->m_sendTime;
	DbpcrHeader* header = (item->m_p).Header<DbpcrHeader>();
	uint8_t senderID = header->senderID;
	uint32_t ps = header->packetSeq;
	LOG(INFO) << "Request's senderID: " << (int)senderID << ", ps: " << ps;
	sendRsp(senderID, ps);
	if (m_reqQueue.size() > 0) {
		ReqQueItem* it = m_reqQueue.front();
		time_t currentTime;	
		currentTime = time(NULL);
		m_handleReqTimerID = SetTimer(it->m_sendTime - (long)currentTime, TimerForHandleReq);
	}
}
//定义Idle状态的函数
void Dbpcr::handleUpperData1(const Ptr<MsgSendDataReq> &msg) {
	LOG(INFO) << "handleUpperData1()..........";
	if (m_dataQueue.size() != 0) {
		CancelTimer(m_handleDataTimerID);	
	}
	uint32_t ps = (m_seq & 0x00ffffff) + (m_id << 24);
	LOG(INFO) << "Data's packetSeq: " << ps;
	m_seq++;
	DbpcrHeader *header = msg->packet.Header<DbpcrHeader>();
	header->senderID = m_id;
	header->packetSeq = ps;
	time_t currentTime;	
	currentTime = time(NULL);
	m_dataQueue.insert(new DataQueItem(msg->packet, (long)currentTime)); //将Data包放入队列
	//构造Request包
	pkt::Packet pkt(0);	
	DbpcrHeader *hdr = pkt.Header<DbpcrHeader>();
	hdr->type = REQUEST;
	hdr->x = m_x;
	hdr->y = m_y;
	hdr->z = m_z;
	hdr->senderID = m_id;
	hdr->packetSeq = ps;
	LOG(INFO) << "Request's senderID: " << (int)header->senderID << ", packetSeq: " << header->packetSeq;
	m_currentHandleSeq = ps;
	hdr->senderDepth = m_depth;
	hdr->depthThreshold = m_depthThreshold;
	pkt.Move<DbpcrHeader>();
	RoutingLayerInfoRecord::Instance().log(to_string(m_id) + string(":") + to_string(header->packetSeq) + string(":source:data"));
	//发送Request包
	Ptr<MsgSendDataReq> m(new MsgSendDataReq);
	m->packet = pkt;
	m->address = BROADCAST;
  RoutingLayerInfoRecord::Instance().log(to_string(m_id) + string(":") + to_string(header->packetSeq) + string(":send:request"));
	SendReq(m, MAC_LAYER, MAC_PID);
	LibevTool::Instance().WakeLoop();
	//设置等待Response的定时器
	m_waitRspTimerID = SetTimer(m_waitRspTime, TimerForWaitRsp);
	//回应上层Rsp
	Ptr<MsgSendDataRsp> rsp(new MsgSendDataRsp);
	SendRsp(rsp, TRA_LAYER, TRA_PID);
}
void Dbpcr::handleRsp1(const Ptr<EvRecvRsp> &msg) {
	LOG(INFO) << "handleRsp1()..........";
	DbpcrHeader *header = msg->packet.Header<DbpcrHeader>();
	uint32_t ps = header->packetSeq;
	int index = m_reqQueue.purge(ps);
	LOG(INFO) << "m_reqQueue.purge index: " << index;
	if (index != -1) {
		LOG(INFO) << "Cancel sending response";
                RoutingLayerInfoRecord::Instance().log(to_string(m_id) + string(":") + to_string(ps) + string(":cancel:response"));
	}
	if (index == 0) { // 如果移除了第一个，则取消定时器
		CancelTimer(m_handleReqTimerID);
		if (m_reqQueue.size() != 0) { // 如果队列还有其他元素，则重新设置定时器
			ReqQueItem* item = m_reqQueue.front();
			time_t currentTime;	
			currentTime = time(NULL);
			m_handleReqTimerID = SetTimer(item->m_sendTime - (long)currentTime, TimerForHandleReq);
		}
	}
}
bool Dbpcr::isTarget1(const Ptr<EvRecvTgtData> &msg) {
	LOG(INFO) << "isTarget1()..........";
	DbpcrHeader *header = msg->packet.Header<DbpcrHeader>();
	if (header->senderDepth == m_id) {
		LOG(INFO) << "isTarget";
		return true;
	}
	LOG(INFO) << "notTarget";
	return false;
} 
void Dbpcr::handleTgtData1(const Ptr<EvRecvTgtData> &msg) {
	LOG(INFO) << "handleTgtData1()..........";
	msg->packet.Move<DbpcrHeader>();//移动头指针并改变方向
	msg->packet.ChangeDir();
	DbpcrHeader *header = msg->packet.Header<DbpcrHeader>();
	//判断是否是水面节点
	if (m_depth == 0) {
		LOG(INFO) << "receive!!!!!!!!!!";
		RoutingLayerInfoRecord::Instance().log(to_string(m_id) + string(":") + to_string(header->packetSeq) + string(":destination:data"));
		return;
	}
	//若不是水面节点，则继续转发
	uint32_t ps = header->packetSeq;
	LOG(INFO) << "Target data's packetSeq: " << ps;
	time_t currentTime;	
	currentTime = time(NULL);
	m_dataQueue.insert(new DataQueItem(msg->packet, (long)currentTime));
	sendReq(ps);
	m_waitRspTimerID = SetTimer(m_waitRspTime, TimerForWaitRsp);
}
void Dbpcr::sendReq(uint32_t ps) {
	LOG(INFO) << "sendReq()..........";
	//构造Request包
	pkt::Packet pkt(0);	
	DbpcrHeader *header = pkt.Header<DbpcrHeader>();
	header->type = REQUEST;
	header->x = m_x;
	header->y = m_y;
	header->z = m_z;
	header->senderID = m_id;
	header->packetSeq = ps;
	LOG(INFO) << "Request's senderID: " << (int)header->senderID << ", packetSeq: " << header->packetSeq;
	m_currentHandleSeq = ps;
	header->senderDepth = m_depth;
	header->depthThreshold = m_depthThreshold;
	pkt.Move<DbpcrHeader>();
	//发送Request包
	Ptr<MsgSendDataReq> m(new MsgSendDataReq);
	m->packet = pkt;
	m->address = BROADCAST;
  RoutingLayerInfoRecord::Instance().log(to_string(m_id) + string(":") + to_string(header->packetSeq) + string(":send:request"));
	SendReq(m, MAC_LAYER, MAC_PID);
	LibevTool::Instance().WakeLoop();
}
void Dbpcr::handleBdctData1(const Ptr<EvRecvBdctData> &msg) {
	LOG(INFO) << "handleBdctData1()";
	msg->packet.Move<DbpcrHeader>();//移动头指针并改变方向
	msg->packet.ChangeDir();
	DbpcrHeader *header = msg->packet.Header<DbpcrHeader>();
	//判断是否是水面节点
	if (m_depth == 0) {
		LOG(INFO) << "receive!!!!!!!!!!";
		RoutingLayerInfoRecord::Instance().log(to_string(m_id) + string(":") + to_string(header->packetSeq) + string(":destination:data"));
		return;
	}
	//若不是水面节点，则继续转发
	uint16_t senderDepth = header->senderDepth;
	uint16_t depthThreshold = header->depthThreshold;
	int depthDiff = senderDepth - m_depth;
	LOG(INFO) << "depthDiff: " << depthDiff;
	if (depthDiff > depthThreshold) {
		double waitTime = (2 * m_transmitRange / SOUND_SPEED) * (m_transmitRange - depthDiff) / m_dataDelta; //计算等待时间
		LOG(INFO) << "waitTime: " << waitTime;	
		time_t currentTime;	
		currentTime = time(NULL);
		double sendTime = waitTime + (long)currentTime;
		LOG(INFO) << "handleTime: " << sendTime;
		if (m_dataQueue.size() == 0) {
			m_dataQueue.insert(new DataQueItem(msg->packet, sendTime));
			m_handleDataTimerID = SetTimer(waitTime, TimerForHandleData);
		} else {
			int index = m_dataQueue.insert(new DataQueItem(msg->packet, sendTime));
			if (index == 0) {
				CancelTimer(m_handleDataTimerID);
				m_handleDataTimerID = SetTimer(waitTime, TimerForHandleData);
			}
		}
	}
}
void Dbpcr::sendRsp(uint8_t destID, uint32_t ps) {
	LOG(INFO) << "sendRsp()..........";
	//构造Response包
	pkt::Packet pkt(0);	
	DbpcrHeader *header = pkt.Header<DbpcrHeader>();
	header->type = RESPONSE;
	header->x = m_x;
	header->y = m_y;
	header->z = m_z;
	header->senderID = m_id;
	header->packetSeq = ps;
	header->senderDepth = destID;
	LOG(INFO) << "Response's destID: " << header->senderDepth;
	pkt.Move<DbpcrHeader>();
	//发送Response包
	Ptr<MsgSendDataReq> m(new MsgSendDataReq);
	m->packet = pkt;
	m->address = BROADCAST;
  	RoutingLayerInfoRecord::Instance().log(to_string(m_id) + string(":") + to_string(header->packetSeq) + string(":send:response"));
	SendReq(m, MAC_LAYER, MAC_PID);
	LibevTool::Instance().WakeLoop();
}
void Dbpcr::handleDataTimeout1(const Ptr<EvHandleDataTimeout> &msg) {
	LOG(INFO) << "handleDataTimeout1()..........";
	DataQueItem* item = m_dataQueue.front();
	DbpcrHeader* header = (item->m_p).Header<DbpcrHeader>();
	uint32_t ps = header->packetSeq;
	sendReq(ps);
	m_waitRspTimerID = SetTimer(m_waitRspTime, TimerForWaitRsp);
}
//定义Busy状态的函数
void Dbpcr::handleUpperData2(const Ptr<MsgSendDataReq> &msg) {
	LOG(INFO) << "handleUpperData2()..........";
	uint32_t ps = (m_seq & 0x00ffffff) + (m_id << 24);
	LOG(INFO) << "Data's packetSeq: " << ps;
	m_seq++;
	DbpcrHeader *header = msg->packet.Header<DbpcrHeader>();
	header->senderID = m_id;
	header->packetSeq = ps;
	time_t currentTime;	
	currentTime = time(NULL);
	RoutingLayerInfoRecord::Instance().log(to_string(m_id) + string(":") + to_string(header->packetSeq) + string(":source:data"));
	m_dataQueue.insert(new DataQueItem(msg->packet, (long)currentTime));
	//回应上层Rsp
	Ptr<MsgSendDataRsp> rsp(new MsgSendDataRsp);
	SendRsp(rsp, TRA_LAYER, TRA_PID);
}
bool Dbpcr::handleRsp2(const Ptr<EvRecvRsp> &msg) {
	LOG(INFO) << "handleRsp2()..........";
	DbpcrHeader *header = msg->packet.Header<DbpcrHeader>();
	uint32_t ps = header->packetSeq;
	LOG(INFO) << "Response's packetSeq: " << ps;
	int index = m_reqQueue.purge(ps);
	LOG(INFO) << "m_reqQueue.purge index: " << index;
	if (index == 0) {
                RoutingLayerInfoRecord::Instance().log(to_string(m_id) + string(":") + to_string(ps) + string(":cancel:response"));
		CancelTimer(m_handleReqTimerID);
		if (m_reqQueue.size() > 0) {
			ReqQueItem* item = m_reqQueue.front();
			time_t currentTime;	
			currentTime = time(NULL);
			m_handleReqTimerID = SetTimer(item->m_sendTime - (long)currentTime, TimerForHandleReq);
		}
		return false;
	} else if (index == -1) {
		uint16_t destID = header->senderDepth;
		LOG(INFO) << "Response's senderID: " << (int)header->senderID;
		LOG(INFO) << "Response's destID: " << destID;
		if (destID == m_id && ps == m_currentHandleSeq) {
			CancelTimer(m_waitRspTimerID);
			uint8_t senderID = header->senderID;			
			uint16_t receiveSNR = header->depthThreshold;
			sendData(senderID, receiveSNR);
			if (m_dataQueue.size() > 0) {
				DataQueItem* item = m_dataQueue.front();
				time_t currentTime;	
				currentTime = time(NULL);
				if (item->m_sendTime <= (long)currentTime) {
					DbpcrHeader* hdr = (item->m_p).Header<DbpcrHeader>();
					uint32_t ps = hdr->packetSeq;					
					sendReq(ps);
					m_currentHandleSeq = ps;
					return false;
				} else {
					time_t currentTime;	
					currentTime = time(NULL);
					m_handleDataTimerID = SetTimer(item->m_sendTime - (long)currentTime, TimerForHandleData);
					return true;
				}
			} else {
				return true;
			}
		}
	} else {
                RoutingLayerInfoRecord::Instance().log(to_string(m_id) + string(":") + to_string(ps) + string(":cancel:response"));
		return false;
	}
}
void Dbpcr::doNothing2_1(const Ptr<EvRecvRsp> &msg) {
	LOG(INFO) << "doNothing2_1()..........";
	//do nothing
}
void Dbpcr::sendData(uint8_t destID, uint16_t receiveSNR) {
	LOG(INFO) << "sendData()...........";
	DataQueItem* item = m_dataQueue.front();
	m_dataQueue.pop();
	//重设包头参数
	pkt::Packet pkt = item->m_p;
	DbpcrHeader *header = pkt.Header<DbpcrHeader>();
	header->type = TGTDATA;
	header->x = m_x;
	header->y = m_y;
	header->z = m_z;
	header->senderID = m_id;
	header->senderDepth = destID;
	LOG(INFO) << "Data's packetSeq: " << header->packetSeq;
	pkt.Move<DbpcrHeader>();
	//发送Data
	Ptr<MsgSendDataReq> m(new MsgSendDataReq);
	m->packet = pkt;
	m->address = BROADCAST;
  RoutingLayerInfoRecord::Instance().log(to_string(m_id) + string(":") + to_string(header->packetSeq) + string(":send:tgtdata"));
	SendReq(m, MAC_LAYER, MAC_PID);
	LibevTool::Instance().WakeLoop();
}
void Dbpcr::handleTgtData2(const Ptr<EvRecvTgtData> &msg) {
	LOG(INFO) << "handleTgtData2()..........";
	msg->packet.Move<DbpcrHeader>();//移动头指针并改变方向
	msg->packet.ChangeDir();
	DbpcrHeader *header = msg->packet.Header<DbpcrHeader>();
	//判断是否是水面节点
	if (m_depth == 0) {
		LOG(INFO) << "receive!!!!!!!!!!";
		RoutingLayerInfoRecord::Instance().log(to_string(m_id) + string(":") + to_string(header->packetSeq) + string(":destination:data"));
		return;
	}
	//若不是水面节点，则继续转发
	uint16_t destID = header->senderDepth;
	if (destID == m_id) {
		time_t currentTime;	
		currentTime = time(NULL);
		m_dataQueue.insert(new DataQueItem(msg->packet, (long)currentTime));
	}
}
void Dbpcr::handleBdctData2(const Ptr<EvRecvBdctData> &msg) {
	LOG(INFO) << "handleBdctData2()..........";
	msg->packet.Move<DbpcrHeader>();//移动头指针并改变方向
	msg->packet.ChangeDir();
	DbpcrHeader *header = msg->packet.Header<DbpcrHeader>();
	//判断是否是水面节点
	if (m_depth == 0) {
		LOG(INFO) << "receive!!!!!!!!!!";
		RoutingLayerInfoRecord::Instance().log(to_string(m_id) + string(":") + to_string(header->packetSeq) + string(":destination:data"));
		return;
	}
	//若不是水面节点，则继续转发
	uint16_t senderDepth = header->senderDepth;
	uint16_t depthThreshold = header->depthThreshold;
	int depthDiff = senderDepth - m_depth;
	LOG(INFO) << "depthDiff: " << depthDiff;
	if (depthDiff > depthThreshold) {
		double waitTime = (2 * m_transmitRange / SOUND_SPEED) * (m_transmitRange - depthDiff) / m_dataDelta; //计算等待时间
		LOG(INFO) << "waitTime: " << waitTime;
		time_t currentTime;	
		currentTime = time(NULL);
		double sendTime = waitTime + (long)currentTime;
		LOG(INFO) << "handleTime: " << sendTime;
		m_dataQueue.insert(new DataQueItem(msg->packet, sendTime));
	}
}
bool Dbpcr::handleWaitRspTimeout2(const Ptr<EvWaitRspTimeout> &msg) {
	LOG(INFO) << "handleWaitRspTimeout2()..........";
	DataQueItem* item = m_dataQueue.front();
	m_dataQueue.pop();
	//重设包头参数
	pkt::Packet pkt = item->m_p;
	DbpcrHeader *header = pkt.Header<DbpcrHeader>();
	header->type = BDCTDATA;
	header->x = m_x;
	header->y = m_y;
	header->z = m_z;
	header->senderID = m_id;
	header->senderDepth = m_depth;
	header->depthThreshold = m_depthThreshold;
	pkt.Move<DbpcrHeader>();
	//发送Data
	Ptr<MsgSendDataReq> m(new MsgSendDataReq);
	m->packet = pkt;
  RoutingLayerInfoRecord::Instance().log(to_string(m_id) + string(":") + to_string(header->packetSeq) + string(":send:bdctdata"));
	m->address = BROADCAST;
	SendReq(m, MAC_LAYER, MAC_PID);
	LibevTool::Instance().WakeLoop();	
	if (m_dataQueue.size() > 0) {
		DataQueItem* item = m_dataQueue.front();
		time_t currentTime;	
		currentTime = time(NULL);
		if (item->m_sendTime <= (long)currentTime) {
			DbpcrHeader* hdr = item->m_p.Header<DbpcrHeader>();
			uint32_t ps = hdr->packetSeq;					
			sendReq(ps);
			m_currentHandleSeq = ps;
			return false;
		} else {
			time_t currentTime;	
			currentTime = time(NULL);
			m_handleDataTimerID = SetTimer(item->m_sendTime - (long)currentTime, TimerForHandleData);
			return true;
		}
	} else {
		return true;
	}
}
void Dbpcr::doNothing2_2(const Ptr<EvWaitRspTimeout> &msg) {
	LOG(INFO) << "doNothing2_2()..........";
	//do nothing
}
MODULE_INIT(Dbpcr)
PROTOCOL_HEADER(DbpcrHeader)
#endif
}
