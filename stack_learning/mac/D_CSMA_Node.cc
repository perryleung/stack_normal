//D_CSMA Node节点，除在测试传输时延外只用做发送用
//需要在测试模式中将目的地址全部改为ＳＩＮＫ节点的地址
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
#include <sys/time.h>
#define CURRENT_PID 77 //

#define PACKET_TYPE_TESTDATA 1 //测试传播时延的数据
#define PACKET_TYPE_DATA 2     //普通数据
extern client *UIClient;       //暂时不用
extern client *TraceClient;
namespace DCSMANODE
{
#if MAC_PID == CURRENT_PID 
//定义各个事件
using msg::MsgRecvDataNtf;  //对接收到的数据要判断类型，对错
using msg::MsgSendDataReq;
using msg::MsgSendDataRsp;
using msg::MsgTimeOut;
using pkt::Packet;
using msg::EvStartRecvData; //PHY层发来到用于载波监听到的事件
//自定义事件
struct RecvOrdinData : hsm::Event<RecvOrdinData>{}; //判断数据类型的事件
struct RecvSetData : hsm::Event<RecvSetData>{}; //判断数据类型的事件

//定义各个状态
struct Top;
struct Idle;
struct WaitSendDataRsp;
struct Remote;
struct Receive;
struct WaitTWOTest;
struct TWOTest;
//DCSMA协议头部
struct DCSMA_SINKheader
    {
        uint8_t type;       //包类型
        uint8_t src;        //源地址
        uint8_t dst;        //目的地址
        uint16_t serialNum; //包序号
        uint8_t second;     //发送节点发送包的时间（秒数），仅在测试阶段有用
        uint16_t usecond;   //发送节点发送包的时间（毫秒数），仅在测试阶段有用
                            //如果接收时到秒数<发送到秒数 那么接受分钟需要+60
                            //这样做到前提是信道中传输的最大时延小于60Ｓ
    };
class DCSMA_NODE : public mod::Module<DCSMA_NODE, MAC_LAYER, CURRENT_PID>
{
    public:
        DCSMA_NODE() { MODULE_CONSTRUCT(Top); } //初始化

        void Init()
        {
            selfMacId = uint8_t(Config::Instance()["D_CSMA"]["MacId"].as<int>());
            CW = (Config::Instance()["D_CSMA"]["CW"].as<int>());
            maxtranstime = (Config::Instance()["D_CSMA"]["maxtranstime"].as<double>());
            freetime = 0;
            RecvTestTimes = 0;
            FirstTestTime = 0;
            SecondTestTime = 0;
            DataSerialNum = 1;
            srand((int)time(NULL));//初始化随机数种子，这个应该放在程序到循环外，只调用一次
            LOG(INFO) << "DCSMA_NODE Init";
        }
        /**
        *@brief 开始发送数据并给上层回复
        **/
        void SendDataANDSendRsp(const Ptr<MsgSendDataReq> &m);

        /**
         *@brief 信道忙碌时有数据要发送，为该数据设置退避定时器 
        **/
        void SetCWTimer(const Ptr<MsgSendDataReq> &m);

        /**
         *@brief 监听到载波，接收到数据的时间,并记录下信道结束空闲状态的时间（freeend） 
        **/
        void WriteTime(const Ptr<EvStartRecvData> &m);

        /**
        *@brief  接收到完整数据，判断数据类型，进而产生是普通数据还是测试时延数据的事件＼
        如果是测试数据则记录下第一次的传播时延并生成ＲecvSetData事件＼
        如果是普通数据则上传并记录下该次接受完成到时间，用于计算信道空闲时间
        **/
        void WhichData(const Ptr<MsgRecvDataNtf> &m);

        /**
        *@brief 第二次传播时延　两次取平均，并取消SetWaitTimer中设置的定时器
        **/
        void CaculateBrocastTime(const Ptr<MsgRecvDataNtf> &m);
        /**
        *@brief  在等待第二个传输数据时设置一个定时器
        **/
        void SetWaitTimer(const Ptr<RecvSetData> &m);

        /**
        *@brief  记录第二次测试数据到达到时间,并取消当前定时器
        **/
        void WriteTimeTWO(const Ptr<EvStartRecvData> &m);

        /**
        *@brief  收到数据发送完毕的回复，设置信道开始空闲的时间
        **/
        void WriteFreeStartTime(const Ptr<MsgSendDataRsp> &m);

        /**
         *@brief 定时器到时，重新请求传送数据
        **/
       void SendDataReq(const Ptr<MsgTimeOut> &m);
       
