#ifndef _hsm_H_
#define _hsm_H_
/*
1.  首先由层间调度有Scheduler来处理：事件队列大部分是由协议产生的，或者是由应用层、物理层等从外界接收回来消息。
2.  Scheduler处理主要是交给Sap：每个协议层都是有接口的，通过接口找到该协议，并把事件交给协议来处理。
3.  协议的处理工作由状态机和当前状态来完成：事件先给到协议的状态机，状态机找到当前状态，然后根据事件类型调用状态下相应的函数（react）。
函数做出的反应可能有：转移状态，推迟事件，无视事件，守护条件，执行动作等。
最终，通过这些反应，可能会产生一些新的事件。这些新的事件可能传到其他协议层，就通过Sap来处理；也可能是交给本层的事件，就可以直接压入本状态机的事件队列中。
总之，最后调度器会一直处理事件队列，直到事件队列为空。
4.  完成这样一轮操作之后，libev就开始下一轮循环。一般只有当通信机接收到数据、应用层有数据下发、协议层有定时器事件，这几种情况会产生新事件。libev就继续在下一轮循环中处理完这些事件。
5.  关于延迟事件：只有状态发送转移的时候才处理延迟事件，不会像事件队列一样把它处理完再结束，而是只处理一个延迟事件（目前看到的代码是这样的）。
（状态机的主要功能是：保存当前状态，找到当前状态处理事件，实现状态转移）
*/
/*
状态机类的作用主要有两个：
1. 管理状态转移过程。转移时在ReactTransitImpl结构体
2. 有两个事件队列（当前事件和延迟事件），保存这两个队列并处理。
*/
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
#include <list>
#include <memory>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <vector>

#define hsm_assert assert
#define std_ptr    std::shared_ptr
#define std_list   std::list
#define std_vector std::vector
#define std_dpc    std::dynamic_pointer_cast
#define hsm_vector boost::mpl::vector
#define hsm_list   boost::mpl::list

#define ABORT_WHEN_AN_EVENT_NOT_HANDLE true
#define ABORT_WHEN_RECEIVE_UNKNOWN_EVENT false

//#define HSM_DEBUG

#ifdef HSM_DEBUG
#define HSM_LOG(msg)                                                           \
    do {                                                                       \
        std::cout << __FILE__ << "(" << __LINE__ << "): " << msg << std::endl; \
    } while (0)
#else
#define HSM_LOG(msg)
#endif

#define HSM_DISCARD_REACTIMPL()                                                \
    hsm::detail::ReactImpl<state_type, decltype(a), hsm::do_discard>()
//丢弃事件
#define HSM_DISCARD(e)                                                         \
    auto react(const Ptr<e> &a)->decltype(HSM_DISCARD_REACTIMPL())             \
    {                                                                          \
        static auto reaction = HSM_DISCARD_REACTIMPL();                        \
        return reaction;                                                       \
    }
//HSM_DEFER在协议栈中只需要传入一个事件，然后最终插入状态机的延迟队列中，只有在该状态转移的时候才会调用
#define HSM_DEFER_REACTIMPL()                                                  \
    hsm::detail::ReactImpl<state_type, decltype(a), hsm::do_defer>()
//延迟事件
#define HSM_DEFER(e)                                                           \
    auto react(const Ptr<e> &a)->decltype(HSM_DEFER_REACTIMPL())               \
    {                                                                          \
        static auto reaction = HSM_DEFER_REACTIMPL();                          \
        return reaction;                                                       \
    }
//这里的a表示传入的事件
#define HSM_WORK_REACTIMPL(f)                                                  \
    hsm::detail::ReactImpl<state_type, decltype(a), hsm::do_work>(GetOwner(), f)
//执行动作输出
#define HSM_WORK(e, f)                                                         \
    auto react(const Ptr<e> &a)->decltype(HSM_WORK_REACTIMPL(f))               \
    {                                                                          \
        static auto reaction(HSM_WORK_REACTIMPL(f));                           \
        return reaction;                                                       \
    }

#define HSM_TRANSIT_REACTIMPL_2(s)                                             \
    hsm::detail::ReactImpl<state_type, decltype(a), hsm::do_transit, s,        \
                           hsm::do_discard, s>()
