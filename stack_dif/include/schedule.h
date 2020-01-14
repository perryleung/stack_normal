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
        //压入ReadQueue队列的是IOData事件
				//包括套接字描述符、指针和长度
				//因此只能通过套接字描述符去找对应的协议层
				/*
				调度器会检测全局的读事件是否为空然后调用处理函数
				此时是Dap而非Sap，因为全局的读事件队列都是通信机给物理层发消息用的
				然后调用物理层协议的处理消息函数
				DapMap中的映射是从套接字描述符到Dap的映射
				*/
        dap::DapBase* p = dap::DapMap::Find(d->fd);

        LOG(TRACE) << "receive Data from FD: " << d->fd;

        p->Notify();
        //例如有定义在QPSKclient.cc
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
