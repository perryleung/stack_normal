#include <cmath>
#include "hsm.h"
#include "message.h"
#include "module.h"
#include "sap.h"
#include "schedule.h"

#define UPPER_LAYER 4
#define UPPER_PROTOCOL 1
#define CURRENT_LAYER 3
#define CURRENT_PROTOCOL 1
#define LOWER_LAYER 2
#define LOWER_PROTOCOL 2

#define RANGE 800
#define RADIUS 350
#define NUMB 10
#define X 1000
#define Y 1000
#define Z 1000
#define R 1000
#define TP_X 1
#define TP_Y 1
#define TP_Z 1
#define T_MUL 1000000

namespace vbf {
#if NET_PID == CURRENT_PROTOCOL

using std::sqrt;

/*
 * declare Events
 */

using msg::MsgSendDataReq;
using msg::MsgSendDataRsp;
using msg::MsgRecvDataNtf;
using msg::MsgTimeOut;
using msg::MsgPosReq;
using msg::MsgPosRsp;
using msg::MsgSendAckNtf;
using msg::MsgSendDataReq;
using msg::Position;

/*
 * declare States
 */
struct Top;
struct Idle;
struct WaitSend;
struct ReEvaluate;

struct VbfHeader
{
    Position op;
    Position tp;
    Position fp;
    unsigned short range;
    unsigned short radius;
};

class Vbf : public mod::Module<Vbf, CURRENT_LAYER, CURRENT_PROTOCOL>
{
public:
    Vbf()
    {
        GetSap().SetOwner(this);
        GetTap().SetOwner(this);
        GetHsm().Initialize<Top>(this);
        tp_.x = TP_X;
        tp_.y = TP_Y;
        tp_.z = TP_Z;
        if (RADIUS <= R)
            del_ =
                T_MUL *
                ((sqrt((RADIUS * sqrt(X * X + Y * Y + Z * Z) / 2) / NUMB) / X) /
                 Y) /
                Z;
        else
            del_ =
                T_MUL *
                ((sqrt((R * sqrt(X * X + Y * Y + Z * Z) / 2) / NUMB) / X) / Y) /
                Z;
    }

    void Init() 
    {
        LOG(INFO) << "Vbf Init"; 

        YAML::Node position = Config::Instance()["vbf"]["position"];
        mp_.x = position[0].as<int>();
        mp_.y = position[1].as<int>();
        mp_.z = position[2].as<int>();

        LOG(INFO) << "Vbf get the node's position is "
                  << "[" << mp_.x << "," << mp_.y << "," << mp_.z << "]";
    }
    void GetPos(const Ptr<MsgPosRsp> &);
    void SendDown(const Ptr<MsgSendDataReq> &);
    void Forward(const Ptr<MsgTimeOut> &);
    void ReEvaluate(const Ptr<MsgTimeOut> &);

    void DoNothing(const Ptr<MsgRecvDataNtf> &) {}
    bool NeedToForward(const Ptr<MsgRecvDataNtf> &);
    bool IsSameAsLast(const Ptr<MsgRecvDataNtf> &);
    void IsSameAsLast2(const Ptr<MsgRecvDataNtf> &msg);

    void AddHead(VbfHeader *);
    bool IsMoreFar(VbfHeader *);
    bool IsTarget(VbfHeader *);
    bool NearTarget(VbfHeader *);
    bool InPipe(VbfHeader *);
    bool Diff(VbfHeader *, VbfHeader *);
    void UpdateFP(VbfHeader *);
    double CompWT(VbfHeader *);
    unsigned short CouPkt(VbfHeader *);
    void SendAckNtf(bool);

private:
    Ptr<MsgRecvDataNtf> last_msg_;

    unsigned short cout_pkt_ = 1;

    Position mp_;
    Position tp_;
    unsigned short range_  = RANGE;
    unsigned short radius_ = RADIUS;
    double del_;
    unsigned short r_ = R;
};

struct Top : hsm::State<Top, hsm::StateMachine, Idle>
{
    typedef hsm_vector<MsgPosRsp> reactions;

    HSM_WORK(MsgPosRsp, &Vbf::GetPos);
};

struct Idle : hsm::State<Idle, Top>
{
    typedef hsm_vector<MsgSendDataReq, MsgRecvDataNtf> reactions;