//转移到其它状态，可能会有动作输出，也可能有守护条件
//其中HSM_TRANSIT有四种，应该是根据参数个数来决定调用哪个（有点像重载）。使用HSM_TRANSIT，如果条件不满足，则事件被丢弃
#define HSM_TRANSIT_2(e, s)                                                    \
    auto react(const Ptr<e> &a)->decltype(HSM_TRANSIT_REACTIMPL_2(s))          \
    {                                                                          \
        static auto reaction = HSM_TRANSIT_REACTIMPL_2(s);                     \
        return reaction;                                                       \
    }
//在hsm.h中的宏定义，相当于有三个入参，这里的ReactImpl结构体和HSM_WORK中使用的结构体不同，有六个模板参数（应该可以提供给另外的几类转移宏定义）
#define HSM_TRANSIT_REACTIMPL_3(s, f)                                          \
    hsm::detail::ReactImpl<state_type, decltype(a), hsm::do_transit, s,        \
                           hsm::do_discard, s>(GetOwner(), f)
#define HSM_TRANSIT_3(e, s, f)                                                 \
    auto react(const Ptr<e> &a)->decltype(HSM_TRANSIT_REACTIMPL_3(s, f))       \
    {                                                                          \
        static auto reaction(HSM_TRANSIT_REACTIMPL_3(s, f));                   \
        return reaction;                                                       \
    }

#define HSM_TRANSIT_REACTIMPL_4(s, f, g)                                       \
    hsm::detail::ReactImpl<state_type, decltype(a), hsm::do_transit, s,        \
                           hsm::do_discard, s>(GetOwner(), f, g)
#define HSM_TRANSIT_4(e, s, f, g)                                              \
    auto react(const Ptr<e> &a)->decltype(HSM_TRANSIT_REACTIMPL_4(s, f, g))    \
    {                                                                          \
        static auto reaction(HSM_TRANSIT_REACTIMPL_4(s, f, g));                \
        return reaction;                                                       \
    }
//其中HSM_TRANSIT有四种，应该是根据参数个数来决定调用哪个（有点像重载）。使用HSM_TRANSIT，如果条件不满足，则事件被丢弃
//转移到其它状态，可能会有动作输出，也可能有守护条件
#define GET_MACRO(_1, _2, _3, _4, NAME, ...) NAME
#define HSM_TRANSIT(...)                                                       \
    GET_MACRO(__VA_ARGS__, HSM_TRANSIT_4, HSM_TRANSIT_3, HSM_TRANSIT_2)        \
    (__VA_ARGS__)
/*
这个地方看上去像是实现了重载，但事实上原理是很简单的：
HSM_TRANSIT(…)中传入不同的参数（2、3、4个），由于占位符有4个，因此，假如只有两个参数，就会解析成：
GET_MACRO( e , s , HSM_TRANSIT_4 , HSM_TRANSIT_3 , HSM_TRANSIT_2 )
从而整个解析式就解析成第五个参数，即HSM_TRANSIT_2。同理，如果放入三个参数，则后退一个参数，就取到了HSM_TRANSIT_3。*/
#define HSM_TRANSIT_DEFER_REACTIMPL(s, f, g)                                   \
    hsm::detail::ReactImpl<state_type, decltype(a), hsm::do_transit, s,        \
                           hsm::do_defer, s>(GetOwner(), f, g)
//当不满足守护条件时，延迟事件而不是丢弃
#define HSM_TRANSIT_DEFER(e, s, f, g)                                          \
    auto react(const Ptr<e> &a)                                                \
        ->decltype(HSM_TRANSIT_DEFER_REACTIMPL(s, f, g))                       \
    {                                                                          \
        static auto reaction(HSM_TRANSIT_DEFER_REACTIMPL(s, f, g));            \
        return reaction;                                                       \
    }

#define HSM_TRANSIT_TRANSIT_REACTIMPL(s, f, g, fs)                             \
    hsm::detail::ReactImpl<state_type, decltype(a), hsm::do_transit, s,        \
                           hsm::do_transit, fs>(GetOwner(), f, g)
