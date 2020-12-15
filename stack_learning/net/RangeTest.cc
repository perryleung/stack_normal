#include "hsm.h"
#include "message.h"
#include "module.h"
#include "sap.h"
#include "schedule.h"
#include "packet.h"
#include "trace.h"
#include "app_datapackage.h"
#include <stdlib.h>
#include <vector>
#include <ev.h>
#include "libev_tool.h"
#include "client.h"

#define CURRENT_PID 8

namespace range_test {
#if NET_PID == CURRENT_PID

using msg::MsgSendDataReq;
using msg::MsgSendDataRsp;
using msg::MsgRecvDataNtf;
using msg::MsgPosNtf;

struct Top;
struct Idle;

struct RangeTestHeader
{
	uint8_t isSender;
};

//定义宏，广播地址
const uint8_t BROADCAST = 255;

class RangeTest : public mod::Module<RangeTest, NET_LAYER, CURRENT_PID>
{
public:
	RangeTest()
	{
            GetSap().SetOwner(this);
            GetHsm().Initialize<Top>(this);
            GetTap().SetOwner(this);
	}
	
	void Init()
	{
		uint16_t x = (uint16_t)Config::Instance()["RangeTest"]["x"].as<int>();
		uint16_t y = (uint16_t)Config::Instance()["RangeTest"]["y"].as<int>();
		uint16_t z = (uint16_t)Config::Instance()["RangeTest"]["z"].as<int>();
        msg::Position tempPos(x, y, z);
        Ptr<MsgPosNtf> m(new MsgPosNtf);
        m->ntfPos = tempPos;
        SendNtf(m, PHY_LAYER, PHY_PID);
	}
	
	void send(const Ptr<MsgSendDataReq> &msg);
	void receive(const Ptr<MsgRecvDataNtf> &msg);
};

struct Top : hsm::State<Top, hsm::StateMachine, Idle>
{
};

struct Idle : hsm::State<Idle, Top>
{	
	typedef hsm_vector<MsgSendDataReq, MsgRecvDataNtf> reactions;

	HSM_WORK(MsgSendDataReq, &RangeTest::send);
	HSM_WORK(MsgRecvDataNtf, &RangeTest::receive);
};

void RangeTest::send(const Ptr<MsgSendDataReq> &msg) {
	LOG(INFO) << "send";
	RangeTestHeader *header = msg->packet.Header<RangeTestHeader>();
	header->isSender = 1;
	//往下发
	msg->packet.Move<RangeTestHeader>();
	msg->address = BROADCAST;
	SendReq(msg, MAC_LAYER, MAC_PID);
	//回上层Rsp
	Ptr<MsgSendDataRsp> m(new MsgSendDataRsp);
	SendRsp(m, TRA_LAYER, TRA_PID);
}

void RangeTest::receive(const Ptr<MsgRecvDataNtf> &msg) {
	LOG(INFO) << "receive";
	RangeTestHeader *header = msg->packet.Header<RangeTestHeader>();
	if (header->isSender == 1) {
		LOG(INFO) << "receive!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!";
	}
}
MODULE_INIT(RangeTest)
PROTOCOL_HEADER(RangeTestHeader)
#endif
}
