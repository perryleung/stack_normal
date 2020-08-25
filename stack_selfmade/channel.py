#!/usr/bin/env python3
# -*- coding:utf-8 -*-

import socket
import asyncio
import signal
import sys
import logging

from enum import Enum
from math import sqrt, log10, pow
from random import randrange, random

BUFSIZE = 2048
X_RANGE = [0, 500]  # unit: m
Y_RANGE = [0, 500]
Z_RANGE = [0, 500]
SALINITY = 35         # unit: ppt
TEMPERATURE = 22         # unit: C
DEPTH = 5          # unit: m
IDLE_ENERGY = 0.12       # unit: w
RECV_ENERGY = 0.8
SEND_ENERGY = 25
FULL_ENERGY = 1e4        # unit: mAh
GAPTIME = 0.1        # unit: s


g_loop = asyncio.get_event_loop()
g_nodes = []
g_id = 1
g_packets = []
g_time = None
g_sink = 1


class Position:

    def __init__(self):
        global g_sink
        if (g_sink > 0):
            self.x = self.get_random_x()
            self.y = self.get_random_y()
            self.z = self.get_random_z()
            g_sink = g_sink + 1
        # elif (g_sink == 1):
        #     self.x = 1
        #     self.y = 1
        #     self.z = 1
        #     g_sink = 2
        # elif (g_sink == 2):
        #     self.x = 500
        #     self.y = 500
        #     self.z = 500
        #     g_sink = 3
        # elif g_sink == 3:
        #     self.x = 1000
        #     self.y = 1000
        #     self.z = 1000
        #     g_sink = 4

    def __str__(self):
        return "[%s, %s, %s]" % (self.x, self.y, self.z)

    def get_random_x(self):
        return randrange(X_RANGE[0], X_RANGE[1])

    def get_random_y(self):
        return randrange(Y_RANGE[0], Y_RANGE[1])

    def get_random_z(self):
        return randrange(Z_RANGE[0], Z_RANGE[1])

    def GetDistance(slef, pos):
        dx = slef.x - pos.x
        dy = slef.y - pos.y
        dz = slef.z - pos.z
        return sqrt(dx**2 + dy**2 + dz**2)


class Battery:

    def __init__(self):
        self.energy = FULL_ENERGY


class TxMode:

    def __init__(self, modetype, datarate, phyrate, cf, bw):
        '''
        Declaration in ns3/src/uan/model/uan-tx-mode.h:156
        Definition  in ns3/src/uan/model/uan-tx-mode.cc:132
        unit:
            datarate             : bps
            phyrate              : sps (symbol per second)
            cf(center frequence) : Hz
            bw(band width)       : Hz
        '''
        self.modetype = modetype
        self.datarate = datarate
        self.phyrate = phyrate
        self.cf = cf
        self.bw = bw

    def GetModeType(self):
        return self.modetype

    def GetDataRate(self):
        return self.datarate

    def GetPhyRate(self):
        return self.phyrate

    def GetCenterFreq(self):
        return self.cf

    def GetBandWidth(self):
        return self.bw


# Five TxMode in AquaSeNt Modem
class TxMode1(TxMode):

    def __init__(self):
        super().__init__(1, 1500, 1500, 24e3, 6e3)


class TxMode2(TxMode):

    def __init__(self):
        super().__init__(2, 3000, 1500, 24e3, 6e3)


class TxMode3(TxMode):

    def __init__(self):
        super().__init__(3, 4500, 1500, 24e3, 6e3)


class TxMode4(TxMode):

    def __init__(self):
        super().__init__(4, 6000, 1500, 24e3, 6e3)


class TxMode5(TxMode):

    def __init__(self):
        super().__init__(5, 9000, 1500, 24e3, 6e3)


class PacketId:
    id = 0

    @classmethod
    def GetId(cls):
        cls.id = cls.id + 1
        return cls.id