#define HSM_TRANSIT_TRANSIT(e, s, f, g, fs)                                    \
    auto react(const Ptr<e> &a)                                                \
        ->decltype(HSM_TRANSIT_TRANSIT_REACTIMPL(s, f, g, fs))                 \
    {                                                                          \
        static auto reaction(HSM_TRANSIT_TRANSIT_REACTIMPL(s, f, g, fs));      \
        return reaction;                                                       \
    }

template <typename T>
using Ptr = std_ptr<T>;

namespace hsm {

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
};

// get the type of state (or event),
// all the object of one certain state (or event) shared the same
// TypeInfoStorage instance
template <typename T>
TypeInfoStorage &GetType()
{
    static TypeInfoStorage TypeInfo(typeid(T));
    return TypeInfo;
}

struct StateBase;

// State creation interface
template <typename T>
struct StateFactory
{
    static Ptr<T> Create() { return std::make_shared<T>(); }
};

template <typename T>
using EventFactory = StateFactory<T>;

enum ReactionResult {
    no_reaction,
    do_discard,
    do_defer,
    do_work,
    do_transit,
};

typedef ReactionResult result;
typedef ReactionResult REACTION_TYPE;
//原目标和公共状态：（写在hsm.h中，类外公共函数）
template <typename SrcState, typename DstState>
//其中，Transition是一个结构体模板，主要是用来生成从当前状态到模板状态的共同状态。然后把这个模板结构体作为模板放入状态机的TransitState模板函数中，通过这个函数完成状态转移
struct Transition
{
    typedef SrcState src_state;
    typedef DstState dst_state;

    typedef typename mpl::find_if<
        typename SrcState::outer_state_type_list,
        mpl::contains<typename DstState::outer_state_type_list,
                      mpl::placeholders::_> >::type common_state_iter;

    typedef typename mpl::deref<common_state_iter>::type com_state;
};

struct EventBase
{
    EventBase() {}
    virtual ~EventBase() {}
    // RTTI interface
    virtual TypeInfoStorage GetEventType() const { return typeid(*this); }
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
};

typedef Ptr<StateBase> StateBasePtr;
/*
1 事件队列中有事件时，派发事件。（就是交给当前状态CurrentState来处理，处理的函数由用户来实现。之后如果有事件再需要派发，也需要用户来实现，通过Sap接口来找到相应的协议）
2 有状态需要转移时，调用TransitState()函数，通过不断地SetState()来改变状态（同时如果有离开、进入状态事件时就调用，即on_enter()，on_exit()）。最终把当前状态CurrentState设置为目标状态。
3 每个协议都有一个状态机作为协议的成员。当然，这些状态机是独立的。每个状态机又有唯一的当前状态CurrentState。有事件需要处理时，交给当前状态处理。（如果处理不了，可能交给外部状态来处理*/
class StateMachine;
/*而State类：
1 主要负责处理事件（如何处理由用户定义）。
2 保存了：本状态，外部状态，初始状态。
3 转移状态时，通过mpl找到公共状态，然后调用状态机的转移函数来进行状态转移。*/
struct StateBase
{
    StateBase() : state_machine_(nullptr) {}
    virtual ~StateBase() {}
    // Accessors
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

    // Overridable functions
    // these two virtual function can be override in a certain state
    // on_enter is invoked when a state is created;
    virtual void on_enter() {}
    // on_exit is invoked just before a state is destroyed
    virtual void on_exit() {}
    virtual void Process(StateBasePtr &, EventBasePtr &) = 0;

    virtual void TransitOuter() = 0;

    virtual bool IsEnter() { return is_enter_; }
    virtual void SetEnter(bool i) { is_enter_ = i; }
private:
    friend class StateMachine;

    // RTTI interface
    virtual TypeInfoStorage GetStateType() const { return typeid(*this); }
    //每个原始事件都有IsType这个函数。传入模板，然后查看当前事件和传入模板事件是否相同。
    template <typename T>
    bool IsType()
    {
        return GetStateType() == GetType<T>();
    }

    // this function is used to call the right react function in user's state,
    // see below
    virtual result Dispatch(StateBasePtr &, EventBasePtr &) = 0;

    // some state may have a initialize state, when enter these state,
    // they will transit to the initialize state immediately
    virtual bool HasInitializeState() = 0;

private:
    StateMachine *state_machine_;

