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
from functools import reduce
import networkx as nx
import matplotlib.pyplot as plt
from datetime import datetime

#import sys
#reload(sys)
#sys.setdefaultencoding('utf8')


"""
说明：
interval_time 为路由改变间隔时间, 单位s
"""
INTERVAL_TIME = 1800
RESULT_EXCEL = "result.xlsx"

"""
时间戳转换工具
"""
def timestamp_datetime(value):
    format = '%Y-%m-%d %H:%M:%S'
    value = time.localtime(value)
    dt = time.strftime(format, value)
    return dt

def datetime_timestamp(value):
    try:
        format = '%Y-%m-%d %H:%M:%S'
        d = datetime.strptime(value, "%Y-%m-%d %H:%M:%S")
        t = d.timetuple()
        timeStamp = int(time.mktime(t))
        timeStamp = float(str(timeStamp) + str("%06d" % d.microsecond))/1000000
        return timeStamp
    except Exception as ex:
        return 0


"""
秒与小时转换工具
"""
def sec2hour(value):
    min, sec = divmod(value, 60)
    hour, min = divmod(min, 60)
    return ("%02d:%02d:%02d" % (hour, min, sec))


"""
求列表平均值
"""
def average(L, total=0.0):
    if len(L) == 0:
        return 0.0
    num = 0
    for item in L:
        total += item
        num += 1
    return total / num

def intLen(i):
    return len('%d'%i)

def char2num(s):
    return {'0':0,'1':1,'2':2,'3':3,'4':4,'5':5,'6':6,'7':7,'8':8,'9':9}[s]

def str2int(s):
    return reduce(lambda x,y:x*10+y,map(char2num,s))

def int2dec(i):
    return i/(10**intLen(i))

def str2float(s):
  return reduce(lambda x,y:x+int2dec(y),map(str2int,s.split('.')))

"""
端到端详细信息，0924 update
"""
def str_to_list(info):
    packet_num = []
    delay_time = []
    delay = 0.0
    for item in info:
        item = item.split(":")
        packet_num.append(int(item[1]))
        if len(item) > 2:
	    each_delay = int(item[2])
	    #print "each_delay: ", each_delay
            delay_time.append(each_delay)
    delay = average(delay_time)
    return packet_num, delay

