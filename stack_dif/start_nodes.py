# _*_coding:utf-8 _*_
#  python3
import os
import sys
import yaml
import time
import shutil

sys.path.append('.')
CONFIGA = 'configA.yaml'
CONFIGB = 'configB.yaml'
CONFIGC = 'configC.yaml'
CONFIGD = 'configD.yaml'
CONFIGE = 'configE.yaml'
CONFIGF = 'configF.yaml'

all_config = [CONFIGA, CONFIGB, CONFIGC, CONFIGD, CONFIGE, CONFIGF]
LIB_DIR = os.path.dirname(__file__)
LOG_DIR = "tracefile"
STACK_MODULE = os.path.join(LIB_DIR, 'moduleconfig.h')

def start_nodes(listNode):
    # 修改配置文件中的文件名
    timeStamp = time.asctime(time.localtime(time.time())).replace(' ', '_').replace(':', '_')
    if not os.path.exists(LOG_DIR):
        os.mkdir(LOG_DIR)

    if not os.path.exists(LOG_DIR + '/' + timeStamp):
        os.mkdir(LOG_DIR + '/' + timeStamp)
    
    if not os.path.exists(LOG_DIR + '/' + timeStamp + '/' + 'config'):
        os.mkdir(LOG_DIR + '/' + timeStamp + '/' + 'config')

    if not os.path.exists(LOG_DIR + '/' + timeStamp + '/' + 'logfile'):
        os.mkdir(LOG_DIR + '/' + timeStamp + '/' + 'logfile')

    for i in listNode:
        configFile = all_config[i]
        config = yaml.load(open(configFile, 'r'), Loader = yaml.FullLoader)
        config['trace']['tracetime'] = timeStamp
        yaml.dump(config, open(configFile,'w'), default_flow_style=False)
        shutil.copy(configFile, LOG_DIR + '/' + timeStamp + '/' + 'config' + '/' + configFile)  

 
    shutil.copy(STACK_MODULE, LOG_DIR + '/' + timeStamp + '/' + 'config' + '/' + STACK_MODULE)  

    for i in listNode:
        configFile = all_config[i]
	command_header ="gnome-terminal -e 'bash -c \"ls; \" '"
	shell_command = './RoastFish '
	shell_command += configFile 

        log_name = LOG_DIR + '/'  +  timeStamp + '/' + 'logfile' + '/' +  str(i + 1)+".log"
        log_to_file = " >> " + log_name + " | tail -f "+ log_name
	shell_command += log_to_file

	command_header = command_header.replace('ls', shell_command)
	print(command_header)

        f = open(log_name,"w")
        f.close()
        
	os.system(command_header)
        time.sleep(1)

if __name__ == '__main__':
    listNode = []
    for i in range(len(sys.argv) - 1): 
        listNode.append(int(sys.argv[i + 1]) - 1)
    print(listNode)
    start_nodes(listNode)


