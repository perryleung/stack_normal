#include <netdb.h>//Unix和Linux特有的头文件，主要定义了与网络有关的结构、变量类型、宏、函数等
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>//是Unix/Linux系统的基本系统数据类型的头文件，含有size_t，time_t，pid_t等类型
#include <string>
#include <ev.h>
#include "libev_tool.h"
#include "hsm.h"
#include "message.h"
#include "module.h"
#include "sap.h"
#include "schedule.h"
#include "client.h"
#include "packet.h"
#include "routetable.h"
#include "app_datapackage.h"
#include "tools.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <stdlib.h>
#define	SA	struct sockaddr
#define	LISTENQ		1024	/* 2nd argument to listen() */

using namespace std;


INITIALIZE_EASYLOGGINGPP //初始化日志库easylogging++

struct client;
client* clientLink = NULL;//是一个全局变量，整个进程只有这一个连接
RouteTable globalRouteTable;
string nodeID;
string logNodeId;
int m_fd = STDIN_FILENO;
int listenfd;
uint16_t packetSerialNum = 0;
uint16_t isTestMode = 0;
client* UIClient = NULL;
client* TraceClient = NULL;
struct sockaddr_in servaddr;
static uint16_t packetnum;
static uint16_t timeintervalMin;
static uint16_t timeintervalMax;
vector<uint16_t> othernodeIDVec;


void read_stdin(EV_P_ ev_io *w, int revents)
{
    // static bool isSend = false;
    // if(isSend)
    // {
        int res;
        char data[1024];
        res = read(m_fd, data, 1024);
    
        pkt::Packet pkt(res-1);

        memcpy(pkt.Raw(), data, res-1);
        msg::MsgSendDataReqPtr m(new msg::MsgSendDataReq);
        m->packet  = pkt;
        char pc[2] = {data[0], '\0'};
        m->address=atoi(pc);
        //+++这里少了一段是否要CRC的代码
        /*
        LOG(INFO)<<"DEMO data[1]"<<data[1]<<"XXXXXXXXXXX";
        if(data[1]=='1'){
            m->needCRC = true;
            LOG(INFO)<<"demo need CRC";
        } else if (data[1]=='0'){
            m->needCRC `=false;
            LOG(INFO)<<"demo  don't need CRC";
        }
        */
       //从SapMap映射中找到，然后调用Sap接口中的函数
        sap::SapBase* p = sap::SapMap::Find(4, 1);
        p->RecvRsp(m);
    // }else{
    
    //     getchar();
    //     isSend = true;
    // }
    //return 0;
}

void read_conn(EV_P_ ev_io *w, int revents);
void read_listen(EV_P_ ev_io *w, int revents)
{
    client* newClient = new client();
    socklen_t clilen = sizeof(newClient->_clientaddr);
    newClient->_socketfd = accept(listenfd, (SA *) &(newClient->_clientaddr), &clilen);
    if (isTestMode == 0 && UIClient == NULL) {
        cout<<"UIClient"<<endl;
        UIClient = newClient;
    } else if (isTestMode == 1 && TraceClient == NULL) {
        cout<<"TraceClient"<<endl;
        TraceClient = newClient;
    } else {
   	LOG(DEBUG) << "Client connect error";
    }
    //TraceClient = newClient;
    LibevTool::Instance().Setnonblock(newClient->_socketfd);

    LOG(DEBUG) << "Connect Successful";

    newClient->_read_watcher = new ev_io;
    
    //ev_io_t m_read_conn;
    ev_io_init(newClient->_read_watcher, read_conn, newClient->_socketfd, EV_READ);
    ev_io_start(LibevTool::Instance().GetLoop(), newClient->_read_watcher);
/*（如果要往UI和Trace写数据的话，就要往队列里压事件。如果UI或者终端需要往协议栈写数据的话，demo已经将监听读事件插入到循环中，不需要另外再写一个队列）
demo回调函数read_listen的处理*/
    if (isTestMode == 0)
    {
        addClient<UIWriteQueue>(newClient);
    } else 
    {
        addClient<TraceWriteQueue>(newClient);
    }
    //addClient<TraceWriteQueue>(newClient);
    // 这样就可以只有一个连接
    // ev_io_stop (EV_A_ w);
}

