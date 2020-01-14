# _*_coding:utf-8 _*_
#  python3
import os
import sys
import re
import yaml
from multiprocessing import Process, Pool
import multiprocessing
import subprocess
import signal
import time
import shutil
import codecs

LIB_DIR = os.path.dirname(__file__)
QT_UI_DIR = os.path.join(LIB_DIR, '..')
STACK_DIR = os.path.join(QT_UI_DIR, '../stack')
LOG_DIR = "./logs"
PROFORMANCE_DIR = os.path.join(STACK_DIR, '../performance_analysis')
DWT_DIR = os.path.join(LIB_DIR, 'img_dwt')
#  STACK_DIR  = "/home/scut/wong/stack"
#TRACE_HANDLER_SERVER = "/home/scut/gitlab/performance_analysis/tracehandler.py"
TRACE_HANDLER_SERVER =os.path.join(PROFORMANCE_DIR , "tracehandler.py")
from testNode import transmitPort

LAST_LOG_DIR = "last_dir.txt"
#STACK_DIR = "/home/scut/wong/stack"
STACK_MODULE = os.path.join(STACK_DIR, 'moduleconfig.h')
CONFIG_A = os.path.join(STACK_DIR, 'configA.yaml')
CONFIG_B = os.path.join(STACK_DIR, 'configB.yaml')
CONFIG_C = os.path.join(STACK_DIR, 'configC.yaml')
CONFIG_D = os.path.join(STACK_DIR, 'configD.yaml')
CONFIG_E = os.path.join(STACK_DIR, 'configE.yaml')
CONFIG_T = 'configC.yaml'

all_config = [CONFIG_A, CONFIG_B, CONFIG_C, CONFIG_D, CONFIG_E]

tra = dict({'0':1})
net = dict({'0':2, '1':5, '2':1})
mac = dict({'0':4, '1':5, '2':6})
phy = dict({'0':1, '1':8, '2':16, '3':32})

TRA_MAP = dict({'1':"UDP"})
NET_MAP = dict({'1':"VBF", '2':"SRT", '5':"DRT"})
MAC_MAP = dict({'4':"Aloha(without Ack)", '5':"UWAloha (with Ack)", '6':"XAloha (UEP)"})
PHY_MAP = dict({'16':"Modem-MFSK", '1':"Simulation Channel", '8':"Modem-QPSK", '32':"Modem-FSK1710"})

PROTOCOL_LIST = [TRA_MAP, NET_MAP, MAC_MAP, PHY_MAP]

def gen_test_mode_yaml(node_id):
    '''
    生成测试模式的配置文件，打开测试模式
    '''
    file_path = all_config[int(node_id) - 1]
    config = yaml.load(open(file_path, 'r'))
    config['test']['testmode'] = 1
    #  config['test']['timeinterval'] = [20, 30]
    yaml.dump(config, open(file_path, 'w'), default_flow_style=False)

def gen_normal_mode_yaml(node_id):
    '''
    生成常规模式的配置文件，关闭测试模式
    '''
    file_path = all_config[int(node_id) - 1]
    print(file_path)
    config = yaml.load(open(file_path, 'r'))
    config['test']['testmode'] = 0
    yaml.dump(config, open(file_path, 'w'), default_flow_style=False)

def start_a_test_node(in_str):
    node_id, delay = [int(ii) for ii in in_str.split(',')]
    gen_test_mode_yaml(int(node_id))
    transmitPort_path = os.path.join(LIB_DIR, "testNode.py")

    config = all_config[node_id - 1]
    command_header ="gnome-terminal -e 'bash -c \"ls; \" '"
    shell_command1 = "python " + transmitPort_path + " "+ str(9080 + node_id - 1) + " "
    shell_command = "sleep " + str(delay) + " && " + shell_command1 + " && "
    shell_command += os.path.join(STACK_DIR, 'RoastFish')
    shell_command += ' '
    shell_command += config
    # fp = codecs.open(os.path.join(LOG_DIR, LAST_LOG_DIR), 'r')
    # log_dir = fp.readline()
    # fp.close()

    configYaml = yaml.load(open(config, 'r'), Loader = yaml.FullLoader)
    timeStamp = configYaml['trace']['tracetime']
    log_name = './logs/' + timeStamp + '/' + str(node_id)+".log"

    f = open(log_name,"w")
    f.close()
 
    print(log_name)
    log_to_file = " >> " + log_name + " | tail -f "+ log_name
    shell_command += log_to_file

    command_header = command_header.replace('ls', shell_command)
    print(command_header)
    os.system(command_header)

   # time.sleep(delay + 2)
   # transmitPort(bytes(str(9080 + node_id - 1), encoding='utf-8'))