    private:
        int CW; //窗口值
        timeval freeStart, freeEnd;
        double freetime;//信道空闲时间
        timeval testtime;//用于记录测试时延到结构
        uint8_t RecvTestTimes;//接受到测试数据的次数
        double FirstTestTime;//收到第一个数的到传输时延
        double SecondTestTime;//收到第二个数据的传输时延
        double averagetime;//两次接受时延取平均
        double maxtranstime;//最大传输时延
        uint8_t selfMacId;
        uint16_t DataSerialNum;//数据包序号
        timer_id_type timeid;//上一次设置到定时器的ＩＤ，用于取消定时器
        Ptr<MsgSendDataReq> storedata;//暂存到待发送的数据
};
    struct Top : hsm::State<Top, hsm::StateMachine, Idle>
    {
        typedef hsm_vector<MsgSendDataReq,MsgTimeOut> reactions;
        HSM_DEFER(MsgSendDataReq);
        HSM_WORK(MsgTimeOut,&DCSMA_NODE::SendDataReq)
    };

    struct Idle : hsm::State<Idle, Top>
    {
        typedef hsm_vector<MsgSendDataReq,EvStartRecvData> reactions;
        HSM_TRANSIT(MsgSendDataReq, WaitSendDataRsp, &DCSMA_NODE::SendDataANDSendRsp);
        HSM_TRANSIT(EvStartRecvData, Remote, &DCSMA_NODE::WriteTime);
    };

    struct WaitSendDataRsp : hsm::State<WaitSendDataRsp, Top>
    {
        typedef hsm_vector<MsgSendDataReq, MsgSendDataRsp> reactions;
        HSM_WORK(MsgSendDataReq, &DCSMA_NODE::SetCWTimer);
        HSM_TRANSIT(MsgSendDataRsp, Idle,&DCSMA_NODE::WriteFreeStartTime);
    };

    struct Remote : hsm::State<Remote, Top>
    {
        typedef hsm_vector<MsgSendDataReq, MsgRecvDataNtf> reactions;
        HSM_WORK(MsgSendDataReq, &DCSMA_NODE::SetCWTimer);
        HSM_TRANSIT(MsgRecvDataNtf, Receive, &DCSMA_NODE::WhichData);
    };

    struct Receive : hsm::State<Receive, Top>
    {
        typedef hsm_vector<MsgSendDataReq, RecvOrdinData, RecvSetData> reactions;
        HSM_WORK(MsgSendDataReq, &DCSMA_NODE::SetCWTimer);
        HSM_TRANSIT(RecvOrdinData, Idle);
        HSM_TRANSIT(RecvSetData, WaitTWOTest);
    };

    struct WaitTWOTest : hsm::State<WaitTWOTest, Top>
    {
        typedef hsm_vector<MsgTimeOut, EvStartRecvData> reactions;
        HSM_TRANSIT(MsgTimeOut, Idle);
        HSM_TRANSIT(EvStartRecvData, TWOTest, &DCSMA_NODE::WriteTimeTWO);
    };

    struct TWOTest : hsm::State<TWOTest, Top>
    {
        typedef hsm_vector<MsgRecvDataNtf> reactions;
        HSM_TRANSIT(MsgRecvDataNtf, Idle, &DCSMA_NODE::CaculateBrocastTime);
    };

