#ifndef _SCHEDULE_H_
#define _SCHEDULE_H_

#include <cstdlib>
#include "dap.h"
#include "message.h"
#include "sap.h"
#include "utils.h"
#include "trace.h"
#include <ev.h>

using std::cout;
using std::endl;

namespace sched {

class Scheduler
{
public:
    static Scheduler& Instance()
    {
        static Scheduler the_instance;
        return the_instance;
    }

    //��Scheduler���еĳ�Ա������ÿ��ѭ��ǰ����¼������Ƿ�Ϊ�գ�һֱִ�е�Ϊ��Ϊֹ
    static void Sched(EV_P_ ev_prepare *w, int revents)
    {
        while (!ReadQueue::Empty() || !ReqQueue::Empty() || !RspQueue::Empty() ||
            !NtfQueue::Empty()) 
	    {
		    if (!ReadQueue::Empty()) {
                SchedRead();
            } else if (!RspQueue::Empty()) {
                SchedRsp();
		    } else if (!ReqQueue::Empty()) {
                SchedReq();
		    } else if (!NtfQueue::Empty()) {
                SchedNtf();
		    }
        }
        //Sched();
    }

private:
    static void SchedRead()
    {
        IODataPtr d = ReadQueue::Front();
        //ѹ��ReadQueue���е���IOData�¼�
				//�����׽�����������ָ��ͳ���
				//���ֻ��ͨ���׽���������ȥ�Ҷ�Ӧ��Э���
				/*
				����������ȫ�ֵĶ��¼��Ƿ�Ϊ��Ȼ����ô�����
				��ʱ��Dap����Sap����Ϊȫ�ֵĶ��¼����ж���ͨ�Ż�������㷢��Ϣ�õ�
				Ȼ����������Э��Ĵ�����Ϣ����
				DapMap�е�ӳ���Ǵ��׽�����������Dap��ӳ��
				*/
    /*
    ѹ��ReadQueue�е��¼�ֻ�������ݺ��ļ�������������Ҫ�����ļ��������ҵ���Ӧ��Э��㡣��ˣ����ֻ��SapMap�Ļ��ǲ����ģ���Ҫ��һ���׽�����������Э������ϵ������DapMap���Ϳ��Ը���fd���ҵ���ӦЭ���ӵ�е�Dap����Ȼ�����Э���Ĵ�������
����ʼ����ʱ���ǻ���SapMap��ע�����Э�飬����DapMap��Ҳ�������Э�顣һ����˵���ϲ�ͨ��SapMap�ҵ�����㣬�׽����յ����ݾ�ͨ��DapMap�ҵ�����㡣��
    */
   //�ڵ������У�ͨ��DapBaseָ����ʵ�Notify()�ǻ���DapBase�еĺ�������˿��Է���(��Sap.h��SapMap���ϵı�ע)
        dap::DapBase* p = dap::DapMap::Find(d->fd);

        LOG(TRACE) << "receive Data from FD: " << d->fd;

        p->Notify();
        //�����ж�����QPSKclient.cc
    }

    static void SchedReq()
    {
        msg::MsgReqPtr d = ReqQueue::Front();

        sap::SapBase* p = sap::SapMap::Find(d->DstLayer, d->DstPid);

        LOG(DEBUG) << "Layer: " << d->DstLayer << " Pid: " << d->DstPid
                   << " receive Request from Layer: " << d->SrcLayer
                   << " Pid: " << d->SrcPid;

        p->RecvReq(d->msg, d->SrcLayer, d->SrcPid);

        ReqQueue::Pop();
    }

//ÿ�δ���һ���¼����ҵ�Sap�ӿڣ�Ȼ����ô���������ע�⣬�¼������е�ָ�붼�ǣ�һ��EventBaseָ�룬Դ��Ŀ���Э��ID�Ͳ�ID������������ݳ�Ա������ܴӶ���ÿ���¼��У��ҵ�Ŀ��Э��㣩
    static void SchedRsp()
    {
        msg::MsgRspPtr d = RspQueue::Front();

        sap::SapBase* p = sap::SapMap::Find(d->DstLayer, d->DstPid);

        LOG(DEBUG) << "Layer: " << d->DstLayer << " Pid: " << d->DstPid
                   << " receive Response from Layer: " << d->SrcLayer
                   << " Pid: " << d->SrcPid;

        p->RecvRsp(d->msg);

        RspQueue::Pop();
    }

//֮��ͽ�������������Scheduler���еĴ�������������ҵ�Ŀ��Э����Sap��Ȼ����øò��Sap��������������ֻ��Ҫ����ԭʼ�¼������ɣ�
    static void SchedNtf()
    {
        msg::MsgNtfPtr d = NtfQueue::Front();

        sap::SapBase* p = sap::SapMap::Find(d->DstLayer, d->DstPid);

        LOG(DEBUG) << "Layer: " << d->DstLayer << " Pid: " << d->DstPid
                   << " receive Notification from Layer: " << d->SrcLayer
                   << " Pid: " << d->SrcPid;

        p->RecvNtf(d->msg);

        NtfQueue::Pop();
    }

    Scheduler() {}
    ~Scheduler() {}
};
}

#endif  // _SCHEDULE_H_

/*
����ȫ�ִ����¼����ǵ�����Scheduler�������𽫸���Sap�������¼���ÿ��ѭ�����д�����ʵ��Э�鷢�����¼�������Ҳ����Sap��ѹ����У�Ȼ���������������е��¼�����
Ȼ����Sap��Dap�Ⱥ�Э�������Ĺ�ϵ��ÿ��Э���ж���Sap��Tap	��״̬���⼸����Ҫ��˽�����ͣ��������������Dap����������������ĳ��Э���Sap�Ĵ�����ʱ��Sap�����ҵ�����owner����Э����󣩣�Ȼ�����owner�ҵ�״̬������״̬���еĺ����������¼�����״̬�����ҵ���ǰ״̬��Ȼ�����״̬����Ӧ�ĺ�������Ӧ�ĺ�����Ҫ��Э�����е�״̬��д�á���֮��ִ����Ӧ�Ĳ�������ת�ƣ�ִ�ж������ӳٵȣ���
*/