void read_conn(EV_P_ ev_io *w, int revents)
{
	
    int res;
    char data[1024];
    int connfd = w->fd;
    LOG(INFO)<<"是demo出现问题了吗";//？
    res = LibevTool::Instance().Readn(connfd, data, sizeof(ImageHeader));
    if(res == 0){
		return;
	}else if(res < 0){
        LOG(DEBUG) << "some error happen when read";
        cleanClient(connfd);
        return;
    }
    
    LOG(DEBUG) << "socket read Successful " << res;
    
    pkt::Packet pkt(res);
    memcpy(pkt.realRaw(), data, res);
    msg::MsgSendDataReqPtr m(new msg::MsgSendDataReq);
    m->packet  = pkt;
    m->address = data[2];
	if(data[0] == 7)
		m->needCRC = true;
    /*
    if(data[1]=='1'){
        m->needCRC = true;
        LOG(INFO)<<"demo need CRC";
    } else if (data[1]=='0'){
        m->needCRC=false;
        LOG(INFO)<<"demo  don't need CRC";
    }
    */

    sap::SapBase* p = sap::SapMap::Find(TRA_LAYER, TRA_PID);
    p->RecvRsp(m);
    //return 0;
    
}

void testsendpacket(EV_P_ ev_timer *w, int revents)
{
    if (globalRouteTable.getSize() > 1 && packetnum > 0) {
        LOG(INFO) << "send packet , there are " << --packetnum << " more packets and I have " << globalRouteTable.getSize() - 1 << " friends";
		int len = sizeof(TestPackage);
		struct TestPackage sendPackage;
        sendPackage.SerialNum = packetSerialNum++;

		sendPackage.DestinationID = (uint8_t)globalRouteTable.getEntry().at(rand_num(1, globalRouteTable.getSize())).destNode;
		//sendPackage.DestinationID = uint8_t(1);//多发一收，1作为收
        /*原：（第一句被注释掉了）
        //sendPackage.DestinationID = (uint8_t)globalRouteTable.getEntry().at(rand_num(1, globalRouteTable.getSize())).destNode;
        sendPackage.DestinationID = uint8_t(othernodeIDVec[0]);
        */

		sendPackage.SourceID = (uint8_t)atoi(nodeID.c_str());
		sendPackage.data_size = sizeof(TestPackage);
		for (unsigned i = 0; i < (len /10 - 1); ++i) { 
		    strcpy(sendPackage.data_buffer + i*10, "underwater"); 
		} 
        // packetSerialNum++;
		
		LOG(INFO)<<"SerialNum = "<<sendPackage.SerialNum;
		LOG(INFO)<<"dest = "<<(int)sendPackage.DestinationID;
		LOG(INFO)<<"sour = "<<(int)sendPackage.SourceID;
		LOG(INFO)<<"data_size = "<<(int)sendPackage.data_size;
		LOG(INFO)<<"data_buffer = "<<sendPackage.data_buffer;

        // char content[sizeof(TestPackage)] = {"0"};
        // memcpy(content, &sendPackage, sizeof(TestPackage));
		pkt::Packet pkt(len);
        memcpy(pkt.Raw(), &sendPackage, len);
        msg::MsgSendDataReqPtr m(new msg::MsgSendDataReq);
        m->packet = pkt;
        m->address = (int)sendPackage.DestinationID;
        //m->needCRC = true; //原这句话未被注释
        sap::SapBase *p = sap::SapMap::Find(TRA_LAYER, TRA_PID);
        //p->RecvRsp(m);    //原这句话未被注释
        p->SendReq(m,TRA_LAYER,TRA_PID);
    }else{
        LOG(INFO) << "I have no friends or no packet to send";
    }

        if (packetnum == 0) {
            ev_timer_stop(EV_A_ w);
        }else{
            ev_timer_stop(EV_A_ w);
            ev_timer_set(w, rand_num(timeintervalMin, timeintervalMax), 1.0);
            ev_timer_start(EV_A_ w);
        }

}