    bool is_enter_ = false;
};

inline void InvokeOnEnter(StateBasePtr &state) { state->on_enter(); }
inline void InvokeOnExit(StateBasePtr &state) { state->on_exit(); }
class StateMachine
{
public:
    StateMachine() : owner_(nullptr) {}
    ~StateMachine() { Stop(); }
    typedef std_list<StateBasePtr> state_list_type;
    typedef std_list<StateBasePtr> state_stack_type;
    typedef std_list<EventBasePtr> event_queue_type;

    typedef mpl::list<> outer_state_type_list;

    // Pops all states off the state stack, including initial state,
    // invoking on_exit on each one in inner-to-outer order.
    void Stop() {}
    // main interface to user
    //状态机大类中的函数：（插入队列中并一个个处理完）
    //找到当前状态，然后调用该状态下的处理派遣函数
    void ProcessEvent(const EventBasePtr &e)
    {
        event_queue_.push_back(e);

        while (!event_queue_.empty()) {
            ProcessQueueEvent();
        }
    }

    // accessors
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

    inline StateBasePtr GetCurrentState() { return current_state_; }
    //GetState的作用是从state_list_中找这个状态，如果找不到这个状态就构造一个这个状态的指针，并且压入state_list_和返回。
    template <typename StateType>
    StateBasePtr GetState()
    {
        typedef state_list_type::iterator state_iter_type;

        for (state_iter_type it = state_list_.begin(); it != state_list_.end();
             ++it) {
            if ((*it)->IsType<StateType>()) {
                return (*it);
            }
        }

        auto state = StateFactory<StateType>::Create();

        state->SetStateMachine(this);

        state_list_.push_back(state);

        return state;
    }

    template <typename StateType>
    void SetState(bool init = true)
    {
        auto state = GetState<StateType>();

        typedef typename StateType::outer_state_type outer_state;

        if (current_state_) {
            if (!current_state_->IsType<outer_state>()) {
                InvokeOnExit(current_state_);
                current_state_->SetEnter(false);
            }
        }

        current_state_ = state;

        if (!state->IsEnter()) {
            state->SetEnter(true);
            InvokeOnEnter(state);
        }

        if (!init) {
            return;
        }

        if (state->HasInitializeState()) {
            SetState<typename StateType::init_state_type>();
        }
    }

    // initializes the state machine
  /*状态机类中的SetState函数：（这个函数可以跳一级状态，首先判断当前状态是否传入状态的外部状态，如果不是的话调用on_exit。如果是，则说明是要进入状态，因此调用on_enter）
1. GetState，把当前状态插入状态机的state_list_中。
2. 由于未初始化的current_state_是NULL，因此不执行。
3. 把当前状态机的状态设置为传入的状态。
4. 把传入的状态的is_enter_设置为true。
5. 调用该状态的on_enter函数，因为相当于进入了这个状态。
6. 默认init参数是true，因此调用HasInitializeState()，如果未设置成初始状态，则递归调用SetState，把状态设置成协议中设定好的初始状态。
注意：
1. 如果是离开状态，则需要把原来的is_enter_设置为false。如果是进入状态，需要把进入之后的状态的is_enter_设置为true。
2. 每个状态的初始is_enter_都是false，表示没有进入该状态。（如果转移以后还是在父状态下，父状态仍为true。）
3. 每个状态机都有一个状态列表state_list_，因此每个状态的对象是唯一的。
4. 每个状态基类都有on_enter函数，需要协议自己写。*/   

    template <typename InitialStateType>
    void Initialize(void *owner = nullptr)
    {
        owner_ = owner;

        SetState<InitialStateType>();
    }
    //状态转移函数：（在状态机类中，这里传入一个模板类型，里面保存了原目标状态和公共状态）
    //状态机中的状态转移函数：（需要注意，这里要判断当前状态和传入的源状态是否相同。因为有可能是当前状态交给父状态处理，因此如果不相同，则先转移到外部状态，然后再调用状态转移函数，类似于递归。）
    template <typename TransitionType>
    void TransitState()
    {
        typedef typename TransitionType::src_state src_state;
        typedef typename TransitionType::dst_state dst_state;
        typedef typename TransitionType::com_state com_state;

        typedef typename src_state::outer_state_type next_state;
        typedef typename dst_state::outer_state_type prev_state;

        if (GetCurrentState()->IsType<src_state>()) {
            TransitSeqImpl<next_state, com_state>::TransitSeq(this);
            TransitRseqImpl<prev_state, com_state>::TransitRseq(this);
            SetState<dst_state>();
        } else {
            GetCurrentState()->TransitOuter();
            TransitState<TransitionType>();
        }
    }

