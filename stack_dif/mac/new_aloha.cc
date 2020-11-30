/*
这是一个都有重传的协议
*/
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

//有应答Aloha的头部有数据类型（1表示ACK，0表示DATA），还有源目标MAC地址，和串号（只是为了保证发出的MAC数据包的唯一性，MAC层每发出一个包就该节点的MAC串号加一。）
#pragma pack(1)
struct NewAlohaHeader{
	uint16_t serialNum;
	uint8_t type;
	uint8_t destID;
	uint8_t sourID;
//	uint16_t serialNum;
};
#pragma pack;

/*保存数据包的方法：（每个节点的MAC层会有一个串号，每发一个数据包就加一）
针对发送端，以免重传
类外定义了PacketInfo的结构体，保存了网络层的数据包（虽然把头部封装好了，但没有移动指针），重传次数（初始值为3），源目标MAC地址*/
struct PackageInfo{
	Packet pkt;
	int reSendTime;
	int sourID;
	int destID;
};
/*当收到Data数据的时候，会创建一个Info结构体，放入收到Data数据的串号和源MAC地址（以便万一再次收到该Data的时候，又往网络层传一次这个Data。
只要确定串号和源MAC地址即可确定是不是同一个MAC数据包）
针对接收端
*/
struct Info{
	uint16_t serialNum;
	uint8_t sourID;
};

class NewAloha : public mod::Module<NewAloha, MAC_LAYER, CURRENT_PROTOCOL>{
	public:
/*一般每个协议构造函数都执行这一句话：（在传入模板参数的时候，这个类就构造完成了，包括类内的Sap、Tap等模板类，而Sap、Tap中的owner_指针是一个野指针，因此需要SetOwner进行初始化，即传入当前协议对象的指针this，在module.h中的宏定义*/
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
		//协议类中的私有成员，一个队列，保存了所有收到数据的串号和源MAC地址。
		//协议类私有的packetGroup映射，保存了串号和对应的PacketInfo对象的映射。即通过串号找到这个对象，从而找到网络层数据包。
		list<Info> recvPacketTemp;
		uint16_t serialNum_ = 0;
		int minReSendTime;
		int reSendTimeRange;
		int discardNum = 0;
		uint16_t isTestMode_;
		
};

