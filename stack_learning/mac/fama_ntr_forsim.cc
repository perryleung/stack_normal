//------------------------------3.11---------------------------------
//version1.0   暂时不添加性能分析部分，系统的最大传输时延还需再确认
//物理层要配套使用qpskclient_cqh.cc使用
//目前发送完CTS发送请求后不等待物理层到回复
//Remote接收到事件后其退避时间可能需要加长，目前为2*maxtransittime
//----------------3.26-----------------------------------------------
//仿真信道目前的比特率为1500
//第一版完成 
//-------------------3.29   
//仿真信道中不会自动将数据补零，而RTS帧的长度有一定要求，所以需要RTS帧不能pkt(0)
//4.11 退避种子由20减小到15
//4.12 退避种子范围于等待发送数据队列长度相关 int backofftime=rand()%(int)(10-Msgqueue.size()/2);//产生0-20之间到随机数
//4.13 msgqueue长度改为30,退避种子变为18
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
#define CURRENT_PID 99//可能需修改

#define PACKET_TYPE_RTS 1
#define PACKET_TYPE_CTS 2
#define PACKET_TYPE_DATA 3
extern client* UIClient;//暂时不用
extern client* TraceClient;
namespace famantr{
//为方便观看先去掉预编译
#if MAC_PID == CURRENT_PID 

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
struct GOToRemoteRSP : hsm::Event<GOToRemoteRSP>{};


//定义各个状态
struct Top;
struct Idle;
struct RTS;
struct XMIT;//数据发送阶段
struct BackOff;//主动退避阶段
struct Remote;//载波监听退避阶段
struct WaitRsp;//等待物理层回复阶段
struct WaitCTS;
struct RemoteWaitRsp;
//fama-ntr协议头部
struct famaheader{
        uint8_t type;  //类型，1RTS 2CTS 3DATA
        uint8_t src;   //源地址
        uint8_t dst;   //目的地址
        uint16_t serialNum;//包序号
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
    srand((int)time(NULL));//初始化随机数种子，这个应该放在程序到循环外，只调用一次
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

    void SetRemoteTime1(const Ptr<MsgSendDataRsp> &m);//收到发出CTS到回复，设置退避时间并返回remote状态
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
    