int main(int argc, char** argv)
{
    // easylogging++ configuration
    //el::Configurations defaultConf;
    //defaultConf.setToDefault();
    //defaultConf.set(el::Level::Global, el::ConfigurationType::Format,
    //                "%datetime{%Y-%M-%dT%H:%m:%s} [%level] [%fbase:%line] %msg");
    //el::Loggers::reconfigureLogger("default", defaultConf);
    //上面这些原都没被注释掉，是多余了？
    if (argc == 1){
        Config::Load("config.yaml");
    }else{
        Config::Load(string(argv[1]));
    }
    nodeID = Config::Instance()["drt"]["nodeID"].as<string>();

    logNodeId = Config::Instance()["Log"]["nodeId"].as<string>();//原少了此句，logNodeId原未用

    uint16_t testmode = Config::Instance()["test"]["testmode"].as<uint16_t>();

    // easylogging++ configuration
    string infoTail = "%datetime{%Y-%M-%dT%H:%m:%s} [%level] [%fbase:%line] %msg";
    string infoHead = "[Node ";
    string info = infoHead + logNodeId + "] " + infoTail;
	
    el::Configurations defaultConf;
    defaultConf.setToDefault();
    defaultConf.set(el::Level::Global, el::ConfigurationType::Format, info);
	if(testmode == 1) {
	//read record log path
		string basePath = Config::Instance()["Log"]["basePath"].as<string>();
		//string fileForReadPath = Config::Instance()["Log"]["fileForReadPath"].as<string>();
		//string fullFilePathForReadpath = basePath + fileForReadPath;
		//ifstream infile;
		//infile.open(fullFilePathForReadpath.data());
		string recordLogPath = Config::Instance()["trace"]["tracetime"].as<string>();
		//getline(infile, recordLogPath);
		//infile.close();
		string recordLogFileName = Config::Instance()["Log"]["recordLogFileName"].as<string>();
		string fullFilePathToRecordLog = basePath + "/" + recordLogPath + recordLogFileName;
		LOG(INFO) << fullFilePathToRecordLog;
		//set path to record log
		defaultConf.set(el::Level::Global, el::ConfigurationType::Filename, fullFilePathToRecordLog);	
	}
	
    el::Loggers::reconfigureLogger("default", defaultConf);


    //uint16_t testmode = Config::Instance()["test"]["testmode"].as<uint16_t>();
    isTestMode = testmode;
    ev_timer m_test_timer;
    if(testmode == 1){          // 开启数据随机发送模式
        uint16_t seed = Config::Instance()["test"]["seed"].as<uint16_t>();
        packetnum = Config::Instance()["test"]["packetnum"].as<uint16_t>();
        YAML::Node othernodeID = Config::Instance()["test"]["othernodeID"];
        othernodeIDVec.reserve(othernodeID.size());
        for (uint16_t i = 0; i < othernodeID.size(); i++) {
            othernodeIDVec.push_back(othernodeID[i].as<uint16_t>());
        }
        // copy(othernodeIDVec.begin(), othernodeIDVec.end(), ostream_iterator<int>(cout, ", "));
        for_each(othernodeIDVec.begin(), othernodeIDVec.end(), [](uint16_t i){LOG(INFO) << "target ID has " << i;});
        timeintervalMin = Config::Instance()["test"]["timeinterval"][0].as<uint16_t>();
        timeintervalMax = Config::Instance()["test"]["timeinterval"][1].as<uint16_t>();
		uint16_t timeforhandshake = Config::Instance()["test"]["timeforhandshake"].as<uint16_t>();
        LOG(INFO) << seed << ' ' << packetnum << ' ' << othernodeID.size() << ' ' << timeintervalMin << ' ' << timeintervalMax << ' ' << timeforhandshake;

        srand(seed);
        ev_timer_init (&m_test_timer, testsendpacket, timeforhandshake, 1.0);
        ev_timer_start (LibevTool::Instance().GetLoop(), &m_test_timer);


    }

    //先把SapMap中各协议初始化再跑循环
    LOG(INFO) << "the size of protocol is " << sap::SapMap::Size();
    LOG(INFO) << "the len of protocol header is " << pkt::PacketHeader::Size();
    Trace::Instance().Init();
    sap::SapMap::Init();

    uint16_t port = Config::Instance()["server"]["port"].as<uint16_t>();
    LOG(INFO) << "post is " << port;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    bind(listenfd, (SA *) &servaddr, sizeof(servaddr));
    listen(listenfd, LISTENQ);
    

    ev_io m_read_stdin;
    ev_io_init(&m_read_stdin, read_stdin, m_fd, EV_READ);
    ev_io_start(LibevTool::Instance().GetLoop(), &m_read_stdin);
    
    ev_io m_read_listen;
    ev_io_init(&m_read_listen, read_listen, listenfd, EV_READ);
    ev_io_start(LibevTool::Instance().GetLoop(), &m_read_listen);

    //把Sched作为回调函数，插入准备事件。每次循环前执行
    ev_prepare prepare_watcher;
    ev_prepare_init(&prepare_watcher, sched::Scheduler::Sched);
    ev_prepare_start(LibevTool::Instance().GetLoop(), &prepare_watcher);

    ev_run(LibevTool::Instance().GetLoop(), 0); // LibevTool::Instance().GetLoop()好像都可以使用宏定义EV_A_代替……
    return 0;
}