def show_test_log():
    a_test_log_dir = save_all_stack_log()
    log_name = a_test_log_dir + "/trace_handler.log"
    f = open(log_name,"w")
    f.close()
    shell_command = "python " + TRACE_HANDLER_SERVER + " >> " + log_name + " | tail -f " + log_name 
    command_header ="gnome-terminal -e 'bash -c \"ls; \" '"
    command_header = command_header.replace('ls', shell_command)
    os.system(command_header)
    print(shell_command)
    
def save_all_stack_log():
    # a_test_log_dir = os.path.join(LOG_DIR, time.asctime(time.localtime(time.time())).replace(' ', '_'))
    # print(a_test_log_dir)
    # fp = codecs.open(os.path.join(LOG_DIR, LAST_LOG_DIR), 'w')
    # fp.write(a_test_log_dir)
    # fp.close()
    timeStamp = time.asctime(time.localtime(time.time())).replace(' ', '_').replace(':', '_')
    for config in all_config:
        configYaml = yaml.load(open(config, 'r'), Loader = yaml.FullLoader)
        configYaml['trace']['tracetime'] = timeStamp 
        configYaml['trace']['tracepath'] = "./logs"
        yaml.dump(configYaml, open(config,'w'), default_flow_style=False)
    log_file = './logs/' + timeStamp 
    if not os.path.exists(log_file):
        os.mkdir(log_file)

    all_config_n = [os.path.join(log_file, config_i.split('/')[-1]) for config_i in all_config]

    shutil.copy(STACK_MODULE, os.path.join(log_file, STACK_MODULE.split('/')[-1]))
    for i in range(len(all_config)):
        shutil.copy(all_config[i], all_config_n[i])
    return log_file 
	

 

def start_node(node_id):
    '''
    打开一个终端，在终端中开启一个节点
    '''
    gen_normal_mode_yaml(node_id)
    config = all_config[node_id - 1]
    shell_command = os.path.join(STACK_DIR, 'RoastFish')
    shell_command += ' '
    shell_command += config
    command_header ="gnome-terminal -e 'bash -c \"ls; \" '"
    command_header = command_header.replace('ls', shell_command)
    print(command_header)
    os.system(command_header)

def start_all_nodes():
    '''
    打开多个个终端，在每个终端中开启一个节点
    '''
    for i in range(len(all_config)):
        start_node(i+1)

def read_yaml(file_path=CONFIG_A):
    config = yaml.load(open(file_path, 'r'))
    return config


def test_main():
    '''
    '''
    test_model()
    start_all_nodes()

def normal_main():
    normal_model()
    start_all_nodes()

def kill_all_RoastFish():
    '''
    杀死所有节点协议栈进程
    '''
    id_list = get_all_pid('RoastFish')
    #  id_list = get_all_pid('Test_mode_drt_newaloha_qpsk')
    if not(id_list == -1):
        print(id_list)
        for pid in id_list:
            a = os.kill(int(pid), signal.SIGKILL)
    else:
        print("process not found")

def get_all_pid(process_name):
    cmd = "ps aux| grep " + process_name + "|grep -v grep" 
    #  cmd = "ps aux| grep 'RoastFish'|grep -v grep " 
    out = subprocess.Popen(cmd,shell=True,stdout=subprocess.PIPE)
    infos = out.stdout.read().splitlines()
    pidlist = []
    if len(infos) >= 1:
        for i in infos:
            pid = i.split()[1]
            if pid not in pidlist:
                pidlist.append(pid)
        return pidlist
    else:
       return -1

def kill_a_node(node_id):
    '''
    根据节点ID杀死相应的协议栈进程
    '''
    id_list = get_a_pid(node_id)
    if not(id_list == -1):
        for pid in id_list:
            print('killing it', pid)
            a = os.kill(int(pid), signal.SIGKILL)
    else:
        print('not found')
        pass