class Packet:

    def __init__(self, txnode, data):
        self.uuid = PacketId.GetId()
        self.txnode = txnode
        self.rxnode = 0
        self.data = data
        #self.send_start = 0
        #self.send_stop  = 0
        #self.recv_start = 0
        #self.recv_stop  = 0
        self.delay = 0
        self.txpower = txnode.txpower
        self.rxpower = 0
        self.txmode = 0
        self.rxmode = 0

    def __eq__(self, other):
        self.uuid == other.uuid

    def GetSize(self):
        return len(self.data)

    def GetRxPower(self):
        return self.rxpower

    def SendUp(self, packet):
        global g_loop
        g_loop.create_task(packet.rxnode.transducer.Receive(packet))

    @asyncio.coroutine
    def Transmit(self, packet):
        yield from asyncio.sleep(packet.delay)
        self.SendUp(packet)


class Propagation:

    def __init__(self):
        # Spreading coefficient used in calculation of Thorp's approximation
        # value set in ns3/src/uan/model/uan-prop-model-thorp.cc:48
        self.spread_coef = 1.5

    def path_loss(self, packet):
        '''
        Declaration in ns3/src/uan/model/uan-prop-model.h:298
        Definition  in ns3/src/uan/model/uan-prop-model-thorp.cc:56
        return unit: dB
        '''
        dist = packet.txnode.GetDistance(packet.rxnode)
        return self.spread_coef * 10 * log10(dist) + \
            (dist / 1000) * self.atten(packet.txmode.GetCenterFreq() / 1000)

    def get_sound_speed(self, S, T, D, equation=1):
        '''
        These code is copy from Acoustics Toolbox(http://oalib.hlsresearch.com/Modes/AcousticsToolbox/).
        Four types of sound-speed equations was implemented.
        equation 1 is Mackenzie equation:
            Mackenzie, K.V. "Nine-term Equation for Sound Speed in the Oceans",
            J. Acoust. Soc. Am. 70 (1981), 807-812.
        equation 2 is Del Grosso equation:
            Del Grosso, "A New Equation for the speed of sound in Natural
            Waters", J. Acoust. Soc. Am. 56#4 (1974).
        equation 3 is Chen and Millero equation:
            Chen and Millero, "The Sound Speed in Seawater", J. Acoust.
            Soc. Am. 62 (1977), 1129-1135
        equation 4 is derivatives of the EOS80 equation.
        s, t means: salinity (ppt), temperature (C), d means DEPTH (m) in Mackenzie equation,
        and means PRESSURE (dbar) in others.
        There, we just implemente the Mackenzie equation.
        Actually, it is almost 1500 m/s, and in nsmiracle(an extend to ns2),
        the author just set it to 1500.
        '''
        c = 1.44896e3
        t = 4.591e0
        t2 = -5.304e-2
        t3 = 2.374e-4
        s = 1.340e0
        d = 1.630e-2
        d2 = 1.675e-7
        ts = -1.025e-2
        td3 = -7.139e-13

        return c + t * T + t2 * T * T + t3 * T * T * T + s * (S - 35.0) + d * D + d2 * D * D + ts * T * (S - 35.0) + td3 * T * D * D * D

    def delay(self, packet):
        '''
        Declaration in ns3/src/uan/model/uan-prop-model.h:317
        Definition  in ns3/src/uan/model/uan-prop-model-thorp.cc:71
        return unit: second
        '''
        dist = packet.txnode.GetDistance(packet.rxnode)
        depth = (packet.txnode.GetDepth() + packet.rxnode.GetDepth()) / 2
        speed = self.get_sound_speed(SALINITY, TEMPERATURE, depth)
        return dist / speed

    def atten(self, freq):
        '''
        Declaration in ns3/src/uan/model/uan-prop-model-thorp.h:66
        Definition  in ns3/src/uan/model/uan-prop-model-thorp.cc:84
        freq   unit: Khz
        return unit: dB/Km
        '''
        fsq = freq**2
        if (freq >= 0.4):
            return 0.11 * fsq / (1 + fsq) + 44 * fsq / (4100 + fsq) + 2.75 * 0.0001 * fsq + 0.003
        else:
            return 0.002 + 0.11 * (freq / (1 + freq)) + 0.011 * freq


