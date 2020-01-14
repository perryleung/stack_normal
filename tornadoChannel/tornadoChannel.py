# -*- coding: utf-8 -*-
import logging
import os
import random
import signal
import struct
from math import sqrt, log10
from enum import Enum
from optparse import OptionParser
from tornado.ioloop import IOLoop
from tornado.tcpserver import TCPServer
from tornado import gen, queues
from tornado.iostream import StreamClosedError
from channelElement import Propagation, Noise
from txmode import TxMode1
from nodeElement import UanPhyPerGenDefault, UanPhyCalcSinrDefault, Battery
import numpy as np

mine_logger = logging.getLogger('mine')
parser = OptionParser()
X_RANGE = []
Y_RANGE = []
Z_RANGE = []
PORT = 0
DATASIZE = 0
NODELIST = []

# 方便python与C++编解码的Pos类------------------------------------------


class Position(object):
    def __init__(self):
        self.type = 8
        self.x = 0
        self.y = 0
        self.z = 0
        self.struct = struct.Struct('!cHHH')

    def ParseFromStringWithMsg(self, data):
        content = self.struct.unpack_from(data[:self.struct.size])
        self.type = content[0]
        self.x = content[1]
        self.y = content[2]
        self.z = content[3]

    def GetPosition(self):
        return (self.x, self.y, self.z)

# 数据包Packet类---------------------------------------------------------


class PacketId:
    id = 0

    @classmethod
    def GetId(cls):
        cls.id = cls.id + 1
        return cls.id


class Packet:
    def __init__(self, node, data):
        self.uuid = PacketId.GetId()
        self.src_node = node
        self.dst_node = 0
        self.data = data
        self.delay = 0
        self.txpower = node.txpower
        self.rxpower = 0
        self.txmode = 0
        self.rxmode = 0

    def __eq__(self, other):
        return self.uuid == other.uuid

    def get_pkt_size(self):
        return len(self.data)

    def get_rx_power(self):
        return self.rxpower

# 共享信道Channel---------------------------------------------------------


class Channel:
    def __init__(self):
        self.prop = Propagation()
        self.noise = Noise()

    @gen.coroutine
    def P2PTxPacket(self, packet):
        mine_logger.debug("enter the function  P2PTxPacket")
        yield gen.sleep(packet.delay)
        mine_logger.debug("the pro delay is %f" % packet.delay)
        self.RxPacket(packet)

    def RxPacket(self, packet):
        mine_logger.debug("enter the function RxPacket")
        IOLoop.current().spawn_callback(packet.dst_node.transducer.receive,
                                        packet)

    @gen.coroutine
    def TxPacket(self, packet):
        mine_logger.debug("enter the function TxPacket and the size of \
        NODELIST is %s" % str(len(NODELIST)))
        for node in NODELIST:
            if node == packet.src_node:
                continue
            p = Packet(packet.src_node, packet.data)
            p.dst_node = node
            p.txmode = packet.txmode
            p.rxmode = packet.rxmode
            p.delay = self.prop.delay(p)
            p.rxpower = packet.txpower - self.prop.path_loss(p)
            mine_logger.info("[TX]Packet{}: Node{} ---> Node{}, TxPower: {}, \
            RxPower: {}, Delay: {}".format(
                p.uuid,
                p.src_node.uuid,
                p.dst_node.uuid,
                p.txpower,
                p.rxpower,
                p.delay
            ))
            # IOLoop.current().spawn_callback(self.P2PTxPacket, p)
            # yield self.P2PTxPacket(p)
            self.P2PTxPacket(p)

        pass


CHANNEL = Channel()
# 节点Node特性------------------------------------------------------------


class TransducerState(Enum):
    TX = 1
    RX = 2


