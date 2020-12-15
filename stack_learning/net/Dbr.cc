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
#include <set>
#include "time.h"
#include "routing_layer_info_record.h"

#define CURRENT_PID 9

extern client* UIClient;
extern client* TraceClient;

using std::string;

namespace dbr{
#if NET_PID == CURRENT_PID

//声明事件
using msg::MsgSendDataReq;
using msg::MsgSendDataRsp;
using msg::MsgRecvDataNtf;
using msg::MsgTimeOut;
using msg::MsgPosNtf;
//定义时间
//时间到了，处理Data
struct EvHandleDataTimeout : hsm::Event<EvHandleDataTimeout>
{
};
//时间到了，GenerateData
struct EvGenerateDataTimeout : hsm::Event<EvGenerateDataTimeout>
{
};
//定义协议包头
struct DbrHeader
{
    uint16_t x;				 //三维坐标，实测时用于判断是否能收到
    uint16_t y;
    uint16_t z;
    uint16_t senderId;   
    uint32_t packetSeq;       
    uint16_t senderDepth;     
    uint16_t depthThreshold;  
};
//广播地址
const uint8_t BROADCAST = 255;
//声音在水下的传播速度
const double SOUND_SPEED = 1500;
//定义定时器类型
const uint8_t TimerForHandleData = 1;
const uint8_t TimerForGenerateData = 2;
//声明状态
struct Top;
struct Idle;
//定义Data队列
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
		DbrHeader *header = ((*iter)->m_p).Header<DbrHeader>();
		if (header->packetSeq == ps) {
			m_queue.erase(iter);
			return i;
		}
		iter++;
		i++;
	}
	return -1;
}
//定义Dbr协议
class Dbr : public mod::Module<Dbr, NET_LAYER, CURRENT_PID>
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
                double m_dataDelta;
		uint32_t m_seq;
		DataQueue m_dataQueue;
                set<uint32_t> m_haveSentData;
		int m_handleDataTimerId;
	public:
		Dbr()
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
			m_id = (uint8_t)Config::Instance()["Dbr"]["id"].as<int>();
			m_x = (uint16_t)Config::Instance()["Dbr"]["x"].as<int>();
			m_y = (uint16_t)Config::Instance()["Dbr"]["y"].as<int>();
			m_z = (uint16_t)Config::Instance()["Dbr"]["z"].as<int>();
			m_depth = (uint16_t)Config::Instance()["Dbr"]["z"].as<int>();
			m_depthThreshold = (uint16_t)Config::Instance()["Dbr"]["depthThreshold"].as<int>();
                        m_transmitRange = (uint16_t)Config::Instance()["Dbr"]["transmitRange"].as<int>();
                        m_dataDelta = m_transmitRange * 1.0 / (uint16_t)Config::Instance()["Dbr"]["dataDeltaParam"].as<int>();
                        m_seq = 0;
			LOG(INFO) << "id: " << (int)m_id << ", depth: " << m_depth << ", depthThreshold: " << m_depthThreshold;
                        LOG(INFO) << "(x, y, z): (" << (int)m_x << ", " << (int)m_y << ", " << (int)m_z << ")" << ", transmitRange: " << m_transmitRange;
                        LOG(INFO) << "dataDelta: " << m_dataDelta;
			//设置三位坐标
		        msg::Position tempPos(m_x, m_y, m_z);
		        Ptr<MsgPosNtf> m(new MsgPosNtf);
		        m->ntfPos = tempPos;
		        SendNtf(m, PHY_LAYER, PHY_PID);
		}
		void handleUpperData(const Ptr<MsgSendDataReq> &msg);
		void handleReceivedData(const Ptr<MsgRecvDataNtf> &msg);
                void parseTimeout(const Ptr<MsgTimeOut> &msg);
		void handleDataTimeout(const Ptr<EvHandleDataTimeout> &msg);
};
//定义状态
struct Top : hsm::State<Top, hsm::StateMachine, Idle>
{
};
struct Idle : hsm::State<Idle, Top>
{
	typedef hsm_vector<MsgSendDataReq, MsgRecvDataNtf, MsgTimeOut, EvHandleDataTimeout, EvGenerateDataTimeout> reactions;

