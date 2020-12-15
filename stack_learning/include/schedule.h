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
    //在Scheduler类中的成员函数，每次循环前检查事件队列是否为空，一直执行到为空为止
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
       /*
        压入ReadQueue中的事件只包括数据和文件描述符，必须要根据文件描述符找到相应的协议层。因此，如果只靠SapMap的话是不够的，需要有一个套接字描述符和协议层的联系。有了DapMap，就可以根据fd，找到相应协议层拥有的Dap对象，然后调用协议层的处理函数。
        （初始化的时候还是会在SapMap里注册这个协议，并且DapMap里也会有这个协议。一般来说，上层通过SapMap找到物理层，套接字收到数据就通过DapMap找到物理层。）
        */
        //在调度器中，通过DapBase指针访问的Notify()是基类DapBase中的函数，因此可以访问(看Sap.h中SapMap类上的备注)
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
//每次处理一个事件，找到Sap接口，然后调用处理函数。（注意，事件队列中的指针都是：一个EventBase指针，源、目标的协议ID和层ID，这样五个数据成员，因此能从队列每个事件中，找到目标协议层）
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
//之后就交给调度器处理，Scheduler类中的处理函数。这里会找到目标协议层的Sap，然后调用该层的Sap处理函数。（这里只需要传入原始事件本身即可）
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
首先全局处理事件的是调度器Scheduler，它负责将各个Sap发布的事件在每个循环进行处理（事实上协议发出的事件，最终也是在Sap中压入队列，然后调度器处理队列中的事件。）
然后是Sap、Dap等和协议类对象的关系。每个协议中都有Sap、Tap	、状态机这几个主要的私有类型（如果是物理层会有Dap）。当调度器调用某层协议的Sap的处理函数时，Sap会先找到它的owner（即协议对象），然后根据owner找到状态机，用状态机中的函数来处理事件。（状态机会找到当前状态，然后调用状态中相应的函数。相应的函数需要在协议类中的状态里写好。）之后执行相应的操作（如转移，执行动作，延迟等）。
*/