    void evstartINremote(const Ptr<EvStartRecvData> &m);//在RemoteWaitRsp里面收到开始接收事件
    void SetRTSTime(const Ptr<MsgSendDataRsp> &m);//设置RTS定时器，防止各个状态机都卡死在RTS状态
    void CancelRTSTime(const Ptr<EvStartRecvData> &m);
    void SetBackOfftime3(const Ptr<MsgTimeOut> &m);
    void havedatatosend(const Ptr<MsgSendDataRsp> &m);//XMIT状态发送完data后看是否由数据要发送进行转移
private:
    queue<Ptr<MsgSendDataReq>> Msgqueue;//用于临时存放数据到数据队列，这里存放到指针类型还需要修改
    bool listening=false;//判断是否仍处于接收数据阶段
    uint8_t selfMacId;
    double maxdistance;//最大传输距离
    double maxtransittime;//最大传输时延=最大传输距离距离/1500m/s
    int lastbackofftimeid;//上一次设置的backoff定时器的MsgID
    timer_id_type timeid;
    int strartrecvtimes;//用于仿真信道中到载波监听次数统计，以对是否退出remote状态做出正确判断
    //在实地水池测试中不会出现一个数据没接收完又监听到新载波了，所以这个参数后面实际上是多余了
    uint16_t DataSerialNum=1;//包序号，每成功发出一个DATA包，包序号加一
    timer_id_type RTStimeid;
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
	HSM_WORK(MsgSendDataReq,&fama_ntr::PushQueue);
    HSM_TRANSIT(MsgSendDataRsp,RTS,&fama_ntr::SetRTSTime);//收到PHY层的回复，转移到RTS状态
    HSM_TRANSIT(EvStartRecvData,Remote,&fama_ntr::SetRemoteTime);//监听到载波，转移到Remote状态并设置退避定时器
};
struct RTS : hsm::State<RTS, Top>{
	typedef hsm_vector<MsgSendDataReq,EvStartRecvData,MsgTimeOut> reactions;
	HSM_WORK(MsgSendDataReq,&fama_ntr::PushQueue);
    HSM_TRANSIT(EvStartRecvData,WaitCTS,&fama_ntr::CancelRTSTime); 
    HSM_TRANSIT(MsgTimeOut,BackOff,&fama_ntr::SetBackOfftime3);
};
struct WaitCTS : hsm::State<WaitCTS, Top>{
	typedef hsm_vector<MsgSendDataReq,EvStartRecvData,MsgRecvDataNtf,RecvCTS,NotCTS> reactions;
    HSM_DISCARD(EvStartRecvData);
    HSM_WORK(MsgRecvDataNtf, &fama_ntr::IsCTS);
    HSM_TRANSIT(RecvCTS,XMIT,&fama_ntr::SendData);
    HSM_TRANSIT(NotCTS,BackOff,&fama_ntr::SetBackOffTime1);
    HSM_WORK(MsgSendDataReq,&fama_ntr::PushQueue);
};
struct XMIT : hsm::State<XMIT, Top>{
	typedef hsm_vector<MsgSendDataRsp,MsgSendDataReq,GoToIdle,GoToBackOff> reactions;
    HSM_WORK(MsgSendDataRsp,&fama_ntr::havedatatosend);//收到PHY层的回复，数据发送完毕，转移到Idle状态
    HSM_WORK(MsgSendDataReq,&fama_ntr::PushQueue);
    HSM_TRANSIT(GoToIdle,Idle);
    HSM_TRANSIT(GoToBackOff,BackOff,&fama_ntr::SetBackOfftime2)
};
struct Remote : hsm::State<Remote, Top>{
	typedef hsm_vector<MsgSendDataReq,EvStartRecvData,MsgRecvDataNtf,MsgTimeOut,GoToIdle,GoToBackOff,GOToRemoteRSP> reactions;
	HSM_WORK(MsgTimeOut,&fama_ntr::IsHaveDataNowOrListening);//超时后调用IsHaveDataNow生成转移事件，如果仍处于接收状态则重新设置接收定时器
    HSM_WORK(MsgRecvDataNtf,&fama_ntr::HandleData);//收到数据，处理数据，Listen位置否，如果发出RTS则需要转移到RemoteWaitRsp
    HSM_WORK(MsgSendDataReq,&fama_ntr::PushQueue);//在该状态延时处理数据发送请求
    HSM_WORK(EvStartRecvData,&fama_ntr::SetListen);//防止在Remote里面接受到数据后又重新接受到数据到情况，在水池里实地测试到话一般不会出现这种情况
    HSM_TRANSIT(GoToBackOff,BackOff,&fama_ntr::SetBackOfftime2);
    HSM_TRANSIT(GoToIdle,Idle);
    HSM_TRANSIT(GOToRemoteRSP,RemoteWaitRsp);
};
struct BackOff : hsm::State<BackOff, Top>{
	typedef hsm_vector<MsgSendDataReq,MsgTimeOut,EvStartRecvData,BackOffTimeOut> reactions;
    HSM_WORK(MsgSendDataReq,&fama_ntr::PushQueue);
    HSM_WORK(MsgTimeOut,&fama_ntr::IsLastBackOffTimeOut);
    HSM_TRANSIT(BackOffTimeOut,WaitRsp,&fama_ntr::SendRTS);
    HSM_TRANSIT(EvStartRecvData,Remote,&fama_ntr::SetRemoteTime);
};

struct RemoteWaitRsp : hsm::State<RemoteWaitRsp, Top>{
    typedef hsm_vector<MsgSendDataRsp,EvStartRecvData,MsgSendDataReq> reactions;
    HSM_TRANSIT(MsgSendDataRsp,Remote,&fama_ntr::SetRemoteTime1);
    HSM_TRANSIT(EvStartRecvData,Remote,&fama_ntr::evstartINremote);
    HSM_WORK(MsgSendDataReq,&fama_ntr::PushQueue);
};
void fama_ntr::SendRtsandPushQueue(const Ptr<MsgSendDataReq> &m)
{
    if(Msgqueue.size()==30)//如果已经有二十个数据来，则删除队首过于陈旧的数据
    {
        Msgqueue.pop();//缓存栈溢出
        LOG(INFO)<<"缓存栈溢出";
        //删除到会是最先到数据包，所以包序号要加一
        DataSerialNum++;
    }
    Msgqueue.push(m);//先将待发数据推入栈中
    LOG(INFO)<<"队列长度为"<<Msgqueue.size();
    auto msg=Msgqueue.front();
    //-------------------生成RTS包-----------------------------------
    Packet pkt(125);//申请一段新内存，大小为header+parameter(此处为0)=header
	famaheader *h = pkt.Header<famaheader>();
	h->type = PACKET_TYPE_RTS;
	h->dst = (uint8_t)msg->address;//目的地址
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
	LOG(INFO)<<"发送RTS并将数据放入队列中";
    //向网络层发送回复
    Ptr<MsgSendDataRsp> rsp(new MsgSendDataRsp);
    SendRsp(rsp,NET_LAYER,NET_PID);
};
void fama_ntr::IsCTS(const Ptr<MsgRecvDataNtf> &m)
{
    famaheader *header = m->packet.Header<famaheader>();//得到MAC层包头
    LOG(INFO)<<"reservation"<<m->reservation;
    LOG(INFO)<<"类型"<<header->type;
    LOG(INFO)<<"收到数据的源地址"<<header->src;
    LOG(INFO)<<"收到数据的目的地址"<<header->dst;
    LOG(INFO)<<"收到数据的序号"<<header->serialNum;
    if(header->type==PACKET_TYPE_CTS && m->reservation==0)//是CTS且数据没有错
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
            LOG(INFO)<<"Receive NotCTS1";
            Ptr<NotCTS> p(new NotCTS);
            SendNtf(p,MAC_LAYER,MAC_PID);
        }
    }
    else//否则生成NotCTS事件，执行退避操作,这个地方出现了问题
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
    h->dst=(uint8_t)msgntf->address;
    h->type=PACKET_TYPE_DATA;;
    h->src=selfMacId;
    h->serialNum=DataSerialNum;
    DataSerialNum++;//发送完一个之后所有数据都属于下一个序号
    msgntf->packet.Move<famaheader>();
    SendReq(msgntf,PHY_LAYER, PHY_PID);
    LOG(INFO)<<"发送数据";
    //------------------------------------------------------------------------
    string logStr = Trace::Instance().Log(MAC_LAYER, MAC_PID, msgntf->packet, -1, -1, -1, -1, -1, (int)h->src, (int)h->dst,-1, "data", "send");
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
    int backofftime=rand()%12;//产生0-20之间到随机数
    LOG(INFO)<<"退避随机数为"<<backofftime;
    LOG(INFO)<<"退避时长为"<<backofftime*maxtransittime;
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
    Packet pkt(125);//申请一段新内存，大小为header+parameter(此处为0)=header
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
    strartrecvtimes++;
    listening=true;//监听位置一，表示仍处于载波监听阶段
    timeid=SetTimer(3.0,0);//Msgid为0代表是Remote状态设置的定时器,timeid为该定时器绑定到ID
    //从开始接收到处理数据大约只需3Ｓ，对于239bytes长的帧
    LOG(INFO)<<"监听到载波，开始退避";
};
void fama_ntr::SetRemoteTime1(const Ptr<MsgSendDataRsp> &m)
{
    LOG(INFO)<<"监听到回复，返回remote";
    timeid=SetTimer(5.5,0);//Msgid为0代表是Remote状态设置的定时器,timeid为该定时器绑定到ID
    //这里的定时时间＝一次传输时延+通信机接收时间+通信机发送时间+一次传输时延
};
void fama_ntr::evstartINremote(const Ptr<EvStartRecvData> &m)//在RemoteWaitRsp里面收到开始接收事件,如果是在实际测试阶段这种情况应该不会发生
{
    strartrecvtimes++;
    listening=true;
    LOG(INFO)<<"在remotewaitrsp接收到载波,返回remote";
    timeid=SetTimer(2*maxtransittime,0);
}
void fama_ntr::HandleData(const Ptr<MsgRecvDataNtf> &m)
{  
    CancelTimer(timeid);//取消原先到Remote时钟
    if(strartrecvtimes>0)
    {
        strartrecvtimes--;
    }
    if(strartrecvtimes==0)
    {
        listening=false;//接收完数据，本次监听结束
    }
    LOG(INFO)<<"reservation为"<<m->reservation;
    famaheader *header = m->packet.Header<famaheader>();//得到MAC层包头
    LOG(INFO)<<"类型"<<header->type;
    LOG(INFO)<<"收到数据的源地址"<<header->src;
    LOG(INFO)<<"收到数据的目的地址"<<header->dst;
    LOG(INFO)<<"收到数据的包序号"<<header->serialNum;
    if(m->reservation==0)//收到正确数据包，正常处理，若错误则只是重新设置退避定时器
    {
        LOG(INFO)<<"接收端无碰撞";
        if(header->type==PACKET_TYPE_DATA)//收到数据包
        {
            if(header->dst==selfMacId)
            {
                m->packet.Move<famaheader>();//将head_指针移动到网络层头部的位置
                SendNtf(m, NET_LAYER, NET_PID);  //向NET层通知消息（实际上是将事件推进了NTFQUEUE）
                LOG(INFO)<<"recv a data packet and send up";
                timeid=SetTimer(maxtransittime,0);//如果是接收到数据，说明信道要开始空闲了，只退避一小段时间
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
            }
            else
            {
                timeid=SetTimer(maxtransittime,0);             
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
                Packet pkt(50);//申请一段新内存，大小为header+parameter(此处为0)=header
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
                Ptr<GOToRemoteRSP> gorsp(new GOToRemoteRSP);
                GetHsm().ProcessEvent(gorsp);
                LOG(INFO)<<"转移";
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
                return;
            }
            else//收到非本节点的ＲＴＳ
            {
                timeid=SetTimer(5.5,0);//重新退避
                return;
            }
            
        }
    }
    else//接收到错误数据
    {
        LOG(INFO)<<"接收端出现数据碰撞";
        timeid=SetTimer(5.5,0);//重新退避,这个时间设置到比较长是因为你接收错误别人不一定会接收错误
        return;
    }
    
    
};
void fama_ntr::IsHaveDataNowOrListening(const Ptr<MsgTimeOut> &m)
{
    if(m->msgId==0)//是否是Remote状态的定时器超时
    {
        if (listening==true)//仍处于载波监听阶段，重新设置定时器
        {
            timeid=SetTimer(2*maxtransittime,0);//最新设置Remote定时器的ID
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
    if(Msgqueue.size()==30)//如果已经有二十个数据来，则删除队首过于陈旧的数据
    {
        Msgqueue.pop();
        LOG(INFO)<<"缓存栈溢出";
        //删除到会是最先到数据包，所以包序号要加一
        DataSerialNum++;
    }
    Msgqueue.push(m);
    LOG(INFO)<<"将数据发送请求入栈";
    LOG(INFO)<<"队列长度为"<<Msgqueue.size();
    Ptr<MsgSendDataRsp> p(new MsgSendDataRsp);
    SendRsp(p,NET_LAYER,NET_PID);
};
void fama_ntr::SetListen(const Ptr<EvStartRecvData> &m)//将监听位置true 
{
    listening=true;
}
void fama_ntr::SetBackOfftime2(const Ptr<GoToBackOff> &m)
{
    LOG(INFO)<<"Set backoff time,转移到backoff";
    //生成随机数种子
    //取得随机数
    int backofftime=rand()%12;//产生0-20之间到随机数
    LOG(INFO)<<"退避随机数为"<<backofftime;
    LOG(INFO)<<"退避时长为"<<backofftime*maxtransittime;
    if(lastbackofftimeid==100)//重置退避定时器到ID
    {
        lastbackofftimeid=0;
        LOG(INFO)<<"重置backoff定时器ID";
    }
    SetTimer(backofftime * maxtransittime, ++lastbackofftimeid); //第二个参数msgId为0代表的是Remote状态的定时器超时，否则是该状态的定时器超时
}
void fama_ntr::SetBackOfftime3(const Ptr<MsgTimeOut> &m)
{
    LOG(INFO)<<"Set backoff time,由RTS到backoff";
    //取得随机数
    int backofftime=rand()%12;//产生0-20之间到随机数
    LOG(INFO)<<"退避随机数为"<<backofftime;
    LOG(INFO)<<"退避时长为"<<backofftime*maxtransittime;
    if(lastbackofftimeid==100)//重置退避定时器到ID
    {
        lastbackofftimeid=0;
        LOG(INFO)<<"重置backoff定时器ID";
    }
    SetTimer(backofftime * maxtransittime, ++lastbackofftimeid);
}
void fama_ntr::SetRTSTime(const Ptr<MsgSendDataRsp> &m)//设置RTS定时器，防止各个状态机因为接收不到发来的新数据都卡死在RTS状态
{
    RTStimeid=SetTimer(3.5,0);//重新退避，这里的等待时间比2传输时延加一个发送时延稍大些

    LOG(INFO)<<"进入RTS并设置等待定时器";
};
void fama_ntr::CancelRTSTime(const Ptr<EvStartRecvData> &m)
{
    CancelTimer(RTStimeid);
    LOG(INFO)<<"取消RTS定时器并转移到WAITCTS";
};
void fama_ntr::havedatatosend(const Ptr<MsgSendDataRsp> &m)//XMIT状态发送完data后看是否由数据要发送进行转移
{
    if(Msgqueue.empty())//如果没有数据待发送，则转移到Idle状态
    {
        LOG(INFO)<<"无待发送数据，转移到IDLE状态";
        Ptr<GoToIdle> p(new GoToIdle);
        GetHsm().ProcessEvent(p);
        return;
    }
    else
    {
        LOG(INFO)<<"有数据，XMIT转移到backoff";
        Ptr<GoToBackOff> p(new GoToBackOff);
        GetHsm().ProcessEvent(p);
        return;
    }    
};
MODULE_INIT(fama_ntr);
PROTOCOL_HEADER(famaheader);
#endif
};
