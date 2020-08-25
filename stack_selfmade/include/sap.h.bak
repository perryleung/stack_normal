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

template <typename OwnerType, int Layer, int Pid>
class Sap : SapBase
{
public:
    typedef OwnerType owner_type;

    Sap() { SapMap::Register(Layer, Pid, this); }
    ~Sap() {}
    inline void SetOwner(owner_type* owner)
    {
        assert(owner != NULL);

        owner_ = owner;
    }

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

    inline void RecvReq(const MessagePtr& msg, unsigned int layer,
                        unsigned int pid)
    {
        GetOwner()->SetReqSrc(layer, pid);
        ProcessMsg(msg);
    }

    inline void RecvRsp(const MessagePtr& msg) { ProcessMsg(msg); }
    inline void RecvNtf(const MessagePtr& msg) { ProcessMsg(msg); }
    inline unsigned GetLayer() const { return layer_; }
    inline unsigned GetPid() const { return pid_; }
private:
    inline owner_type* GetOwner() const { return owner_; }
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