    // CurrentState != CommomState forever
    //状态转移函数分成两个步骤，先转移到公共状态，然后再转移到目标状态。1. 转移到公共状态：（也是一种递归，先转移到下一状态，即当前状态的外部状态。然后把这个外部状态的外部状态作为模板，传到下一结构体模板。当传入的模板相同，即已经到达公共状态，则不操作。从而完成转移。）
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

    template <typename NextState>
    struct TransitSeqImpl<NextState, NextState>
    {
        static void TransitSeq(StateMachine *) {}
    };
//状态转移函数分成两个步骤，先转移到公共状态，然后再转移到目标状态。
//2. 从公共状态转移到目标状态：（原理差不多。只是这里先递归，再进行状态转移。主要是因为不知道公共状态下一状态是什么，因此先不转移，当递归找到了以后才开始转移。）
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

    // defer event
    inline void DeferEvent(EventBasePtr e) { defer_queue_.push_back(e); }
    //状态机大类中的处理队列函数：首先获得当前状态，然后调用该状态中的处理函数Process。（每个状态机都有一个私有成员，是当前的状态，找到当前状态就可以调用状态中的函数）
    void ProcessQueueEvent()
    {
        hsm_assert(!event_queue_.empty());

        auto event = event_queue_.front();
        event_queue_.pop_front();

        GetCurrentState()->Process(current_state_, event);
    }

    //状态机中处理延迟事件的一个函数，处理的时候会先获得当前状态然后调用状态里的函数
    void ProcessDeferredEvent()
    {
        if (!defer_queue_.empty()) {
            auto event = defer_queue_.front();
            defer_queue_.pop_front();

            GetCurrentState()->Process(current_state_, event);
        }
    }

private:
    void *owner_;

    state_list_type state_list_;
    //而状态机中，包含了当前状态，延迟队列和事件队列。（因此，只能有一个状态。）
    StateBasePtr current_state_;

    event_queue_type event_queue_;
    event_queue_type defer_queue_;
};

namespace detail {

template <typename StateType, typename EventType, REACTION_TYPE ActionType,
          typename TargetState           = void,
          REACTION_TYPE FalseTransitType = no_reaction,
          typename FalseTatgetState      = void>
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
//生成一个ReactImpl，然后可以在之后调用里面的这个()运算符重载函数，传入状态指针和事件类型
template <typename StateType, typename EventType>
struct ReactImpl<StateType, EventType, do_defer>
{
    result operator()(Ptr<StateType> s, EventType e)
    {
        HSM_LOG("Defer event");

        s->GetStateMachine().DeferEvent(e);

        return do_defer;
    }
};
//这里通过react函数，返回的reaction是其中一种reactImpl结构体：（返回了这个结构体之后，再传入模板，这个模板应该是一种状态）
template <typename StateType, typename EventType>
struct ReactImpl<StateType, EventType, do_work>
{
    typedef void (*func_ptr_type)(EventType);
    typedef std::function<void(EventType)> func_type;

