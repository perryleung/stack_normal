#ifndef _hsm_H_
#define _hsm_H_

#include <boost/mpl/at.hpp>
#include <boost/mpl/back.hpp>
#include <boost/mpl/begin.hpp>
#include <boost/mpl/begin_end.hpp>
#include <boost/mpl/contains.hpp>
#include <boost/mpl/deref.hpp>
#include <boost/mpl/distance.hpp>
#include <boost/mpl/find_if.hpp>
#include <boost/mpl/front.hpp>
#include <boost/mpl/identity.hpp>
#include <boost/mpl/if.hpp>
#include <boost/mpl/is_sequence.hpp>
#include <boost/mpl/list.hpp>
#include <boost/mpl/placeholders.hpp>
#include <boost/mpl/pop_front.hpp>
#include <boost/mpl/push_front.hpp>
#include <boost/mpl/vector.hpp>
#include <cassert>
#include <functional>
#include <iostream>
#include <list> //容器
#include <memory>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <vector> //容器

#define hsm_assert  assert
#define std_ptr     std::shared_ptr //?
#define std_list    std::list
#define std_vector  std::vector
#define std_dpc     std::dynamic_pointer_cast //?
#define hsm_vector  boost::mpl::vector //?
#define hsm_list    boost::mpl::list //?

#define ABORT_WHEN_AN_EVENT_NOT_HANDLE true
#define ABORT_WHEN_RECEIVE_UNKNOWN_EVENT false

#ifdef HSM_DEBUG
#define HSM_LOG(msg)                                                            \
    do {                                                                        \
        std::cout << __FILE__ << "(" << __LINE__ << "): " << msg << std::endl;  \
    } while (0)
#else
#define HSM_LOG(msg)
#endif

#define HSM_DISCARD_REACTIMPL()                                                 \
    hsm::detail::ReactImpl<state_type, decltype(a), hsm::do_discard>()
//丢弃事件
#define HSM_DISCARD(e)                                                          \
    auto react(const Ptr<e> &a)->decltype(HSM_DISCARD_REACTIMPL())              \
    {                                                                           \
        static auto reaction = HSM_DISCARD_REACTIMPL();                         \
        return reaction;                                                        \
    }

//HSM_DEFER在协议栈中只需要传入一个事件，然后最终插入状态机的延迟队列中，只有在该状态转移的时候才会调用
#define HSM_DEFER_REACTIMPL()                                                   \
    hsm::detail::ReactImpl<state_type, decltype(a), hsm::do_defer>()
//延迟事件
#define HSM_DEFER(e)                                                            \
    auto react(cosnt Ptr<e> &a)->decltype(HSM_DEFER_REACTIMPL())                \
    {                                                                           \
        static auto reaction = HSM_DEFER_REACTIMPL();                           \
        return reaction;                                                        \
    }

//这里的a表示传入的事件
#define HSM_WORK_REACTIMPL(f)                                                   \
    hsm::detail::ReactImpl<state_type, decltype(a), hsm::do_work>(GetOwner(), f)
//执行动作输出
#define HSM_WORK(e, f)                                                          \
    auto react(const Ptr<e> &a)->decltype(HSM_WORK_REACTIMPL(f))                \
    {                                                                           \
        static auto reaction(HSM_WORK_REACTIMPL(f));                            \
        return reaction;                                                        \
    }

#define HSM_TRANSIT_REACTIMPL_2(s)                                              \
    hsm::detail::ReactImpl<state_type, decltype(a), hsm::do_transit, s, hsm::do_discard, s>()
#define HSM_TRANSIT_2(e, s)                                                     \
    auto react(const Ptr<e> &a)->decltype(HSM_TRANSIT_REACTIMPL_2(s))           \
    {                                                                           \
        static auto reaction = HSM_TRANSIT_REACTIMPL_2(s);                      \
        return reaction;                                                        \
    }

#define HSM_TRANSIT_REACTIMPL_3(s, f)                                           \
    hsm::detail::ReactImpl<state_type, decltype(a), hsm::do_transit, s, hsm::do_discard, s>(GetOwner(), f)