class Noise:

    def __init__(self):
        # Wind speed in m/s
        # value set in ns3/src/uan/model/uan-noise-model-default.cc:47
        self.wind = 1
        # Shipping contribution to noise between 0 and 1
        # value set in ns3/src/uan/model/uan-noise-model-default.cc:51
        self.shipping = 0

    def GetNoise(self, freq):
        '''
        Declaration in ns3/src/uan/model/uan-noise-model.h:48
        Definition  in ns3/src/uan/model/uan-noise-model-default.cc:61
        freq   unit: Khz
        return unit: dB/Hz
        '''
        turbDb = 17.0 - 30.0 * log10(freq)
        turb = pow(10, turbDb * 0.1)

        shipDb = 40.0 + 20.0 * (self.shipping - 0.5) + \
            26.0 * log10(freq) - 60.0 * log10(freq + 0.03)
        ship = pow(10.0, (shipDb * 0.1))

        windDb = 50.0 + 7.5 * pow(self.wind, 0.5) + \
            20.0 * log10(freq) - 40.0 * log10(freq + 0.4)
        wind = pow(10.0, windDb * 0.1)

        thermalDb = -15 + 20 * log10(freq)
        thermal = pow(10, thermalDb * 0.1)

        noiseDb = 10 * log10(turb + ship + wind + thermal)

        return noiseDb


class TransducerState(Enum):
    TX = 1
    RX = 2


class Transducer:
    '''
    Declaration in ns3/src/uan/model/uan-transducer-hd.h
    Half duplex implementation of transducer object
    传感器类
    '''

    def __init__(self, phy):
        self.state = TransducerState.RX
        self.phy = phy
        self.arrivallist = []

    def SendDown(self, packet):
        global g_channel
        g_channel.TxPacket(packet)

    def SendUp(self, packet):
        global g_loop
        g_loop.create_task(self.phy.StartRxPacket(packet))

    @asyncio.coroutine
    def Receive(self, packet):
        '''
        Definition in ns3/src/uan/model/uan-transducer-hd.cc:136
        '''
        self.arrivallist.append(packet)  # 记录接收过的数据包,计算SINR的时候起作用

        if (self.state == TransducerState.RX):  # 为什么不执行了下面的接收时延再SendUp
            self.SendUp(packet)

        rxdelay = packet.GetSize() * 8 / packet.rxmode.GetDataRate()

        yield from asyncio.sleep(rxdelay)

        logging.info("[RX End]   Phy %s  <--  Phy %s",
                     packet.rxnode.uuid, packet.txnode.uuid)

        for p in self.arrivallist:
            if (p.uuid == packet.uuid):
                self.arrivallist = [
                    o for o in self.arrivallist if o.uuid != p.uuid]  # 好像是要在列表中去除这个packet...？

    @asyncio.coroutine
    def Transmit(self, packet):
        '''
        Definition in ns3/src/uan/model/uan-transducer-hd.cc:153
        This implementation is different from ns3,
        because ns3 is a event-driven framework, can cancel an event
        before happen, but python asyncio module can not do that.
        in ns3, when the transducer is transmitting, a new packet arrive from upper,
        it will cancel the origin TxEnd event and re-schedule,
        make the TxEnd event notify after the last packet sent.
        there, I just drop it.
        '''
        if (self.state == TransducerState.TX):
            logging.info(
                "Transducer requested to TX while already Transmitting. Dropping packet.")
            return

        self.state = TransducerState.TX

        self.SendDown(packet)

        txdelay = packet.GetSize() * 8 / packet.txmode.GetDataRate()

        yield from asyncio.sleep(txdelay)

        self.state = TransducerState.RX