    explicit ReactImpl(func_ptr_type fp) noexcept : func_(fp) {}
    template <class C>
    explicit ReactImpl(void *o, void (C::*const mp)(EventType)) noexcept
        : func_(std::bind(mp, static_cast<C *>(o), std::placeholders::_1))
    {
    }
//通过这个结构体重载的()运算符，在StateImpl中的一个结构体的Dispatch函数中，返回reactImpl，然后调用运算符重载，调用在协议栈中设置好的回调函数：（其中，传入的参数是s和e，也就是状态和事件）
//hsm.h 第710行
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

template <typename StateType, typename EventType,
          REACTION_TYPE FalseTransitType, typename FalseTatgetState = void>
//当状态需要发生转移时：
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

template <typename StateType, typename EventType, typename TargetState,
          REACTION_TYPE FalseTransitType, typename FalseTatgetState>
//ReactImpl有六个入参，分别代表：当前状态（状态基类中定义的），事件
struct ReactImpl<StateType, EventType, do_transit, TargetState,
                 FalseTransitType, FalseTatgetState>
{
    typedef void (*action_ptr_type)(EventType);
    typedef bool (*guard_ptr_type)(EventType);
    typedef std::function<void(EventType)> action_type;
    typedef std::function<bool(EventType)> guard_type;

    ReactImpl() = default;

    explicit ReactImpl(action_ptr_type ap) noexcept : action_(ap) {}
    explicit ReactImpl(action_ptr_type ap, guard_ptr_type gp) noexcept
        : action_(ap),
          guard_(gp)
    {
    }
    //这里只传入了目标状态和执行动作，因此调用：
    template <class C>
    explicit ReactImpl(void *o, void (C::*const mp)(EventType)) noexcept
        : action_(std::bind(mp, static_cast<C *>(o), std::placeholders::_1))
    {
    }

    template <class C>
    explicit ReactImpl(void *o, bool (C::*const mp)(EventType)) noexcept
        : guard_(std::bind(mp, static_cast<C *>(o), std::placeholders::_1))
    {
    }

    template <class C>
    explicit ReactImpl(void *o, void (C::*const ap)(EventType),
                       bool (C::*const gp)(EventType)) noexcept
        : action_(std::bind(ap, static_cast<C *>(o), std::placeholders::_1)),
          guard_(std::bind(gp, static_cast<C *>(o), std::placeholders::_1))
    {
    }

    typedef Transition<StateType, TargetState> transit_type;

//然后调用()运算符重载：（函数里会找到状态的状态机，然后进行状态转移。因此状态转移应该都在这个地方进行）
    result operator()(Ptr<StateType> s, EventType e)
    {
        HSM_LOG("Transit event");

        // check if a guard condition exist
        if (guard_) {
            if (!guard_(e)) {
                return detail::ReactTransitImpl<StateType, EventType,
                                                FalseTransitType,
                                                FalseTatgetState>::react(s, e);
            }
        }

        s->GetStateMachine().template TransitState<transit_type>();

        if (action_) {
            action_(e);
        }

        return do_transit;
    }

private:
    action_type action_;
    guard_type guard_;
};

}  // end namespace detail
/*
StateImpl类中的函数：
1. 至此，整个过程对原始事件到底是什么仍是未知的，直到在Dispatch函数中调用IsType来查询。
2. 利用DispatchImpl中的Dispatch函数来进行递归查找：先查看第一个事件与传入事件是否匹配，否则把列表中下一个状态作为开始，再生产一个DispatchImpl调用Dispatch函数来查找，如此类推。如果找到了end也没有，就返回no_reaction。
3. 注意，每个状态都必须先写上一个叫reactions的列表，Dispatch会从这个列表中找是否存在能够处理的事件。
4. 当得知事件类型event_type之后，就可以将传入的事件强转成该事件原型，然后调用该状态下已经用宏定义好的react函数，然后reactImpl结构体，再调用其中的括号运算符重载，调用需要执行的动作、延时、抛弃、转移等。
*/
template <typename SelfType, typename OuterState, typename InitState>
struct StateImpl : StateBase
{
    typedef SelfType state_type;
    typedef OuterState outer_state_type;
    typedef InitState init_state_type;

    // outer_state_type_list contain all the outer state of this state
    typedef typename mpl::push_front<
        typename outer_state_type::outer_state_type_list,
        outer_state_type>::type outer_state_type_list;

    typedef mpl::vector<> reactions;

    // has an initialize state? we know it at compile time
    //查看当前状态是不是已经设置成初始状态。
    virtual bool HasInitializeState()
    {
        typedef typename mpl::if_c<is_same<SelfType, InitState>::value,
                                   mpl::false_, mpl::true_>::type type;

        return type::value;
    }