#define HSM_TRANSIT_3(e, s, f)                                                  \
    auto react(const Ptr<e> &a)->decltype(HSM_TRANSIT_REACTIMPL_3(s, f))        \
    {                                                                           \
        static auto reaction(HSM_TRANSIT_REACTIMPL_3(s, f));                    \
        return reaction;                                                        \
    }

#define HSM_TRANSIT_REACTIMPL_4(s, f, g)                                        \
    hsm::detail::ReactImpl<state_type, decltype(a), hsm::do_transit, s, hsm::do_discard, s>(GetOwner(), f, g)
#define HSM_TRANSIT_4(e, s, f, g)                                               \
    auto react(const Ptr<e> &a)->decltype(HSM_TRANSIT_REACTIMPL_4(s, f, g))     \
    {                                                                           \
        static auto reaction(HSM_TRANSIT_REACTIMPL_4(s, f, g));                 \
        return reaction;                                                        \
    }

#define GET_MACRO(_1, _2, _3, _4, NAME, ...) NAME
#define HSM_TRANSIT(...)                                                        \
    GET_MACRO(__VA_ARGS__, HSM_TRANSIT_4, HSM_TRANSIT_3, HSM_TRANSIT_2)         \
    (__VA_ARGS__)

#define HSM_TRANSIT_DEFER_REACTIMPL(s, f, g)                                    \
    hsm::detail::ReactImpl<state_type, decltype(a), hsm::do_transit, s, hsm::do_defer, s>(GetOwner(), f, g)
#define HSM_TRANSIT_DEFER(e, s, f, g)                                           \
    auto react(const Ptr<e> &a)->decltype(HSM_TRANSIT_DEFER_REACTIMPL(s, f, g)) \
    {                                                                           \
        static auto reaction(HSM_TRANSIT_DEFER_REACTIMPL(s, f, g));             \
        return reaction;                                                        \
    }

#define HSM_TRANSIT_TRANSIT_REACTIMPL(s, f, g, fs)                              \
    hsm::detail::ReactImpl<state_type, decltype(a), hsm::do_transit, s, hsm::do_transit, fs>(GetOwner(), f, g)
#define HSM_TRANSIT_TRANSIT(e, s, f, g, fs)                                     \
    auto react(const Ptr<e> &a)->decltype(HSM_TRANSIT_DEFER_REACTIMPL(s, f, g)) \
    {                                                                           \
        static auto reaction(HSM_TRANSIT_TRANSIT_REACTIMPL(s, f, g, fs));       \
        return reaction;                                                        \
    }

template <typename T>
using Ptr = std_ptr<T>; //Ptr是一个智能指针

namespace hsm{

using namespace boost;

// a wrapper to storage the type of state and event
struct TypeInfoStorage
{
    TypeInfoStorage() : type_info_(nullptr) {}
    TypeInfoStorage(const std::type_info &t) : type_info_(&t) {}
    bool operator==(const TypeInfoStorage &rhs) const
    {
        return *type_info_ == *rhs.type_info_;
    }
private:
    const std::type_info *type_info_;
}

// get the type of state (or event),
// all the object of one certain state (or event) shared the same
// TypeInfoStorage instance
template <typename T>
TypeInfoStorage &GetType()
{
    static TypeInfoStorage TypeInfo(typeid(T)); //typeid操作符的返回结果是名为type_info的标准库类型的对象的引用
    return TypeInfo;
}

struct StateBase;

// State creation interface
// 初始化一个T类型的智能指针
// shared_ptr<T> p = make_shared<T>();
// shared_ptr<string> s = make_shared<string>("hello");
template <typename T>
struct StateFactory
{
    static Ptr<T> Create() 
    {
        return std::make_shared<T>();
    }
};

// 将事件智能指针与状态智能指针的初始化对等
template <typename T>
using EventFactory = StateFactory<T>;

// 枚举状态，5种状态类型
enum ReactionResult
{
    no_reaction,
    do_discard,
    do_defer,
    do_work,
    do_transit,
};

typedef ReactionResult result;
typedef ReactionResult REACTION_TYPE;

// Transition是一个结构体模板，主要是用来生成从当前状态到模板状态的共同状态。然后把这个模板结构体作为模板放入状态机的TransitState模板函数中，通过这个函数完成状态转移
template <typename SrcState, typename DstState>
struct Transition // ??????
{
    typedef SrcState src_state;
    typedef DstState dst_state;
    //find_if ?? 
    typedef typename mpl::find_if<
        typename SrcState::outer_state_type_list, mpl::contains<
            typename DstState::outer_state_type_list, mpl::placeholders::_> >::type common_state_iter;