class Transducer:
    def __init__(self, owner):
        self.state = TransducerState.RX
        self.owner = owner
        self.arrivalList = []
        self.pkt_queue = queues.Queue(1)
        IOLoop.current().spawn_callback(self.handle_queue)

    @gen.coroutine
    def transmit(self, packet):
        mine_logger.debug("enter the function : transmit")
        yield self.pkt_queue.put(packet)
        # if self.state == TransducerState.TX:
        #     mine_logger.info("Transducer requested to TX while already \
        #                      Transmitting. Input Queue")
        #     return
        self.state = TransducerState.TX
        # mine_logger.info("TransducerState change from RX to TX")
        # IOLoop.current().spawn_callback(self.handle_queue)

    @gen.coroutine
    def handle_queue(self):
        while True:
            try:
                packet = yield self.pkt_queue.get()
                txdelay = (
                    packet.get_pkt_size() * 8.0 / packet.txmode.GetDataRate())
                nxt = gen.sleep(txdelay)
                yield CHANNEL.TxPacket(packet)
                yield nxt
            finally:
                self.pkt_queue.task_done()
            if self.pkt_queue.empty():
                self.state = TransducerState.RX

    @gen.coroutine
    def receive(self, packet):
        mine_logger.debug("enter function receive")
        '''
        Definition in ns3/src/uan/model/uan-transducer-hd.cc:136
        '''
        self.arrivalList.append(packet)  # 记录接收过的数据包,计算SINR的时候起作用
        rxdelay = packet.get_pkt_size() * 8.0 / packet.rxmode.GetDataRate()
        nxt = gen.sleep(rxdelay)
        if (self.state == TransducerState.RX):  # 为什么不执行了下面的接收时延再SendUp
            # yield gen.sleep(rxdelay)
            mine_logger.info("[RX]Packet{}: Node{}--->Node{}".format(
                packet.uuid,
                packet.src_node.uuid,
                packet.dst_node.uuid
            ))
            IOLoop.current().spawn_callback(self.owner.recv_packet, packet)

        yield nxt

        for p in self.arrivalList:
            if (p.uuid == packet.uuid):
                # 好像是要在列表中去除这个packet...？
                self.arrivalList = [
                    o for o in self.arrivalList if o.uuid != p.uuid]


class NodePosition:
    def __init__(self):
        global X_RANGE, Y_RANGE, Z_RANGE
        self.position = self.get_random_pos()

    def __str__(self):
        return "[x, y, z] = [%s, %s, %s]" % self.position

    def get_x(self):
        return self.position[0]

    def get_y(self):
        return self.position[1]

    def get_z(self):
        return self.position[2]

    def get_random_pos(self):
        return (random.randint(X_RANGE[0], X_RANGE[1]),
                random.randint(Y_RANGE[0], Y_RANGE[1]),
                random.randint(Z_RANGE[0], Z_RANGE[1]))

    def get_distance(self, othernode):
        dx = self.get_x() - othernode.position.get_x()
        dy = self.get_y() - othernode.position.get_y()
        dz = self.get_z() - othernode.position.get_z()

        return sqrt(dx**2 + dy**2 + dz**2)

    def set_position(self, newPosition):
        self.position = newPosition


class NodeId:
    id = 0

    @classmethod
    def GetId(self):
        self.id = self.id + 1
        return self.id


class NodeState(Enum):
    IDLE = 1
    BUSY = 2
    RX = 3
    TX = 4
    SLEEP = 5
    DISABLED = 6