    virtual void TransitOuter() {}
    template <typename TargetState>
    void TransitImpl()
    {
        typedef Transition<state_type, TargetState> transit_type;

        GetStateMachine().template TransitState<transit_type>();
    }
    //状态类父类（StateImpl）中的一个函数：
    virtual result Dispatch(StateBasePtr &s, EventBasePtr &e)
    {
        typedef typename state_type::reactions list;
        typedef typename mpl::begin<list>::type first;
        typedef typename mpl::end<list>::type last;

        return DispatchImpl<first, last>::Dispatch(s, e);
    }

    //派遣函数中调用这个括号运算符重载，然后将事件压入延迟事件
    //然后调用状态StateImpl中的，一个结构体名为DispatchImpl中的函数（与StateImpl中的Dispatch函数不同，只是同名而已）
    //这里面，传入的状态s强转成当前类型，然后调用函数（react函数已经定义成了一些宏，协议栈中只需要用宏来写就行了。运行到这里的时候，编译器应该会自动找到匹配的react函数，然后调用函数）
    //例如：在Mac层的协议里面
    //struct Top : hsm::State<Top, hsm::StateMachine, Idle>{
	//typedef hsm_vector<MsgRecvDataNtf, MsgTimeOut> reactions;
	//HSM_WORK(MsgRecvDataNtf, &NewAloha::SendUp);
	//HSM_WORK(MsgTimeOut, &NewAloha::ReSendDown);

    template <typename Begin, typename End>
    struct DispatchImpl
    {
        static result Dispatch(StateBasePtr &s, EventBasePtr &e)
        {
            typedef typename Begin::type event_type;
            if (e->IsType<event_type>()) {
                // use struct Dispatcher to find out the real type of event,
                // then cast it, so that we can call the right react function.
                // beacuse react function is not virtual, so we need to cast
                // state to the right type too
                // return std_dpc<state_type>(s)->react(std_dpc<event_type>(e));
                static auto a =
                    std_dpc<state_type>(s)->react(std_dpc<event_type>(e));
                return a(std_dpc<state_type>(s), std_dpc<event_type>(e));
            } else {
                return DispatchImpl<typename mpl::next<Begin>::type,
                                    End>::Dispatch(s, e);
            }
        }
    };

    template <typename End>
    struct DispatchImpl<End, End>
    {
        static result Dispatch(StateBasePtr &, const EventBasePtr &)
        {
            return no_reaction;
        }
    };
};

void *StateBase::GetOwner() { return GetStateMachine().GetOwner(); }
template <typename SelfType, typename OuterState, typename InitState = SelfType>
struct State : StateImpl<SelfType, OuterState, InitState>
{
    typedef SelfType state_type;
    typedef OuterState outer_state_type;
    typedef InitState init_state_type;

    virtual void TransitOuter()
    {
        this->GetStateMachine().template SetState<outer_state_type>(false);
    }
//状态中的一个成员函数，即处理函数，在返回值为transit的时候，处理延迟队列中的事件
//状态中的处理函数，之后交给Dispatch函数（父类StateImpl中的一个函数）
    virtual void Process(StateBasePtr &s, EventBasePtr &e)
    {
        switch (this->Dispatch(s, e)) {
            case no_reaction: {
                auto state = s->GetStateMachine().GetState<outer_state_type>();
                state->Process(state, e);
                break;
            }
            case do_discard:
            case do_defer:
            case do_work:
                break;
            case do_transit:
                this->GetStateMachine().ProcessDeferredEvent();
                break;
        }
    }
};

template <typename SelfType, typename InitState>
struct State<SelfType, StateMachine, InitState>
    : StateImpl<SelfType, StateMachine, InitState>
{
    typedef SelfType state_type;
    typedef StateMachine outer_state_type;
    typedef InitState init_state_type;

    typedef mpl::vector<> reactions;
//一般情况下，如果需要交给父状态处理，是不需要先转移到父状态的。但是如果父状态处理时，需要进行状态转移，则交给状态机中的TransitState函数，从函数内容可以知道需要先转移到外部状态，然后再进行状态转移
    virtual void Process(StateBasePtr &s, EventBasePtr &e)
    {
        switch (this->Dispatch(s, e)) {
            case no_reaction:
            case do_discard:
            case do_defer:
            case do_work:
                break;
            case do_transit:
                this->GetStateMachine().ProcessDeferredEvent();
                break;
        }
    }
};

}  // end namespace hsm

#endif  // _hsm_H_
