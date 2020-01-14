# -*- coding: utf-8 -*-   
import socket
import Queue
import threading
import traceback
import time

from uwan_performance_auto import *

shareQueue = Queue.Queue(maxsize=0)

def put_log(uwan):
    while 1:
        msg = shareQueue.get()
        msg_list = msg.splitlines()
        for msg in msg_list: 
            #print 'receive message ->', msg, 'number of node ->', getNodeNum()
            log_dict = uwan.init_msg(msg)
            if log_dict is not None:
                uwan.loglist.append(uwan.init_msg(msg))
        
    
def performanceAnalysis(path):
    uwan = UWANPerformance(path) # 性能计算
    '''
      针对每个性能生成图
      Route: deli_rate_clean 净收包率
             delay 端到端时延
        throughput 网络吞吐量
      Mac:   delay 点到点时延
             throughput MAC层吞吐量
             total_deli_rate 总收包/总发包
    '''
    layer_index = ['MAC-deli_rate_without_broadcast', 'MAC-Throughput', 'MAC-Delay',
                   'Route-deli_rate_clean', 'Route-Throughput', 'Route-Delay']
    location_num = [231, 232, 233, 234, 235, 236]
    # 生成画布
    plt.style.use('seaborn-notebook')
    fig = plt.figure(figsize=(40, 30))
    fig.suptitle('HSM Stack Protocol Performance Display -- Running...')
    plt.subplots_adjust(left=0.1, right=0.9, bottom=0.1, top=0.9, wspace=0.3, hspace=0.25)
    plt.ion()
    display_list = []
    for i, item in enumerate(layer_index):
        layer = item.split('-')[0]
        index = item.split('-')[1]
        performance_display = Display(layer, index)
        performance_display.generate(fig, location_num[i])
        display_list.append(performance_display)

    putlog_thread = threading.Thread(target=put_log, args=(uwan,))
    putlog_thread.start()
    while True:
        try:
            uwan.performance_detail()
            result = uwan.json_data
            print "result: ", result
            endTime = datetime_timestamp(result['Overall']['end_time'])
            if time.time() - endTime >= 100 and endTime != 0:
                stopTime = time.time() - endTime
                fig.suptitle("HSM Stack Protocol Performance Display -- Elapsed Time since Last Refresh: %ds..." % stopTime)
                print "No Data Transfer in %ds" % stopTime 
                plt.pause(0.001)
            else:
                for item in display_list:
                    layer = item.layer
                    index = item.index
                    value = result[layer][index]
                    if value != "" and endTime != 0 and (time.time() - endTime < 100):
                        simu_time = time.time() - start_time # 仿真进行持续时间
                        item.add_scatter(simu_time, value)
                        plt.pause(0.001) 
                    else:
                        print "No Data Transfer Now, Please Wait! "
                        break
        except Exception as ex:
            print 'traceback.print_exc():'; traceback.print_exc()
        # 每 5s 计算一次性能
        time.sleep(5)

if __name__ == '__main__':
    path = sys.argv[1]
    performanceAnalysis(path)





