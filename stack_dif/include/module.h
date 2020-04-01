#ifndef _MODULE_H_
#define _MODULE_H_

#include <set>
#include "hsm.h"
#include "sap.h"
#include "timer.h"
#include "utils.h"
#include "../moduleconfig.h"

#define MODULE_INIT(type) static type Module_##type;

#define MODULE_CONSTRUCT(state)                                                \
		
		//需要向某个协议层发事件时，都是通过dap或sap，因为dap和sap是接口
		//每个协议的构造函数里都会执行这个宏定义把顶层状态传入
		//物理层较Sap外还多了一个Dap在driver
		
    do {                                                                       \
        GetSap().SetOwner(this);                                               \
        GetTap().SetOwner(this);                                               \
        GetHsm().Initialize<state>(this);                                      \
    } while (0)

namespace mod {

/*
 * This is the base class of all the module,
 * hold a Hsm to implete Statechar,
 * SAP was used to connect with other module
 */
template <typename SelfType, int Layer, int Pid>
class Module
{
	// 每个下协议在继承的时候先传入模板参数，然后构造出类模板
public:
    typedef SelfType                         class_type;
    typedef hsm::StateMachine                hsm_type;
    typedef timer::Tap<class_type>           tap_type;
    typedef sap::Sap<class_type, Layer, Pid> sap_type;
    //创建模板类对象时会自动生成一个相应模板的Sap在对象内部（在私有成员，并非这里的public）
    typedef int                              timer_id_type;
    typedef std::set<timer_id_type>          timer_set_type;

    Module() : timer_count_(0) {}
    ~Module() {}
    inline sap_type &GetSap() { return sap_; }
    inline hsm_type &GetHsm() { return hsm_; }
    inline tap_type &GetTap() { return tap_; }
    inline unsigned int GetLastReqLayer() const { return last_req_layer_; }
    inline unsigned int GetLastReqPid() const { return last_req_pid_; }
    inline void SetReqSrc(unsigned int layer, unsigned int pid)
    {
        last_req_layer_ = layer;
        last_req_pid_   = pid;
    }

    inline void Init() {}
    inline void SendReq(const MessagePtr &msg, unsigned layer, unsigned pid)
    {
        GetSap().SendReq(msg, layer, pid);
    }

    inline void SendRsp(const MessagePtr &msg, unsigned layer, unsigned pid)
    {
        GetSap().SendRsp(msg, layer, pid);
    }

    inline void SendNtf(const MessagePtr &msg, unsigned layer, unsigned pid)
    {
        GetSap().SendNtf(msg, layer, pid);
    }

    inline timer_id_type SetTimer(double time, uint16_t msgId = 0) // 发布订阅模式，订阅Time事件
    {
        GetTap().SetTimer(time, ++timer_count_, msgId);
        timer_set_.insert(timer_count_);

        return timer_count_;
    }

    inline void CancelTimer(timer_id_type id)
    {
        auto item = timer_set_.find(id);
        timer_set_.erase(item);
    }

    // 一个协议有多个定时器event的话，通过id来寻找目标状态,Timer通过回调这个函数来向指定消费对象发布timer信息。而TimerMap则可以决定去往不同协议的定时信息，无需特意寻找目标协议.每个协议有自己的Tap，所以不需要担心不同协议会共同收到一个相同的定时器时间
    inline void TimeOut(timer_id_type id, uint16_t msgId)
    {
        auto item = timer_set_.find(id);

        if (item != timer_set_.end()) {
            timer_set_.erase(item);
            // 不过好像假如有多个定时事件的话也没有区别对待，应该是一个协议一时刻只有一个定时事件
            msg::MsgTimeOutPtr msg(new msg::MsgTimeOut);
            // 通过加上定时器ID来区别不同的定时器时间
            msg->msgId = msgId;
            GetHsm().ProcessEvent(msg);
        }
    }

private:
    Module(const Module &);     // 禁止拷贝操作
    Module &operator=(const Module &); // 同上

private:
    friend sap_type;            // 友元类，可访问私有成员
    friend tap_type;

    sap_type sap_;
    tap_type tap_;
    hsm_type hsm_;

    timer_id_type timer_count_;
    timer_set_type timer_set_;

    unsigned int last_req_layer_;
    unsigned int last_req_pid_;
};

}  // end namespace mod

#endif