def get_a_pid(node_id):
    try:
        cmd = "ps aux| grep 'RoastFish'|grep -v grep | grep " + all_config[node_id - 1]
    except IndexError:
        print('we don\'t have that nodes')
        return -1
    out = subprocess.Popen(cmd,shell=True,stdout=subprocess.PIPE)
    infos = out.stdout.read().splitlines()
    pidlist = []
    if len(infos) >= 1:
        for i in infos:
            pid = i.split()[1]
            if pid not in pidlist:
                pidlist.append(pid)
        print(pidlist)
        return pidlist
    else:
        return -1

def set_test_packet_num(in_str):
    node_id, packet_num = [int(ii) for ii in in_str.split(',')]
    config = all_config[node_id - 1]
    config_i = yaml.load(open(config, 'r'))
    config_i['test']['packetnum'] = packet_num
    yaml.dump(config_i, open(config, 'w'), default_flow_style=False)
    

def set_test_time_for_handshake(in_str):
    time_t = int(in_str)
    for config in all_config:
        config_t = yaml.load(open(config, 'r'))
        config_t['test']['timeforhandshake'] = time_t
        yaml.dump(config_t, open(config, 'w'), default_flow_style=False)



def set_test_packet_interval(in_str):
    '''
    设置测试模式的发包间隔
    '''
    node_id, i_min, i_max = [int(ii) for ii in in_str.split(',')]
    config = all_config[node_id - 1]
    config_i = yaml.load(open(config, 'r'))
    config_i['test']['timeinterval'] = [i_min, i_max]
    yaml.dump(config_i, open(config, 'w'), default_flow_style=False)

def change_stack_packet_size(packet_size):
    '''
    修改stack/app_datapackage.h 中 TestPackage 成员 大小
    '''
    app_data_packet_path = os.path.join(STACK_DIR, 'include/app_datapackage.h')
    app_datapacket = open(app_data_packet_path, 'r').read()
    app_data_packet_path_new = re.sub(
            "(?<=#define PACKAGE_SIZE )[0-9]{1,4}",
            str(packet_size), app_datapacket)
    fout = open(app_data_packet_path, 'w')
    fout.write(app_data_packet_path_new)
    fout.close()

def set_fsk_gain(in_str):
    change_stack_packet_size(30)
    node_id, gain, tx_gain = [float(ii) for ii in in_str.strip(',').split(',')]
    node_id = int(node_id)
    config = yaml.load(open(all_config[node_id - 1], 'r'))
    config['fskclient']['gain'] = gain
    config['fskclient']['transmitAmp'] = tx_gain
    yaml.dump(config, open(all_config[node_id - 1], 'w'), default_flow_style=False)

def set_fsk1710_gain(in_str):
    node_id, gain, tx_gain, data_size = [float(ii) for ii in in_str.strip(',').split(',')]
    node_id = int(node_id)
    data_size = int(data_size)
    change_stack_packet_size(30)
    config = yaml.load(open(all_config[node_id - 1], 'r'))
    config['fsknewclient']['gain'] = gain
    config['fsknewclient']['transmitAmp'] = tx_gain
    config['fsknewclient']['datasize'] = data_size
    yaml.dump(config, open(all_config[node_id - 1], 'w'), default_flow_style=False)


def set_qpsk_rxgain_txgain(in_str):
    change_stack_packet_size(1183)
    node_id, rxgain, txgain = [float(ii) for ii in in_str.strip(',').split(',')]
    node_id = int(node_id)
    config = yaml.load(open(all_config[node_id - 1], 'r'))
    config['qpskclient']['rxgain'] = rxgain
    config['qpskclient']['txgain'] = txgain
    yaml.dump(config, open(all_config[node_id - 1], 'w'), default_flow_style=False)