    typedef typename mpl::deref<common_state_iter>::type com_state;
}

struct EventBase
{
    EventBase() {}
    virtual ~EventBase() {}
    virtual TypeInfoStorage GetEventType() const 
    {
        return typeid(*this);
    }
    template <typename T>
    bool IsType()
    {
        return GetEventType() == GetType<T>();
    }
};


typedef Ptr<EventBase> EventBasePtr;

template <typename SelfType>
struct Event : EventBase
{
    typedef SelfType event_type;
}

typedef Ptr<StateBase> StateBasePtr;


class StateMachine;

struct StateBase
{
    StateBase() : state_machine_(nullptr) {}
    virtual ~StateBase() {}

    inline StateMachine &GetStateMachine()
    {
        hsm_assert(state_machine_ != nullptr);
        return *state_machine_;
    }
    inline const StateMachine &GetStateMachine() const
    {
        hsm_assert(state_machine_ != nullptr);
        return *state_machine_;
    }
    inline void SetStateMachine(StateMachine *state_machine)
    {
        hsm_assert(state_machine != nullptr);
        state_machine_ = state_machine;
    }

    inline void *GetOwner();
    virtual void on_enter() {}
    virtual void on_exit() {}
    virtual void Process(StateBasePtr &, EventBasePtr &) = 0;
    virtual void TransitOuter() = 0;
    virtual bool IsEnter() { return is_enter_; }
    virtual void SetEnter(bool i) { is_enter_ = i; }

private:
    friend class StateMachine;
    virtual TypeInfoStorage GetStateType() const
    {
        return typeid(*this);
    }
    //每个原始事件都有IsType这个函数。传入模板，然后查看当前事件和传入模板事件是否相同
    template <typename T>
    bool IsType()
    {
        return GetStateType() == GetType<T>();
    }
    virtual result Dispatch(StateBasePtr &, EventBasePtr &) = 0;
    virtual bool HasInitializeState() = 0;

private:
    StateMachine *state_machine_;
    bool is_enter_ = false;
} //end of struct StateBase

inline void InvokeOnEnter(StateBasePtr &state)
{
    state->on_enter();
}
inline void InvokeOnExit(StateBasePtr &state)
{
    state->on_exit();
}

class StateMachine
{
public:
    StateMachine() : owner_(nullptr) {}
    ~StateMachine() { Stop(); }
    typedef std_list<StateBasePtr> state_list_type;
    typedef std_list<StateBasePtr> state_stack_type;
    typedef std_list<EventBasePtr> event_queue_type;

    typedef mpl::list<> outer_state_type_list;

    void Stop() {}

    void ProcessEvent(const EventBasePtr &e)
    {
        event_queue_.push_back(e);
        while(!event_queue_.empty()){
            ProcessQueueEvent();
        }
    }
    void ProcessQueueEvent()
    {
        hsm_assert(!event_queue_.empty());
        auto event = event_queue_.front();
        event_queue_.pop_front();
        GetCurrentState()->Process(current_state_, event);
    }

    inline StateBasePtr GetCurrentState()
    {
        return current_state_;
    }