class SinrDefaultCacl:

    def DbToKp(self, db):
        '''
        Definition in ns3/src/uan/model/uan-phy.h:82
        Convert dB re 1 uPa to kilopascals
        '''
        return pow(10, db / 10)

    def KpToDb(self, kp):
        '''
        Definition in ns3/src/uan/model/uan-phy.h:92
        Convert kilopascals to dB re 1 uPa.
        '''
        return 10 * log10(kp)

    def CaclSinr(self, packet, noise, arrivallist):
        '''
        Declaration in ns3/src/uan/model/uan-phy-gen.h:157
        Definition  in ns3/src/uan/model/uan-phy-gen.cc:73
        '''
        kp = -self.DbToKp(packet.rxpower)

        for it in arrivallist:
            kp = kp + self.DbToKp(it.GetRxPower())

        # if more than one packet arrive together,
        # kp will greater 0, is a noise to other signal
        total = self.KpToDb(kp + self.DbToKp(noise))

        return packet.rxpower - total


class PerDefaultCacl:

    def __init__(self):
        # SINR cutoff for good packet reception.
        # value set in ns3/src/uan/model/uan-phy-gen.cc:242
        self.thresh = 8

    def CaclPer(self, packet, sinr, mode):
        if sinr >= self.thresh:
            return 0
        else:
            return 1


class Channel:

    def __init__(self):
        self.prop = Propagation()
        self.noise = Noise()

    def TxPacket(self, packet):
        '''
        Declaration in ns3/src/uan/model/uan-channel.h:75
        Definition  in ns3/src/uan/model/uan-channel.cc:149
        This is different from ns3
        '''
        global g_nodes

        for n in g_nodes:
            if n == packet.txnode:  # 不发给自己
                continue

            p = Packet(packet.txnode, packet.data)
            p.txmode = packet.txmode
            p.rxmode = p.txmode  # 应该是 = packet.rxmode??? 虽然paacket的txmode = rxmode
            p.rxnode = n
            p.delay = self.prop.delay(p)
            p.rxpower = packet.txpower - self.prop.path_loss(p)
            logging.info("[TX] Phy %s  -->  Phy %s, Packet: %s, TxPower: %s, RxPower: %s, Delay: %s",
                         p.txnode.uuid, p.rxnode.uuid, p.uuid, p.txpower, p.rxpower, p.delay)

            g_loop.create_task(p.Transmit(p))


g_channel = Channel()


class PhyId:
    id = 0

    @classmethod
    def GetId(cls):
        cls.id = cls.id + 1
        return cls.id


class PhyState(Enum):
    IDLE = 1
    BUSY = 2
    RX = 3
    TX = 4
    SLEEP = 5


