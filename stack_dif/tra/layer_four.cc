#include <iostream>
#include "hsm.h"
#include "message.h"
#include "module.h"
#include "sap.h"

#define LOWER_LAYER 3
#define LOWER_PROTOCOL 4
#define CURRENT_LAYER 4
#define CURRENT_PROTOCOL 2

namespace layer_four {

/*
 * declare Events
 */
using msg::MsgSendDataReq;
using msg::MsgSendAckReq;
using msg::MsgSendDataRsp;
using msg::MsgSendAckRsp;
using msg::MsgRecvDataNtf;
using msg::MsgTimeOut;
using msg::MsgSendAckNtf;

using pkt::Packet;

struct LayerState;
struct Idle;
struct WaitRsp;

struct LayerFour;
struct Idle;

struct LayerFour : mod::Module<LayerFour, CURRENT_LAYER, CURRENT_PROTOCOL>
{
    LayerFour()
    {
        GetSap().SetOwner(this);
        GetTap().SetOwner(this);
        GetHsm().Initialize<LayerState>(this);
    }

    void Init() { LOG(INFO) << "LayerFour Init"; }
    void SendDown(const Ptr<MsgSendDataReq> &msg)
    {
        SendReq(msg, LOWER_LAYER, LOWER_PROTOCOL);
    }
};

struct LayerState : hsm::State<LayerState, hsm::StateMachine, Idle>
{
};

struct Idle : hsm::State<Idle, LayerState>
{
    typedef hsm_vector<MsgSendDataReq> reactions;

    HSM_WORK(MsgSendDataReq, &LayerFour::SendDown);
};

// MODULE_INIT(LayerFour)
}  // end namespace layer_four