    //状态机中处理延迟事件的一个函数，处理的时候会先获得当前状态然后调用状态里的函数
    void ProcessDeferredEvent()
    {
        if (!defer_queue_.empty())
        {
            auto event = defer_queue_.front();
            defer_queue_.pop_front();
            GetCurrentState()->Process(current_state_, event);
        }
    }

    inline void DeferEvent(EventBasePtr e) {
        defer_queue_.push_back(e);
    }

    //accessors
    inline void *GetOwner()
    {
        hsm_assert(owner_ != nullptr);
        return owner_;
    }
    inline const void *GetOwner() const
    {
        hsm_assert(owner_ != nullptr);
        return owner_;
    }

    //GetState的作用是从state_list_中找这个状态，如果找不到这个状态就构造一个这个状态的指针，并且压入state_list_和返回
    template <typename StateType>
    StateBasePtr GetState()
    {
        typedef state_list_type::iterator state_iter_type;

        for (state_iter_type it = state_list_.begin(); it != state_list_.end(); ++it){
            if((*it)->IsType<StateType>()){
                return (*it);
            }
        }

        auto state = StateFactory<StateType>::Create();
        state->SetStateMachine(this);
        state_list_.push_back(state);
        return state;
    }

    template <typename StateType>
    void SetState(bool init = true){
        auto state = GetState<StateType>();
        typedef typename StateType::outer_state_type outer_state; 
        if(current_state_){
            if(!current_state_->IsType<outer_state>()){
                InvokeOnExit(current_state_);
                current_state_->SetEnter(false);
            }
        }
        current_state_ = state;
        if(!state->IsEnter()){
            state->SetEnter(true);
            InvokeOnEnter(state);
        }
        if(!init){
            return;
        }
        if(state->HasInitializeState()){
            SetState<typename StateType::init_state_type>();
        }
    }

    template <typename InitialStateType>
    void Initialize(void *owner = nullptr){
        owner_ = owner;
        SetState<InitialStateType>();
    }

    template <typename TransitionType>
    void TransitState()
    {
        typedef typename TransitionType::src_state src_state;
        typedef typename TransitionType::dst_state dst_state;
        typedef typename TransitionType::com_state com_state;
        typedef typename src_state::outer_state_type next_state;
        typedef typename dst_state::outer_state_type prev_state;

        if(GetCurrentState()->IsType<src_state>()) {
            TransitSeqImpl<next_state, com_state>::TransitSeq(this);
            TransitRseqImpl<prev_state, com_state>::TransitRseq(this);
            SetState<dst_state>();
        }else{
            GetCurrentState()->TransitOuter();
            TransitState<TransitionType>();
        }
    }

    template <typename NextState, typename CommomState>
    struct TransitSeqImpl
    {
        typedef typename NextState::outer_state_type outer_state;

        static void TransitSeq(StateMachine *sm)
        {
            sm->SetState<NextState>(false);
            TransitSeqImpl<outer_state, CommomState>::TransitSeq(sm);
        }
    };
    struct TransitSeqImpl<NextState, NextState>
    {
        static void TransitSeq(StateMachine *) {}
    };

    template <typename PrevState, typename CommomState>
    struct TransitRseqImpl
    {
        typedef typename PrevState::outer_state_type outer_state;

        static void TransitRseq(StateMachine *sm)
        {
            TransitRseqImpl<outer_state, CommomState>::TransitRseq(sm);
            sm->SetState<PrevState>(false);
        }
    };
    template <typename PrevState>
    struct TransitRseqImpl<PrevState, PrevState>
    {
        static void TransitRseq(StateMachine *) {}
    };

private:
    void *owner_;
    state_list_type state_list_;
    StateBasePtr current_state_;
    event_queue_type event_queue_;
    event_queue_type defer_queue_;

}; //end of class StateMachine

namespace detail {

template <typename StateType, typename EventType, REACTION_TYPE ActionType,                           typename TargetState = void,                 REACTION_TYPE FalseTransitType = no_reaction,     typename FalseTatgetState = void>
struct ReactImpl
{
};

template <typename StateType, typename EventType>
struct ReactImpl<StateType, EventType, do_discard>
{
    result operator()(Ptr<StateType>, EventType)
    {
        HSM_LOG("Discard event");
        return do_discard;
    }
};

template <typename StateType, typename EventType>
struct ReactImpl<StateType, EventType, do_defer>
{
    result operator()(Ptr<StateType> s, EventType e)
    {
        HSM_LOG("Defer event");
        s->GetSateMachine().DeferEvent(e);
        return do_defer;
    }
};

template <typename StateType, typename EventType>
struct ReactImpl<StateType, EventType, do_work>
{
    typedef void (*func_ptr_type)(EventType);
    typedef std::function<void(EventType)> func_type;

