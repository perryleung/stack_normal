# -*- coding: utf-8 -*-


# 物理节点发射模式-----------------------------------------------
class TxMode(object):

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
        super(TxMode1, self).__init__(1, 1500, 1500, 24e3, 6e3)


class TxMode2(TxMode):

    def __init__(self):
        super(TxMode2, self).__init__(2, 3000, 1500, 24e3, 6e3)


class TxMode3(TxMode):

    def __init__(self):
        super(TxMode3, self).__init__(3, 4500, 1500, 24e3, 6e3)


class TxMode4(TxMode):

    def __init__(self):
        super(TxMode4, self).__init__(4, 6000, 1500, 24e3, 6e3)


class TxMode5(TxMode):

    def __init__(self):
        super(TxMode5, self).__init__(5, 9000, 1500, 24e3, 6e3)
