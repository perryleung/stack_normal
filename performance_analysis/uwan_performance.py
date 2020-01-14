# -*- coding: utf-8 -*-
"""
Created on Tue Jan 23 16:34:17 2018

@author: lab
"""

import os
import time
import openpyxl
import sys 
import copy
import json

import networkx as nx
import matplotlib.pyplot as plt

#import sys
#reload(sys)
#sys.setdefaultencoding('utf8')
from performanceWriter import *
from performanceTool import *
from display import *

"""
说明：
interval_time 为路由改变间隔时间, 单位s
"""
INTERVAL_TIME = 1800
RESULT_EXCEL = "result.xlsx"


class UWANPerformance(object):
    def __init__(self):
        self.path = "../trace/" #路径
        self.filenames = "" 
        self.node_num = 0
    
        self.loglist = [] 
        self.recv_field = {1:"recv", 2:"recv", 3:"sendUp"}
        self.all_log_list = []
        self.sheet_data = []
        self.data = []
        self.performance_data = dict()
        
        self.startTime =0
        self.endTime = 0
        self.row_count = 3
        self.sheet_num = 1
        
        self.route_data = []
        self.mac_data = []
        self.json_data = {
			"Overall":{
			"start_time":"",
			"end_time":"",
			"duration":""
			},
			"Route": {
			"Throughput":"",
			"send_num":"",
			"recv_num":"",
			"deli_rate":"",
			"drop_rate":"",
			"deli_rate_clean":"",
			"Delay":"",
			},
			"MAC":{
			"Throughput":"",
			"send_data_num": "",
			"recv_data_num": "",
			"send_num":"",
			"recv_num":"",
			"deli_rate":"",
			"drop_rate":"",
			"total_deli_rate":"",
            "deli_rate_without_broadcast":"",
			"Delay":"",
			}
		}
        
    def init_msg(self, log):
        """
        初始化log列表
        """
        title = ['time', 'layerID','protocolID', 
                 'packetID', 'length', 
                 'srcID', 'lastID', 'nextID', 'destID', 'helloSeq',
                 'mSrcID', 'mDestID', 'ackSeq',
                 'ptype', 'status']
        try:
            # loglist.append(dict(zip(title, item)))
            loglist = log.split(':')
            #print "loglist length: ", len(loglist)
            if len(loglist) != 15:
                print "ERROR: length != 15"
                pass
            else:
                log_dict = {title[i]: loglist[i] for i in range(len(title))}
                for item in title:
                    if item not in ['ptype', 'status']:
                        log_dict[item] = int(log_dict[item])    
                if log_dict['layerID'] == 2 and (log_dict['status'] == 'send' or log_dict['status'] == 'resend' or log_dict['status'] == 'forward'):
                    log_dict['nodeID'] = log_dict['mSrcID']
                elif log_dict['layerID'] == 2 and log_dict['status'] == 'recv':
                    log_dict['nodeID'] = log_dict['mDestID']
                elif log_dict['layerID'] == 3 and log_dict['status'] == 'send' or log_dict['status'] == 'forward':
                    log_dict['nodeID'] = log_dict['lastID']
                elif log_dict['layerID'] == 3 and log_dict['status'] == 'sendup':
                    log_dict['nodeID'] = log_dict['destID']
                # 筛选指定目的和源id的路由信息
                # if log_dict['sid'] == 1 and log_dict['did'] == 3:
                #     log_dict_list.append(log_dict)
                # 不需要筛选指定目的和源id的路由信息
                return log_dict
        except Exception as ex:
            print "init_log: ", ex
            pass
    

    def init_log(self, log):
        """
        初始化log列表
        """
        title = ['time', 'layerID','protocolID', 
                 'packetID', 'length', 
                 'srcID', 'lastID', 'nextID', 'destID', 'helloSeq',
                 'mSrcID', 'mDestID', 'ackSeq',
                 'ptype', 'status']
        log_dict_list = []
        try:
            for item in log:
                # loglist.append(dict(zip(title, item)))
                log_dict = {title[i]: item[i] for i in range(len(title))}
                for item in title:
                    if item not in ['ptype', 'status']:
                        log_dict[item] = int(log_dict[item])    
                if log_dict['layerID'] == 2 and (log_dict['status'] == 'send' or log_dict['status'] == 'resend' or log_dict['status'] == 'forward'):
                    log_dict['nodeID'] = log_dict['mSrcID']
                elif log_dict['layerID'] == 2 and log_dict['status'] == 'recv':
                    log_dict['nodeID'] = log_dict['mDestID']
                elif log_dict['layerID'] == 3 and log_dict['status'] == 'send' or log_dict['status'] == 'forward':
                    log_dict['nodeID'] = log_dict['lastID']
                elif log_dict['layerID'] == 3 and log_dict['status'] == 'sendup':
                    log_dict['nodeID'] = log_dict['destID']
                # 筛选指定目的和源id的路由信息
                # if log_dict['sid'] == 1 and log_dict['did'] == 3:
                #     log_dict_list.append(log_dict)
                # 不需要筛选指定目的和源id的路由信息
                log_dict_list.append(log_dict)
        except Exception as ex:
            print "init_log log: ", log
            print "init_log item: ", item
            print "init_log: ", ex
        return log_dict_list
    
    
    def load_log(self, path):
        """
        载入log文件，返回列表形式进行处理
        """        
        loglist = []
        logfile = open(path)
        next(logfile)
        for line in logfile:
            line = line.strip('\n')
            if len(line) == 0: # 删除空行
                break
            else:
                loglist.append(line.split(":"))
        return loglist    
        
    
    def read_log_file(self):
        self.filenames = os.listdir(self.path)
        for filename in self.filenames:
            if len(filename.split('.')) == 2 and filename.split('.')[1] == 'log':
                self.node_num += 1
                logpath = self.path + filename
                self.loglist += self.init_log(self.load_log(logpath))

    """
    吞吐量
    """
    def throughput(self, loglist, layerID):
        packet_size = 0
        flag = 0
        start_time = 0
        end_time = 0
    
        for logdict in loglist:
            if(flag == 0):
                start_time = logdict['time']
            flag = 1
            
            if (logdict['layerID'] == layerID and logdict['ptype'] == 'data' and logdict['status'] == self.recv_field[layerID]):
                packet_size += logdict['length']
            if (logdict['layerID'] == layerID and logdict['ptype'] == "data" and logdict['status'] == self.recv_field[layerID]):
                if (logdict['time'] > end_time):
                    end_time = logdict['time']
    
        if (float(end_time - start_time)) != 0:
            th = packet_size / float(end_time - start_time)
        else:
            th = 0
        return th
    
        
    
    """
    统计平均延时，需要包id
    """
    def ave_delay(self, loglist):
        delay = []
        sum_delay = 0
        ave_delay = 0
        index = 0
        num = 0
        for send in loglist:
            index += 1
            if (send['status'] == 'send' and send['layerID'] == 3 and send['ptype'] == 'data'):
                # for recv in loglist[index::]:
                for recv in loglist:
                    if (recv['packetID'] == send['packetID'] and \
                                    recv['srcID'] == send['srcID']and \
                                    recv['destID'] == send['destID']and \
                                    recv['status'] == 'sendUp' and \
                                    recv['layerID'] == 3 and recv['ptype'] == 'data'):
                        send_time = send['time']
                        recv_time = recv['time']
                        each_delay = (recv_time - send_time)
                        delay.append(each_delay)
                        sum_delay += each_delay
                        num += 1
                        break
        if num != 0:
            ave_delay = float(sum_delay) / num
        else:
            ave_delay = 0
        return ave_delay
    
    
    """
    数据包送达率
    1. 1010增加CRC校验信息
    """
    def delivery_rate(self, loglist):
    # 物理层
        phy_send_data_num = 0
        phy_recv_data_num = 0
        phy_crc_d1_num = 0 # CRC校验正确
        phy_crc_b1_num = 0 # CRC校验错误
        phy_deli_rate = 0.0
    
    # MAC层
        mac_send_data_num = 0
        mac_recv_data_num = 0
        mac_send_ack_num = 0
        mac_recv_ack_num = 0
        mac_send_broadcast_num = 0
        mac_recv_broadcast_num = 0
        mac_resend_num = 0
        mac_deli_rate = 0.0
        mac_deli_rate_without_broadcast = 0.0
    
    #    resend 在resend函数中实现 
    # 路由层
        route_send_data_num = 0
        route_recv_data_num = 0
        route_send_other_num = 0
        route_recv_other_num = 0
        route_forward_num = 0
        route_deli_rate = 0.0
       
    
        for log in loglist:
            # 统计物理层
            if log['layerID'] == 1:
                if log['ptype'] == "data" and log['status'] == "send":
                    phy_send_data_num += 1
                    print "Phy ++ !!!"
                if log['ptype'] == "data" and log['status'] == "recv":
                    phy_recv_data_num += 1  
                if log['ptype'] == "data" and log['status'] == "CRCerrord1":
                    phy_crc_d1_num += 1  
                if log['ptype'] == "data" and log['status'] == "CRCerrorb1":
                    phy_crc_b1_num += 1                  
                  
            # 统计 MAC 层 ack、braodcast、 resend
            if log['layerID'] == 2:
                if log['ptype'] == "data" and log['status'] == "send":
                    mac_send_data_num += 1
                if log['ptype'] == "data" and log['status'] == "recv":
                    mac_recv_data_num += 1  
                if log['ptype'] == "ack" and log['status'] == "send":
                    mac_send_ack_num += 1
                if log['ptype'] == "ack" and log['status'] == "recv":
                    mac_recv_ack_num += 1   
                if log['ptype'] == "broadcast" and log['status'] == "send":
                    mac_send_broadcast_num += 1
                if log['ptype'] == "broadcast" and log['status'] == "recv":
                    mac_recv_broadcast_num += 1   
                if log['ptype'] == "data" and log['status'] == "resend":
                    mac_resend_num += 1 
            
            # 统计 路由层 forward、其他路由信息包
            if log['layerID'] == 3:
                if log['ptype'] == "data" and log['status'] == "send":
                    route_send_data_num +=1
                if log['ptype'] == "data" and log['status'] == "sendUp":
                    route_recv_data_num +=1
                if log['ptype'] == "data" and log['status'] == "forward":   
                    route_forward_num += 1
                if log['ptype'] != "data" and log['status'] == "send":   
                    route_send_other_num += 1
                if log['ptype'] != "data" and log['status'] == "recv":   
                    route_recv_other_num += 1
                    
    #    print "forward_num", forward_num, " ", send_num, recv_num
    #    print "++++++++"
    #    send_num -= forward_num
    #    recv_num -= forward_num 
     
        """
        说明：
        转发节点有两条记录 recv 和 forward， 这条recv不能算是接收， 所以要用总共的recv 减去 forward 的计数
        """
        mac_send_data_num += mac_resend_num
        
        if phy_send_data_num != 0:
            phy_deli_rate = (float(phy_recv_data_num) / float(phy_send_data_num))*100.0
        else:
            phy_deli_rate = 0.0
            
        if mac_send_data_num != 0:
            #print "mac_recv_data_num: ", mac_recv_data_num, "mac_recv_ack_num: ", mac_recv_ack_num, " mac_recv_broadcast_num: ", mac_recv_broadcast_num 
            #print "mac_send_data_num: ", mac_send_data_num, "mac_send_ack_num: ", mac_send_ack_num, " mac_send_broadcast_num: ", mac_send_broadcast_num 
            #print "mac_resend_num: ", mac_resend_num
            mac_deli_rate = (float(mac_recv_data_num + mac_recv_broadcast_num) / float(mac_send_data_num + mac_send_broadcast_num))*100.0
        else:
            mac_deli_rate = 0.0

        if mac_send_data_num + mac_send_ack_num > 0:
            mac_deli_rate_without_broadcast = (float(mac_recv_data_num + mac_recv_ack_num) / float(mac_send_data_num + mac_send_ack_num))*100.0
            
        if route_send_data_num != 0:
            route_deli_rate = (float(route_recv_data_num) / float(route_send_data_num))*100.0
        else:
            route_deli_rate = 0.0
        
        
        
        result = {"phy_send_data_num": phy_send_data_num, 
                  "phy_recv_data_num": phy_recv_data_num, 
                  "phy_crc_d1_num" : phy_crc_d1_num,
                  "phy_crc_b1_num" : phy_crc_b1_num,
                  "mac_send_data_num" : mac_send_data_num, 
                  "mac_recv_data_num" : mac_recv_data_num, 
                  "mac_send_ack_num" : mac_send_ack_num, 
                  "mac_recv_ack_num" : mac_recv_ack_num, 
                  "mac_send_broadcast_num" : mac_send_broadcast_num, 
                  "mac_recv_broadcast_num" : mac_recv_broadcast_num, 
                  "mac_resend_num" : mac_resend_num, 
                  "route_send_data_num" : route_send_data_num, 
                  "route_recv_data_num" : route_recv_data_num, 
                  "route_send_other_num" : route_send_other_num, 
                  "route_recv_other_num" : route_recv_other_num, 
                  "route_forward_num" : route_forward_num, 
                  "phy_deli_rate" : "%.2f" % phy_deli_rate + "%", 
                  "phy_drop_rate" : "%.2f" % float(100- phy_deli_rate) + "%",
                  "mac_deli_rate" : "%.2f" % mac_deli_rate + "%", 
                  "mac_drop_rate" : "%.2f" % float(100- mac_deli_rate) + "%",
                  "mac_deli_rate_without_broadcast" : "%.2f" % mac_deli_rate_without_broadcast + "%",
                  "route_deli_rate" : "%.2f" % route_deli_rate + "%",
                  "route_drop_rate" : "%.2f" % float(100- route_deli_rate) + "%"}
    
    #    print "发送次数：", send_num, "接收次数:", recv_num
        return result
    
    
    def phy_drop_rate(self, loglist):
        send_num = 0.0
        recv_num = 0.0
        for log in loglist:
            if (log['layerID'] == 1 and log['ptype'] == "data" and log['status'] == "send"):
                send_num += 1
            if (log['layerID'] == 1 and log['ptype'] == "data" and log['status'] == "recv"):
                recv_num += 1
        if send_num != 0:
            deli_rate = (float(recv_num) / float(send_num))*100.0
        else:
            deli_rate = 0
    
    #    print "发送次数：", send_num, "接收次数:", recv_num
        return deli_rate, send_num, recv_num
    """
    统计网络运行时间
    """
    def run_time(self, loglist):
        start_time = 0
        end_time = 0
        flag = 1
        '''
        for log in loglist:
            if (flag and log['layerID'] == 3 and log['ptype'] == "data" and log['status'] == "send"):
                start_time = log['time']
                flag = 0
            if (log['layerID'] == 3 and log['ptype'] == "data" and log['status'] == "sendUp"):
                if (log['time'] > end_time):
                    end_time = log['time']
        '''
        start_time = loglist[0]['time']
        end_time = loglist[-1]['time']
        return timestamp_datetime(start_time), \
               timestamp_datetime(end_time), \
               sec2hour(end_time - start_time)


                
    def find_route_table(self, loglist):
        forward_list = []
        route_list = []
        for log in loglist:
            if log['status'] == "forward":
                forward_list.append(log)
    
            if log['status'] == 'send' and log['ptype'] == 'data' and\
                  log['layerID'] == 2:
                route = str(log['mSrcID'])+"->"+str(log['mDestID']) 
                route_list.append(route)
            if log['status'] == 'forward' and log['layerID'] == 3:
                route = str(log['srcID'])+"->"+str(log['lastID'])+"->"+str(log['nextID']) 
                route_list.append(route)
        forward_list = list(set(route_list))
    #    print "forward_list", len(forward_list), forward_list
        return forward_list                
                


    def draw_topo(self, forward_list, draw=True):
        '''
        画图部分
        '''  
        """构造转发二维数组"""
        node_matrix = [[0 for col in range(self.node_num)] for row in range(self.node_num)]
        for route_item in forward_list:
            print "route_item", route_item
            # route_item "4->3"
            node_matrix[int(route_item[0]) - 1][int(route_item[3]) - 1] = 1
        
        """建立有向图"""
        D = nx.DiGraph() # 有向图
        for i in range(self.node_num):
            D.add_node(i+1)
        for i in range(self.node_num):
            for j in range(self.node_num):
                if node_matrix[i][j] == 1:
                    D.add_edge(i+1,j+1)
        
        if draw:
            nx.draw(D, with_labels=True, 
                    node_size=2000, node_color='k', 
                    font_size=30, font_color='w',
                    style='solid',
                    edge_color='r')                          #绘制网络G
            plt.savefig("topo.png")           #输出方式1: 将图像存为一个png格式的图片文件
            plt.show()                            #输出方式2: 在窗口中显示这幅图像
        else:
            path_list = []
            path = dict(nx.all_pairs_shortest_path(D)) # old version: path = nx.all_pairs_shortest_path(D) 
            for i in range(self.node_num):
                for j in range(self.node_num):
                    if i!=j:
                        try:
                            path_list.append(path[i+1][j+1])   
                        except:
                            pass
            return path_list
        

    def split_by_route(self, interval_time=INTERVAL_TIME): 
        route_start_time = 0
        route_start_flag = 1  
        count = 0
        self.loglist.sort(key=lambda x:x['time'])
        
        each_route_list = []
        all_route_list = []
        for log in self.loglist:
            if route_start_flag and log['ptype'] == 'hello' and log['status'] == 'send':
                route_start_time = log['time']
                route_start_flag = 0
            each_route_list.append(log)
    
            if log['time'] > (route_start_time + count*interval_time):
    #            print log['time']
                count += 1
                filename = "split_file_" + str(count) + ".txt"
                with open(filename, 'w') as log_file:
                    for item in each_route_list:
                        print >> log_file, item
                all_route_list.append(each_route_list)
                each_route_list = []
        
        # 最后一次
        filename = "split_file_" + str(count+1) + ".txt"
        with open(filename, 'w') as log_file:
            for item in each_route_list:
                print >> log_file, item
        all_route_list.append(each_route_list)                 
        return all_route_list
        
    
    def performance(self, loglist): 
        """
        result = {"phy_send_data_num": phy_send_data_num, 
                  "phy_recv_data_num": phy_recv_data_num,  
                  "mac_send_data_num" : mac_send_data_num, 
                  "mac_recv_data_num" : mac_recv_data_num, 
                  "mac_send_ack_num" : mac_send_ack_num, 
                  "mac_recv_ack_num" : mac_recv_ack_num, 
                  "mac_send_broadcast_num" : mac_send_broadcast_num, 
                  "mac_recv_broadcast_num" : mac_recv_broadcast_num, 
                  "mac_resend_num" : mac_resend_num, 
                  "route_send_data_num" : route_send_data_num, 
                  "route_recv_data_num" : route_recv_data_num, 
                  "route_send_other_num" : route_send_other_num, 
                  "route_recv_other_num" : route_recv_other_num, 
                  "route_forward_num" : route_forward_num, 
                  "phy_deli_rate" : "%.2f" % phy_deli_rate + "%", 
                  "phy_drop_rate" : "%.2f" % float(100- phy_deli_rate) + "%",
                  "mac_deli_rate" : "%.2f" % mac_deli_rate + "%", 
                  "mac_drop_rate" : "%.2f" % float(100- mac_deli_rate) + "%",
                  "route_deli_rate" : "%.2f" % route_deli_rate + "%",
                  "route_drop_rate" : "%.2f" % float(100- route_deli_rate) + "%"}
        """
        routelist = self.find_route_table(loglist)
    #    
	# print "routelist", routelist
        # topo graph
	#self.draw_topo(routelist)
        
	print "=========网络性能========="
        e2e_throughput = self.throughput(loglist,3)
        p2p_throughput = self.throughput(loglist,2)
        print "端到端吞吐量:" "%.2f" % e2e_throughput
        print "点到点吞吐量:" "%.2f" % p2p_throughput
        
        delivery_info = self.delivery_rate(loglist)
        
        print "物理层"
        print "总发包数", delivery_info['phy_send_data_num'], "总收包数", delivery_info['phy_recv_data_num']
        print "发数据包数：", delivery_info['phy_send_data_num'], "收数据包数：", delivery_info['phy_recv_data_num'],\
        "传送率：", delivery_info['phy_deli_rate'], "丢包率：", delivery_info['phy_drop_rate']
        print "CRC正确数：", delivery_info['phy_crc_d1_num'], "CRC错误数：", delivery_info['phy_crc_b1_num']
        print "-----------------------------------"
        
        print "MAC层" 
        mac_send_num = delivery_info['mac_send_data_num']+ delivery_info['mac_send_ack_num'] + \
                                    delivery_info['mac_send_broadcast_num']
        mac_recv_num = delivery_info['mac_recv_data_num']+ delivery_info['mac_recv_ack_num'] + \
                                    delivery_info['mac_recv_broadcast_num']
        mac_send_data_num = delivery_info['mac_send_data_num'] + delivery_info['mac_send_broadcast_num']
        mac_recv_data_num = delivery_info['mac_recv_data_num'] + delivery_info['mac_recv_broadcast_num']
        print "总发包数",mac_send_num, "总收包数", mac_recv_num
        
        print "发数据包数：", delivery_info['mac_send_data_num'], "收数据包数：", delivery_info['mac_recv_data_num'],\
        "传送率：", delivery_info['mac_deli_rate'], "丢包率：", delivery_info['mac_drop_rate']
        print "发ACK：", delivery_info['mac_send_ack_num'],   "收ACK：", delivery_info['mac_recv_ack_num']
        if delivery_info['mac_send_ack_num'] == 0:
            drop_ack_rate = 0.0
        else:
            drop_ack_rate = 100.0 - float(delivery_info['mac_recv_ack_num'])/float(delivery_info['mac_send_ack_num'])*100.0
        print "丢ACK率：", "%.2f" % drop_ack_rate + "%"
        print "除去广播包的交付率", delivery_info['mac_deli_rate_without_broadcast']
        print "发其他包：", delivery_info['mac_send_broadcast_num'],   "收其他包：", delivery_info['mac_recv_broadcast_num']
        print "重传个数：", delivery_info['mac_resend_num']
        print "-----------------------------------"
        
        print "路由层"
        route_send_num = delivery_info['route_send_data_num']+ delivery_info['route_send_other_num'] + \
                                    delivery_info['route_forward_num']  
        route_recv_num = delivery_info['route_recv_data_num']+ delivery_info['route_recv_other_num'] + \
                                    delivery_info['route_forward_num']
        print "总发包数",route_send_num, "总收包数", route_recv_num
        
        print "发数据包数：", delivery_info['route_send_data_num'], "收数据包数：", delivery_info['route_recv_data_num'],\
        "传送率：", delivery_info['route_deli_rate'], "丢包率：", delivery_info['route_drop_rate']
        print "发其他包：", delivery_info['route_send_other_num'], "收其他包：", delivery_info['route_recv_other_num']
        print "转发数：", delivery_info['route_forward_num']
        print "-----------------------------------"
        
        starttime, endtime, runtime = self.run_time(loglist)
        aveDelay = self.ave_delay(loglist)
        print "平均传输时延：" "%.2f" % aveDelay, "s"
        print "节点开始发送数据时间: ", starttime
        print "所有节点停止发送接收数据时间：", endtime
        print "网络工作时间：", runtime
        self.performance_data = {"throughPut":"%.2f" % e2e_throughput, 
                            "e2eSendNum": delivery_info['route_send_data_num'],
                            "e2eRecvNum": delivery_info['route_recv_data_num'],
                            "e2eDeliRate": delivery_info['route_deli_rate'], 
                            "e2eDropRate": delivery_info['route_drop_rate'],  
                            "aveDelay": "%.2f" % aveDelay + "s", 
                            "starttime": starttime, "endtime": endtime, "runtime": runtime}

        mac_send_list, mac_recv_list, mac_send_data_list, \
            mac_recv_data_list, mac_resend_data_list, mac_send_ack_list, \
            mac_recv_ack_list, mac_send_b_list, mac_recv_b_list = self.mac_detail(loglist)
        mac_data_delay = str_to_list(mac_recv_data_list)[1]
        mac_ack_delay = str_to_list(mac_recv_ack_list)[1]
        mac_delay =  (mac_data_delay + mac_ack_delay)/2.0
        mac_delay =  mac_data_delay
        
        route_send_list, route_recv_list = self.route_detail(loglist)
        route_send_clean_num = len(list(set(route_send_list)))
        route_recv_clean_num = len(list(set(route_recv_list)))
        if route_send_clean_num != 0:
            e2eDeliRateClean = 100.0 * float(route_recv_clean_num) / float(route_send_clean_num)
        else:
            e2eDeliRateClean = 0

        if mac_send_num != 0:
            p2pDeliRateTotal = 100.0 * float(mac_recv_num) / float(mac_send_num)
        else:
            p2pDeliRateTotal = 0

        self.json_data['Overall']['start_time'] = starttime
        self.json_data['Overall']['end_time'] = endtime
        self.json_data['Overall']['duration'] = runtime 
        self.json_data['Route']['Throughput'] = float("%.2f" % e2e_throughput)
        self.json_data['Route']['send_num'] = delivery_info['route_send_data_num'] 
        self.json_data['Route']['recv_num'] = delivery_info['route_recv_data_num']
        self.json_data['Route']['deli_rate'] = float(delivery_info['route_deli_rate'].split('%')[0])
        self.json_data['Route']['drop_rate'] = float(delivery_info['route_drop_rate'].split('%')[0])
        self.json_data['Route']['deli_rate_clean'] = e2eDeliRateClean
        self.json_data['Route']['Delay'] =  float("%.2f" % aveDelay)
        self.json_data['MAC']['Throughput'] = float("%.2f" % p2p_throughput)
        self.json_data['MAC']['send_data_num'] = mac_send_data_num 
        self.json_data['MAC']['recv_data_num'] = mac_recv_data_num
        self.json_data['MAC']['send_num'] = mac_send_num 
        self.json_data['MAC']['recv_num'] = mac_recv_num
        self.json_data['MAC']['deli_rate'] = float(delivery_info['mac_deli_rate'].split('%')[0])
        self.json_data['MAC']['drop_rate'] = float(delivery_info['mac_drop_rate'].split('%')[0])
        self.json_data['MAC']['total_deli_rate'] = p2pDeliRateTotal
        self.json_data['MAC']['deli_rate_without_broadcast'] = float(delivery_info['mac_deli_rate_without_broadcast'].split('%')[0])
        self.json_data['MAC']['Delay'] = float("%.2f" % mac_delay)
        
        
    def end2end(self, loglist, path_list):
        end2end_loglist = []
        each_data = []
        data = []
        e2eSendNum = 0
        e2eRecvNum = 0
        end2endTh = 0
        e2eDeliRate = 0
        e2eAveDelay = 0
        for i in range(self.node_num):
            for j in range(self.node_num):
                if i != j:
                    for log in loglist:
                        if log['srcID'] == i+1 and log['destID'] == j+1:
                            end2end_loglist.append(log)
                    
                    end2endTh = self.throughput(end2end_loglist, 3)
                    delivery_info = self.delivery_rate(end2end_loglist)
                    e2eDeliRate = delivery_info['route_deli_rate']
                    e2eSendNum = delivery_info['route_send_data_num']
                    e2eRecvNum = delivery_info['route_recv_data_num']
                    e2eAveDelay = self.ave_delay(end2end_loglist)
                    route = [i+1, j+1]
                    each_data.append(str(route))
	            
                    if route not in path_list:
                        for index, item in enumerate(path_list):
                            if i+1 == item[0] and j+1 == item[-1]:
                                each_data.append(str(item))
                                break
                            elif index == len(path_list)-1:
                                each_data.append(str(route)+u"不存在")
                                break        
                    else: 
                        each_data.append(str(route))
    
                    route_send_list, route_recv_list = self.route_detail(end2end_loglist)
    #                print "route_send_list", route_send_list
    #                print "route_recv_list", route_recv_list
                    route_send = str_to_list(route_send_list)[0]
                    route_recv = str_to_list(route_recv_list)[0]
                    route_recv_clean = list(set(route_recv))
                    route_recv_clean.sort(key=route_recv.index)
    #                route_send_num = len(route_send)
    #                route_recv_num = len(route_recv)
                    route_recv_num_clean = len(route_recv_clean)
                    each_data.append(e2eSendNum)
                    each_data.append(e2eRecvNum)
                    each_data.append(str(route_send))
                    each_data.append(str(route_recv))
                    each_data.append(route_recv_num_clean)
                    each_data.append(e2eDeliRate)
                    if e2eSendNum != 0:
                        e2eDeliRateClean = 100.0 * float(route_recv_num_clean) / float(e2eSendNum)
                    else:
                        e2eDeliRateClean = 0
                    each_data.append(str("%.2f" % e2eDeliRateClean)+"%")
                    each_data.append("%.2f" % end2endTh)
                    each_data.append("%.2f" % e2eAveDelay)
                    
                    data.append(each_data)
                    each_data = []
                    end2end_loglist = []
        return data


    def point2point(self, loglist):
        p2p_loglist = []
        each_data = []
        data = []
        p2pTh = 0
        p2pDeliRate = 0
        p2pAveDelay = 0
        for i in range(self.node_num):
            for j in range(self.node_num):
                if i != j:
                    for log in loglist:
                        if log['mSrcID'] == i+1 and log['mDestID'] == j+1:
                            p2p_loglist.append(log)
                            
                    mac_send_list, mac_recv_list, mac_send_data_list, \
                    mac_recv_data_list, mac_resend_data_list, mac_send_ack_list, \
                    mac_recv_ack_list, mac_send_b_list, mac_recv_b_list = self.mac_detail(p2p_loglist)
                    
                    mac_send_data = str_to_list(mac_send_data_list)[0]
                    mac_recv_data, data_delay = str_to_list(mac_recv_data_list)
                    mac_recv_data_clean = list(set(mac_recv_data))
                    mac_send_ack = str_to_list(mac_send_ack_list)[0]
                    mac_recv_ack, ack_delay = str_to_list(mac_recv_ack_list)
                    
                    mac_resend_data_num = len(str_to_list(mac_resend_data_list)[0]) # 重传包数
                    mac_send_data_clean_num = len(mac_send_data) # 净发D包数
                    mac_send_data_num = mac_send_data_clean_num + mac_resend_data_num # 发D包总数
                    mac_recv_data_num = len(mac_recv_data )# 收D包总数
                    mac_recv_data_clean_num = len(mac_recv_data_clean) # 净收D包数
                    
                    
                    mac_send_num = len(str_to_list(mac_send_list)[0]) # 总发包数
                    mac_recv_num = len(str_to_list(mac_recv_list)[0]) # 总收包数
                    mac_send_ack_num = len(mac_send_ack)
                    mac_recv_ack_num = len(mac_recv_ack)
                    mac_send_b_num = len(str_to_list(mac_send_b_list)[0])
                    mac_recv_b_num = len(str_to_list(mac_recv_b_list)[0])
    
                    if mac_send_num != 0:
                        p2pDeliRate = 100.0 * float(mac_recv_num) / float(mac_send_num)
                    else:
                        p2pDeliRate = 0 # 毛交付率                 
                     
                    if mac_send_data_num != 0:
                        p2pDataDeliRate = 100.0 * float(mac_recv_data_num) / float(mac_send_data_num)
                    else:
                        p2pDataDeliRate = 0 # D包毛交付率
                        
                    if mac_send_data_clean_num != 0:
                        p2pDataDeliRateClean = 100.0 * float(mac_recv_data_clean_num) / float(mac_send_data_clean_num)
                    else:
                        p2pDataDeliRateClean = 0 # D包净交付率
                        
                    if mac_send_ack_num != 0:
                        p2pACKDeliRate = 100.0 * float(mac_recv_ack_num) / float(mac_send_ack_num)
                    else: 
                        p2pACKDeliRate = 0 # ack交付率   
                        
                    if mac_send_data_num != 0:
                        p2pResendDeliRate = 100.0 * float(mac_resend_data_num) / float(mac_send_data_num)
                    else:
                        p2pResendDeliRate = 0 # 重传率        
                        
                    if mac_send_b_num != 0:
                        p2pBDeliRate = 100.0 * float(mac_recv_b_num) / float(mac_send_b_num)
                    else:
                        p2pBDeliRate = 0 # 重传率  
                    
                    
                    p2pAveDelay = (data_delay + ack_delay)/2.0
		
		    #print "data_delay: ", data_delay, " ack_delay: ", ack_delay			

                    p2pTh = self.throughput(p2p_loglist, 2)
                    p2p = [i+1, j+1]
                    
                    """
    row_0 = [u'点->点',u'跳延时', u'吞吐量', u'总发包',u'总收包',u'毛交付率',\
        u'发D包序号（可重复）', u'收D包序号（可重复）', u'总发D包数', u'总收D包数', u'重传D包数', u'净发D包数', u'净收D包数', u'重传率', u'D包毛交付率', u'D包净交付率'\
        u'ACK发包序号（可重复）',u'ACK收包序号（可重复）', u'总发ACK数', u'总收ACK数',u'ACK交付率', \
        u'B发包数', u'B收包数',u'B包交付率']  
                    """
                    each_data.append(str(p2p)) # 例：[1,2]
                    each_data.append( "%.2f" % p2pAveDelay) #跳延时
                    each_data.append( "%.2f" % p2pTh) # 吞吐量
                    each_data.append(mac_send_num) # 总发包
                    each_data.append(mac_recv_num) # 总收包
                    each_data.append(str("%.2f" % p2pDeliRate) + "%") # 毛交付率
                    each_data.append(str(mac_send_data)) # 发D包序号（可重复）
                    each_data.append(str(mac_recv_data)) # 收D包序号（可重复）
                    each_data.append(mac_send_data_num) # 总发D包数
                    each_data.append(mac_recv_data_num) # 总收D包数
                    each_data.append(mac_resend_data_num) # 重传D包数
                    each_data.append(mac_send_data_clean_num) # 净发D包数
                    each_data.append(mac_recv_data_clean_num) #净收D包数 
                    each_data.append(str("%.2f" % p2pResendDeliRate) + "%") # 重传率
                    each_data.append(str("%.2f" % p2pDataDeliRate) + "%") # D包毛交付率
                    each_data.append(str("%.2f" % p2pDataDeliRateClean) + "%")# D包净交付率
                    each_data.append(str(mac_send_ack))# ACK发包序号(可能重复)
                    each_data.append(str(mac_recv_ack))# ACK收包序号(可能重复)
                    each_data.append(mac_send_ack_num)# 总发ACK数
                    each_data.append(mac_recv_ack_num)# 总收ACK数
                    each_data.append(str("%.2f" % p2pACKDeliRate) + "%")# ACK包交付率
                    each_data.append(mac_send_b_num)
                    each_data.append(mac_recv_b_num)
                    each_data.append(str("%.2f" % p2pBDeliRate) + "%")   
                    
                    data.append(each_data)
                    each_data = []
                    p2p_loglist = []
        return data

    def route_detail(self, loglist):
        loglist_copy = copy.deepcopy(loglist)
        route_send_list = []
        route_recv_list = []

        for log in loglist_copy:
            if log['layerID'] == 3 and log['status'] == 'send' and log['ptype'] == 'data':
                route_send_list.append(str(log['nodeID'])+"-"+str(log['destID'])+":"+str(log['packetID']))
               
                for log2 in loglist_copy:
                    if log2['layerID'] == 3 and log2['status'] == 'sendUp' and \
                           log2['srcID'] == log['srcID'] and log2['destID'] == log['destID'] and\
                               log2['packetID'] == log['packetID']:
                                   route_recv_list.append(str(log['nodeID'])+"-"+str(log['destID'])+":"+str(log['packetID']))
                           
                                   
    #    print "route_send_list", len(route_send_list),route_send_list
    #    print "route_recv_list", len(route_recv_list),route_recv_list
        return route_send_list, route_recv_list

    def mac_detail(self, loglist):
        loglist_copy = copy.deepcopy(loglist)
        sorted_list = sorted(loglist_copy, key=lambda x: x['time'])
        mac_send_data_list = []
        mac_recv_data_list = []
        mac_resend_data_list = []
        mac_send_ack_list = []
        mac_recv_ack_list = []
        mac_send_b_list = []
        mac_recv_b_list = []
        
        mac_send_list = []
        mac_recv_list = []

        #for index, log in enumerate(loglist_copy):
        for index, log in enumerate(sorted_list):
            if log['layerID'] == 2 and (log['status'] == 'send' or log['status'] == 'resend'):
                mac_send_list.append(str(log['mSrcID'])+"-"+str(log['mDestID'])+":"+str(log['ackSeq']))
                
            if log['layerID'] == 2 and log['status'] == 'recv':
                mac_recv_list.append(str(log['mSrcID'])+"-"+str(log['mDestID'])+":"+str(log['ackSeq']))
                
                
            if log['layerID'] == 2 and log['status'] == 'send' and log['ptype'] == 'data':
                mac_send_data_list.append(str(log['mSrcID'])+"-"+str(log['mDestID'])+":"+str(log['ackSeq']))
                for log2 in sorted_list[index:]:
                    if log2['layerID'] == 2 and log2['status'] == 'resend' and log2['ptype'] == 'data' and\
                        log2['mSrcID'] == log['mSrcID'] and log2['mDestID'] == log['mDestID'] and\
                        log2['ackSeq'] == log['ackSeq']:
                            log = log2
                    if log2['layerID'] == 2 and log2['status'] == 'recv' and log2['ptype'] == 'data' and\
                           log2['mSrcID'] == log['mSrcID'] and log2['mDestID'] == log['mDestID'] and\
                               log2['ackSeq'] == log['ackSeq']:
                                   delay = log2['time'] - log['time']
                                   #delay_list.append(delay)
                                   #print delay, log['time'], log2['time'], log2['mSrcID'] , log2['mDestID']
                                   mac_recv_data_list.append(str(log['mSrcID'])+"-"+str(log['mDestID'])+":"+str(log['ackSeq'])+":"+str(delay))
                                   break
                                 
            
            if log['layerID'] == 2 and log['status'] == 'resend' and log['ptype'] == 'data':
                mac_resend_data_list.append(str(log['mSrcID'])+"-"+str(log['mDestID'])+":"+str(log['ackSeq']))
            
            if log['layerID'] == 2 and log['status'] == 'send' and log['ptype'] == 'ack':
                mac_send_ack_list.append(str(log['mSrcID'])+"-"+str(log['mDestID'])+":"+str(log['ackSeq']))
                for log2 in sorted_list[index:]:
                    if log2['layerID'] == 2 and log2['status'] == 'recv' and log2['ptype'] == 'ack' and\
                           log2['mSrcID'] == log['mSrcID'] and log2['mDestID'] == log['mDestID'] and\
                               log2['ackSeq'] == log['ackSeq']:
                                   delay = log2['time'] - log['time']
                                   
                                   mac_recv_ack_list.append(str(log['mSrcID'])+"-"+str(log['mDestID'])+":"+str(log['ackSeq'])+":"+str(delay))
                                   break
                               
            if log['layerID'] == 2 and log['status'] == 'send' and log['ptype'] == 'broadcast':
                mac_send_b_list.append(str(log['mSrcID'])+"-"+str(log['mDestID'])+":"+str(log['ackSeq']))

            if log['layerID'] == 2 and log['status'] == 'recv' and log['ptype'] == 'broadcast':
                mac_recv_b_list.append(str(log['mSrcID'])+"-"+str(log['mDestID'])+":"+str(log['ackSeq']))
             
             
            
    #    print "mac_send_data_list", len(mac_send_data_list), mac_send_data_list
    #    print "mac_recv_data_list", len(mac_recv_data_list), mac_recv_data_list
    #    print "mac_send_ack_list", len(mac_send_ack_list), mac_send_ack_list
    #    print "mac_recv_ack_list", len(mac_recv_ack_list), mac_recv_ack_list
    #    print "mac_send_b_list", len(mac_send_b_list), mac_send_b_list
    #    print "mac_recv_b_list", len(mac_recv_b_list), mac_recv_b_list
    #    print "mac_send_list", len(mac_send_list), mac_send_list
    #    print "mac_recv_list", len(mac_recv_list), mac_recv_list
        
        return mac_send_list, mac_recv_list, mac_send_data_list, mac_recv_data_list, mac_resend_data_list, mac_send_ack_list, mac_recv_ack_list, mac_send_b_list, mac_recv_b_list

    def performance_detail(self):
        routelist = self.find_route_table(self.loglist)
        print "routelist ", routelist
        try:
            if len(routelist) > 0:
                self.performance(self.loglist)
                performance_write_to_excel(self.performance_data,
                [self.row_count, 1], createflag=True)
                #self.row_count += 1
                #self.sheet_num += 1
                path_list = self.draw_topo(routelist, draw=False)
                sheet_route_data = self.end2end(self.loglist, path_list)
                sheet_mac_data = self.point2point(self.loglist)
                self.route_data.append(sheet_route_data)
                self.mac_data.append(sheet_mac_data)
            for index, each_route_data in enumerate(self.route_data):
                e2e_write_to_excel(index+1, each_route_data)
            for index, each_mac_data in enumerate(self.mac_data):
                p2p_write_to_excel(index+1, each_mac_data)
        except Exception as ex:
            print "performance_detail: ", ex
            pass
     