	HSM_WORK(MsgSendDataReq, &Dbr::handleUpperData);
	HSM_WORK(MsgRecvDataNtf, &Dbr::handleReceivedData);
	HSM_WORK(MsgTimeOut, &Dbr::parseTimeout);
        HSM_WORK(EvHandleDataTimeout, &Dbr::handleDataTimeout)
        HSM_WORK(EvGenerateDataTimeout, &Dbr::generatePacket);
};
//定义函数
void Dbr::startGenerate() {
        LOG(INFO) << "startGenerate()..........";
        SetTimer(rand_num(m_timeintervalMin, m_timeintervalMax), TimerForGenerateData);
}
void Dbr::generatePacket(const Ptr<EvGenerateDataTimeout> &msg) {
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
void Dbr::parseTimeout(const Ptr<MsgTimeOut> &msg) {
	LOG(INFO) << "parseTimeout()..........";
        if (msg->msgId == TimerForHandleData) {
            LOG(INFO) << "TimerForHandleData";
            Ptr<EvHandleDataTimeout> m(new EvHandleDataTimeout);
            GetHsm().ProcessEvent(m);
        } else if (msg->msgId == TimerForGenerateData) {
            LOG(INFO) << "TimerForGenerateData";
            Ptr<EvGenerateDataTimeout> m(new EvGenerateDataTimeout);
            GetHsm().ProcessEvent(m);        
        }
}
void Dbr::handleUpperData(const Ptr<MsgSendDataReq> &msg) {
	LOG(INFO) << "handleUpperData()..........";

        DbrHeader *header = msg->packet.Header<DbrHeader>();
        uint32_t ps = (m_seq & 0x00ffffff) + (m_id << 24);
        m_seq++;
        RoutingLayerInfoRecord::Instance().log(to_string(m_id) + string(":") + to_string(ps) + string(":source:data"));
        //处理数据包头部
        header->x = m_x;
        header->y = m_y;
        header->z = m_z;
	header->senderId = m_id;
	header->packetSeq = ps;
        header->senderDepth = m_depth;
        header->depthThreshold = m_depthThreshold;
        //发送数据包
        msg->packet.Move<DbrHeader>();
        msg->address = BROADCAST;
        SendReq(msg, MAC_LAYER, MAC_PID);
        RoutingLayerInfoRecord::Instance().log(to_string(m_id) + string(":") + to_string(header->packetSeq) + string(":send:data"));
        LibevTool::Instance().WakeLoop();
        //发送之后将包的packetSeq存起来
        m_haveSentData.insert(ps);
	//回应上层Rsp
	Ptr<MsgSendDataRsp> rsp(new MsgSendDataRsp);
	SendRsp(rsp, TRA_LAYER, TRA_PID);
}
void Dbr::handleReceivedData(const Ptr<MsgRecvDataNtf> &msg) {
        LOG(INFO) << "handleReceivedData()..........";
        //判断逻辑距离是否在通信范围内
        DbrHeader *header = msg->packet.Header<DbrHeader>();
        uint16_t x = header->x;
	uint16_t y = header->y;
	uint16_t z = header->z;
        double dist = std::sqrt((x - m_x) * (x - m_x) + (y - m_y) * (y - m_y) + (z - m_z) * (z - m_z));
        if (dist > m_transmitRange) {
	        LOG(INFO) << "Not receive(based on position)!!!!!!!!!!";
	        return;
	}
        LOG(INFO) << "to_string(header->senderId) + to_string(header->packetSeq): " << to_string(header->senderId) + to_string(header->packetSeq);
        RoutingLayerInfoRecord::Instance().log(to_string(m_id) + string(":") + to_string(header->packetSeq) + string(":receive:data"));
        //判断是否到达sink
        if (m_depth == 0) {
                LOG(INFO) << "receive!!!!!!!!!!!!!!!!";
                RoutingLayerInfoRecord::Instance().log(to_string(m_id) + string(":") + to_string(header->packetSeq) + string(":destination:data"));
                return;
        }
        //判断是否发送过该数据包
        uint16_t senderId = header->senderId;
        uint32_t packetSeq = header->packetSeq;
        set<uint32_t>::iterator iter;
        iter = m_haveSentData.find(packetSeq);
        if (iter != m_haveSentData.end()) {
                return;
        }
        //处理数据包
        uint16_t senderDepth = header->senderDepth;
        uint16_t depthThreshold = header->depthThreshold;
        int depthDiff = senderDepth - m_depth;
        LOG(INFO) << "depthDiff: " << depthDiff;
        if (depthDiff > depthThreshold) {
                double waitTime = (2 * m_transmitRange / SOUND_SPEED) * (m_transmitRange - depthDiff) / m_dataDelta;
                LOG(INFO) << "waitTime: " << waitTime;
                time_t currentTime;	
                currentTime = time(NULL);	
                double sendTime = waitTime + (long)currentTime;
                if (m_dataQueue.size() == 0) {
                        m_handleDataTimerId = SetTimer(waitTime, TimerForHandleData);		
                        m_dataQueue.insert(new DataQueItem(msg->packet, sendTime));			
                } else {
                        int index = m_dataQueue.insert(new DataQueItem(msg->packet, sendTime));
                        if (index == 0) {
                                CancelTimer(m_handleDataTimerId);
                                m_handleDataTimerId = SetTimer(waitTime, TimerForHandleData);
                        }
                }
        } else {
                int index = m_dataQueue.purge(packetSeq);
                if (index != -1) {
                      RoutingLayerInfoRecord::Instance().log(to_string(m_id) + string(":") + to_string(packetSeq) + string(":cancel:data"));  
                }
                if (index == 0) {
                        CancelTimer(m_handleDataTimerId);
                        if (m_dataQueue.size() > 0) {
                                DataQueItem* it = m_dataQueue.front();
                                time_t currentTime;	
                                currentTime = time(NULL);
                                m_handleDataTimerId = SetTimer(it->m_sendTime - (long)currentTime, TimerForHandleData);
                        }         
                }
        }
}
void Dbr::handleDataTimeout(const Ptr<EvHandleDataTimeout> &msg) {
        LOG(INFO) << "handleDataTimeout()..........";
        //取出数据包发送
        DataQueItem* item = m_dataQueue.front();
        m_dataQueue.pop();
        DbrHeader* header = (item->m_p).Header<DbrHeader>();
        header->x = m_x;
        header->y = m_y;
        header->z = m_z;
        uint16_t senderId = header->senderId;
        uint32_t packetSeq = header->packetSeq;
        header->senderDepth = m_depth;
        (item->m_p).ChangeDir();
        Ptr<MsgSendDataReq> req(new MsgSendDataReq);
	req->packet = item->m_p;
	req->address = BROADCAST;
        RoutingLayerInfoRecord::Instance().log(to_string(m_id) + string(":") + to_string(header->packetSeq) + string(":send:data"));
        SendReq(req, MAC_LAYER, MAC_PID);
        LibevTool::Instance().WakeLoop();
        //发送之后将包的packetSeq存起来
        m_haveSentData.insert(packetSeq);
        //设置下一个定时器
        if (m_dataQueue.size() > 0) {
                DataQueItem* it = m_dataQueue.front();
                time_t currentTime;	
                currentTime = time(NULL);
                m_handleDataTimerId = SetTimer(it->m_sendTime - (long)currentTime, TimerForHandleData);
	}    
}
MODULE_INIT(Dbr)
PROTOCOL_HEADER(DbrHeader)
#endif
}
