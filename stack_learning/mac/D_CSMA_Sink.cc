//D_CSMA SINK节点，除在计算CW窗口值会起发送作用外其他时间都只做接收用
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
#include <vector>
#include <queue>
#include <iomanip>
#include <ctime>
#include<sys/time.h>
#define CURRENT_PID 100//可能需修改

#define PACKET_TYPE_TESTDATA 1//测试传播时延的数据
#define PACKET_TYPE_DATA 2//普通数据
extern client* UIClient;//暂时不用
extern client* TraceClient;
namespace DCSMASINK{
#if MAC_PID == CURRENT_PID 


//定义各个事件
using msg::MsgSendDataReq;
using msg::MsgSendDataRsp;
using msg::MsgRecvDataNtf;//对接收到的数据要判断类型，对错
using msg::MsgTimeOut;
using pkt::Packet;
using msg::EvStartRecvData;//PHY层发来到用于载波监听到的事件
//自定义事件


//定义各个状态
struct Top;
struct Idle;
struct WaitSendTestDataRsp;
struct WaitTimeOut;
struct WaitSendTestDataRspAgain;
struct ReceiveData;


struct DCSMA_SINKheader{
        uint8_t type; //包类型
        uint8_t src;   //源地址
        uint8_t dst;   //目的地址
        uint16_t serialNum;//包序号
        uint8_t second;//发送节点发送包的时间（秒数），仅在测试阶段有用
        uint16_t usecond;//发送节点发送包的时间（毫秒数），仅在测试阶段有用
        //如果接收时到秒数<发送到秒数 那么接受分钟需要+60
};
class DCSMA_SINK : public mod::Module<DCSMA_SINK, MAC_LAYER, CURRENT_PID>
{
public:
    DCSMA_SINK() { MODULE_CONSTRUCT(Top); }//初始化

    void Init() {
         LOG(INFO) << "DCSMA_SINK Init"; 
         BrocastTime=(Config::Instance()["D_CSMA"]["BrocastTime"].as<int>());
         selfMacId = uint8_t(Config::Instance()["D_CSMA"]["MacId"].as<int>());
         maxtranstime = (Config::Instance()["D_CSMA"]["maxtranstime"].as<double>());
         SetTimer(BrocastTime);
         }
    /**
     *@brief  接受到数据，上传给上一层，如果后面有重发机制则要先判断是否是重发数据
     **/ 
    void SendUp(const Ptr<MsgRecvDataNtf> &m);
    /**
     *@brief 通过Ｉnit()中到初始化定时器超时事件来开始发送测试数据，要在数据帧头部加上当前到时间
     **/ 
    void SendTestData(const Ptr<MsgTimeOut> &m);
    /**
     *@brief　 设置发送第二次测试数据的定时器，要在数据帧头部加上当前到时间 
     **/
    void SetTimerAgain(const Ptr<MsgSendDataRsp> &m);

private:
    int BrocastTime;//刚启动的等待时间
    int selfMacId;
    uint16_t DataSerialNum=1;//包序号，每成功发出一个DATA包，包序号加一
    double maxtranstime;
};
struct Top : hsm::State<Top, hsm::StateMachine, Idle>
{															  
	typedef hsm_vector<> reactions; 
};
struct Idle : hsm::State<Idle, Top>{
    typedef hsm_vector<MsgTimeOut,EvStartRecvData> reactions;
    HSM_TRANSIT(MsgTimeOut,WaitSendTestDataRsp,&DCSMA_SINK::SendTestData);
    HSM_TRANSIT(EvStartRecvData,ReceiveData);
};
struct ReceiveData : hsm::State<ReceiveData, Top>{
    typedef hsm_vector<MsgRecvDataNtf> reactions;
    HSM_TRANSIT(MsgRecvDataNtf,Idle,&DCSMA_SINK::SendUp);
};  
struct WaitSendTestDataRsp : hsm::State<WaitSendTestDataRsp, Top>{
    typedef hsm_vector<MsgSendDataRsp> reactions;
    HSM_TRANSIT(MsgSendDataRsp,WaitTimeOut,&DCSMA_SINK::SetTimerAgain);
};                                                                     
struct WaitTimeOut : hsm::State<WaitTimeOut, Top>{
    typedef hsm_vector<MsgTimeOut> reactions;
    HSM_TRANSIT(MsgTimeOut,WaitSendTestDataRspAgain,&DCSMA_SINK::SendTestData);
};
struct WaitSendTestDataRspAgain : hsm::State<WaitSendTestDataRspAgain, Top>{
    typedef hsm_vector<MsgSendDataRsp> reactions;
    HSM_TRANSIT(MsgSendDataRsp,Idle);
}; 
void DCSMA_SINK::SendUp(const Ptr<MsgRecvDataNtf> &m)
{
    DCSMA_SINKheader *header = m->packet.Header<DCSMA_SINKheader>();//得到MAC层包头
    LOG(INFO)<<"类型"<<header->type;
    LOG(INFO)<<"收到数据的源地址"<<header->src;
    LOG(INFO)<<"收到数据的目的地址"<<header->dst;
    LOG(INFO)<<"收到数据的包序号"<<header->serialNum;
    //---------------------------------------------------------------------------
    string logStr1 = Trace::Instance().Log(MAC_LAYER, MAC_PID,m->packet, -1, -1, -1, -1, -1, (int)header->src, (int)header->dst,(int)header->serialNum, "data","recv");
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
    //----------------------------------------------------------------------------- 
    if(m->reservation==0)//收到正确数据包,则上传
    {
        m->packet.Move<DCSMA_SINKheader>();//将head_指针移动到网络层头部的位置
        SendNtf(m, NET_LAYER, NET_PID);  //向NET层通知消息（实际上是将事件推进了NTFQUEUE）
        LOG(INFO)<<"recv a data packet and send up";    
    }
    else
    {
        LOG(INFO)<<"接受数据有错误";
    }
    LOG(INFO)<<"向idle转移";
};

void DCSMA_SINK::SendTestData(const Ptr<MsgTimeOut> &m)
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    Packet pkt(1000);//申请一段新内存，大小为header+parameter(此处为1000)=header+1000
    DCSMA_SINKheader *h = pkt.Header<DCSMA_SINKheader>();
    h->type = PACKET_TYPE_TESTDATA;
    h->dst = 0;//广播包
    h->src = selfMacId;//自身地址
    h->serialNum=DataSerialNum++;//注意包序号自加
    h->second=tv.tv_sec%60;//获得秒数，这里为什么这样取余可以结合数据头的设计来看
    h->usecond=tv.tv_usec/1000; 
    LOG(INFO)<<"发送时秒数为"<<h->second;
    LOG(INFO)<<"发送时毫秒数为"<<h->usecond;
    pkt.Move<DCSMA_SINKheader>();
    Ptr<MsgSendDataReq> req(new MsgSendDataReq);
    req->packet = pkt;
    SendReq(req, PHY_LAYER, PHY_PID);
    LOG(INFO)<<"发送广播包";                                    
};

 void DCSMA_SINK::SetTimerAgain(const Ptr<MsgSendDataRsp> &m)
 {
     SetTimer(maxtranstime*2);
 }
MODULE_INIT(DCSMA_SINK);
PROTOCOL_HEADER(DCSMA_SINKheader);
#endif
};
