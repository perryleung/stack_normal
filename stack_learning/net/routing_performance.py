#!/usr/bin/python3
# -*- coding:utf-8 -*-

import sys
import io
from abc import ABC, abstractmethod
from string import capwords
from warnings import warn

"""
分析器的抽象基类
"""


class AbstractRoutingInfoAnalyzer(ABC):

    def __init__(self, file_num, protocol_name):
        self.file_num = file_num
        self.protocol_name = protocol_name
        self.infos = []
        self.__get_infos()
        self.packet_record = {}

    def __get_infos(self):
        for i in range(int(self.file_num)):
            with io.open("./testInfo/" + str(int(i) + 1) + "_" + self.protocol_name, "r", encoding="utf-8") as file:
                all_infos = file.readlines()
                new_infos_start_line = -1
                while not all_infos[new_infos_start_line].startswith("===="):
                    all_infos[new_infos_start_line] = all_infos[new_infos_start_line].strip()
                    new_infos_start_line -= 1
                if new_infos_start_line != -1: # -1 + 1 = 0
                    self.infos.extend(all_infos[new_infos_start_line + 1:])

    @abstractmethod
    def analyze_info(self):
        pass


"""
DBPCR协议信息分析器
"""


class DbpcrAnalyzer(AbstractRoutingInfoAnalyzer):

    TIME_POS = 0
    NODE_POS = 1
    SEQ_POS = 2
    STATUS_POS = 3
    TYPE_POS = 4
    INFO_SEGMENT_NAME = ["time", "nodeId", "sequence", "status", "type"]

    def __init__(self, file_num, protocol_name):
        super().__init__(file_num, protocol_name)
        self.generate_data = 0
        self.delivery_data = 0
        self.send_request = 0
        self.send_response = 0
        self.send_tgtdata = 0
        self.send_bdctdata = 0
        self.receive_request = 0
        self.receive_response = 0
        self.receive_tgtdata = 0
        self.receive_bdctdata = 0
        self.cancel_response = 0
        self.transmit_delay = 0

    def analyze_info(self):
        for info in self.infos:
            info_segments = info.split(":")
            info_dict = {DbpcrAnalyzer.INFO_SEGMENT_NAME[DbpcrAnalyzer.NODE_POS]: info_segments[DbpcrAnalyzer.NODE_POS], DbpcrAnalyzer.INFO_SEGMENT_NAME[DbpcrAnalyzer.TIME_POS]: info_segments[DbpcrAnalyzer.TIME_POS], DbpcrAnalyzer.INFO_SEGMENT_NAME[DbpcrAnalyzer.STATUS_POS]: info_segments[DbpcrAnalyzer.STATUS_POS], DbpcrAnalyzer.INFO_SEGMENT_NAME[DbpcrAnalyzer.TYPE_POS]: info_segments[DbpcrAnalyzer.TYPE_POS]}
            if self.packet_record.get(info_segments[DbpcrAnalyzer.SEQ_POS]):
                self.packet_record[info_segments[DbpcrAnalyzer.SEQ_POS]].append(info_dict)
            else:
                self.packet_record[info_segments[DbpcrAnalyzer.SEQ_POS]] = [info_dict]
        for value in self.packet_record.values():
            value.sort(key=lambda ele: int(ele[DbpcrAnalyzer.INFO_SEGMENT_NAME[DbpcrAnalyzer.TIME_POS]]))
        for value in self.packet_record.values():
            self.count(value)
        print("generate_data: ", self.generate_data)
        print("delivery_data: ", self.delivery_data)
        print("send_request: ", self.send_request)
        print("send_response: ", self.send_response)
        print("send_tgtdata: ", self.send_tgtdata)
        print("send_bdctdata: ", self.send_bdctdata)
        print("receive_request: ", self.receive_request)
        print("receive_response: ", self.receive_response)
        print("receive_tgtdata: ", self.receive_tgtdata)
        print("receive_bdctdata: ", self.receive_bdctdata)
        print("cancel_response: ", self.cancel_response)
        print("transmit_delay: ", self.transmit_delay / 1000, "s")
        print("The delivery radio is %.2f%%" % (self.delivery_data / self.generate_data * 100))
        print("The average transmission delay is %.2fs" % (self.transmit_delay / 1000))
        print("The network average packet is", (self.send_request + self.send_response + self.send_tgtdata + self.send_bdctdata) / self.generate_data)

    def count(self, info_list):
        generate_time = 0
        delivery_time = 0
        for dict in info_list:
            if dict["status"] == "source":
                self.generate_data += 1
                generate_time = int(dict[DbpcrAnalyzer.INFO_SEGMENT_NAME[DbpcrAnalyzer.TIME_POS]])
            elif dict["status"] == "send":
                if dict["type"] == "request":
                    self.send_request += 1
                elif dict["type"] == "response":
                    self.send_response += 1
                elif dict["type"] == "tgtdata":
                    self.send_tgtdata += 1
                elif dict["type"] == "bdctdata":
                    self.send_bdctdata += 1
            elif dict["status"] == "receive":
                if dict["type"] == "request":
                    self.receive_request += 1
                elif dict["type"] == "response":
                    self.receive_response += 1
                elif dict["type"] == "tgtdata":
                    self.receive_tgtdata += 1
                elif dict["type"] == "bdctdata":
                    self.receive_bdctdata += 1
            elif dict["status"] == "destination":
                self.delivery_data += 1
                delivery_time = int(dict[DbpcrAnalyzer.INFO_SEGMENT_NAME[DbpcrAnalyzer.TIME_POS]])
            elif dict["status"] == "cancel" and dict["type"] == "response":
                self.cancel_response += 1
            else:
                warn("error packet!")
        if generate_time != 0 and delivery_time != 0:
            delay = delivery_time - generate_time
            self.transmit_delay = (delay + self.transmit_delay * (self.delivery_data - 1)) / self.delivery_data