def set_aloha_resend(in_str):
    '''
    根据node_id 修改相应的aloha协议重传时间间隔
    '''
    #  node_id = int(node_id)
    #  min_time = int(min_time)
    #  max_time = int(max_time)
    in_str = in_str.strip(',')
    node_id, min_time, max_time = [int(ii) for ii in in_str.split(',')]
    config = yaml.load(open(all_config[node_id - 1], 'r'))
    config['newAloha']['MinReSendTime'] = min_time
    config['newAloha']['ReSendTimeRange'] = max_time - min_time
    yaml.dump(config, open(all_config[node_id - 1], 'w'), default_flow_style=False)

def set_srt_entrys(node_id, entrys_str):
    '''
    根据node_id，修改相应节点的静态路由表
    '''
    node_id = int(node_id)
    config = yaml.load(open(all_config[node_id - 1], 'r'))
    entrys_str = entrys_str.strip(';')
    entrys = entrys_str.split(';')
    for i, entry in enumerate(entrys):
        config['Srt']['entry'+str(i+1)] = [int(ii) for ii in entry.split(',')]
    yaml.dump(config, open(all_config[node_id - 1], 'w'), default_flow_style=False)

def get_current_roastfish_recipe():
    module_config = open(os.path.join(STACK_DIR, 'moduleconfig.h'), 'r').readlines()
    module_config = [ii  for ii in module_config if "PID" in ii]
    fout = open(os.path.join(QT_UI_DIR, "recipe.txt"), 'w')
    for i, line in enumerate(module_config):
        match = re.search(r"([0-9]{1,2})", line)
        fout.write(PROTOCOL_LIST[i][str(match.group(1))] + "\n")
        print(match.group(1))


        




def set_module_config(in_str):
    '''
    tra:[0] -> [1]
    net:[0, 1] -> [2, 4]
    mac:[0, 1] -> [5, 4]
    phy:[0, 1, 2] -> [4, 8, 1]
    '''
    in_str = in_str.strip(',')
    tra_id, net_id, mac_id, phy_id = in_str.split(',')

    module_config = open(os.path.join(STACK_DIR, 'moduleconfig.h'), 'r').read()
    print(str(tra[tra_id]), str(net[net_id]), str(mac[mac_id]), str(phy[phy_id]))

    module_config = re.sub("TRA_PID \([0-9]{1,2}\)", 
            "TRA_PID (" + str(tra[tra_id])+ ")", module_config)

    module_config = re.sub("NET_PID \([0-9]{1,2}\)", 
            "NET_PID (" + str(net[net_id])+ ")", module_config)

    module_config = re.sub("MAC_PID \([0-9]{1,2}\)", 
            "MAC_PID (" + str(mac[mac_id])+ ")", module_config)

    module_config = re.sub("PHY_PID \([0-9]{1,2}\)", 
            "PHY_PID (" + str(phy[phy_id])+ ")", module_config)
    fout = open(os.path.join(STACK_DIR, 'moduleconfig.h'), 'w')
    fout.write(module_config)

def gen_RoastFish(in_str):
    set_module_config(in_str)
    shell_command0 = "cd " + STACK_DIR + " && make clean"
    shell_command1 = "cd " + STACK_DIR + " && make"
    command_header ="gnome-terminal -e 'bash -c \"ls; \" '"
    #  command_header = command_header.replace('ls', shell_command0)
    os.system(command_header.replace('ls', shell_command0))
    os.system(command_header.replace('ls', shell_command1))

def qt_fountain_manager(argv_list):
    ana_py = '/home/ucc/anaconda2/bin/ipython '
    interface = os.path.join(DWT_DIR, 'qt_fountain_manager.py')

    argv_list[0] = interface
    argv_list = [ii+' ' for ii in argv_list]
    shell_command = ''.join(argv_list)
    print(shell_command)
    command_header ="gnome-terminal -e 'bash -c \"ls; \" '"
    os.system(command_header.replace('ls', ana_py + shell_command))


if __name__ == '__main__':
    pass
    #  config = read_yaml(CONFIG_T)
    #  my_yaml_dump(config, 'a')
    #  gen_test_mode_yaml(CONFIG_T)
    #  test_main()
    #  start_all_nodes()
    #  time.sleep(5)
    #  kill_all_node()
    #  set_aloha_resend(3, 100, 102)
    #  gen_RoastFish()
    #  set_module_config("1,1,1")
    #  change_stack_packet_size(1000)
    save_all_stack_log()


    



