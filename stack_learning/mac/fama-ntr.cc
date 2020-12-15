//version1.0   暂时不添加性能分析部分，系统的最大传输时延还需再确认
//物理层要配套使用qpskclient_cqh.cc使用
//目前发送完CTS发送请求后不等待物理层到回复
//Remote接收到事件后其退避时间可能需要加长，目前为2*maxtransittime
//------------------------------3.11---------------------------------
//第一版完成
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
#define CURRENT_PID 88//可能需修改

#define PACKET_TYPE_RTS 1
#define PACKET_TYPE_CTS 2
#define PACKET_TYPE_DATA 3
extern client* UIClient;//暂时不用
extern client* TraceClient;
namespace famantr{
//为方便观看先去掉预编译
#if MAC_PID == CURRENT_PID 


//不清楚这个方法是否可行，或者是直接在msg中定义这个事件给QPSK和fama使用
//extern struct EvStartRecvData；
//extern struct EvFinishRecvData;
//定义各个事件
using msg::MsgSendDataReq;
using msg::MsgSendDataRsp;
using msg::MsgRecvDataNtf;//对接收到的数据要判断类型，对错
using msg::MsgTimeOut;
using pkt::Packet;
using msg::EvStartRecvData;//PHY层发来到用于载波监听到的事件
//自定义事件
struct GoToIdle : hsm::Event<GoToIdle>{};//由IsHaveDataNow()产生到事件
struct GoToBackOff : hsm::Event<GoToBackOff>{};//由IsHaveDataNow()产生到事件
struct RemoteTimeOut : hsm::Event<RemoteTimeOut>{};//实际为Remote状态到MsgTimeUp，并没有真正使用这个事件
struct BackOffTimeOut : hsm::Event<BackOffTimeOut>{};
struct RecvCTS : hsm::Event<RecvCTS>{};
struct NotCTS : hsm::Event<NotCTS>{};


//定义各个状态
struct Top;
struct Idle;
struct RTS;
struct XMIT;//数据发送阶段
struct BackOff;//主动退避阶段
struct Remote;//载波监听退避阶段
struct WaitRsp;//等待物理层回复阶段
struct WaitCTS;
//fama-ntr协议头部
struct famaheader{
        uint8_t type;  //类型，1RTS 2CTS 3DATA
        uint8_t src;   //源地址
        uint8_t dst;   //目的地址
        uint8_t serialNum;//包序号
};
class fama_ntr : public mod::Module<fama_ntr, MAC_LAYER, CURRENT_PID>
{
public:
    fama_ntr() { MODULE_CONSTRUCT(Top); }//初始化

    void Init() { LOG(INFO) << "fama_ntr Init"; 
    selfMacId = uint8_t(Config::Instance()["famantr"]["MacId"].as<int>());
    maxdistance=(Config::Instance()["famantr"]["Maxtransitdistance"].as<double>());     
    LOG(INFO)<<"ID"<<selfMacId;
    LOG(INFO)<<"最远传输距离"<<maxdistance;                                                                                                                                                                                                                                           
    maxtransittime=maxdistance/1500;                                
    LOG(INFO)<<"最大传输时延"<<maxtransittime;
    lastbackofftimeid=0;
    LOG(INFO)<<"fama_ntr complete";
    }//系统数据初始化
    //后续还需要初始化再补充
   
    //定义函数，注意如果函数中需要使用这些事件，一定要引用调参，即加上&
    //在HSM_TRANSIT中调用到函数，其参数必须为const Ptr<T> &，T为接收到的事件类型,所以出现了SetBackOffTime1
    //和SetBackOffTime2，实际上两个函数的功能是一致的，如果类似到数量较多可以使用模板
    void SendRtsandPushQueue(const Ptr<MsgSendDataReq> &m);//Passive状态接到发送数据请求，发送RTS并将数据推入数据队列
    /**
    *@brief 设置Remote退避时间，且设置监听位
    **/
    void SetRemoteTime(const Ptr<EvStartRecvData> &m);
    /**
    @brief 判断是否栈空且是否仍处于监听阶段，是的话重置定时器
    **/
    void IsHaveDataNowOrListening(const Ptr<MsgTimeOut> &m);
    /**
    *@brief 控制收到的数据，并销毁原先到定时器，重新设置定时器进入Remote状态
    **/
    void HandleData(const Ptr<MsgRecvDataNtf> &m);

