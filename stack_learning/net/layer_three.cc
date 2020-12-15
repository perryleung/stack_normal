#include "hsm.h"
#include "message.h"
#include "module.h"
#include "sap.h"

namespace layer_three {

typedef msg::MsgSendDataReq EvReq;
typedef msg::MsgSendDataRsp EvRsp;
typedef msg::MsgRecvDataNtf EvNtf;

typedef msg::MsgSendDataReqPtr EvReqPtr;
typedef msg::MsgSendDataRspPtr EvRspPtr;
typedef msg::MsgRecvDataNtfPtr EvNtfPtr;

struct EvTimeOut : hsm::Event<EvTimeOut>
{
};
typedef std::tr1::shared_ptr<EvTimeOut> EvTimeOutPtr;

struct LayerState;
struct Ready;
struct WaitRsp;

struct LayerThree;

struct LayerState : hsm::State<LayerState, hsm::StateMachine, Ready>
{
};

struct Ready : hsm::StateWithOwner<Ready, LayerState, LayerThree>
{
    typedef hsm_vector<EvReq, EvNtf> reactions;

    hsm::result react(const EvReqPtr &);
    hsm::result react(const EvNtfPtr &);
};

struct WaitRsp : hsm::StateWithOwner<WaitRsp, LayerState, LayerThree>
{
    typedef hsm_vector<EvReq, EvRsp, EvNtf, EvTimeOut> reactions;

    hsm::result react(const EvReqPtr &);
    hsm::result react(const EvRspPtr &);
    hsm::result react(const EvNtfPtr &);
    hsm::result react(const EvTimeOutPtr &);
};

struct LayerThree : mod::Module<LayerThree, 3, 3>
{
    LayerThree()
    {
        GetSap().SetOwner(this);
        GetTap().SetOwner(this);
        GetHsm().initialize<LayerState>(this);
    }

    void Init() { LOG(INFO) << "LayerThree Init"; }
    void TimeOut() { std::cout << "LayerThree Timeout\n"; }
};

hsm::result Ready::react(const EvReqPtr &msg)
{
    LOG(INFO) << "LayerThree receive data from upper layer";

    Owner()->SendReq(msg, 2, 2);

    EvRspPtr p(new EvRsp);
    Owner()->SendRsp(p, 4, 4);

    LOG(INFO) << "LayerThree transit state: Ready --> WaitRsp";
    return transit<WaitRsp>();
}
hsm::result Ready::react(const EvNtfPtr &msg)
{
    LOG(INFO) << "LayerThree receive data from lower layer, and send it to "
                 "upper layer";
    Owner()->SendNtf(msg, 4, 4);
    return finish();
}

hsm::result WaitRsp::react(const EvReqPtr &)
{
    LOG(INFO) << "LayerThree receive a Request at state WaitRsp, defer it";
    return defer();
}
hsm::result WaitRsp::react(const EvRspPtr &)
{
    LOG(INFO) << "LayerThree receive Response at state WaitRsp";
    LOG(INFO) << "LayerThree transit state: WaitRsp --> Ready";
    return transit<Ready>();
}
hsm::result WaitRsp::react(const EvNtfPtr &)
{
    LOG(INFO) << "LayerThree receive a Notification at state WaitRsp, defer it";
    return defer();
}
hsm::result WaitRsp::react(const EvTimeOutPtr &)
{
    LOG(INFO) << "receive timeout";
    return transit<Ready>();
}

// MODULE_INIT(LayerThree)

}  // end namespace layer_three
