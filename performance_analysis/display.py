# -*- coding: utf-8 -*-

"""
python_visual_animation.py by xianhu
"""

import numpy as np
import matplotlib
import matplotlib.pyplot as plt
import matplotlib.font_manager as fm
from matplotlib import style
from mpl_toolkits.mplot3d import Axes3D
import json
import sys
reload(sys)
sys.setdefaultencoding('utf8')
import threading
import time
import random

def load_result(layer, index):
    result_list = []
    f = open('result.json')
    lines = f.readlines()
    for line in lines:
        line = json.loads(line)
        result_list.append(line[layer][index])
    return result_list


class Display(object):
    def __init__(self, layer, index):
        self.layer = layer
        self.index = index

    def generate(self, fig, location):
        self.ax = fig.add_subplot(location)
        self.ax.grid(True, linestyle='-.')
        self.ax.tick_params(axis='x', labelsize=10, width=3)
        self.ax.tick_params(axis='y', labelsize=10, width=3)

        # 设置X轴
        self.ax.set_xlabel("Simulation time (s)")
        #self.ax.set_xticks(np.linspace(100, 500, 5, endpoint=True))

        # 设置Y轴
        layer_dict = {'Route': 'E2E', 'MAC': 'P2P'}
        self.ax.set_ylabel(str(self.layer+' '+self.index))
        if self.index in ['deli_rate_clean']:
            self.ax.set_ylabel(layer_dict[self.layer] + ' '+ 'Delivery Rate' + " (%)")
            self.ax.set_ylim(0, 100)
            self.ax.set_yticks(np.linspace(0, 100, 10, endpoint=True))
        if self.index in ['deli_rate_without_broadcast']:
            self.ax.set_ylabel(layer_dict[self.layer] +' '+ 'Delivery Rate' + " (%)")
            self.ax.set_ylim(0, 100)
            self.ax.set_yticks(np.linspace(0, 100, 10, endpoint=True))
        if self.index in ['Throughput']:
            self.ax.set_ylabel(layer_dict[self.layer] +' '+self.index + " (bit/s)")
        if self.index in ['Delay']:
            self.ax.set_ylabel(layer_dict[self.layer] +' '+self.index + " (s)")
        

    def add_scatter(self, x, y):
        self.ax.scatter(x, y, c='b', s=25)
'''
# 生成画布
fig = plt.figure(figsize=(40, 30))
# 设置图例位置,loc可以为[upper, lower, left, right, center]
#plt.legend(loc="center", shadow=True)
fig.tight_layout()
plt.subplots_adjust(left=0.1, right=0.9, bottom=0.1, top=0.9,
wspace=0.3,hspace=0.3)
'''

if __name__ == "__main__":
    #route_deli_rate_display = Display('route', 'deli_rate')
    #route_deli_rate_display.generate(221)
    #route_th_display = Display('route', 'throughput')
    #route_th_display.generate(222)
    #route_deli_rate_list = load_result('route', 'deli_rate') 
    #route_th_list = load_result('route', 'throughput')

    start_time = time.time()
    last_time = start_time
    plt.ion()
    layer_index = ['route-deli_rate_clean', 'route-delay', 'route-throughput',
                            'mac-delay', 'mac-throughput', 'mac-total_deli_rate']
    location_num = [131, 132, 133, 231, 232, 233]
    display = []
    for i, item in enumerate(layer_index):
        layer = item.split('-')[0]
        index = item.split('-')[1]
        performance_display = Display(layer, index)
        performance_display.generate(location_num[i])
        display.append(performance_display)
    while 1:
        performance_display.add_scatter(1,1)
        plt.pause(0.001)

'''
    while True:
        now_time = time.time()
        simu_time = now_time - start_time
        if now_time - last_time >= 2:
            print 'now_time: ', now_time, ' last_time: ', last_time
            last_time = time.time()
            for item in display:
                layer = item.layer
                index = item.index
                value = random.sample(load_result(layer, index), 1)[0] 
                print value
                item.add_scatter(simu_time, value)
            plt.pause(0.001)
'''
