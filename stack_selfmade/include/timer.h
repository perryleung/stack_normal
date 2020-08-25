#ifndef _TIMER_H_
#define _TIMER_H_

#include <cassert>
#include <map>
#include <tr1/memory>
// #include "event.h"
#include <ev.h>
#include "libev_tool.h"

namespace timer {

class TapBase
{
public:
    // when data income, scheduler Notify the correspond
    // DAP to handle it
    // virtual void TimeOut() const = 0;
};

class TimerBase
{
public:
    virtual void SetOwner(TapBase*){};
    virtual void SetTimer(double, int, uint16_t) = 0;
    virtual void TimerOut() = 0;
};

class TimerMap
{
public:
    typedef TimerMap class_type;
    typedef ev_timer* index_type;
    typedef TimerBase* value_type;
    typedef std::map<index_type, value_type> map_type;
    typedef map_type::iterator map_iter_type;

    static inline class_type& Instance()
    {
        static class_type instance_;
        return instance_;
    }
//TimerMap中的注册函数：（事件和Timer对象的映射）
    static inline void Register(index_type index, value_type value)
    {
        GetMap()[index] = value;
    }

    static inline value_type Find(index_type index) { return GetMap()[index]; }
    static inline bool Empty() { return GetMap().empty(); }
private:
    TimerMap() {}
    ~TimerMap() {}
    TimerMap(const TimerMap&);
    TimerMap& operator=(const TimerMap&);

    static inline map_type& GetMap() { return class_type::Instance().map_; }
private:
    map_type map_;
};
//回调函数是在类外的，当定时器事件触发，就会在TimerMap中找到该Timer对象，然后调用其中的超时函数。（通过在TimerMap中注册的事件，找到Timer对象，然后通过Timer对象找到Tap对象，再找到Owner，最后把MsgTImeOut事件压入该协议的事件队列中。）
static void TimeOutCB(EV_P_ ev_timer *w, int revents)
{
    TimerBase* p = TimerMap::Find(w);

    p->TimerOut();

    //return 0;
}

template <typename T>
class Timer : TimerBase
{
public:
    typedef T tap_type;

    inline void SetOwner(tap_type* tap)
    {
        assert(tap != NULL);

        tap_ = tap;
    }

//在Timer类中，调用函数，把定时器事件插入循环中，回调函数是TimeOutCB。同时，向TimerMap注册，事实上是一个在TimerMap里得到该定时器事件和该timer的一个映射关系。
    inline void SetTimer(double time, int id, uint16_t msgId)
    {
        ev_timer_init(&timer_, TimeOutCB, time, 0.); // repeat为0,自动停止的
        ev_timer_start(LibevTool::Instance().GetLoop(), &timer_);

        id_ = id;
        msgId_ = msgId;

        TimerMap::Register(&timer_, this);
    }

//Timer类中的超时函数：（tap_即Timer类对象的owner，Timer类中的SetOwner函数来设置）
    inline void TimerOut()
    {
        tap_->TimeOut(id_, msgId_);
        // ev_timer_stop(LibevTool::Instance().GetLoop(), &timer_);  repeat为0,自动停止的

        delete this;            // TimeOut之后，这个Timer就可以去掉了,包括其成员变量
    }

private:
    ev_timer timer_;
    int id_;
    tap_type* tap_;
    uint16_t msgId_;
};

// Tap与Timer的关系是一对多的关系,感觉Timer好像没什么作用，可以去掉，直接在Tap接触TimerMap
// 主要是为了封装ev_timer
//然后调用Tap类中的函数。首先，Tap属于传入模板OwenrType，生成了一个Tap模板类；然后，把这个Tap模板类作为Timer的模板参数传入Timer，由此生成了一个“owner”是这个Tap模板类的Timer。得到了这各Timer对象之后，就调用其中SetTimer函数
template <typename OwnerType>
class Tap
{
public:
    typedef Tap<OwnerType> class_type;
    typedef OwnerType owner_type;

    inline void SetOwner(owner_type* owner)
    {
        assert(owner != NULL);

        owner_ = owner;
    }

    inline void SetTimer(double time, int id, uint16_t msgId)
    {
        auto timer = new Timer<class_type>;

        timer->SetOwner(this);
        timer->SetTimer(time, id, msgId);
    }
//超时函数：
    inline void TimeOut(int id, uint16_t msgId) { GetOwner()->TimeOut(id, msgId); }
private:
    inline owner_type* GetOwner() const { return owner_; }
private:
    owner_type* owner_;
};

}  // end namespace timer

#endif  // _TIMER_H_

/*
每个协议对象有一个Tap，需要设置定时器时调用协议中的SetTimer函数。这样就会在Tap生成一个Timer对象。然后，向TimerMap注册这个事件Timer对象的对应关系。
当这个定时器事件发生，就会调用回调函数，在TimerMap中找到这个事件对应的Timer，然后找到Tap，找到Owner，从而找到最终的协议。
timer_count_和msgId，这两个数字是保存在Timer对象中的，因此插到循环中的定时器事件发生时，需要通过TimerMap找到Timer对象，然后再调用超时的函数。（个人认为，Timer的存在就是为了保存这两个数字，定时器事件发生时传给协议，协议才能唯一找到该事件。）
然而协议Module基类中有一个函数CancelTimer，需要根据timer_count_来消除这个定时器事件。事实上协议那边知道的只有msgId，除非协议那边另外保存了这个数字，否则不能通过调用这个函数来取消这个事件。
*/