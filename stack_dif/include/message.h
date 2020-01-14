#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#include <string>
#include <memory>
#include "hsm.h"
#include "packet.h"

typedef hsm::EventBase Message;
typedef std::shared_ptr<Message> MessagePtr;

namespace msg {

struct MsgError : hsm::Event<MsgError>
{
};

struct MsgSendDataReq : hsm::Event<MsgSendDataReq>
{
	bool needCRC = false;
    bool isData;
    int address;
    pkt::Packet packet;
};
typedef std::shared_ptr<MsgSendDataReq> MsgSendDataReqPtr;

struct MsgSendAckReq : hsm::Event<MsgSendAckReq>
{
    pkt::Packet packet;
};
typedef std::shared_ptr<MsgSendAckReq> MsgSendAckReqPtr;

struct MsgSendDataRsp : hsm::Event<MsgSendDataReq>
{
};
typedef std::shared_ptr<MsgSendDataRsp> MsgSendDataRspPtr;

struct MsgSendAckRsp : hsm::Event<MsgSendAckRsp>
{
};
typedef std::shared_ptr<MsgSendAckRsp> MsgSendAckRspPtr;

struct MsgRecvDataNtf : hsm::Event<MsgSendDataReq>
{
	bool needCRC = false;
    pkt::Packet packet;
    int reservation;
};
typedef std::shared_ptr<MsgRecvDataNtf> MsgRecvDataNtfPtr;

struct MsgSendAckNtf : hsm::Event<MsgSendAckNtf>
{
    bool send;
};

struct MsgTimeOut : hsm::Event<MsgTimeOut>
{
    uint16_t msgId;
};
typedef std::shared_ptr<MsgTimeOut> MsgTimeOutPtr;

struct MsgBase
{
    int SrcLayer;
    int SrcPid;
    int DstLayer;
    int DstPid;
    MessagePtr msg;
};

struct Position
{
    uint16_t x;
    uint16_t y;
    uint16_t z;
    Position(uint16_t tx = 0, uint16_t ty = 0, uint16_t tz = 0) : x(tx), y(ty), z(tz){}
};

struct MsgPosReq : hsm::Event<MsgPosReq>
{
    Position reqPos;
};

struct MsgPosRsp : hsm::Event<MsgPosRsp>
{
    Position rspPos;
};

struct MsgPosNtf : hsm::Event<MsgPosNtf>
{
    Position ntfPos;
};


typedef MsgBase MsgReq;
typedef MsgBase MsgRsp;
typedef MsgBase MsgNtf;

typedef std::shared_ptr<msg::MsgReq> MsgReqPtr;
typedef std::shared_ptr<msg::MsgRsp> MsgRspPtr;
typedef std::shared_ptr<msg::MsgNtf> MsgNtfPtr;

}  // end namespace msg

#endif  // _MESSAGE_H_
