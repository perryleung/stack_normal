from math import pow, log10

IDLE_ENERGY = 0.12       # unit: w
RECV_ENERGY = 0.8
SEND_ENERGY = 25
FULL_ENERGY = 1e4        # unit: mAh


class UanPhyPerGenDefault:
    # SINR cutoff for good packet reception.
    # value set in ns3/src/uan/model/uan-phy-gen.cc:223
    threshold = 8

    # Default PER calculation simply compares SINR to a threshold which is
    # configurable via an attribute.
    def calc_per(self, sinr):
        if sinr >= self.threshold:
            return 0
        else:
            return 1


class UanPhyCalcSinrDefault:
    def db_to_kp(self, db):
        return pow(10, db / 10.0)

    def kp_to_db(self, kp):
        return 10 * log10(kp)

    def calc_sinr_db(self, rxPowerDb, ambNoiseDb, arrivalList):
        intKp = -self.db_to_kp(rxPowerDb)
        for item in arrivalList:
            intKp += self.db_to_kp(item.get_rx_power())
        totalIntDb = self.kp_to_db(intKp + self.db_to_kp(ambNoiseDb))
        return rxPowerDb - totalIntDb


class Battery:
    def __init__(self):
        self.energy = FULL_ENERGY

    def get_energy(self):
        return self.energy

    def consume_energy(self, electricity):
        self.energy - electricity