//hsm.h 第700行
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
//收到从物理层传来的MsgRecvDataNtf时，就会调用SendUp函数。（物理的上传来的Packet对象一般都是Up的，因此调用Header就会得到当前指针，强转成MAC层的封装头部即可找到对应的头部信息。）
	NewAlohaHeader *header = m->packet.Header<NewAlohaHeader>();
    uint16_t saveSerialNum = header->serialNum;
	LOG(INFO)<<"new aloha recv something for MacId : "<<(int)header->destID;
	if(header->type == PACKET_TYPE_DATA){ 
	//当获得了所有头部信息之后，协议通过判断type是DATA还是ACK。
	//如果是DATA，就判断目标MAC地址是不是自己，或者是不是广播包（其他情况都不处理）
		if (header->destID == selfMacId){

			//如果是数据类型，并且目的地址是自己，就把包上发并发出ack
			//假如MAC地址是自己的：先判断recvPacketTemp中是否已经满了（20个），如果满了就剔出第一个，然后从队列中找是否有对应的包（串号和源MAC都一样），再设置标志位（如果是重复的DATA，则置1）。
			//这里的判断原本是>20
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
			//协议栈中需要写入的时候，先判断TraceClient是否存在，然后调用client.cc中的一个函数cliwrite（这里因为cliwrite写在类外，因此可以直接调用）
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
			//如果收到重复的DATA，就不往上传，否则往上传。
			if(!isResendPacket){
				m->packet.Move<NewAlohaHeader>();
				SendNtf(m, NET_LAYER, NET_PID);
				LOG(INFO)<<"recv a data packet and send up";
			}
			//但不论是否重复DATA，都要发送ACK。（生成的是一个只有所有协议头的Packet对象，这时没有上层的封装，但是当MAC层收到这个包的时候，就知道这是一个ACK包，因此不再往上传）
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
																		//字符串长度str.length(),字符数组大小strlen(cstr),在cstring.h
					char* sendBuff = new char[logStr2.length()];		//字符数组
					memcpy(sendBuff, logStr2.data(), logStr2.length());	//将字符串的内容赋值给字符数组
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
			//假如收到的是一个广播包：直接上传，不需要做其他操作（不需要发ACK，也不需要重传）		
			m->packet.Move<NewAlohaHeader>();
			SendNtf(m, NET_LAYER, NET_PID);
			LOG(INFO)<<"recv a broadcast packet and send up";
			} 
		} else {
			LOG(INFO)<<"recv a ack type packet";
		//如果是ack类型，在packageGroup中寻找序号与目的id都相符合项，将其删掉
		//如果是ACK包：需要判断ACK包的目标MAC是不是自身，并且串号和源MAC是否能对应上。（存在的话，只要把packetGroup中对应的包删去即可，没有往网络层发其他消息。这里定时器事件应该还是会发生的，只是在重传的时候，在packetGroup中找不到该数据包，就没有发出去了。）
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

//收到网络层下发的数据包时，即MAC层收到了MsgSendDataReq的时候，协议调用回调函数SendDown。
void NewAloha::SendDown(const Ptr<MsgSendDataReq> &m){
	//首先，需要将网络层的数据包进行封装（new_aloha的头共5 bytes），利用Packet类的函数进行封装：
	NewAlohaHeader *header = m->packet.Header<NewAlohaHeader>();
    header->serialNum = serialNum_;
    cout<<"send down serialNum : "<<(int)
	//协议中的改参数(封装)
	header->serialNum<<endl;
	header->type = PACKET_TYPE_DATA;
	header->destID = (uint8_t)m->address;
	header->sourID = selfMacId;
	serialNum_++;
	//将数据包和序号作为一个项目插入packetGroup
    //如果是广播包，则不需要设置定时器
	/*
不是广播包的处理：
1. 把MAC层数据包下放完了以后，需要先把这个数据包保存起来，如果要重传的话，就找到这个包然后下放物理层。
2. 不是广播包意味着可能需要重传，因此需要设置定时器，通过Module基类的函数SetTimer()函数。时间到了，就触发MsgTimeOut事件，然后调用重传函数*/
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
				//重传定时器事件：
                srand((unsigned)time(NULL));
                int times=rand()%reSendTimeRange + minReSendTime;
                LOG(INFO)<<"packet No"<<(int)header->serialNum<<" resend in "<<times<<" s ";
                SetTimer(times, header->serialNum);
                LOG(INFO)<<"send down data packet to MacId: "<<(int)m->address<<" and settimer";
        }else{//这里已经判断是广播包，广播包后操作如下
			//接下来需要判断是不是广播包，如果是的话，不需要设置定时器（因为非广播包没收到ACK时，需要超时重传）广播包的处理：
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

//超时重传是由MsgTimeOut事件触发的，只有从packetGroup中找到对应的PacketInfo的时候才会重传（如果在超时之前，收到了该数据的ACK，则会从packetGroup中删去该数据包结构体）
//如果需要重传，则会先往Trace写信息（这部分代码在其他地方基本一样）

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
		//如果是第一次重传，则先把指针移动。
		if(packetGroup[m->msgId].reSendTime == MAX_TIMEOUT_COUNT)
			packetGroup[m->msgId].pkt.Move<NewAlohaHeader>();
		//下发到物理层时，只对数据包赋值了。可能是因为结构体中其他的值，物理层并没有用到
		Ptr<MsgSendDataReq> req(new MsgSendDataReq);
		req->packet = packetGroup[m->msgId].pkt;
		SendReq(req, PHY_LAYER, PHY_PID);
		//然后把相应的数据包的重传此时减一
		packetGroup[m->msgId].reSendTime--;
		LOG(INFO)<<"resend package No."<<(uint16_t)m->msgId<<" has "<<packetGroup[m->msgId].reSendTime<<" time to ReSendDown=================";
	//如果超时次数为 0 ，则在packetGroup中把该包删除，否则继续设置该包的计时器
	//这时，如果重传次数小于等于0，说明之后不再需要重传（数据包没收到ACK），因此不需要设置定时器，协议类私有成员discardNum加一。（注意：只有收到了该数据包的ACK才会从packetGroup中删去这个数据包）只有需要重传的才设置定时器
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