    void DCSMA_NODE::SendDataANDSendRsp(const Ptr<MsgSendDataReq> &m)
    {
        auto h = m->packet.Header<DCSMA_SINKheader>();
        //修改ＭＡＣ层帧头数据
        h->dst = (uint8_t)m->address;
        h->type = PACKET_TYPE_DATA;
        h->src = selfMacId;
        h->serialNum = DataSerialNum++;
        h->second = 0;
        h->usecond = 0;
        m->packet.Move<DCSMA_SINKheader>(); //移动指针位置
        SendReq(m, PHY_LAYER, PHY_PID);     //发送给物理层
        LOG(INFO) << "发送数据";
        //------------------------------------------------------------------------
        string logStr = Trace::Instance().Log(MAC_LAYER, MAC_PID, m->packet, -1, -1, -1, -1, -1, (int)h->src, (int)h->dst, -1, "data", "send");
        if (TraceClient != NULL)
        {
            IODataPtr pkt(new IOData);
            pkt->fd = TraceClient->_socketfd;
            char *sendBuff = new char[logStr.length()];
            memcpy(sendBuff, logStr.data(), logStr.length());
            pkt->data = sendBuff;
            pkt->len = logStr.length();
            TraceWriteQueue::Push(pkt);
            cliwrite(TraceClient);
        }
        //----------------------------------------------------------------------
        Ptr<MsgSendDataRsp> rsp(new MsgSendDataRsp);
        SendRsp(rsp, NET_LAYER, NET_PID);
        LOG(INFO) << "发送回复给网络层";
    };
    void DCSMA_NODE::SetCWTimer(const Ptr<MsgSendDataReq> &m){
        storedata=m;
        freetime=(freeEnd.tv_sec-freeStart.tv_sec)+(freeEnd.tv_usec-freeStart.tv_usec)/1000000.0;
        LOG(INFO)<<"空闲时间为"<<freetime;
/*
        if (freetime<0.3)//规定ＣＷ的最大值为20
        {
            CW=20;
        }
        else
        {
            CW=ceil(2+1.25*(5.5+averagetime)/freetime);
        }
*/
        CW=12;
        LOG(INFO)<<"退避窗口数更新为"<<CW;
        int time=rand()%CW;
        SetTimer(time*maxtranstime);
        LOG(INFO)<<"信道忙，退避窗口为"<<time;
    };
    void DCSMA_NODE::WriteTime(const Ptr<EvStartRecvData> &m)
    {
        gettimeofday(&testtime, NULL); //得到数据开始接受到的时间
        freeEnd=testtime;
        LOG(INFO) << "监听到数据";
    }
    void DCSMA_NODE::WhichData(const Ptr<MsgRecvDataNtf> &m) //一般情况下只发不收，所以在这不用记录trace
    {
        gettimeofday(&freeStart,NULL);//接收完一个数据也是信道空闲的开始
        auto h = m->packet.Header<DCSMA_SINKheader>();
        if (h->type == PACKET_TYPE_TESTDATA) //是用于测试时延到广播包则转移到ＷAITTWOTEST等待第二个测试包到到来
        {

            uint8_t startsec=testtime.tv_sec%60;
            uint16_t startusec=testtime.tv_usec/1000;
            if (startsec<h->second)//说明是到了下一分钟，当前秒数要加60再计算
            {
               FirstTestTime=startsec*1.0+60.0+startusec/1000.0-(h->second*1.0+h->usecond/1000.0);
            }
            else
            {
                FirstTestTime=startsec*1.0+startusec/1000.0-(h->second*1.0+h->usecond/1000.0);
            }
            Ptr<RecvSetData> msg(new RecvSetData);
            GetHsm().ProcessEvent(msg);
            LOG(INFO) << "收到测试时延数据";
        }
        else
        {
            if (h->dst==selfMacId)//一般不会出现这种情况，因为是多发一收，节点一般只做发送工作
            {
                m->packet.Move<DCSMA_SINKheader>();
                SendNtf(m, NET_LAYER, NET_PID);  //向NET层通知消息（实际上是将事件推进了NTFQUEUE）
                LOG(INFO)<<"recv a data packet and send up";  
            }
            Ptr<RecvOrdinData> msg(new RecvOrdinData);
            GetHsm().ProcessEvent(msg);
            LOG(INFO) << "收到普通数据";

        }
    };
    void DCSMA_NODE::SetWaitTimer(const Ptr<RecvSetData> &m)
    {
        timeid=SetTimer(15);
    };
    void DCSMA_NODE::WriteTimeTWO(const Ptr<EvStartRecvData> &m)
    {   
        gettimeofday(&testtime, NULL); //得到数据开始接受到的时间
        LOG(INFO) << "监听到数据";
        CancelTimer(timeid);
    }
    void DCSMA_NODE::CaculateBrocastTime(const Ptr<MsgRecvDataNtf> &m)
    {
        auto h = m->packet.Header<DCSMA_SINKheader>();
         if (h->type == PACKET_TYPE_TESTDATA) //是用于测试时延到广播包则转移到ＷAITTWOTEST等待第二个测试包到到来
        {

            uint8_t startsec=testtime.tv_sec%60;
            uint16_t startusec=testtime.tv_usec/1000;
            if (startsec<h->second)//说明是到了下一分钟，当前秒数要加60再计算
            {
               SecondTestTime=startsec*1.0+60.0+startusec/1000.0-(h->second*1.0+h->usecond/1000.0);
            }
            else
            {
                SecondTestTime=startsec*1.0+startusec/1000.0-(h->second*1.0+h->usecond/1000.0);
            }
            LOG(INFO) << "第二次收到测试时延数据";
            averagetime=(FirstTestTime+SecondTestTime)/2;
        }
    }
    void DCSMA_NODE::WriteFreeStartTime(const Ptr<MsgSendDataRsp> &m)
    {
        gettimeofday(&freeStart,NULL);
        LOG(INFO)<<"数据发送完毕，记录信道空闲时间";
    }
    void DCSMA_NODE::SendDataReq(const Ptr<MsgTimeOut> &m)
    {
        SendNtf(storedata,MAC_LAYER,MAC_PID);
    }
MODULE_INIT(DCSMA_NODE);
PROTOCOL_HEADER(DCSMA_SINKheader);
#endif
}; // namespace DCSMANODE