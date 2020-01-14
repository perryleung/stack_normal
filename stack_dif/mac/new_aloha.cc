#include <string>
#include "hsm.h"
#include "message.h"
#include "module.h"
#include "sap.h"
#include "schedule.h"
#include "tools.h"
#include "trace.h"
#include <map> 
#include <list>
#include <time.h> 
#include "client.h"
#include "app_datapackage.h"

#define CURRENT_PROTOCOL 5

#define MAX_TIMEOUT_COUNT 3
#define BROADCAST_ADDERSS 255
#define PACKET_TYPE_DATA 0
#define PACKET_TYPE_ACK 1

extern client* TraceClient;

namespace new_aloha{

#if MAC_PID == CURRENT_PROTOCOL
using msg::MsgSendDataReq;
using msg::MsgSendDataRsp;
using msg::MsgRecvDataNtf;
using msg::MsgTimeOut;
using pkt::Packet;


struct Top;
struct Idle;
struct WaitRsp;

#pragma pack(1)
struct NewAlohaHeader{
	uint16_t serialNum;
	uint8_t type;
	uint8_t destID;
	uint8_t sourID;
//	uint16_t serialNum;
};
#pragma pack;

struct PackageInfo{
	Packet pkt;
	int reSendTime;
	int sourID;
	int destID;
};
struct Info{
	uint16_t serialNum;
	uint8_t sourID;
};

class NewAloha : public mod::Module<NewAloha, MAC_LAYER, CURRENT_PROTOCOL>{
	public:
		NewAloha(){MODULE_CONSTRUCT(Top);}
		void Init(){
			LOG(INFO)<<"NewAloha init";
			selfMacId = uint8_t(Config::Instance()["newAloha"]["MacId"].as<int>());
			minReSendTime = Config::Instance()["newAloha"]["MinReSendTime"].as<int>();
			reSendTimeRange = Config::Instance()["newAloha"]["ReSendTimeRange"].as<int>();
			isTestMode_ = Config::Instance()["test"]["testmode"].as<int16_t>();
                        cout << "the size of new_aloha is :" << sizeof(NewAlohaHeader)<<endl;

		}
		void SendUp(const Ptr<MsgRecvDataNtf> &);
		void SendDown(const Ptr<MsgSendDataReq> &);
		void ReSendDown(const Ptr<MsgTimeOut> &);
	private:
		//packerGroup用于保存发出去的数据包，key为包序号，value为整个数据包加上剩余重发次数
		uint8_t selfMacId;
		map<uint16_t, PackageInfo> packetGroup;
		list<Info> recvPacketTemp;
		uint16_t serialNum_ = 0;
		int minReSendTime;
		int reSendTimeRange;
		int discardNum = 0;
		uint16_t isTestMode_;
		
};

struct Top : hsm::State<Top, hsm::StateMachine, Idle>{
	typedef hsm_vector<MsgRecvDataNtf, MsgTimeOut> reactions;
	HSM_WORK(MsgRecvDataNtf, &NewAloha::SendUp);
	HSM_WORK(MsgTimeOut, &NewAloha::ReSendDown);
};
struct Idle : hsm::State<Idle, Top>{
	typedef hsm_vector<MsgSendDataReq> reactions;
	HSM_TRANSIT(MsgSendDataReq, WaitRsp, &NewAloha::SendDown);
};
struct WaitRsp : hsm::State<WaitRsp, Top>{
	typedef hsm_vector<MsgSendDataRsp, MsgSendDataReq> reactions;
	HSM_TRANSIT(MsgSendDataRsp, Idle);
	HSM_DEFER(MsgSendDataReq);
};

void NewAloha::SendUp(const Ptr<MsgRecvDataNtf> &m){

	NewAlohaHeader *header = m->packet.Header<NewAlohaHeader>();
    uint16_t saveSerialNum = header->serialNum;
	LOG(INFO)<<"new aloha recv something for MacId : "<<(int)header->destID;
	if(header->type == PACKET_TYPE_DATA){ 
		if (header->destID == selfMacId){

			//如果是数据类型，并且目的地址是自己，就把包上发并发出ack
			while(recvPacketTemp.size() > 100){//如果保存的包信息大于20个，则删除最先收到的包信息
				recvPacketTemp.pop_front();
			}
			bool isResendPacket = false;
			list<Info>::iterator itor;
			for(itor = recvPacketTemp.begin(); itor != recvPacketTemp.end(); itor++){
				if((*itor).serialNum == header->serialNum && (*itor).sourID == header->sourID){//如果收到的包信息已经在保存的包信息里，说明这是一个重传的包
					isResendPacket = true;
					LOG(INFO)<<"recv a resend packet,discard it---------------------";
					break;
				}
			}
			recvPacketTemp.push_back({header->serialNum, header->sourID});

			string logStr1 = Trace::Instance().Log(MAC_LAYER, MAC_PID, m->packet, -1, -1, -1, -1, -1, (int)header->sourID, (int)header->destID, (int)header->serialNum, "data","recv");
			if (TraceClient != NULL)
			{
				IODataPtr pkt(new IOData);

				pkt->fd = TraceClient->_socketfd;

				char* sendBuff = new char[logStr1.length()];
				memcpy(sendBuff, logStr1.data(), logStr1.length());
				pkt->data = sendBuff;
				pkt->len = logStr1.length();
				TraceWriteQueue::Push(pkt);
				cliwrite(TraceClient);
			}
			if(!isResendPacket){
				m->packet.Move<NewAlohaHeader>();
				SendNtf(m, NET_LAYER, NET_PID);
				LOG(INFO)<<"recv a data packet and send up";
			}
				Packet pkt(0);
				NewAlohaHeader *h = pkt.Header<NewAlohaHeader>();
				h->type = PACKET_TYPE_ACK;
				h->destID = header->sourID;
				h->sourID = selfMacId;
		        /*struct TestPackage temp;
		        struct App_DataPackage temp1;
				if(isTestMode_){
					memcpy(&temp, m->packet.realRaw(), sizeof(TestPackage));
					h->serialNum = temp.SerialNum;
		            cout<<"is testmode"<<endl;
		            cout<<"raw serialNum : "<<(int)temp.SerialNum;
				}else{
					memcpy(&temp1, m->packet.realRaw(), sizeof(App_DataPackage));
					h->serialNum = temp1.SerialNum;
		            cout<<"is not testmode"<<endl;
		            cout<<"raw serialNum : "<<(int)temp1.SerialNum;
				}*/
		        h->serialNum = saveSerialNum;
		        cout<<"ack SerialNum : "<<(int)h->serialNum<<endl;
				string logStr2 = Trace::Instance().Log(MAC_LAYER, MAC_PID, -1, -1, -1, -1, -1, -1, -1, (int)h->sourID, (int)h->destID, (int)h->serialNum, "ack", "send");
				if (TraceClient != NULL)
				{
					IODataPtr pkt(new IOData);

					pkt->fd = TraceClient->_socketfd;

					char* sendBuff = new char[logStr2.length()];
					memcpy(sendBuff, logStr2.data(), logStr2.length());
					pkt->data = sendBuff;
					pkt->len = logStr2.length();
					TraceWriteQueue::Push(pkt);
					cliwrite(TraceClient);
				}			
				pkt.Move<NewAlohaHeader>();
			
				Ptr<MsgSendDataReq> req(new MsgSendDataReq);
				req->packet = pkt;
				SendReq(req, PHY_LAYER, PHY_PID);
				LOG(INFO)<<"send ack";

		} else if (header->destID == BROADCAST_ADDERSS){
         	//如果是广播包则发到上层，不需要回复ack       
			string logStr3 = Trace::Instance().Log(MAC_LAYER, MAC_PID, -1, -1, -1, -1, -1, -1, -1, (int)header->sourID, (int)header->destID, -1, "broadcast","recv");
			if (TraceClient != NULL)
			{
				IODataPtr pkt(new IOData);

				pkt->fd = TraceClient->_socketfd;

				char* sendBuff = new char[logStr3.length()];
				memcpy(sendBuff, logStr3.data(), logStr3.length());
				pkt->data = sendBuff;
				pkt->len = logStr3.length();
				TraceWriteQueue::Push(pkt);
				cliwrite(TraceClient);
			}			
			m->packet.Move<NewAlohaHeader>();
			SendNtf(m, NET_LAYER, NET_PID);
			LOG(INFO)<<"recv a broadcast packet and send up";
			} 
		} else {
			LOG(INFO)<<"recv a ack type packet";
		//如果是ack类型，在packageGroup中寻找序号与目的id都相符合项，将其删掉
		map<uint16_t, PackageInfo>::iterator iter;
		for(iter = packetGroup.begin(); iter!= packetGroup.end(); iter++){
			if(iter->first == header->serialNum 
                        && iter->second.destID == header->sourID 
                        && iter->second.sourID == header->destID){
                string logStr4 = Trace::Instance().Log(MAC_LAYER, MAC_PID, -1, -1, -1, -1, -1, -1, -1, (int)header->sourID, (int)header->destID, (int)header->serialNum, "ack","recv");
				if (TraceClient != NULL)
				{
					IODataPtr pkt(new IOData);

					pkt->fd = TraceClient->_socketfd;

					char* sendBuff = new char[logStr4.length()];
					memcpy(sendBuff, logStr4.data(), logStr4.length());
					pkt->data = sendBuff;
					pkt->len = logStr4.length();
					TraceWriteQueue::Push(pkt);
					cliwrite(TraceClient);
				}
                packetGroup.erase(iter);
                LOG(INFO)<<"recv a ack and delete the packet in map>>>>>>>>>>>>>>>>>>>>>>>>>>";
                break;
			} 

        }

	}
}

void NewAloha::SendDown(const Ptr<MsgSendDataReq> &m){
	
	NewAlohaHeader *header = m->packet.Header<NewAlohaHeader>();
    header->serialNum = serialNum_;
    cout<<"send down serialNum : "<<(int)header->serialNum<<endl;
	header->type = PACKET_TYPE_DATA;
	header->destID = (uint8_t)m->address;
	header->sourID = selfMacId;
	serialNum_++;
	//将数据包和序号作为一个项目插入packetGroup
        //如果是广播包，则不需要设置定时器
        if(m->address != 255){
			string logStr = Trace::Instance().Log(MAC_LAYER, MAC_PID, m->packet, -1, -1, -1, -1, -1, (int)header->sourID, (int)header->destID, (int)header->serialNum, "data", "send");
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
			PackageInfo backUp = {m->packet, MAX_TIMEOUT_COUNT, (int)header->sourID, (int)header->destID};
			packetGroup[header->serialNum] = backUp;

			m->packet.Move<NewAlohaHeader>();
			Ptr<MsgSendDataReq> req(new MsgSendDataReq);
			req->packet = m->packet;
			SendReq(req, PHY_LAYER, PHY_PID);

                srand((unsigned)time(NULL));
                int times=rand()%reSendTimeRange + minReSendTime;
                LOG(INFO)<<"packet No"<<(int)header->serialNum<<" resend in "<<times<<" s ";
                SetTimer(times, header->serialNum);
                LOG(INFO)<<"send down data packet to MacId: "<<(int)m->address<<" and settimer";
        }else{
			header->destID = BROADCAST_ADDERSS; 
			string logStr = Trace::Instance().Log(MAC_LAYER, MAC_PID, -1, -1, -1, -1, -1, -1, -1, (int)header->sourID, (int)header->destID, -1, "broadcast", "send");
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
			m->packet.Move<NewAlohaHeader>();
			Ptr<MsgSendDataReq> req(new MsgSendDataReq);
			req->packet = m->packet;
			SendReq(req, PHY_LAYER, PHY_PID);

			LOG(INFO)<<"send down broadcast packet";
        }
	Ptr<MsgSendDataRsp> rsp(new MsgSendDataRsp);
	SendRsp(rsp, NET_LAYER, NET_PID);
	LOG(INFO)<<"send rsp to NET_LAYER";
}

void NewAloha::ReSendDown(const Ptr<MsgTimeOut> &m){
	//收到超时事件后，在packetGroup将相应id的包发出，并把超时次数减 1
	if(packetGroup.count(m->msgId)){
		string logStr = Trace::Instance().Log(MAC_LAYER, MAC_PID, packetGroup[m->msgId].pkt, -1, -1, -1, -1, -1, (int)packetGroup[m->msgId].sourID, packetGroup[m->msgId].destID, m->msgId, "data", "resend");
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
		NewAlohaHeader *header = packetGroup[m->msgId].pkt.Header<NewAlohaHeader>();
		cout<<"new : ";
		cout<<"destID :"<<packetGroup[m->msgId].destID<<"  sourID : "<<packetGroup[m->msgId].sourID
		   <<"  serialNum: "<<(int)header->serialNum<<endl;
		if(packetGroup[m->msgId].reSendTime == MAX_TIMEOUT_COUNT)
			packetGroup[m->msgId].pkt.Move<NewAlohaHeader>();
		Ptr<MsgSendDataReq> req(new MsgSendDataReq);
		req->packet = packetGroup[m->msgId].pkt;
		SendReq(req, PHY_LAYER, PHY_PID);
		packetGroup[m->msgId].reSendTime--;
		LOG(INFO)<<"resend package No."<<(uint16_t)m->msgId<<" has "<<packetGroup[m->msgId].reSendTime<<" time to ReSendDown=================";
	//如果超时次数为 0 ，则在packetGroup中把该包删除，否则继续设置该包的计时器
		if(packetGroup[m->msgId].reSendTime <= 0){
                        discardNum++;
                        LOG(INFO)<<"packet discardNum : "<<discardNum;
                } else{
                        srand((unsigned)time(NULL));
                        int times=rand()%reSendTimeRange + minReSendTime;
                        LOG(INFO)<<"packet No"<<m->msgId<<" resend in "<<times<<" s ";
                        SetTimer(times, m->msgId);
                }
	}
        map<uint16_t, PackageInfo>::iterator iter;
        cout<<"after resend packetGroup :"<<endl;
        for(iter = packetGroup.begin(); iter!= packetGroup.end(); iter++){
			cout<<(int)iter->first<<": "<<(int)iter->second.destID<<" : "<<(int)iter->second.sourID<<endl;
        }


} 

MODULE_INIT(NewAloha)
PROTOCOL_HEADER(NewAlohaHeader)	
#endif
}//end namespace NewAloha

