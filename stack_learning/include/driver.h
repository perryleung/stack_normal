#ifndef _DRIVER_H_
#define _DRIVER_H_

#include <map>
#include "dap.h"
#include "module.h"

#define DRIVER_CONSTRUCT(state)                                                \
		//每个协议层都有Sap，而物理层多一个Dap
		//每个接口类都有一个私有类是协议类的指针
		//Sap或Dap通过这个指针访问协议层的事件处理函数
    do {                                                                       \
        MODULE_CONSTRUCT(state);                                               \
        GetDap().SetOwner(this);                                               \
    } while (0)

namespace drv {

/*
 * This is the base class of driver,
 * hold an DAP to wait the input data notify
 */
template <typename SelfType, int Layer, int Pid>
class Driver : public mod::Module<SelfType, Layer, Pid>
{
    //继承了自Module的类，本质上也是协议类的基类，其中主要多了Notify()函数
	//Notify()函数是提供来处理通信机发来的数据(ReadQueue)，另外多了一个Dap对象
public:
    typedef dap::Dap<SelfType> dap_type;

    inline void Notify() {}
    inline dap_type& GetDap() { return dap_; }
private:
    dap_type dap_;
};

}  // end namespace drv

#endif  // _DRIVER_H_
