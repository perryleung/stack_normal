from math import log10

SALINITY = 35         # unit: ppt
TEMPERATURE = 22         # unit: C


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
        dist = packet.src_node.get_distance(packet.dst_node)
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
        dist = packet.src_node.get_distance(packet.dst_node)
        depth = (packet.src_node.get_depth() + packet.dst_node.get_depth()) / 2
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
