#ifndef _DAP_H_
#define _DAP_H_

#include <cassert>
#include <map>
#include "message.h"
#include "utils.h"

namespace dap {

/*
 * interface for DAP
 */
class DapBase
{
public:
    // when data income, scheduler Notify the correspond
    // DAP to handle it
    virtual void Notify() const = 0;
};

/*
 * This class map DAP and an opened FD,
 * because there are may be more than one Phy Module,
 * when data income, you can only get the FD and data from Reactor,
 * so, map the FD then DAP, and find the DAP handle data
 */
class DapMap : public utils::Singleton<DapMap>
{
public:
    typedef DapMap class_type;
    typedef int index_type;
    typedef DapBase* value_type;
    typedef std::map<index_type, value_type> map_type;
    typedef map_type::iterator map_iter_type;

    static inline void Register(index_type index, value_type value)
    {
        GetMap()[index] = value;
    }

    static inline value_type Find(index_type index) { return GetMap()[index]; }
    static inline bool Empty() { return GetMap().empty(); }
private:
    static inline map_type& GetMap() { return class_type::Instance().map_; }
private:
    map_type map_;
};

template <typename OwnerType>
class Dap : DapBase
{
public:
    typedef OwnerType owner_type;

    inline void SetOwner(owner_type* owner)
    {
        assert(owner != NULL);

        owner_ = owner;
    }

    // map FD and DAP
    inline void Map(int fd) { DapMap::Register(fd, this); }
    //根据GetFd()找到对应的协议层
    //在Dap中获得的套接字描述符后要向DapMap注册（物理层ID与协议指针）
    //在Dap中，需要获得套接字描述符之后，在向DapMap注册，对应物理层的代码里面Map()函数
    //最终在QPSK中，完成Dap与fd关系的建立。（最终调度器可以在DapMap中，通过套接字描述符，找到相应的Dap，就可以找到对应的协议层）
    inline void Notify() const { GetOwner()->Notify(); }
private:
    inline owner_type* GetOwner() const { return owner_; }
private:
    owner_type* owner_;
};

}  // end namespace dap

#endif  // _DAP_H_