class Phy:

    def __init__(self, connection):
        self.connection = connection
        self.uuid = PhyId.GetId()

        self.state = PhyState.IDLE
        self.position = Position()
        self.battery = Battery()
        self.transducer = Transducer(self)

        # Required SNR for signal acquisition in dB
        # value set in ns3/src/uan/model/uan-phy-gen.cc:447
        self.threshold = 10
        # Transmission output power in dB
        # value set in ns3/src/uan/model/uan-phy-gen.cc:452
        self.txpower = 85
        # Gain added to incoming signal at receiver
        # value set in ns3/src/uan/model/uan-phy-gen.cc:457
        self.rxgain = 0
        # Functor to calculate PER based on SINR and TxMode
        # value set in ns3/src/uan/model/uan-phy-gen.cc:467
        self.per = PerDefaultCacl()
        # Functor to calculate SINR based on pkt arrivals and modes
        # value set in ns3/src/uan/model/uan-phy-gen.cc:472
        self.sinr = SinrDefaultCacl()  # SINR：信号与干扰加噪声比

    def __eq__(self, other):
        return self.uuid == other.uuid

    def __str__(self):
        return "Phy ID: %s\tPositon: %s" % (self.uuid, self.position)

    def fileno(self):
        return self.connection.fileno()

    def GetDepth(self):
        return self.position.z

    def GetDistance(self, node):
        return self.position.GetDistance(node.position)

    def SendDown(self, packet):
        global g_loop
        g_loop.create_task(self.transducer.Transmit(packet))

    def ParseRequest(self, request):
        '''
        Parse request from Physices,
        may be  SendDataReq = 1
                PositionReq = 2 
        '''
        global g_loop
        if (request[0] == 1):
            # self就是Packet构造函数里面的txnode，而request就是data
            packet = Packet(self, request)
            g_loop.create_task(self.SendPacket(packet))
        elif (request[0] == 2):  # 需要phy层收到MsgPosReq信息才会向channel询问位置信息，好像已经不再使用,vbf从配置文件读取位置信息。
            # [3]指的是数值3,在stack中指的是PACKET_TYPE_POSITION_RSP
            rsp = bytes([3]) + \
                self.position.x.to_bytes(2, byteorder='little') + \
                self.position.y.to_bytes(2, byteorder='little') + \
                self.position.z.to_bytes(2, byteorder='little')
            self.connection.send(rsp)  # 向客户端回复位置信息rsp

    @asyncio.coroutine
    def StartRxPacket(self, packet):
        '''
        Declaration in ns3/src/uan/model/uan-phy.h:249
        Definition  in ns3/src/uan/model/uan-phy-gen.cc:589
        '''
        global g_loop
        if (self.state == PhyState.TX):
            logging.info(
                "Phy %s receive packet while transmitting." % self.uuid)
            return
        elif (self.state == PhyState.SLEEP):
            logging.info("Phy %s receive packet while sleeping." % self.uuid)
            return
        elif (self.state == PhyState.IDLE):
            noise = g_channel.noise.GetNoise(packet.txmode.GetCenterFreq() / 1000
                                             + 10 * log10(packet.txmode.GetBandWidth()))
            newsinr = self.sinr.CaclSinr(
                packet, noise, self.transducer.arrivallist)
            logging.info("[RX Begin] Phy %s  <--  Phy %s, Noise: %s, SINR %s",
                         self.uuid, packet.txnode.uuid, noise, newsinr)
            if newsinr > self.threshold:
                self.state = PhyState.RX
                rxdelay = packet.GetSize() * 8 / packet.txmode.GetDataRate()
                yield from asyncio.sleep(rxdelay)
                if random() > self.per.CaclPer(packet, newsinr, packet.txmode):
                    yield from g_loop.sock_sendall(self.connection, packet.data)
                    self.state = PhyState.IDLE

    @asyncio.coroutine
    def SendPacket(self, packet):
        '''
        Definition in ns3/src/uan/model/uan-phy-gen.cc:521
        '''
        if (self.state == PhyState.TX):
            logging.info(
                "PHY requested to TX while already Transmitting. Dropping packet.")
            return
        elif (self.state == PhyState.SLEEP):
            logging.info(
                "PHY requested to TX while sleeping. Dropping packet.")
            return

        self.state = PhyState.TX

        packet.txmode = TxMode1()
        packet.rxmode = TxMode1()

        self.SendDown(packet)   # 重点分析对象

        txdelay = packet.GetSize() * 8 / packet.txmode.GetDataRate()

        yield from asyncio.sleep(txdelay)

        self.state = PhyState.IDLE


def create_node(conn):
    global g_nodes
    node = Phy(conn)
    g_nodes.append(node)
    return node


@asyncio.coroutine
def run_node(conn):
    global g_loop
    conn.setblocking(False)
    node = create_node(conn)
    logging.info("New Phy Enter: %s", node)
    while True:
        data = yield from g_loop.sock_recv(node.connection, BUFSIZE)
        if len(data):
            node.ParseRequest(data)
        else:
            logging.info(("Phy %s Exit") % node.uuid)
            node.connection.close()
            g_nodes.remove(node)
            del node
            break


@asyncio.coroutine
def main():
    global g_loop, g_time
    # listen for new node entry
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(('localhost', 30001))
    sock.listen(5)
    sock.setblocking(False)

    while True:
        conn, addr = yield from g_loop.sock_accept(sock)
        g_loop.create_task(run_node(conn))


def SignalHandler(signum, frame):
    global g_loop
    g_loop.stop()
    sys.exit(0)


if __name__ == '__main__':
    logging.basicConfig(format='%(asctime)s %(message)s', level=logging.INFO)
    signal.signal(signal.SIGINT, SignalHandler)
    g_loop.create_task(main())
    g_loop.run_forever()
