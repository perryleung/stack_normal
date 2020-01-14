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

    inline void SetTimer(double time, int id, uint16_t msgId)
    {
        ev_timer_init(&timer_, TimeOutCB, time, 0.); // repeat为0,自动停止的
        ev_timer_start(LibevTool::Instance().GetLoop(), &timer_);

        id_ = id;
        msgId_ = msgId;

        TimerMap::Register(&timer_, this);
    }

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

    inline void TimeOut(int id, uint16_t msgId) { GetOwner()->TimeOut(id, msgId); }
private:
    inline owner_type* GetOwner() const { return owner_; }
private:
    owner_type* owner_;
};

}  // end namespace timer

#endif  // _TIMER_H_
