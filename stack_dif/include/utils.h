#ifndef _UTILS_H_
#define _UTILS_H_

#include <memory>
#include <queue>

#define ELPP_DISABLE_TRACE_LOGS
#define ELPP_DISABLE_DEFAULT_CRASH_HANDLING
#define ELPP_STL_LOGGING
#include "easylogging++.h"

#include "message.h"
#include "singleton.h"
#include "config.h"

template <typename T>
using Ptr = std::shared_ptr<T>;//声明为智能的指针，共享对象

struct IOData
{
	//IOData事件包含了数据和文件描述符fd，调度器通过fd找到Dap，从而通知该协议收到了数据
    int fd;
    char* data;
    size_t len;
};

typedef Ptr<IOData> IODataPtr;//进入WriteQueue和ReadQueue事件的是IODataPtr类型
//其他队列压入的是MsgReq等三种事件
//声明IODataPtr是一个指向IOData的智能指针

template <typename SelfType, typename DataType>
class QueueBase : public utils::Singleton<SelfType>
{
public:
    typedef SelfType class_type;
    typedef DataType data_type;
    typedef std::queue<data_type> queue_type;

    //静态函数可以在类的外面使用等等功能 QueueBase::Empty()
    static inline data_type& Front() { return GetQueue().front(); }
    static inline data_type& Back() { return GetQueue().back(); }
    static inline bool Empty() { return GetQueue().empty(); }
    static inline size_t Size() { return GetQueue().size(); }
    static inline void Push(const data_type& v)
    {
        LOG(TRACE) << "Push data to Queue";
        return GetQueue().push(v);
    }

    static inline void Pop()
    {
        LOG(TRACE) << "Pop data from Queue";
        return GetQueue().pop();
    }

private:
    static inline queue_type& GetQueue()
    {
        return class_type::Instance().queue_;
    }

private:
    queue_type queue_;
};

//WriteQueue和ReadQueue是提供来与通信机通信的
class ReadQueue : public QueueBase<ReadQueue, IODataPtr>
{
};
class WriteQueue : public QueueBase<WriteQueue, IODataPtr>
{//目前调度器并没有对WriteQueue进行检测，需要自己另外处理这个队列
};
class UIWriteQueue : public QueueBase<UIWriteQueue, IODataPtr>
{
};
class TraceWriteQueue : public QueueBase<TraceWriteQueue, IODataPtr>
{
};
class ReqQueue : public QueueBase<ReqQueue, msg::MsgReqPtr>
{
};
class RspQueue : public QueueBase<RspQueue, msg::MsgRspPtr>
{
};
class NtfQueue : public QueueBase<NtfQueue, msg::MsgNtfPtr>
{
};



#endif  // _UTILS_H_