class Node:
    def __init__(self, stream, address):
        self.connection = stream
        self.ipaddress = address
        self.uuid = NodeId.GetId()
        self.state = NodeState.IDLE
        self.position = NodePosition()
        self.battery = Battery()
        self.transducer = Transducer(self)  # 换能器

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
        self.per = UanPhyPerGenDefault()
        # Functor to calculate SINR based on pkt arrivals and modes
        # value set in ns3/src/uan/model/uan-phy-gen.cc:472
        self.sinr = UanPhyCalcSinrDefault()  # SINR：信号与干扰加噪声比

        self.my_isdrop = self.random_drop()

    def random_drop(self, ber=0.0001):
        chunk_size = 84
        my_per = 1 - (1 - ber)**(chunk_size * 8)
        drop = [0, 1]
        d_prob = [1-my_per, my_per]
        while True:
            i = np.random.choice(drop, 1, False, d_prob)[0]
            yield i

    def __eq__(self, other):
        return self.uuid == other.uuid

    def __str__(self):
        return "Phy ID: %s\tPositon: %s" % (self.uuid, self.position)

    def get_distance(self, node):
        return self.position.get_distance(node)

    def get_depth(self):
        return self.position.get_z()

    def parse_request(self, req):
        '''
        Parse request from stack
        PACKET_TYPE_DATA_REQ = 1
        '''
        if req[0] == '\x01':
            packet = Packet(self, req)
            IOLoop.current().spawn_callback(self.send_packet, packet)
        elif req[0] == '\x09':
            temp = Position()
            temp.ParseFromStringWithMsg(req)
            mine_logger.debug(
                "set pos is " +
                ",".join([str(item) for item in temp.GetPosition()]))
            IOLoop.current().spawn_callback(self.set_position,
                                            temp.GetPosition())
        else:
            mine_logger.debug("recv wrong data from client")

    @gen.coroutine
    def set_position(self, newPosition):
        self.position.set_position(newPosition)
        mine_logger.info("new position : %s" % self)
        # send rsp
        rsp = struct.pack("B", 10)  # 编码成二进制发送
        yield self.connection.write(rsp)

    @gen.coroutine
    def open_channel(self):
        while True:
            try:
                data = yield self.connection.read_until_regex(
                    '..$', max_bytes=DATASIZE)
                self.parse_request(data)
            except StreamClosedError:
                mine_logger.warning("Lost node%s " % str(self.uuid))
                break
            except Exception as e:
                mine_logger.error("Error: ", e)

    @gen.coroutine
    def send_packet(self, packet):
        mine_logger.debug(
            "[Node%d] enter the function : send_packet" % self.uuid)
        if self.state == NodeState.TX:
            mine_logger.info(
                "[Node%d] PHY requested to TX while already Transmitting. \
                Dropping packet." % self.uuid)
            # return
        elif self.state == NodeState.SLEEP:
            mine_logger.info(
                "[Node%d] PHY requested to TX while sleeping. \
                Dropping packet." % self.uuid)
            # return
        elif self.state == NodeState.DISABLED:
            mine_logger.info(
                "[Node%d] energy depleted, node cannot transmit any packet. \
                Dropping." % self.uuid)
            # return
        else:
            self.state = NodeState.TX
            packet.txmode = TxMode1()
            packet.rxmode = TxMode1()

            txdelay = packet.get_pkt_size() * 8.0 / packet.txmode.GetDataRate()
            mine_logger.debug("[Node%d] the txdelay is %f" %
                              (self.uuid, txdelay))
            # nxt = gen.sleep(txdelay)
            yield gen.sleep(txdelay)
            yield self.transducer.transmit(packet)
            # yield nxt
            self.state = NodeState.IDLE

        # send rsp
        mine_logger.debug(
            'send back PACKET_TYPE_DATA_RSP to node%d ' % self.uuid)
        rsp = struct.pack("B", 8)  # 编码成二进制发送
        yield self.connection.write(rsp)
        pass

    @gen.coroutine
    def recv_packet(self, packet):
        '''
        Declaration in ns3/src/uan/model/uan-phy.h:249
        Definition  in ns3/src/uan/model/uan-phy-gen.cc:589
        '''
        if self.state == NodeState.TX:
            mine_logger.info(
                "Node%d receive packet while transmitting." % self.uuid)
            return
        elif self.state == NodeState.SLEEP:
            mine_logger.info(
                "Node%d receive packet while sleeping." % self.uuid)
            return
        elif self.state == NodeState.DISABLED:
            mine_logger.info(
                "Node%d energy depleted, node cannot transmit any packet. \
                Dropping." % self.uuid)
            return
        elif self.state == NodeState.RX:
            mine_logger.info(
                "Node%d receive packet while receiving." % self.uuid)
            return

        elif (self.state == NodeState.IDLE):
            noise = CHANNEL.noise.GetNoise(packet.txmode.GetCenterFreq(
            ) / 1000 + 10 * log10(packet.txmode.GetBandWidth()))
            newsinr = self.sinr.calc_sinr_db(
                packet.rxpower, noise, self.transducer.arrivalList)
            mine_logger.info("[RX Begin] Node%s  <--  Node%s, Noise: %s, \
            SINR %s", self.uuid, packet.src_node.uuid, noise, newsinr)
            if newsinr > self.threshold:
                self.state = NodeState.RX
                rxdelay = packet.get_pkt_size(
                ) * 8.0 / packet.rxmode.GetDataRate()
                yield gen.sleep(rxdelay)
                mine_logger.debug("[Node%d] the rxdelay is %f" %
                                  (self.uuid, rxdelay))
                if random.random() > self.per.calc_per(newsinr):
                    is_drop = self.my_isdrop.next()
                    if not is_drop:
                        yield self.connection.write(packet.data)
                        print('recv++++++++++++++++++++++')
                    else:
                        print("drop----------------------")
                    self.state = NodeState.IDLE

        pass


