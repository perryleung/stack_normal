#ifndef _SAP_H_
#define _SAP_H_

#include <cassert>
#include <map>
#include "hsm.h"
#include "message.h"
#include "utils.h"

namespace sap {

/*
 * This is  the interface class for SAP,
 */

class SapBase
{
public:
    virtual void Init() const = 0;

    virtual void SendReq(const MessagePtr&, unsigned, unsigned) = 0;
    virtual void SendRsp(const MessagePtr&, unsigned, unsigned) = 0;
    virtual void SendNtf(const MessagePtr&, unsigned, unsigned) = 0;

    virtual void RecvReq(const MessagePtr&, unsigned int, unsigned int) = 0;
    virtual void RecvRsp(const MessagePtr&) = 0;
    virtual void RecvNtf(const MessagePtr&) = 0;

    virtual void ProcessMsg(const MessagePtr&) = 0;

    virtual unsigned GetLayer() const = 0;
    virtual unsigned GetPid() const   = 0;
};

/*
 * This class map <layer, pid> and SAP instance,
 * the scheduler will find the SAP through this by <layer, pid>
 */
/*
赋值兼容规则永远不变：父类指针既可以指向父类对象，也可以指向子类对象；当父类指针指向父类对象时，访问父类的成员；当父类指针指向子类对象时，那么只能访问子类中从父类继承下来的那部分成员；不能访问子类独有的成员，如果访问，编译阶段会报错；

对于SapMap和DapMap，映射是协议层数字或者文件描述符到基类指针的映射（SapBase和DapBase基类指针）。因此如果通过Map来获得Sap或Dap指针，只能访问从基类那里继承过来的函数。
*/
class SapMap : public utils::Singleton<SapMap>
{
public:
    typedef SapMap class_type;
    typedef std::pair<int, int> index_type;
    typedef SapBase* value_type;
    typedef std::map<index_type, value_type> map_type;
    typedef map_type::iterator map_iter_type;
    ;

    static inline void Register(int layer, int pid, const value_type value) // 91行在调用
    {
        index_type index(layer, pid);
        GetMap()[index] = value;
    }
    
    static inline value_type Find(int layer, int pid)
    {
        index_type index(layer, pid);
        return GetMap()[index];
    }

    static inline int Size(){ return GetMap().size(); }

    static inline bool Empty() { return GetMap().empty(); }
    // invoke Init() function in all module
    // all module create before the main() run,
    // so may be there are something can't do during their constructor
    // the can do it in the Init(), after main() run
    //SapMap的Init()会调用映射中每个Sap的Init()
    static inline void Init()
    {
        LOG(INFO) << "Start to initialize each module";

        map_type map = GetMap();

        for (map_iter_type i = map.begin(); i != map.end(); ++i) {
            i->second->Init();
        }

        LOG(INFO) << "Finish module initialize";
    }

private:
    static inline map_type& GetMap() { return class_type::Instance().map_; }
private:
    map_type map_;
};

//Sap中对传入模板的利用
//当向Sap类传入模板的时候，就生成了模板类sap_type，协议类通过模板类sap_type来构造对象sap_
template <typename OwnerType, int Layer, int Pid>
class Sap : SapBase
{
public:
    typedef OwnerType owner_type;
    //每个Sap对象在构造时，会先向SapMap注册，因此获得唯一的映射关系（层ID和协议指针）：
    //在Sap类中，构造函数如下，其中SapMap是单例的，并且Sap在构造函数就完成了注册（相当于绑定了当前Sap和SapMap的关系）
    Sap() { SapMap::Register(Layer, Pid, this); }
    ~Sap() {}
    //构造函数里要向SapMap建立唯一映射关系：层ID与协议指针
    //设置Sap中owner的函数（owner指的都是协议）
    inline void SetOwner(owner_type* owner)
    {
        assert(owner != NULL);

        owner_ = owner;
    }

    //Sap中的Init()就会调用各个协议中的Init()
    inline void Init() const { GetOwner()->Init(); }
    inline void SendReq(const MessagePtr& msg, unsigned layer, unsigned pid)
    {
        LOG(DEBUG) << "Layer: " << GetLayer() << " Pid: " << GetPid()
                   << " send Request to Layer: " << layer << " Pid: " << pid;
        Send<msg::MsgReqPtr, msg::MsgReq, ReqQueue>(msg, layer, pid);
    }

    inline void SendRsp(const MessagePtr& msg, unsigned layer, unsigned pid)
    {
        LOG(DEBUG) << "Layer: " << GetLayer() << " Pid: " << GetPid()
                   << " send Response to Layer: " << layer << " Pid: " << pid;

        Send<msg::MsgRspPtr, msg::MsgRsp, RspQueue>(msg, layer, pid);
    }

    inline void SendNtf(const MessagePtr& msg, unsigned layer, unsigned pid)
    {
        LOG(DEBUG) << "Layer: " << GetLayer() << " Pid: " << GetPid()
                   << " send Notification to Layer: " << layer << " Pid: " << pid;
        Send<msg::MsgNtfPtr, msg::MsgNtf, NtfQueue>(msg, layer, pid);
    }

    inline void ProcessMsg(const MessagePtr& msg)
    {
        GetOwner()->GetHsm().ProcessEvent(msg);
    }

    //Sap中的处理函数，最终交给HSM中的函数
    inline void RecvReq(const MessagePtr& msg, unsigned int layer,
                        unsigned int pid)
    {
        GetOwner()->SetReqSrc(layer, pid);
        ProcessMsg(msg);
    }

    inline void RecvRsp(const MessagePtr& msg) { ProcessMsg(msg); }
    //Sap类中的处理函数：（事实上，RecvReq和RecvNtf、RecvRsp区别不大，RecvReq会设置协议类中的私有成员：传入事件的层和ID，主要是为了方便协议找到上一层。但事实上协议直接使用宏来得知上一层。个人认为这里设置没有必要，Sap中这几个处理函数是一样的，最后协议可以根据原始事件的类型来得知是否需要Rsp。）
    //除了以下的RecvNtf还有ProcessMsg
    inline void RecvNtf(const MessagePtr& msg) { ProcessMsg(msg); }
    inline unsigned GetLayer() const { return layer_; }
    inline unsigned GetPid() const { return pid_; }
private:
    inline owner_type* GetOwner() const { return owner_; }

    //Sap类中的Send函数，这里模板参数需要传入：事件本身的指针，层间事件的指针，事件插入的队列。Send函数完成对原始事件的包装（加入原目标层和ID），然后压入层间事件队列的过程
    template <typename PtrType, typename MsgType, typename QueueType>
    inline void Send(const MessagePtr& msg, unsigned layer, unsigned pid)
    {
        PtrType p(new MsgType);

        p->msg      = msg;
        p->DstLayer = layer;
        p->DstPid   = pid;
        p->SrcLayer = GetLayer();
        p->SrcPid   = GetPid();

        QueueType::Push(p);
    }

private:
    const static unsigned layer_ = Layer;
    const static unsigned pid_   = Pid;

    owner_type* owner_;
};

}  // end namespace sap

#endif  // _SAP_H_