    explicit ReactImpl(func_ptr_type fp) noexcept : func_(fp) {}
    template <class C>
    explicit ReactImpl(void *o, void (C::*const mp)(EventType)) noexcept : func_(std::bind(mp, static_cast<C *>(o), std::placeholders::_1))
    {}

    result operator()(Ptr<StateType>, EventType e)
    {
        HSM_LOG("Work event");
        hsm_assert(func_);
        func_(e);
        return do_work;
    }

private:
    func_type func_;
};

template <typename StateType, typename EventType, REACTION_TYPE FalseTransitType, typename FalseTatgetState = void>
struct ReactTransitImpl
{
    static result react(Ptr<StateType> s, EventType e)
    {
        static auto r = ReactImpl<StateType, EventType, FalseTransitType>();
        r(s, e);
        return do_discard;
    }
};

template <typename StateType, typename EventType, typename FalseTatgetState>
struct ReactTransitImpl<StateType, EventType, do_transit, FalseTatgetState>
{
    typedef Transition<StateType, FalseTatgetState> false_transit_type;
    static result react(Ptr<StateType> s, EventType e)
    {
        s->GetStateMachine().template TransitState<false_transit_type>();
        return do_transit;
    }
};

template <typename StateType, typename EventType, typename TargetState, REACTION_TYPE FalseTransitType, typename FalseTatgetState>
struct ReactImpl<StateType, EventType, do_transit, TargetState, FalseTransitType, FalseTatgetState>
{
    typedef void (*action_ptr_type)(EventType);
    typedef bool (*guard_ptr_type)(EventType);
    typedef std::function<void(EventType)> action_type;
    typedef std::function<bool(EventType)> guard_type;

    ReactImpl() = default;

    explicit ReactImpl(action_ptr_type ap) noexcept : action_(ap) {}
    explicit ReactImpl(action_ptr_type ap, guard_ptr_type gp) noexcept : action_(ap), guard_(gp)
    {}

    template <class C>
    explicit ReactImpl(void *o, void (C::*const mp)(EventType)) noexcept : action_(std::bind(mp, static_cast<C *>(o), std::placeholders::_1))
    {}
    
    template <class C>
    explicit ReactImpl(void *o, bool (C::*const mp)(EventType)) noexcept : guard_(std::bind(mp, static_cast<C *>(o), std::placeholders::_1))
    {}

    template <class C>
    explicit ReactImpl(void *o, void (C::*const ap)(EventType), bool (C::*const gp)(EventType)) noexcept : action_(std::bind(ap, static_cast<C *>(o), std::placeholders::_1)), guard_(std::bind(gp, static_cast<C *>(o), std::placeholders::_1))
    {}
    
    typedef Transition<StateType, TargetState> transit_type;

    result operator()(Ptr<StateType> s, EventType e)
    {
        HSM_LG("Transit event");
        if(guard_){
            if(!guard_(e)){
                return detail::ReactTransitImpl<StateType, EventType, FalseTransitType, FalseTatgetState>::react(s, e);
            }
        }

        s->GetStateMachine().template TransitState<transit_type>();

        if(action_){
            action_(e);
        }
        return do_transit;
    }

private:
    action_type action_;
    guard_type guard_;

};

} // end of namespace detail


} //end of namespace hsm

#endif //end of define hsm_H_