if __name__ == '__main__':
    UWAN = UWANPerformance()
    UWAN.read_log_file()
    
    '''
    整个网络运行时间总体性能，写入excel表第一页
    '''

    UWAN.performance_detail() 
    with open("result.json", "w") as f:
    	json.dump(UWAN.json_data, f)
	print "json dump finish..."
#    '''
#    路由有间隔更新的情况，若没有可将下面一段注释掉
#    1.将某时间段的整体性能写入excel第一页
#    2.将某时间段端到端的情况统计写入excel的sheet
#    '''

    """
    split_loglist = UWAN.split_by_route()
    UWAN.startTime =0
    UWAN.endTime = 0
    UWAN.row_count = 3
    UWAN.sheet_num = 1
    """

"""
    for log in split_loglist:  
        routelist = UWAN.find_route_table(log)
#        print "routelist2", routelist
        if len(routelist) > 0:
            print "====================="
            UWAN.performance(log)
	

            performance_write_to_excel(UWAN.performance_data, [UWAN.row_count,1], createflag=True)
            UWAN.row_count += 1
            UWAN.sheet_num += 1
            path_list = UWAN.draw_topo(routelist, draw=False)
            sheet_route_data = UWAN.end2end(log, path_list)
            sheet_mac_data = UWAN.point2point(log)
            
            UWAN.route_data.append(sheet_route_data)
            UWAN.mac_data.append(sheet_mac_data)
           
    for index, each_route_data in enumerate(UWAN.route_data):
        e2e_write_to_excel(index+1, each_route_data)
    for index, each_mac_data in enumerate(UWAN.mac_data):
        p2p_write_to_excel(index+1, each_mac_data)

"""

