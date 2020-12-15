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
#include "app_datapackage.h"
#include "client.h"

#define CURRENT_PROTOCOL 6
#define MAX_TIMEOUT_COUNT 3
#define BROADCAST_ADDERSS 255
#define PACKET_TYPE_DATA 0
#define PACKET_TYPE_ACK 1
#define PACKET_TYPE_CRC 2

extern client* UIClient;
namespace aloha_special{

#if MAC_PID == CURRENT_PROTOCOL
using msg::MsgSendDataReq;
using msg::MsgSendDataRsp;
using msg::MsgRecvDataNtf;
using msg::MsgTimeOut;
using pkt::Packet;


struct Top;
struct Idle;
struct WaitRsp;

struct AlohaSpecialHeader{
	uint8_t type;
	uint8_t destID;
	uint8_t sourID;
	uint16_t serialNum;
};
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
static unsigned int crc32_tab[] = {
   0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
   0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
   0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
   0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
   0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
   0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
   0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
   0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
   0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
   0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
   0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
   0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
   0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
   0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
   0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
   0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
   0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
   0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
   0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
   0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
   0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
   0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
   0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
   0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
   0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
   0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
   0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
   0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
   0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
   0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
   0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
   0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
   0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
   0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
   0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
   0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
   0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
   0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
   0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
   0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
   0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
   0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
   0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

class AlohaSpecial : public mod::Module<AlohaSpecial, MAC_LAYER, CURRENT_PROTOCOL>{
	public:
		AlohaSpecial(){MODULE_CONSTRUCT(Top);}
		void Init(){
			LOG(INFO)<<"AlohaSpecial init";
			selfMacId = uint8_t(Config::Instance()["newAloha"]["MacId"].as<int>());
			minReSendTime = Config::Instance()["newAloha"]["MinReSendTime"].as<int>();
			reSendTimeRange = Config::Instance()["newAloha"]["ReSendTimeRange"].as<int>();
			isTestMode_ = Config::Instance()["test"]["testmode"].as<int16_t>();

		}
		void SendUp(const Ptr<MsgRecvDataNtf> &);
		void SendDown(const Ptr<MsgSendDataReq> &);
		void ReSendDown(const Ptr<MsgTimeOut> &);
		unsigned int GenCRCCheckSum(char* data, uint16_t len);
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
	HSM_WORK(MsgRecvDataNtf, &AlohaSpecial::SendUp);
	HSM_WORK(MsgTimeOut, &AlohaSpecial::ReSendDown);
};
struct Idle : hsm::State<Idle, Top>{
	typedef hsm_vector<MsgSendDataReq> reactions;
	HSM_TRANSIT(MsgSendDataReq, WaitRsp, &AlohaSpecial::SendDown);
};
struct WaitRsp : hsm::State<WaitRsp, Top>{
	typedef hsm_vector<MsgSendDataRsp, MsgSendDataReq> reactions;
	HSM_TRANSIT(MsgSendDataRsp, Idle);
	HSM_DEFER(MsgSendDataReq);
};

void AlohaSpecial::SendUp(const Ptr<MsgRecvDataNtf> &m){

	AlohaSpecialHeader *header = m->packet.Header<AlohaSpecialHeader>();
    uint16_t saveSerialNum = header->serialNum;
	LOG(INFO)<<"new aloha recv something for MacId : "<<(int)header->sourID<<"--->>"<<(int)header->destID<<" serialNum : "<<saveSerialNum;
			uint8_t *pi = (uint8_t*)m->packet.realRaw();
		cout<<endl;
		for (unsigned i = 0; i < sizeof(ImageHeader); ++i) { 
			cout<<hex<<(int)*(pi + i);
		} 
		cout<<dec<<endl;
//收到需要CRC校验的包
	if(header->type == PACKET_TYPE_CRC && header->destID == selfMacId){
		//校验数据长度待修改，1024
		LOG(INFO)<<"recv a data packet need to check CRC";
		int dataOffset = sizeof(ImageHeader) - PICTUERHEADER_PACKAGE_SIZE;
		cout<<"dataOffset : "<<dataOffset<<endl;
        uint16_t *dataSzie = (uint16_t*)m->packet.realRaw() + 2;
        cout<<"dataSzie : "<<*dataSzie<<endl;
		unsigned int crcGen = GenCRCCheckSum(m->packet.realRaw() + dataOffset, *dataSzie);
		unsigned int recvCRC;
		memcpy(&recvCRC, m->packet.realRaw() + dataOffset - sizeof(recvCRC), sizeof(recvCRC));
		cout<<"crcGen = "<<crcGen<<"  recvCRC = "<<recvCRC<<endl;
		if(crcGen == recvCRC){
			LOG(INFO)<<"CRC check successful";

			m->packet.Move<AlohaSpecialHeader>();
            m->needCRC = true;
			SendNtf(m, NET_LAYER, NET_PID);
			LOG(INFO)<<"recv a data packet which CRC check and send up";

			Packet pkt(0);
			AlohaSpecialHeader *h = pkt.Header<AlohaSpecialHeader>();
			h->type = PACKET_TYPE_ACK;
			h->destID = header->sourID;
			h->sourID = selfMacId;
			h->serialNum = saveSerialNum;
			pkt.Move<AlohaSpecialHeader>();
			Ptr<MsgSendDataReq> req(new MsgSendDataReq);
			req->packet = pkt;
			SendReq(req, PHY_LAYER, PHY_PID);
			LOG(INFO)<<"send ack SerialNum : "<<(int)h->serialNum;

		} else {
			LOG(INFO)<<"CRC check fail";
		}
	//收到ack	
	} else if(header->type == PACKET_TYPE_ACK && header->destID == selfMacId){

			LOG(INFO)<<"recv a ack type packet";
			map<uint16_t, PackageInfo>::iterator iter;
			for(iter = packetGroup.begin(); iter!= packetGroup.end(); iter++){
				if(iter->first == header->serialNum 
                        && iter->second.destID == header->sourID 
                        && iter->second.sourID == header->destID){
					packetGroup.erase(iter);
					if(UIClient != NULL){
						struct ImageHeader recvPackage;
						memcpy(&recvPackage, iter->second.pkt.realRaw(), sizeof(recvPackage));
						recvPackage.Package_type = 9;
						char* sendBuff = new char[sizeof(recvPackage)];
						memcpy(sendBuff, &recvPackage, sizeof(recvPackage));
						IODataPtr pkt(new IOData);
						pkt->fd   = UIClient->_socketfd;
						pkt->data = sendBuff;
						pkt->len  = sizeof(recvPackage);
						UIWriteQueue::Push(pkt);
						cliwrite(UIClient);
					}

					LOG(INFO)<<"recv a ack and delete the packet in map>>>>>>>>>>>>>>>>>>>>>>>>";
					break;
			} 
        }
	//收到广播包或不需要回复ack的包		
	} else  if(header->destID == selfMacId){
		LOG(INFO)<<"rcv a packet for "<<(int)header->destID<<" and send up";
		m->packet.Move<AlohaSpecialHeader>();
		SendNtf(m, NET_LAYER, NET_PID);
	} else if (header->destID == BROADCAST_ADDERSS){
		LOG(INFO)<<"rcv a broadcast packet and send up";
		m->packet.Move<AlohaSpecialHeader>();
		SendNtf(m, NET_LAYER, NET_PID);
	}
}

void AlohaSpecial::SendDown(const Ptr<MsgSendDataReq> &req){
	AlohaSpecialHeader *header = req->packet.Header<AlohaSpecialHeader>();
    header->serialNum = serialNum_;
    cout<<"send down serialNum : "<<(int)header->serialNum<<endl;
	// header->type = PACKET_TYPE_DATA;
	header->destID = (uint8_t)req->address;
	header->sourID = selfMacId;
	LOG(INFO)<<"send down a packet to macID "<<(int)selfMacId<<"--->>"<<(int)(header->destID)<<" serialNum : "<<header->serialNum;
	serialNum_++;
			uint8_t *pi = (uint8_t*)req->packet.realRaw();
			cout<<endl;
			for (unsigned i = 0; i < sizeof(ImageHeader); ++i) { 
				cout<<hex<<(int)*(pi + i);
			} 
			cout<<dec<<endl;

	if(req->address != 255){//非广播包
		if(req->needCRC){//需要加CRC校验
			LOG(INFO)<<"send down a packet with CRCCheckSum";
			header->type = PACKET_TYPE_CRC;
			PackageInfo backUp = {req->packet, MAX_TIMEOUT_COUNT, (int)header->sourID, (int)header->destID};
			packetGroup[header->serialNum] = backUp;
			req->packet.Move<AlohaSpecialHeader>();
			Ptr<MsgSendDataReq> m(new MsgSendDataReq);
			m->packet = req->packet;
			//校验数据长度待修改，1024
			int dataOffset = sizeof(ImageHeader) - PICTUERHEADER_PACKAGE_SIZE;
            uint16_t *dataSzie = (uint16_t*)req->packet.realRaw() + 2;
            cout<<"dataSzie : "<<*dataSzie<<endl;
			cout<<"dataOffset : "<<dataOffset<<endl;
			unsigned int crcTemp = GenCRCCheckSum(m->packet.realRaw() + dataOffset, *dataSzie);
			cout<<"genCRC : "<<crcTemp<<endl;
			memcpy(m->packet.realRaw() + dataOffset - sizeof(crcTemp), &crcTemp, sizeof(crcTemp));
			SendReq(m, PHY_LAYER, PHY_PID);

			srand((unsigned)time(NULL));
			int times=rand()%reSendTimeRange + minReSendTime;
			LOG(INFO)<<"packet No"<<(int)header->serialNum<<" resend in "<<times<<" s ";
			SetTimer(times, header->serialNum);
			LOG(INFO)<<"send down data packet to MacId: "<<(int)req->address<<" and settimer";
			
		} else {//不需要加CRC校验
			LOG(INFO)<<"send down a packet without CRCCheckSum";
			header->type = PACKET_TYPE_DATA;
			req->packet.Move<AlohaSpecialHeader>();
			Ptr<MsgSendDataReq> m(new MsgSendDataReq);
			m->packet = req->packet;
			SendReq(m, PHY_LAYER, PHY_PID);
			LOG(INFO)<<"send down data packet to MacId: "<<(int)req->address;

		}
	} else {//广播包
		LOG(INFO)<<"send down a broadcast packet";
		header->type = PACKET_TYPE_DATA;
		req->packet.Move<AlohaSpecialHeader>();
		Ptr<MsgSendDataReq> m(new MsgSendDataReq);
		m->packet = req->packet;
		SendReq(m, PHY_LAYER, PHY_PID);

	}
	Ptr<MsgSendDataRsp> rsp(new MsgSendDataRsp);
	SendRsp(rsp, NET_LAYER, NET_PID);
	LOG(INFO)<<"send rsp to NET_LAYER";
}

void AlohaSpecial::ReSendDown(const Ptr<MsgTimeOut> &m){
	if(packetGroup.count(m->msgId)){
		AlohaSpecialHeader *header = packetGroup[m->msgId].pkt.Header<AlohaSpecialHeader>();
		cout<<"new : ";
		cout<<"destID :"<<packetGroup[m->msgId].destID<<"  sourID : "<<packetGroup[m->msgId].sourID
		   <<"  serialNum: "<<(int)header->serialNum<<endl;
		if(packetGroup[m->msgId].reSendTime == MAX_TIMEOUT_COUNT)
			packetGroup[m->msgId].pkt.Move<AlohaSpecialHeader>();
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

unsigned int AlohaSpecial::GenCRCCheckSum(char* data, uint16_t len){

	unsigned int crc = 0;
    for (unsigned int i = 0; i < len; i++) {  
        crc = crc32_tab[(crc ^ data[i]) & 0xff] ^ (crc >> 8); 
	}	
    return crc;  
}
MODULE_INIT(AlohaSpecial)
PROTOCOL_HEADER(AlohaSpecialHeader)
#endif
}//end namespace AlohaSpecial	