"""
DBR协议信息分析器
"""


class DbrAnalyzer(AbstractRoutingInfoAnalyzer):

    TIME_POS = 0
    NODE_POS = 1
    SEQ_POS = 2
    STATUS_POS = 3
    TYPE_POS = 4
    INFO_SEGMENT_NAME = ["time", "nodeId", "sequence", "status", "type"]

    def __init__(self, file_num, protocol_name):
        super().__init__(file_num, protocol_name)
        self.generate_data = 0
        self.delivery_data = 0
        self.send_data = 0
        self.receive_data = 0
        self.cancel_data = 0
        self.transmit_delay = 0

    def analyze_info(self):
        for info in self.infos:
            info_segments = info.split(":")
            info_dict = {DbrAnalyzer.INFO_SEGMENT_NAME[DbrAnalyzer.NODE_POS]: info_segments[DbrAnalyzer.NODE_POS], DbrAnalyzer.INFO_SEGMENT_NAME[DbrAnalyzer.TIME_POS]: info_segments[DbrAnalyzer.TIME_POS], DbrAnalyzer.INFO_SEGMENT_NAME[DbrAnalyzer.STATUS_POS]: info_segments[DbrAnalyzer.STATUS_POS], DbrAnalyzer.INFO_SEGMENT_NAME[DbrAnalyzer.TYPE_POS]: info_segments[DbrAnalyzer.TYPE_POS]}
            if self.packet_record.get(info_segments[DbrAnalyzer.SEQ_POS]):
                self.packet_record[info_segments[DbrAnalyzer.SEQ_POS]].append(info_dict)
            else:
                self.packet_record[info_segments[DbrAnalyzer.SEQ_POS]] = [info_dict]
        for value in self.packet_record.values():
            value.sort(key=lambda ele: int(ele[DbrAnalyzer.INFO_SEGMENT_NAME[DbrAnalyzer.TIME_POS]]))
        for value in self.packet_record.values():
            self.count(value)
        print("generate_data: ", self.generate_data)
        print("delivery_data: ", self.delivery_data)
        print("send_data: ", self.send_data)
        print("receive_data: ", self.receive_data)
        print("cancel_data: ", self.cancel_data)
        print("transmit_delay: ", self.transmit_delay / 1000, "s")
        print("The delivery radio is %.2f%%" % (self.delivery_data / self.generate_data * 100))
        print("The average transmission delay is %.2fs" % (self.transmit_delay / 1000))
        print("The network average packet is", (self.send_data) / self.generate_data)

    def count(self, info_list):
        generate_time = 0
        delivery_time = 0
        for dict in info_list:
            if dict["status"] == "source":
                self.generate_data += 1
                generate_time = int(dict[DbrAnalyzer.INFO_SEGMENT_NAME[DbrAnalyzer.TIME_POS]])
            elif dict["status"] == "send" and dict["type"] == "data":
                    self.send_data += 1
            elif dict["status"] == "receive" and dict["type"] == "data":
                    self.receive_data += 1
            elif dict["status"] == "destination":
                self.delivery_data += 1
                delivery_time = int(dict[DbrAnalyzer.INFO_SEGMENT_NAME[DbrAnalyzer.TIME_POS]])
            elif dict["status"] == "cancel" and dict["type"] == "data":
                self.cancel_data += 1
            else:
                warn("error packet!")
        if generate_time != 0 and delivery_time != 0:
            delay = delivery_time - generate_time
            self.transmit_delay = (delay + self.transmit_delay * (self.delivery_data - 1)) / self.delivery_data


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("请按如下格式输入: python3 routing_performance.py info文件数量 协议名称")
        print("如: python3 routing_performance.py 3 dbpcr")
        sys.exit(0)

    analyzer_module = sys.modules["__main__"]
    protocol_analyzer_class = getattr(analyzer_module, capwords(sys.argv[2]) + "Analyzer")
    analyzer = protocol_analyzer_class(sys.argv[1], sys.argv[2])
    analyzer.analyze_info()