# 信道服务器---------------------------------------------------------------


class ChannelServer(TCPServer):
    @gen.coroutine
    def handle_stream(self, stream, address):
        node = Node(stream, address)
        NODELIST.append(node)
        mine_logger.debug("NODELIST append and the size is %d" % len(NODELIST))
        mine_logger.info("create a new node on %s:%s" % address)
        mine_logger.info("new node mes : %s" % node)
        yield node.open_channel()
        # node.open_channel()
        NODELIST.remove(node)
        mine_logger.debug("NODELIST remove and the size is %d" % len(NODELIST))
    #     while True:
    #         try:
    #             data = yield stream.
    #             pass
    #         except StreamClosedError:
    #             mine_logger.info("the node on %s:%s is dead" % address)
    #             stream.close_fd()
    #             break
    #         except Exception as e:
    #             mine_logger.error("read error : ", e)
    # pass


def init():
    global X_RANGE, Y_RANGE, Z_RANGE, PORT, DATASIZE
    parser.add_option("-f", "--logfile", type="string",
                      dest="logfilename", default="./mes.log",
                      help="the path of the logfile")
    parser.add_option("-q", "--quite", action="store_false",
                      dest="isOutputToFile", default=True,
                      help="don't print log to file")
    parser.add_option("-l", "--length", type=int, dest="length",
                      default=500, help="the length of the sea")
    parser.add_option("-w", "--width", type=int, dest="width",
                      default=500, help="the width of the sea")
    parser.add_option("-d", "--depth", type=int, dest="depth",
                      default=500, help="the depth of the sea")
    parser.add_option("-p", "--port", type=int, dest="port",
                      default=30000, help="the port of the channel")
    parser.add_option("-s", "--datasize", type=int, dest="datasize",
                      default=1024, help="the max size of the data")
    parser.add_option("-m", "--logmode", type="string", dest="logmode",
                      default="info", help="the log level :info or debug ")

    (options, args) = parser.parse_args()

    X_RANGE = [0, options.length]
    Y_RANGE = [0, options.width]
    Z_RANGE = [0, options.depth]
    PORT = options.port
    DATASIZE = options.datasize

    # mine_logger.setLevel(logging.DEBUG)
    if options.logmode == 'debug':
        mine_logger.setLevel(logging.DEBUG)
    else:
        mine_logger.setLevel(logging.INFO)

    mine_logger.propagate = 0

    formatter = logging.Formatter('[ %(asctime)s %(levelname)s] ' +
                                  '%(filename)s:%(lineno)d %(message)s')

    console_handler = logging.StreamHandler()
    console_handler.setFormatter(formatter)
    mine_logger.addHandler(console_handler)

    if options.isOutputToFile:
        logfiledir = os.path.split(options.logfilename)[0]
        if not os.path.isdir(logfiledir):
            os.makedirs(logfiledir)

        if not os.path.exists(options.logfilename):
            os.system(r'touch %s' % options.logfilename)

        file_handler = logging.FileHandler(options.logfilename, mode='w')
        file_handler.setFormatter(formatter)
        mine_logger.addHandler(file_handler)


def SigintHandle(signum, frame):
    global NODELIST
    if len(NODELIST) != 0:
        for node in NODELIST:
            node.connection.close()
        NODELIST = []
    IOLoop.current().stop()


if __name__ == '__main__':
    init()
    mine_logger.info(
        'start the channel server on port %s to simulate the channel' % PORT)
    signal.signal(signal.SIGINT, SigintHandle)
    channelServer = ChannelServer()
    channelServer.listen(PORT)
    IOLoop.current().start()