    HSM_WORK(MsgSendDataReq, &Vbf::SendDown);
    HSM_TRANSIT(MsgRecvDataNtf, WaitSend, &Vbf::NeedToForward);
};

struct WaitSend : hsm::State<WaitSend, Top>
{
    typedef hsm_vector<MsgSendDataReq, MsgRecvDataNtf, MsgTimeOut> reactions;

    HSM_DEFER(MsgSendDataReq);
    HSM_TRANSIT_DEFER(MsgRecvDataNtf, ReEvaluate, &Vbf::DoNothing,
                      &Vbf::IsSameAsLast);
    HSM_TRANSIT(MsgTimeOut, Idle, &Vbf::Forward);
};

struct ReEvaluate : hsm::State<ReEvaluate, Top>
{
    typedef hsm_vector<MsgSendDataReq, MsgRecvDataNtf, MsgTimeOut> reactions;

    HSM_DEFER(MsgSendDataReq);
    // TODO
    // Defer Event if it is not same as the last one
    HSM_WORK(MsgRecvDataNtf, &Vbf::IsSameAsLast2);
    HSM_TRANSIT(MsgTimeOut, Idle, &Vbf::ReEvaluate);
};

void Vbf::SendDown(const Ptr<MsgSendDataReq> &msg)
{
    LOG(INFO) << "Vbf receiver a packet from upper layer(Idle,MsgSendDataReq)";

    VbfHeader *header = msg->packet.Header<VbfHeader>();

    AddHead(header);

    msg->packet.Move<VbfHeader>();

    SendReq(msg, LOWER_LAYER, LOWER_PROTOCOL);

    LOG(INFO) << "Vbf send the packet to lower layer(Idle,MsgSendDataReq)";

    Ptr<MsgSendDataRsp> p(new MsgSendDataRsp);
    SendRsp(p, UPPER_LAYER, UPPER_PROTOCOL);
}

bool Vbf::NeedToForward(const Ptr<MsgRecvDataNtf> &msg)
{
    LOG(INFO) << "Vbf receive a packet from lower layer";

    VbfHeader *header = msg->packet.Header<VbfHeader>();

    if (IsTarget(header)) {
        //LOG(INFO) << msg->packet.Pdu();
        SendAckNtf(true);
        LOG(INFO) << "Vbf ask lower to send ack";
        LOG(INFO) << "Vbf send the packet to upper layer";
        msg->packet.Move<VbfHeader>();
        SendNtf(msg, UPPER_LAYER, UPPER_PROTOCOL);
    } else if (NearTarget(header)) {
        SendAckNtf(false);
        LOG(INFO) << "Vbf ask lower NOT to send ack";
        LOG(INFO) << "Vbf discard the packet";
    } else if (InPipe(header)) {
        if (IsMoreFar(header)) {
            SendAckNtf(false);
            LOG(INFO) << "Vbf ask lower NOT to send ack";
            LOG(INFO) << "Vbf discard the packet(far)";
        } else {
            double wt = CompWT(header);
            LOG(INFO) << "TimeOut is " << wt;
            SetTimer(wt);
            last_msg_ = msg;
            LOG(INFO) << "Vbf transit state: Idle --> "
                         "WaitSend";
            return true;
        }
    } else {
        SendAckNtf(false);
        LOG(INFO) << "Vbf ask lower NOT to send ack";
        LOG(INFO) << "Vbf discard the packet(!InPipe)";
    }

    return false;
}

bool Vbf::IsSameAsLast(const Ptr<MsgRecvDataNtf> &msg)
{
    LOG(INFO) << "Vbf receive a packet from lower layer";

    VbfHeader *l_header = last_msg_->packet.Header<VbfHeader>();

    VbfHeader *header = msg->packet.Header<VbfHeader>();

    if (Diff(l_header, header)) {
        LOG(INFO) << "Vbf defer the packet(Diff)(WaitSend,MsgRecvDataNtf)";
        return false;
    } else {
        LOG(INFO) << "Vbf transit state: WaitSend --> "
                     "ReEvaluate(!Diff)(WaitSend,MsgRecvDataNtf)";
        return true;
    }
}

void Vbf::Forward(const Ptr<MsgTimeOut> &msg)
{
    VbfHeader *header = last_msg_->packet.Header<VbfHeader>();

    UpdateFP(header);

    Ptr<MsgSendDataReq> m(new MsgSendDataReq);

    m->packet = last_msg_->packet;

    SendReq(m, LOWER_LAYER, LOWER_PROTOCOL);

    SendAckNtf(true);

    LOG(INFO) << "Vbf ask lower to send ack";

    LOG(INFO) << "Vbf send the packet to lower layer(WaitSend,MsgTimeOut)";
}

void Vbf::IsSameAsLast2(const Ptr<MsgRecvDataNtf> &msg)
{
    LOG(INFO)
        << "Vbf receive a packet from lower layer(ReEvaluate,MsgRecvDataNtf)";

    VbfHeader *l_header = last_msg_->packet.Header<VbfHeader>();
    VbfHeader *header = msg->packet.Header<VbfHeader>();

    if (Diff(l_header, header)) {
        LOG(INFO) << "Vbf defer the packet(Diff)(ReEvaluate,MsgRecvDataNtf)";
    } else {
        cout_pkt_ = cout_pkt_ + 1;
        LOG(INFO) << "Vbf discard the packet(!Diff)(ReEvaluate,MsgRecvDataNtf)";
    }
}

void Vbf::ReEvaluate(const Ptr<MsgTimeOut> &msg)
{
    if (cout_pkt_ <= 2) {
        VbfHeader *header = last_msg_->packet.Header<VbfHeader>();

        UpdateFP(header);

        SendReq(last_msg_, LOWER_LAYER, LOWER_PROTOCOL);

        SendAckNtf(true);
        LOG(INFO) << "Vbf ask lower to send ack";

        LOG(INFO)
            << "Vbf send the packet to lower layer(ReEvaluate,MsgTimeOut)";
    } else {
        SendAckNtf(false);
        LOG(INFO) << "Vbf ask lower NOT to send ack";
        LOG(INFO) << "Vbf discard the packet(ReEvaluate,MsgTimeOut)";
    }
}

void Vbf::GetPos(const Ptr<MsgPosRsp> &msg)
{
    mp_ = msg->pos;

    LOG(INFO) << "Target Position"
              << "[" << tp_.x << "," << tp_.y << "," << tp_.z << "]";
    LOG(INFO) << "Vbf get the node's position"
              << "[" << mp_.x << "," << mp_.y << "," << mp_.z << "]";
}
void Vbf::AddHead(VbfHeader *hdr)
{
    hdr->op.x   = mp_.x;
    hdr->op.y   = mp_.y;
    hdr->op.z   = mp_.z;
    hdr->fp.x   = mp_.x;
    hdr->fp.y   = mp_.y;
    hdr->fp.z   = mp_.z;
    hdr->tp.x   = tp_.x;
    hdr->tp.y   = tp_.y;
    hdr->tp.z   = tp_.z;
    hdr->range  = range_;
    hdr->radius = radius_;

    LOG(INFO) << "AddHead,"
              << "op.x," << hdr->op.x;
    LOG(INFO) << "AddHead,"
              << "op.y," << hdr->op.y;
    LOG(INFO) << "AddHead,"
              << "op.z," << hdr->op.z;
    LOG(INFO) << "AddHead,"
              << "fp.x," << hdr->fp.x;
    LOG(INFO) << "AddHead,"
              << "fp.y," << hdr->fp.y;
    LOG(INFO) << "AddHead,"
              << "fp.z," << hdr->fp.z;
    LOG(INFO) << "AddHead,"
              << "tp.x," << hdr->tp.x;
    LOG(INFO) << "AddHead,"
              << "tp.y," << hdr->tp.y;
    LOG(INFO) << "AddHead,"
              << "tp.z," << hdr->tp.z;
    LOG(INFO) << "AddHead,"
              << "range," << hdr->range;
    LOG(INFO) << "AddHead,"
              << "radius," << hdr->radius;
}

bool Vbf::IsTarget(VbfHeader *hdr)
{
    unsigned short tx, ty, tz;

    tx = hdr->tp.x;
    ty = hdr->tp.y;
    tz = hdr->tp.z;
    LOG(INFO) << "IsTarget,"
              << "tp.x," << tx;
    LOG(INFO) << "IsTarget,"
              << "tp.y," << ty;
    LOG(INFO) << "IsTarget,"
              << "tp.z," << tz;
    LOG(INFO) << "IsTarget,"
              << "mp.x," << mp_.x;
    LOG(INFO) << "IsTarget,"
              << "mp.y," << mp_.y;
    LOG(INFO) << "IsTarget,"
              << "mp.z," << mp_.z;

    if (tx == mp_.x && ty == mp_.y && tz == mp_.z)
        return 1;
    else
        return 0;
}

bool Vbf::NearTarget(VbfHeader *hdr)
{
    float fx, fy, fz, tx, ty, tz, r;
    float d_ft;

    fx   = hdr->fp.x;
    fy   = hdr->fp.y;
    fz   = hdr->fp.z;
    tx   = hdr->tp.x;
    ty   = hdr->tp.y;
    tz   = hdr->tp.z;
    r    = hdr->range;
    d_ft = sqrt((fx - tx) * (fx - tx) + (fy - ty) * (fy - ty) +
                (fz - tz) * (fz - tz));

    LOG(INFO) << "NearTarget,"
              << "fp.x," << fx;
    LOG(INFO) << "NearTarget,"
              << "fp.y," << fy;
    LOG(INFO) << "NearTarget,"
              << "fp.z," << fz;
    LOG(INFO) << "NearTarget,"
              << "tp.x," << tx;
    LOG(INFO) << "NearTarget,"
              << "tp.y," << ty;
    LOG(INFO) << "NearTarget,"
              << "tp.z," << tz;
    LOG(INFO) << "NearTarget,"
              << "d_ft," << d_ft;
    LOG(INFO) << "NearTarget,"
              << "range," << r;

    if (d_ft <= r)
        return 1;
    else
        return 0;
}

bool Vbf::InPipe(VbfHeader *hdr)
{
    float ox, oy, oz, mx, my, mz, tx, ty, tz, ra;
    float ux, uy, uz, vx, vy, vz;
    float uxv, u, prj;

    ox  = hdr->op.x;
    oy  = hdr->op.y;
    oz  = hdr->op.z;
    mx  = mp_.x;
    my  = mp_.y;
    mz  = mp_.z;
    tx  = hdr->tp.x;
    ty  = hdr->tp.y;
    tz  = hdr->tp.z;
    ra  = hdr->radius;
    ux  = tx - ox;
    uy  = ty - oy;
    uz  = tz - oz;
    vx  = mx - ox;
    vy  = my - oy;
    vz  = mz - oz;
    uxv = sqrt((uy * vz - uz * vy) * (uy * vz - uz * vy) +
               (uz * vx - ux * vz) * (uz * vx - ux * vz) +
               (ux * vy - uy * vx) * (ux * vy - uy * vx));
    u   = sqrt(ux * ux + uy * uy + uz * uz);
    prj = uxv / u;

    LOG(INFO) << "InPipe,"
              << "op.x," << ox;
    LOG(INFO) << "InPipe,"
              << "op.y," << oy;
    LOG(INFO) << "InPipe,"
              << "op.z," << oz;
    LOG(INFO) << "InPipe,"
              << "mp.x," << mx;
    LOG(INFO) << "InPipe,"
              << "mp.y," << my;
    LOG(INFO) << "InPipe,"
              << "mp.z," << mz;
    LOG(INFO) << "InPipe,"
              << "tp.x," << tx;
    LOG(INFO) << "InPipe,"
              << "tp.y," << ty;
    LOG(INFO) << "InPipe,"
              << "tp.z," << tz;
    LOG(INFO) << "InPipe,"
              << "u.x," << ux;
    LOG(INFO) << "InPipe,"
              << "u.y," << uy;
    LOG(INFO) << "InPipe,"
              << "u.z," << uz;
    LOG(INFO) << "InPipe,"
              << "v.x," << vx;
    LOG(INFO) << "InPipe,"
              << "v.y," << vy;
    LOG(INFO) << "InPipe,"
              << "v.z," << vz;
    LOG(INFO) << "InPipe,"
              << "radius," << ra;
    LOG(INFO) << "InPipe,"
              << "i," << uy * vz - uz * vy;
    LOG(INFO) << "InPipe,"
              << "j," << uz * vx - ux * vz;
    LOG(INFO) << "InPipe,"
              << "k," << ux * vy - uy * vx;
    LOG(INFO) << "InPipe,"
              << "i*i," << (uy * vz - uz * vy) * (uy * vz - uz * vy);
    LOG(INFO) << "InPipe,"
              << "j*j," << (uz * vx - ux * vz) * (uz * vx - ux * vz);
    LOG(INFO) << "InPipe,"
              << "k*k," << (ux * vy - uy * vx) * (ux * vy - uy * vx);
    LOG(INFO) << "InPipe,"
              << "i*i+j*j+k*k,"
              << (uy * vz - uz * vy) * (uy * vz - uz * vy) +
                     (uz * vx - ux * vz) * (uz * vx - ux * vz) +
                     (ux * vy - uy * vx) * (ux * vy - uy * vx);
    LOG(INFO) << "InPipe,"
              << "uxv," << uxv;
    LOG(INFO) << "InPipe,"
              << "u," << u;
    LOG(INFO) << "InPipe,"
              << "projection," << prj;

    if (prj <= ra)
        return 1;
    else
        return 0;
}

void Vbf::UpdateFP(VbfHeader *hdr)
{
    hdr->fp.x = mp_.x;
    hdr->fp.y = mp_.y;
    hdr->fp.z = mp_.z;

    LOG(INFO) << "UpdataFP,"
              << "fp.x" << hdr->fp.x;
    LOG(INFO) << "UpdataFP,"
              << "fp.y" << hdr->fp.y;
    LOG(INFO) << "UpdataFP,"
              << "fp.z" << hdr->fp.z;
}

double Vbf::CompWT(VbfHeader *hdr)
{
    float ox, oy, oz, fx, fy, fz, mx, my, mz, tx, ty, tz, ra, vo = 1500;
    float ux, uy, uz, vx, vy, vz, wx, wy, wz;
    float uxv, u, prj, u_w, w, cos, a;
    float wt;

    ox = hdr->op.x;
    oy = hdr->op.y;
    oz = hdr->op.z;
    fx = hdr->fp.x;
    fy = hdr->fp.y;
    fz = hdr->fp.z;
    mx = mp_.x;
    my = mp_.y;
    mz = mp_.z;
    tx = hdr->tp.x;
    ty = hdr->tp.y;
    tz = hdr->tp.z;
    ra = hdr->radius;
    ux = tx - ox;
    uy = ty - oy;
    uz = tz - oz;
    vx = mx - ox;
    vy = my - oy;
    vz = mz - oz;
    wx = mx - fx;
    wy = my - fy;
    wz = mz - fz;

    uxv = sqrt((uy * vz - uz * vy) * (uy * vz - uz * vy) +
               (uz * vx - ux * vz) * (uz * vx - ux * vz) +
               (ux * vy - uy * vx) * (ux * vy - uy * vx));
    u   = sqrt(ux * ux + uy * uy + uz * uz);
    prj = uxv / u;
    u_w = ux * wx + uy * wy + uz * wz;
    w   = sqrt(wx * wx + wy * wy + wz * wz);
    cos = (u_w / w) / u;
    a   = prj / ra + (r_ - w * cos) / r_;
    wt  = sqrt(a) * del_ + (r_ - w) / vo;

    LOG(INFO) << "CompWT,"
              << "op.x," << ox;
    LOG(INFO) << "CompWT,"
              << "op.y," << oy;
    LOG(INFO) << "CompWT,"
              << "op.z," << oz;
    LOG(INFO) << "CompWT,"
              << "fp.x," << fx;
    LOG(INFO) << "CompWT,"
              << "fp.y," << fy;
    LOG(INFO) << "CompWT,"
              << "fp.z," << fz;
    LOG(INFO) << "CompWT,"
              << "mp.x," << mx;
    LOG(INFO) << "CompWT,"
              << "mp.y," << my;
    LOG(INFO) << "CompWT,"
              << "mp.z," << mz;
    LOG(INFO) << "CompWT,"
              << "tp.x," << tx;
    LOG(INFO) << "CompWT,"
              << "tp.y," << ty;
    LOG(INFO) << "CompWT,"
              << "tp.z," << tz;
    LOG(INFO) << "CompWT,"
              << "u.x," << ux;
    LOG(INFO) << "CompWT,"
              << "u.y," << uy;
    LOG(INFO) << "CompWT,"
              << "u.z," << uz;
    LOG(INFO) << "CompWT,"
              << "v.x," << vx;
    LOG(INFO) << "CompWT,"
              << "v.y," << vy;
    LOG(INFO) << "CompWT,"
              << "v.z," << vz;
    LOG(INFO) << "CompWT,"
              << "w.x," << wx;
    LOG(INFO) << "CompWT,"
              << "w.y," << wy;
    LOG(INFO) << "CompWT,"
              << "w.z," << wz;
    LOG(INFO) << "CompWT,"
              << "uxv," << uxv;
    LOG(INFO) << "CompWT,"
              << "u," << u;
    LOG(INFO) << "CompWT,"
              << "projection," << prj;
    LOG(INFO) << "CompWT,"
              << "u_w," << u_w;
    LOG(INFO) << "CompWT,"
              << "w," << w;
    LOG(INFO) << "CompWT,"
              << "cos," << cos;
    LOG(INFO) << "CompWT,"
              << "a," << a;
    LOG(INFO) << "CompWT,"
              << "del," << del_;
    LOG(INFO) << "CompWT,"
              << "wt," << wt;

    return wt;
}

bool Vbf::Diff(VbfHeader *l_hdr, VbfHeader *n_hdr)
{
    unsigned short l_ox, l_oy, l_oz, l_tx, l_ty, l_tz, n_ox, n_oy, n_oz, n_tx,
        n_ty, n_tz;

    l_ox = l_hdr->op.x;
    l_oy = l_hdr->op.y;
    l_oz = l_hdr->op.z;
    l_tx = l_hdr->tp.x;
    l_ty = l_hdr->tp.y;
    l_tz = l_hdr->tp.z;
    n_ox = n_hdr->op.x;
    n_oy = n_hdr->op.y;
    n_oz = n_hdr->op.z;
    n_tx = n_hdr->tp.x;
    n_ty = n_hdr->tp.y;
    n_tz = n_hdr->tp.z;

    LOG(INFO) << "Diff,"
              << "l_op.x" << l_ox;
    LOG(INFO) << "Diff,"
              << "l_op.y" << l_oy;
    LOG(INFO) << "Diff,"
              << "l_op.z" << l_oz;
    LOG(INFO) << "Diff,"
              << "l_tp.x" << l_tx;
    LOG(INFO) << "Diff,"
              << "l_tp.y" << l_ty;
    LOG(INFO) << "Diff,"
              << "l_tp.z" << l_tz;
    LOG(INFO) << "Diff,"
              << "n_op.x" << n_ox;
    LOG(INFO) << "Diff,"
              << "n_op.y" << n_oy;
    LOG(INFO) << "Diff,"
              << "n_op.z" << n_oz;
    LOG(INFO) << "Diff,"
              << "n_tp.x" << n_tx;
    LOG(INFO) << "Diff,"
              << "n_tp.y" << n_ty;
    LOG(INFO) << "Diff,"
              << "n_tp.z" << n_tz;

    if (n_ox == l_ox && n_oy == l_oy && n_oz == l_oz && n_tx == l_tx &&
        n_ty == l_ty && n_tz == l_tz)
        return 0;
    else
        return 1;
}

void Vbf::SendAckNtf(bool send)
{
    Ptr<MsgSendAckNtf> m(new MsgSendAckNtf);
    m->send = send;

    SendNtf(m, LOWER_LAYER, LOWER_PROTOCOL);
}

bool Vbf::IsMoreFar(VbfHeader *hdr)
{
    float mx, my, mz, fx, fy, fz, md, fd;

    fx = hdr->fp.x;
    fy = hdr->fp.y;
    fz = hdr->fp.z;
    mx = mp_.x;
    my = mp_.y;
    mz = mp_.z;

    md = (mx - TP_X) * (mx - TP_X) + (my - TP_Y) * (my - TP_Y) +
         (mz - TP_Z) * (mz - TP_Z);
    fd = (fx - TP_X) * (fx - TP_X) + (fy - TP_Y) * (fy - TP_Y) +
         (fz - TP_Z) * (fz - TP_Z);

    if (md >= fd)
        return 1;
    else
        return 0;
}
  MODULE_INIT(Vbf)
  PROTOCOL_HEADER(VbfHeader)
#endif
}
