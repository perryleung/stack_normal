#include <boost/regex.hpp>
#include <cassert>
#include <string>
#include <cstring>
#include <string>
#include "dap.h"
#include "driver.h"
#include "event.h"
#include "hsm.h"
#include "message.h"
#include "packet.h"
#include "schedule.h"
#include "serial.h"

#define CURRENT_PID 2

namespace aquasent {
#if (PHY_PID | CURRENT_PID) == PHY_PID

using std::string;
using serial::Serial;

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

static bool correct_data = false;

/*
 * Send Data
 * Read Register
 * Write Register
 * Change Mode
 *
 * Error Info
 * Receive Data
 * Boot Info
 */

#define MODEM_ERROR(XX) XX(MMERR, "\\$MMERR,([0-9]+)")

// Data Info Index, Data
#define MODEM_DATA(XX)                                                         \
    XX(MMRXA, "\\$MMRXA,([0-9]+),([0-9]+),(.*)")                               \
    XX(MMRXD, "\\$MMRXD,([0-9]+),([0-9]+),(.*)")

#define MODEM_TDN(XX) XX(MMTDN, "\\$MMTDN,([0-9]+),([0-9]+)")

#define SENDDATA_COMMAND(XX)                                                   \
    XX(CMD_HHTXD, "$HHTXD,%d,%d,%d,%s\r\n", "\\$MMOKY,HHTXD,([0-9]+)")         \
    XX(CMD_HHTXA, "$HHTXA,%d,%d,%d,%s\r\n", "\\$MMOKY,HHTXA,([0-9]+)")

// Command Index, Command, Response
#define COMMAND_RESPONSE(XX)                                                         \
    XX(CMD_HHCTD,         "$HHCTD\r\n",            "\\$MMOKY,HHCTD")                 \
    XX(CMD_ESCAPE,        "%s\r\n",                "\\$MMOKY")                       \
    XX(CMD_HHCRR_MID,     "$HHCRR,MID\r\n",        "\\$MMOKY,HHCRR,MID,([0-9]+)")    \
    XX(CMD_HHCRR_BD,      "$HHCRR,BD\r\n",         "\\$MMOKY,HHCRR,BD,([0-9]+)")     \
    XX(CMD_HHCRR_ECHO,    "$HHCRR,ECHO\r\n",       "\\$MMOKY,HHCRR,ECHO,([0-9])")    \
    XX(CMD_HHCRR_MMCHK,   "$HHCRR,MMCHK\r\n",      "\\$MMOKY,HHCRR,MMCHK,([0-9])")   \
    XX(CMD_HHCRR_PGAP,    "$HHCRR,PGAP\r\n",       "\\$MMOKY,HHCRR,PGAP,([0-9]+)")   \
    XX(CMD_HHCRR_ESC,     "$HHCRR,ESC\r\n",        "\\$MMOKY,HHCRR,ESC,(.+)")        \
    XX(CMD_HHCRR_RXFMT,   "$HHCRR,RXFMT\r\n",      "\\$MMOKY,HHCRR,RXFMT,([0-9])")   \
    XX(CMD_HHCRR_TXDONE,  "$HHCRR,TXDONE\r\n",     "\\$MMOKY,HHCRR,TXDONE,([0-9])")  \
    XX(CMD_HHCRR_UIMODE,  "$HHCRR,UIMODE\r\n",     "\\$MMOKY,HHCRR,UIMODE,([A-Z]+)") \
    XX(CMD_HHCRR_CMDTERM, "$HHCRR,CMDTERM\r\n",    "\\$MMOKY,HHCRR,CMDTERM,(.*)")    \
    XX(CMD_HHCRR_DEBUG_R, "$HHCRR,DEBUG,R\r\n",    "\\$MMOKY,HHCRR,DEBUG,(.*)")      \
    XX(CMD_HHCRR_DEBUG_T, "$HHCRR,DEBUG,T\r\n",    "\\$MMOKY,HHCRR,DEBUG,(.*)")      \
    XX(CMD_HHCRR_ERRPKT,  "$HHCRR,ERRPKT\r\n",     "\\$MMOKY,HHCRR,ERRPKT,([0-9]+)") \
    XX(CMD_HHCRR_RTC,     "$HHCRR,RTC\r\n",        "\\$MMOKY,HHCRR,RTC,(.*)")        \
    XX(CMD_HHCRR_GDT,     "$HHCRR,GDT\r\n",        "\\$MMOKY,HHCRR,GDT,([0-9]+)")    \
    XX(CMD_HHCRR_NUMCHL,  "$HHCRR,NUMCHL\r\n",     "\\$MMOKY,HHCRR,NUMCHL,([0-9]+)") \
    XX(CMD_HHCRR_RXGAIN,  "$HHCRR,RXGAIN\r\n",     "\\$MMOKY,HHCRR,RXGAIN,([0-9]+)") \
    XX(CMD_HHCRR_TXPWR,   "$HHCRR,TXPWR\r\n",      "\\$MMOKY,HHCRR,TXPWR,([-0-9]+)") \
    XX(CMD_HHCRR_RXFLG,   "$HHCRR,RXFLG\r\n",      "\\$MMOKY,HHCRR,RXFLG,(*)")       \
    XX(CMD_HHCRR_TXDST,   "$HHCRR,TXDST\r\n",      "\\$MMOKY,HHCRR,TXDST,([0-9]+)")  \
    XX(CMD_HHCRR_TXMODE,  "$HHCRR,TXMODE\r\n",     "\\$MMOKY,HHCRR,TXMODE,([0-9]+)") \
    XX(CMD_HHCRR_TXTYPE,  "$HHCRR,TXTYPE\r\n",     "\\$MMOKY,HHCRR,TXTYPE,([0-9]+)") \
    XX(CMD_HHCRR_BIV_C5,  "$HHCRR,BIV,C5\r\n",     "\\$MMOKY,HHCRR,BIV,C5,(.*)")     \
    XX(CMD_HHCRR_BIV_C7,  "$HHCRR,BIV,C7\r\n",     "\\$MMOKY,HHCRR,BIV,C7,(.*)")     \
    XX(CMD_HHCRR_LSMODE,  "$HHCRR,LSMODE\r\n",     "\\$MMOKY,HHCRR,LSMODE,([0-9])")  \
    XX(CMD_HHCRR_TSTDB,   "$HHCRR,TSTDB\r\n",      "\\$MMOKY,HHCRR,TSTDB,([0-9]+)")  \
    XX(CMD_HHCRW_MID,     "$HHCRW,MID,%d\r\n",     "\\$MMOKY,HHCRW,MID")             \
    XX(CMD_HHCRW_BD,      "$HHCRW,BD,%d\r\n",      "\\$MMOKY,HHCRW,BD")              \
    XX(CMD_HHCRW_ECHO,    "$HHCRW,ECHO,%d\r\n",    "\\$MMOKY,HHCRW,ECHO")            \
    XX(CMD_HHCRW_MMCHK,   "$HHCRW,MMCHK,%d\r\n",   "\\$MMOKY,HHCRW,MMCHK")           \
    XX(CMD_HHCRW_PGAP,    "$HHCRW,PGAP,%d\r\n",    "\\$MMOKY,HHCRW,PGAP")            \
    XX(CMD_HHCRW_ESC,     "$HHCRW,ESC,%s\r\n",     "\\$MMOKY,HHCRW,ESC")             \
    XX(CMD_HHCRW_RXFMT,   "$HHCRW,RXFMT,%d\r\n",   "\\$MMOKY,HHCRW,RXFMT")           \
    XX(CMD_HHCRW_TXDONE,  "$HHCRW,TXDONE,%d\r\n",  "\\$MMOKY,HHCRW,TXDONE")          \
    XX(CMD_HHCRW_UIMODE,  "$HHCRW,UIMODE,%d\r\n",  "\\$MMOKY,HHCRW,UIMODE")          \
    XX(CMD_HHCRW_CMDTERM, "$HHCRW,CMDTERM,%s\r\n", "\\$MMOKY,HHCRW,CMDTERM")         \
    XX(CMD_HHCRW_DEBUG_R, "$HHCRW,DEBUG,R,%d\r\n", "\\$MMOKY,HHCRW,DEBUG")           \
    XX(CMD_HHCRW_DEBUG_T, "$HHCRW,DEBUG,T,%d\r\n", "\\$MMOKY,HHCRW,DEBUG")           \
    XX(CMD_HHCRW_RTC,     "$HHCRW,RTC,%s\r\n",     "\\$MMOKY,HHCRW,RTC")             \
    XX(CMD_HHCRW_GDT,     "$HHCRW,GDT,%d\r\n",     "\\$MMOKY,HHCRW,GDT")             \
    XX(CMD_HHCRW_RXGAIN,  "$HHCRW,RXGAIN,%d\r\n",  "\\$MMOKY,HHCRW,RXGAIN")          \
    XX(CMD_HHCRW_TXPWR,   "$HHCRW,TXPWR,%d\r\n",   "\\$MMOKY,HHCRW,TXPWR")           \
    XX(CMD_HHCRW_RXFLG,   "$HHCRW,RXFLG,%d\r\n",   "\\$MMOKY,HHCRW,RXFLG")           \
    XX(CMD_HHCRW_TXDST,   "$HHCRW,TXDST,%d\r\n",   "\\$MMOKY,HHCRW,TXDST")           \
    XX(CMD_HHCRW_TXMODE,  "$HHCRW,TXMODE,%d\r\n",  "\\$MMOKY,HHCRW,TXMODE")          \
    XX(CMD_HHCRW_TXTYPE,  "$HHCRW,TXTYPE,%d\r\n",  "\\$MMOKY,HHCRW,TXTYPE")          \
    XX(CMD_HHCRW_LSMODE,  "$HHCRW,LSMODE,%d\r\n",  "\\$MMOKY,HHCRW,LSMODE")          \
    XX(CMD_HHCRW_TSTDB,   "$HHCRW,TSTDB,%d\r\n",   "\\$MMOKY,HHCRW,TSTDB")           \
    XX(CMD_HHCRW_FXN,     "$HHCRW,FXN,%s\r\n",     "\\$MMOKY,HHCRW,FXN")

enum COMMAND_INDEX {
#define XX(i, c, r) i,
    SENDDATA_COMMAND(XX) COMMAND_RESPONSE(XX)
#undef XX
};

enum DATA_INDEX {
#define XX(i, r) i,
    MODEM_DATA(XX)
#undef XX
};

enum ERROR_INDEX {
#define XX(i, r) i,
    MODEM_ERROR
#undef XX
};

const static char *command_string[] = {
#define XX(i, c, r) c,
    COMMAND_RESPONSE(XX)
#undef XX
};

#define hex_to_num(c) ((unsigned char)(c >= 'A' ? c - 'A' + 10 : c - '0'))

static const char hex_table[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                 '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

template <typename T>
struct EvBase : hsm::Event<T>
{
    DATA_INDEX data_index;
    boost::cmatch result;
};

struct EvTxCmd : EvBase<EvTxCmd>
{
};
struct EvRxErr : EvBase<EvRxErr>
{
};
struct EvRxUnknow : EvBase<EvRxUnknow>
{
};
struct EvRxData : EvBase<EvRxData>
{
};
struct EvRxDataBegin : EvBase<EvRxDataBegin>
{
};
struct EvRxDataEnd : EvBase<EvRxDataEnd>
{
};
struct EvRxRsp : EvBase<EvRxRsp>
{
};
struct EvRxRspEnd : EvBase<EvRxRspEnd>
{
};
struct EvRxTdn : EvBase<EvRxTdn>
{
};
struct EvRxTxRsp : EvBase<EvRxTxRsp>
{
};

class Aquasent;

class Parser
{
public:
    void Parse(const char *);
    inline void SetCmd(COMMAND_INDEX cmd) { last_cmd_ = cmd; }
    inline void SetOwner(Aquasent *owner) { owner_ = owner; }
    inline Aquasent *Owner() const { return owner_; }
private:
    COMMAND_INDEX last_cmd_;

    Aquasent *owner_;
};

struct Top;
struct Idle;
struct WaitRsp;
struct WaitTdn;

class Aquasent : public drv::Driver<Aquasent, PHY_LAYER, CURRENT_PID>
{
public:
    Aquasent()
    {
        GetSap().SetOwner(this);
        GetDap().SetOwner(this);
        GetHsm().Initialize<Top>(this);
        GetParser().SetOwner(this);
    }

    void Init()
    {
        LOG(INFO) << "Aquasent Init";

        std::string port = Config::Instance()["aquasent"]["port"].as<std::string>();
        std::string baudrate = Config::Instance()["aquasent"]["baudrate"].as<std::string>();

        if (GetSerial().Init(port, baudrate)) {
            LOG(INFO) << "Map the opened FD of serial and DAP";
            GetDap().Map(GetFd());
        }
    }

    void SendDown(const Ptr<MsgSendDataReq> &);
    void SendDown1(const Ptr<MsgSendAckReq> &);
    void SendUp(const Ptr<EvRxData> &);
    void SendBackRsp(const Ptr<EvRxRsp> &);
    void SendDataRsp(const Ptr<EvRxTdn> &);

    void HandleMMRXA(int, int, const string &);
    void HandleMMRXD(int, int, const string &);

    void Notify()
    {
        // std::cout << "Aquasent recv data from device, send it to aloha.\n";

        static char *buffer = new char[1024];
        static int len      = 0;

        IODataPtr data = ReadQueue::Front();

        char *p = data->data;
        int l   = data->len;

        for (int i = 0; i < l; ++i) {
            if (p[i] == '\r') {
                buffer[len] = '\0';
                parser_.Parse(buffer);
                len = 0;
            } else if (p[i] == '\n') {
                ;
            } else {
                buffer[len++] = p[i];
            }
        }

        ReadQueue::Pop();
    }

    inline void Byte2Hex(char *hex, const char *src, size_t size)
    {
        size_t i;
        for (i = 0; i < size; i++) {
            hex[i * 2]     = hex_table[src[i] >> 4 & 0x0F];
            hex[i * 2 + 1] = hex_table[src[i] & 0x0F];
        }
    }

    inline void Hex2Byte(char *dst, const char *hex, size_t size)
    {
        size_t i = 0;
        for (i = 0; i < size; i += 2) {
            dst[i / 2] = (hex_to_num(hex[i]) << 4) + hex_to_num(hex[i + 1]);
        }
    }

private:
    inline Serial &GetSerial() { return serial_; }
    inline int GetFd() { return GetSerial().GetFd(); }
    inline Parser &GetParser() { return parser_; }
private:
    Serial serial_;
    Parser parser_;
};

struct Top : hsm::State<Top, hsm::StateMachine, Idle>
{
    typedef hsm_vector<MsgSendDataReq, EvRxErr, EvRxData> reactions;

    HSM_DISCARD(EvRxErr);
    HSM_WORK(EvRxData, &Aquasent::SendUp);
    HSM_DEFER(MsgSendDataReq);
};
struct Idle : hsm::State<Idle, Top>
{
    typedef hsm_vector<MsgSendDataReq, MsgSendAckReq> reactions;

    HSM_TRANSIT(MsgSendDataReq, WaitRsp, &Aquasent::SendDown);
    HSM_TRANSIT(MsgSendAckReq, WaitRsp, &Aquasent::SendDown1);
};
struct WaitRsp : hsm::State<WaitRsp, Top>
{
    typedef hsm_vector<EvRxRsp, EvRxTxRsp> reactions;

    HSM_TRANSIT(EvRxRsp, Idle, &Aquasent::SendBackRsp);
    HSM_TRANSIT(EvRxTxRsp, WaitTdn);
};
struct WaitTdn : hsm::State<WaitTdn, Top>
{
    typedef hsm_vector<EvRxTdn> reactions;

    HSM_TRANSIT(EvRxTdn, Idle, &Aquasent::SendDataRsp);
};

void Parser::Parse(const char *input)
{
    // response for command
    boost::regex rsp("^(\\$MMOKY)");
    // MMTDN
    boost::regex tdn("^(\\$MMTDN)");
    // input data from other modem
    boost::regex dat("^((\\$MMRXA)|(\\$MMRXD))");
    // error info
    boost::regex err("^(\\$MMERR)");

    boost::cmatch result;

    if (boost::regex_search(input, rsp)) {
        switch (last_cmd_) {
            case CMD_HHTXA: {
                boost::regex reg("\\$MMOKY,HHTXA,([0-9]+)");
                if (boost::regex_match(input, result, reg)) {
                    Ptr<EvRxTxRsp> p(new EvRxTxRsp);
                    p->result = result;
                    Owner()->GetHsm().ProcessEvent(p);
                }
                break;
            }
            case CMD_HHTXD: {
                boost::regex reg("\\$MMOKY,HHTXD,([0-9]+)");
                if (boost::regex_match(input, result, reg)) {
                    Ptr<EvRxTxRsp> p(new EvRxTxRsp);
                    p->result = result;
                    Owner()->GetHsm().ProcessEvent(p);
                }
                break;
            }
#define XX(i, c, r)                                                            \
    case i: {                                                                  \
        boost::regex reg(r);                                                   \
        if (boost::regex_match(input, result, reg)) {                          \
            Ptr<EvRxRsp> p(new EvRxRsp);                                       \
            p->result = result;                                                \
            Owner()->GetHsm().ProcessEvent(p);                                 \
        }                                                                      \
        break;                                                                 \
    }

                COMMAND_RESPONSE(XX)

#undef XX
        }

    } else if (boost::regex_search(input, dat)) {
#define XX(i, r) boost::regex regex_##i(r);
        MODEM_DATA(XX)
#undef XX

        if (boost::regex_match(input, result, regex_MMRXA)) {
            Ptr<EvRxData> p(new EvRxData);
            p->result     = result;
            p->data_index = MMRXA;
            Owner()->GetHsm().ProcessEvent(p);
        } else if (boost::regex_match(input, result, regex_MMRXD)) {
            Ptr<EvRxData> p(new EvRxData);
            p->result     = result;
            p->data_index = MMRXD;
            Owner()->GetHsm().ProcessEvent(p);
        }
    } else if (boost::regex_search(input, err)) {
        boost::regex regex_err("\\$MMERR,([0-9]+)");
        if (boost::regex_match(input, result, regex_err)) {
            Ptr<EvRxErr> p(new EvRxErr);
            p->result = result;
            Owner()->GetHsm().ProcessEvent(p);
        }
    } else if (boost::regex_search(input, tdn)) {
        boost::regex regex_err("\\$MMTDN,([0-9]+),([0-9]+)");
        if (boost::regex_match(input, result, regex_err)) {
            Ptr<EvRxTdn> p(new EvRxTdn);
            p->result = result;
            Owner()->GetHsm().ProcessEvent(p);
        }
    }
}

void Aquasent::SendDown(const Ptr<MsgSendDataReq> &msg)
{
    LOG(INFO) << "Aquasent receive data from upper layer";

    // msg::MsgSendDataRspPtr r(new msg::MsgSendDataRsp);
    // Owner()->SendRsp(r, 2, 2);

    LOG(INFO) << "Try to send packet to the Aquasent Modem";

    // active the write event in reactor and then
    // make packet and push it to the write queue
    GetSerial().Write();

    auto pkt = msg->packet;

    char *data = new char[13 + pkt.Size() * 2 + 2];

    std::strncpy(data, "$HHTXD,0,0,0,", 13);

    Byte2Hex(data + 13, pkt.Raw(), pkt.Size());

    data[13 + pkt.Size() * 2]     = '\r';
    data[13 + pkt.Size() * 2 + 1] = '\n';

    IODataPtr p(new IOData);

    p->fd   = GetFd();
    p->data = data;
    p->len  = 13 + pkt.Size() * 2 + 2;

    WriteQueue::Push(p);
    GetParser().SetCmd(CMD_HHTXD);
}

void Aquasent::SendUp(const Ptr<EvRxData> &msg)
{
    LOG(INFO) << "Aquasent receive data from serial";

    if (msg->data_index == MMRXA) {
        HandleMMRXA(std::stod(msg->result[1]), std::stod(msg->result[2]),
                    msg->result[3]);
    } else if (msg->data_index == MMRXD) {
        HandleMMRXD(std::stod(msg->result[1]), std::stod(msg->result[2]),
                    msg->result[3]);
    }
}

void Aquasent::SendBackRsp(const Ptr<EvRxRsp> &msg) {}
void Aquasent::SendDataRsp(const Ptr<EvRxTdn> &msg)
{
    Ptr<MsgSendDataRsp> p(new MsgSendDataRsp);

    SendRsp(p, MAC_LAYER, MAC_PID);
}

void Aquasent::HandleMMRXA(int, int, const string &data)
{
    char *d = new char[data.size()];

    std::memcpy(d, data.c_str(), data.size());

    pkt::Packet pkt(d, data.size());

    msg::MsgRecvDataNtfPtr m(new msg::MsgRecvDataNtf);

    m->packet = pkt;

    SendNtf(m, MAC_LAYER, MAC_PID);
}

void Aquasent::HandleMMRXD(int, int, const string &data)
{
    LOG(INFO) << "Aquasent receive data from device";

    char *d = new char[data.size() / 2];

    Hex2Byte(d, data.c_str(), data.size());

 //   std::cout << "data is " << d << std::endl;

    pkt::Packet pkt(d, data.size());
	
    std::cout << "data is " << pkt.MoveP(30) << std::endl;

    msg::MsgRecvDataNtfPtr m(new msg::MsgRecvDataNtf);

    m->packet = pkt;

    SendNtf(m, MAC_LAYER, MAC_PID);
}
void Aquasent::SendDown1(const Ptr<MsgSendAckReq> &msg)
{
    LOG(INFO) << "Aquasent receive data from upper layer";

    // msg::MsgSendDataRspPtr r(new msg::MsgSendDataRsp);
    // Owner()->SendRsp(r, 2, 2);

    LOG(INFO) << "Try to send packet to the Aquasent Modem";

    // active the write event in reactor and then
    // make packet and push it to the write queue
    GetSerial().Write();

    auto pkt = msg->packet;

    char *data = new char[13 + pkt.Size() * 2 + 2];

    std::strncpy(data, "$HHTXD,0,0,0,", 13);

    Byte2Hex(data + 13, pkt.Raw(), pkt.Size());

    data[13 + pkt.Size() * 2]     = '\r';
    data[13 + pkt.Size() * 2 + 1] = '\n';

    IODataPtr p(new IOData);

    p->fd   = GetFd();
    p->data = data;
    p->len  = 13 + pkt.Size() * 2 + 2;

    WriteQueue::Push(p);
    GetParser().SetCmd(CMD_HHTXD);
}
MODULE_INIT(Aquasent)
#endif

}  // end namespace aquasent