    void PushQueue(const Ptr<MsgSendDataReq> &m);//Remote状态下接到发送数据请求
    void SetBackOffTime1(const Ptr<NotCTS> &m);
    void SetBackOfftime2(const Ptr<GoToBackOff> &m);
    void SendRTS(const Ptr<BackOffTimeOut> &m);
    void IsCTS(const Ptr<MsgRecvDataNtf> &m);//确定收到到是CTS则生成RecvCTS事件，否则生成NOTCTS事件
    void SendData(const Ptr<RecvCTS> &m);
    void SetListen(const Ptr<EvStartRecvData> &m);//在Remote阶段每次接受到EvStartRecvData事件时设置listening位
    /**
    *@brief 通过超时事件到第二个参数来判断是否是对应的定时器超时
    **/
    void IsLastBackOffTimeOut(const Ptr<MsgTimeOut> &m);//用于判断是否是最后一次BackOff退避定时器超时，是的话生成BackoffTimeOUT事件
    
private:
    queue<Ptr<MsgSendDataReq>> Msgqueue;//用于临时存放数据到数据队列，这里存放到指针类型还需要修改
    bool listening=false;//判断是否仍处于接收数据阶段
    uint8_t selfMacId;
    double maxdistance;//最大传输距离
    double maxtransittime;//最大传输时延=最大传输距离距离/1500m/s
    int lastbackofftimeid;//上一次设置的backoff定时器的MsgID
    timer_id_type timeid;
    uint16_t DataSerialNum=1;//包序号，每成功发出一个DATA包，包序号加一
};
struct Top : hsm::State<Top, hsm::StateMachine, Idle>//第三个状态只是一个初始状态，不一定要写Idle
{															  
	typedef hsm_vector<MsgRecvDataNtf,MsgSendDataRsp> reactions; //每个状态都必须先写上一个叫reactions的列表
	//Dispatch会从这个列表中找是否存在能够处理的事件
    HSM_DISCARD(MsgRecvDataNtf);
    HSM_DISCARD(MsgSendDataRsp);//这个应该也可以不加，但为来防止意外情况到出现还是加上
};

struct Idle : hsm::State<Idle, Top>{
    typedef hsm_vector<MsgSendDataReq, EvStartRecvData> reactions;
    HSM_TRANSIT(MsgSendDataReq, WaitRsp, &fama_ntr::SendRtsandPushQueue);//收到数据发送要求，发送RTS请求并将数据入栈
    HSM_TRANSIT(EvStartRecvData,Remote,&fama_ntr::SetRemoteTime);//监听到载波，转移到Remote状态并设置退避定时器
};

struct WaitRsp : hsm::State<WaitRsp, Top>{
	typedef hsm_vector<MsgSendDataReq,MsgSendDataRsp,EvStartRecvData> reactions;
	HSM_DEFER(MsgSendDataReq);//在该状态延时处理数据发送请求
    HSM_TRANSIT(MsgSendDataRsp,RTS);//收到PHY层的回复，转移到RTS状态
    HSM_TRANSIT(EvStartRecvData,Remote,&fama_ntr::SetRemoteTime);//监听到载波，转移到Remote状态并设置退避定时器
};
struct RTS : hsm::State<RTS, Top>{
	typedef hsm_vector<MsgSendDataReq,EvStartRecvData> reactions;
	HSM_DEFER(MsgSendDataReq);//在该状态延时处理数据发送请求
    HSM_TRANSIT(EvStartRecvData,WaitCTS); 
};
struct WaitCTS : hsm::State<WaitCTS, Top>{
	typedef hsm_vector<MsgSendDataReq,EvStartRecvData,MsgRecvDataNtf,RecvCTS,NotCTS> reactions;
	HSM_DEFER(MsgSendDataReq);//在该状态延时处理数据发送请求
    HSM_DISCARD(EvStartRecvData);
    HSM_WORK(MsgRecvDataNtf, &fama_ntr::IsCTS);
    HSM_TRANSIT(RecvCTS,XMIT,&fama_ntr::SendData);
    HSM_TRANSIT(NotCTS,BackOff,&fama_ntr::SetBackOffTime1);
};
struct XMIT : hsm::State<XMIT, Top>{
	typedef hsm_vector<MsgSendDataRsp，MsgSendDataReq> reactions;
    HSM_TRANSIT(MsgSendDataRsp,Idle);//收到PHY层的回复，数据发送完毕，转移到Idle状态
    HSM_DEFER(MsgSendDataReq);//在该状态延时处理数据发送请求
};
struct Remote : hsm::State<Remote,Top>{
	typedef hsm_vector<MsgSendDataReq,EvStartRecvData,MsgRecvDataNtf,MsgTimeOut,GoToIdle,GoToBackOff> reactions;
	HSM_WORK(MsgTimeOut,&fama_ntr::IsHaveDataNowOrListening);//超时后调用IsHaveDataNow生成转移事件，如果仍处于接收状态则重新设置接收定时器
    HSM_WORK(MsgRecvDataNtf,&fama_ntr::HandleData);//收到数据，处理数据，Listen位置否
    HSM_WORK(MsgSendDataReq,&fama_ntr::PushQueue);//在该状态延时处理数据发送请求
    HSM_WORK(EvStartRecvData,&fama_ntr::SetListen);//防止在Remote里面接受到数据后又重新接受到数据到情况
    HSM_TRANSIT(GoToBackOff,BackOff,&fama_ntr::SetBackOfftime2);
    HSM_TRANSIT(GoToIdle,Idle);
};
struct BackOff : hsm::State<BackOff, Top>{
	typedef hsm_vector<MsgSendDataReq,MsgTimeOut,EvStartRecvData,BackOffTimeOut> reactions;
    HSM_WORK(MsgSendDataReq,&fama_ntr::PushQueue);
    HSM_WORK(MsgTimeOut,&fama_ntr::IsLastBackOffTimeOut);
    HSM_TRANSIT(BackOffTimeOut,WaitRsp,&fama_ntr::SendRTS);
    HSM_TRANSIT(EvStartRecvData,Remote,&fama_ntr::SetRemoteTime);
};

void fama_ntr::SendRtsandPushQueue(const Ptr<MsgSendDataReq> &m)
{
    if(Msgqueue.size()==20)//如果已经有二十个数据来，则删除队首过于陈旧的数据
    {
        Msgqueue.pop();//缓存栈溢出
        LOG(INFO)<<"缓存栈溢出";
        //删除到会是最先到数据包，所以包序号要加一
        DataSerialNum++;
    }
    Msgqueue.push(m);//先将待发数据推入栈中
    //-------------------生成RTS包-----------------------------------
    Packet pkt(0);//申请一段新内存，大小为header+parameter(此处为0)=header
	famaheader *h = pkt.Header<famaheader>();
	h->type = PACKET_TYPE_RTS;
	h->dst = (uint8_t)m->address;//目的地址
	h->src = selfMacId;//自身地址
    h->serialNum=DataSerialNum;//包序号
    //-------------------------------------------------------------
    string logStr2 = Trace::Instance().Log(MAC_LAYER, MAC_PID, -1, -1, -1, -1, -1, -1, -1, (int)h->src, (int)h->dst,(int)h->serialNum, "RTS", "send");
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
    //---------------------------------------------------------
    pkt.Move<famaheader>();
	Ptr<MsgSendDataReq> req(new MsgSendDataReq);
	req->packet = pkt;
	SendReq(req, PHY_LAYER, PHY_PID);//实际上最后是推入了该事件 QueueType::Push(p);
	LOG(INFO)<<"send RTS and push data";
    //向网络层发送回复
    Ptr<MsgSendDataRsp> rsp(new MsgSendDataRsp);
    SendRsp(rsp,NET_LAYER,NET_PID);
};
void fama_ntr::IsCTS(const Ptr<MsgRecvDataNtf> &m)
{
    famaheader *header = m->packet.Header<famaheader>();//得到MAC层包头
    LOG(INFO)<<"类型"<<header->type;
    LOG(INFO)<<"收到数据的源地址"<<header->src;
    LOG(INFO)<<"收到数据的目的地址"<<header->dst;
    LOG(INFO)<<"收到数据的序号"<<header->serialNum;
    if(header->type==PACKET_TYPE_CTS)
    {
        if(header->dst==selfMacId)
        {
            //---------------------------------------------------
            string logStr1 = Trace::Instance().Log(MAC_LAYER, MAC_PID, -1, -1, -1, -1, -1, -1, -1, (int)header->src, (int)header->dst, (int)header->serialNum, "CTS","recv");
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
            //----------------------------------------------------
            LOG(INFO)<<"Receive my CTS";
            //生成RevCTS事件
            Ptr<RecvCTS> p(new RecvCTS);
            SendNtf(p,MAC_LAYER,MAC_PID);
        }
        else//否则生成NotCTS事件，执行退避操作
        {
            LOG(INFO)<<"Receive NotCTS";
            Ptr<NotCTS> p(new NotCTS);
            SendNtf(p,MAC_LAYER,MAC_PID);       
        }
    }
    else//否则生成NotCTS事件，执行退避操作
    {
        LOG(INFO)<<"Receive NotCTS2";
        Ptr<NotCTS> p(new NotCTS);
        SendNtf(p,MAC_LAYER,MAC_PID);
    }
};
void fama_ntr::SendData(const Ptr<RecvCTS> &m)//收到CTS了，开始发送数据
{
    auto msgntf=Msgqueue.front();//取出消息队列中第一个元素
    Msgqueue.pop();//弹出待发送到元素，FAMANTR协议没有ACK机制，只要发出则认为收到（不管是否真的收到）
    auto h=msgntf->packet.Header<famaheader>();
    h->dst=msgntf->address;
    h->type=PACKET_TYPE_DATA;;
    h->src=selfMacId;
    h->serialNum=DataSerialNum;
    DataSerialNum++;//发送完一个之后所有数据都属于下一个序号
    msgntf->packet.Move<famaheader>();
    SendReq(msgntf,PHY_LAYER, PHY_PID);
    LOG(INFO)<<"发送数据";
    //------------------------------------------------------------------------
    string logStr = Trace::Instance().Log(MAC_LAYER, MAC_PID, msgntf->packet, -1, -1, -1, -1, -1, (int)h->src, (int)h->src,-1, "data", "send");
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
    //----------------------------------------------------------------------
};
void fama_ntr::SetBackOffTime1(const Ptr<NotCTS> &m)
{
    LOG(INFO)<<"Set backoff time,由waitCTS到backoff";
    //生成随机数种子
    srand((int)time(0));
    //取得随机数
    int backofftime=rand()%21;//产生0-20之间到随机数
    if(lastbackofftimeid==100)//重置退避定时器到ID
    {
        lastbackofftimeid=0;
    }
    SetTimer(backofftime * maxtransittime, ++lastbackofftimeid); //第二个参数msgId为0代表的是Remote状态的定时器超时，否则是该状态的定时器超时
};
void fama_ntr::IsLastBackOffTimeOut(const Ptr<MsgTimeOut> &m)
{
    if(m->msgId==lastbackofftimeid)//是对应的定时器超时，转移到WaitRsp状态
    {
        LOG(INFO)<<"Backoff Time Out";
            //生成RevCTS事件
        Ptr<BackOffTimeOut> p(new BackOffTimeOut);
        SendNtf(p,MAC_LAYER,MAC_PID);
    }    
};
void fama_ntr::SendRTS(const Ptr<BackOffTimeOut> &m)
{
    Packet pkt(0);//申请一段新内存，大小为header+parameter(此处为0)=header
	famaheader *h = pkt.Header<famaheader>();
	h->type = PACKET_TYPE_RTS;
    auto msgntf=Msgqueue.front();//取出栈中第一个要发送的数据
	h->dst = (uint8_t)msgntf->address;//目的地址
	h->src = selfMacId;//自身地址
    h->serialNum=DataSerialNum;
    
    //-------------------------------------------------------------
    string logStr2 = Trace::Instance().Log(MAC_LAYER, MAC_PID, -1, -1, -1, -1, -1, -1, -1, (int)h->src, (int)h->dst,(int)h->serialNum, "RTS", "send");
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
    //---------------------------------------------------------
    pkt.Move<famaheader>();
	Ptr<MsgSendDataReq> req(new MsgSendDataReq);
	req->packet = pkt;
	SendReq(req, PHY_LAYER, PHY_PID);
	LOG(INFO)<<"send RTS";
};
void fama_ntr::SetRemoteTime(const Ptr<EvStartRecvData> &m)//设置Remote退避时间，且设置监听位
{
    listening=true;//监听位置一，表示仍处于载波监听阶段
    timeid=SetTimer(2*maxtransittime,0);//Msgid为0代表是Remote状态设置的定时器,timeid为该定时器绑定到ID
    LOG(INFO)<<"监听到载波，开始退避";
};
void fama_ntr::HandleData(const Ptr<MsgRecvDataNtf> &m)
{  
    CancelTimer(timeid);//取消原先到Remote时钟
    listening=false;//接收完数据，本次监听结束
    famaheader *header = m->packet.Header<famaheader>();//得到MAC层包头
    LOG(INFO)<<"类型"<<header->type;
    LOG(INFO)<<"收到数据的源地址"<<header->src;
    LOG(INFO)<<"收到数据的目的地址"<<header->dst;
    LOG(INFO)<<"收到数据的包序号"<<header->serialNum;
    if(header->type==PACKET_TYPE_DATA)//收到数据包
    {
        if(header->dst==selfMacId)
        {
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
            m->packet.Move<famaheader>();//将head_指针移动到网络层头部的位置
		    SendNtf(m, NET_LAYER, NET_PID);  //向NET层通知消息（实际上是将事件推进了NTFQUEUE）
			LOG(INFO)<<"recv a data packet and send up";
        }
    }
    else if(header->type==PACKET_TYPE_RTS)//收到RTS包
    {
         if(header->dst==selfMacId)
        {
            //----------------------------------------------------------------------------
                string logStr4 = Trace::Instance().Log(MAC_LAYER, MAC_PID, -1, -1, -1, -1, -1, -1, -1, (int)header->src, (int)header->dst, (int)header->serialNum, "RTS","recv");
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
                //-------------------------------------------------------------------------
            Packet pkt(0);//申请一段新内存，大小为header+parameter(此处为0)=header
	        famaheader *h = pkt.Header<famaheader>();
	        h->type = PACKET_TYPE_CTS;
	        h->dst = header->src;//目的地址
	        h->src = selfMacId;//自身地址
            h->serialNum=header->serialNum;
            pkt.Move<famaheader>();
	        Ptr<MsgSendDataReq> req(new MsgSendDataReq);
	        req->packet = pkt;
	        SendReq(req, PHY_LAYER, PHY_PID);
	        LOG(INFO)<<"send CTS";
            //-------------------------------------------------------------
                string logStr2 = Trace::Instance().Log(MAC_LAYER, MAC_PID, -1, -1, -1, -1, -1, -1, -1, (int)h->src, (int)h->dst,(int)h->serialNum, "CTS", "send");
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
            //---------------------------------------------------------
        }
    }
   
    timeid=SetTimer(2*maxtransittime,0);//重新退避
};
void fama_ntr::IsHaveDataNowOrListening(const Ptr<MsgTimeOut> &m)
{
    if(m->msgId==0)//是否是Remote状态的定时器超时
    {
        if (listening==true)//仍处于载波监听阶段，重新设置定时器
        {
            timeid=SetTimer(2*maxtransittime,0);//最新设置到Remote定时器的ID
            LOG(INFO)<<"仍处于载波监听阶段，重新执行退避";
            return;
        }
        else if(listening==false)//已经接收到了数据，退出来载波监听阶段
        {
            if (Msgqueue.empty())//如果待发送的数据栈是空的，则转移到Idle阶段
            {
                Ptr<GoToIdle> p(new GoToIdle);
                LOG(INFO)<<"没有待发送的数据，转移到Idle";
                SendNtf(p,MAC_LAYER,MAC_PID);
                return;
            }
            else
            {
                Ptr<GoToBackOff> p(new GoToBackOff);
                LOG(INFO)<<"有数据待发送，转移到BackOff";
                SendNtf(p,MAC_LAYER,MAC_PID);
                return;
            }
        }
    }
};
void fama_ntr::PushQueue(const Ptr<MsgSendDataReq> &m)//Remote状态下接到发送数据请求
{
    if(Msgqueue.size()==20)//如果已经有二十个数据来，则删除队首过于陈旧的数据
    {
        Msgqueue.pop();
        LOG(INFO)<<"缓存栈溢出";
        //删除到会是最先到数据包，所以包序号要加一
        DataSerialNum++;
    }
    Msgqueue.push(m);
    LOG(INFO)<<"将数据发送请求入栈";
    Ptr<MsgSendDataRsp> p(new MsgSendDataRsp);
    SendNtf(p,NET_LAYER,NET_PID);
};
void fama_ntr::SetListen(const Ptr<EvStartRecvData> &m)//将监听位置true 
{
    listening=true;
}
void fama_ntr::SetBackOfftime2(const Ptr<GoToBackOff> &m)
{
    LOG(INFO)<<"Set backoff time,由remote到backoff";
    //生成随机数种子
    srand((int)time(0));
    //取得随机数
    int backofftime=rand()%21;//产生0-20之间到随机数
    if(lastbackofftimeid==100)//重置退避定时器到ID
    {
        lastbackofftimeid=0;
        LOG(INFO)<<"重置backoff定时器ID";
    }
    SetTimer(backofftime * maxtransittime, ++lastbackofftimeid); //第二个参数msgId为0代表的是Remote状态的定时器超时，否则是该状态的定时器超时
}
MODULE_INIT(fama_ntr);
PROTOCOL_HEADER(famaheader);
#endif
};
