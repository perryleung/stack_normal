#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>


#include "clientsocket.h"
// clientsocket与client不同，前者是用来与通信机进行通信的，后者是与UI、Trace进行通信的


// bool ClientSocket::is_connected_ = false;
ClientSocketMap ClientSocket::clientsocketmap_;

void
ClientSocket::ReadCB(EV_P_ ev_io *w, int revents)
{
    IODataPtr data(new IOData);
    //先创建一个IOData对象，然后读取1032个字节的数据
    //再对对象的赋值压入全局的读事件队列中
    //最后由调度器来调用
    char *p = (char *)malloc(sizeof(char) * BUFFER_SIZE);
    if (!p) {                   // 内存分配失败
        return;
    }
    memset((void*)p, 0, BUFFER_SIZE);


    ssize_t count = LibevTool::Instance().Readn(w->fd, p, BUFFER_SIZE);

    if (count == 0) {           // 没读任何数据

        free(p);                // 释放地址
        close(w->fd);           // 关闭TCP连接  真的需要关闭连接吗？ 需要
        ev_io_stop(EV_A_ w);          // 停止事件     真的需要结束事件吗？ 需要
        LOG(DEBUG) << "socket close, stop the connection between computer and point";
        clientsocketmap_.Delete(w->fd);
        return;
    }else if(count == -1)       // 出错
        {
            free(p);            // 自己加上的，觉得应该释放地址
            close(w->fd);       // 自己加上的
            ev_io_stop(EV_A_ w);       // 自己加上的
            LOG(DEBUG) << "socket error, stop the connection between computer and point";
            clientsocketmap_.Delete(w->fd);
            // is_connected_ = false;
            return;          // 为啥这个不用释放地址关闭连接？
        }
//   p[count] = '\0';

    data->fd   = w->fd;
    data->data = p;
    data->len  = count;

    LOG(DEBUG) << "Read " << data->len << " bytes data from fd " << data->fd;

    ReadQueue::Push(data);      // 数据往上递交？

    return;
}

ClientSocketPtr
ClientSocket::NewClient(string hostname, string port)
{
    pair<string, string> sock = make_pair(hostname, port);
    ClientSocketPtr targetClient =  clientsocketmap_.Find(sock);

    if (targetClient == NULL) {
    	  //先检查有没有这个IP和端口对应的连接，如果没有就创建一个
        targetClient = make_shared<ClientSocket>();
        if (targetClient->Init(hostname, port)) {
        	  //调用Init初始化
            clientsocketmap_.Insert(sock, targetClient->fd_, targetClient); // 计数加一
        }
    }
    //除了初始化以外，还在clientsocketmap_映射对象中插入套接字、文件/套接字描述符、ClientSocke指针
    //可以用ClientSocketMap类中的Find函数来通过套接字找到对应的ClientSocke对象返回指针，如果没有则返回NULL

    return targetClient;// 虽然返回值是一个智能指针，但是返回的对象赋值给一个弱智针，
    									  // 所以并不会增加计数的，因此上面即使是make_shared了，
    									  // 但是创建套接字失败的话，该智能指针计数还是会为0
}


bool
ClientSocket::Init(string hostname, string port)
{
    //建立TCP连接、初始化私有成员
    server_addr_ = hostname;
    server_port_ = port;
    
    //Dap对象初始化的时候会在SapMap里注册这个协议，并且DapMap里也会有这个协议
    //上层通过SapMap找到物理层，套接字收到数据就通过DapMap找到物理层

    struct addrinfo hints;      // UNIX网络编程卷一（下称作U1）第246页,需要返回的地址的一些配置信息
    struct addrinfo *res, *p;   // res是getaddrinfo的查询结果，p用作浅复制
    int m_sock = 0;

    fd_ = -1;

    memset(&hints, 0, sizeof(struct addrinfo)); // 先将hints清空,hints = 0

    hints.ai_family   = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags    = AI_CANONNAME; // 返回主机的规范名字
    hints.ai_protocol = 0; /* Any protocol */

    if (getaddrinfo(server_addr_.c_str(), server_port_.c_str(), &hints, &res) != 0) { //U1 P246 传入hostname与port，以及期望返回的信息在结构体hints中，返回包含所有符合要求的地址（以链表的形式）
        return false;
    }

    for (p = res; p != NULL; p = p->ai_next) { // 可以参照U1 P255
        m_sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (m_sock == -1) {
            continue;
        }
        if (connect(m_sock, p->ai_addr, p->ai_addrlen) != -1) { // 客户端连接，只要连接到一个即可退出
            break;
        }
        close(m_sock);          // 不能成功connect的话就需要close；
    }

    if (p == NULL) {
        return false;
    }
        // is_connected_ = true;
    freeaddrinfo(res);          // U1 P251

    LibevTool::Instance().Setnonblock(m_sock);        // 设置非阻塞 U1 P343
    fd_ = m_sock;			//套接字描述符
    ev_io_init(&read_io_, ReadCB, fd_, EV_READ);			//初始化read_io_和write_io_
    //创建完TCP连接后设置非阻塞连接，然后便可以监听该端口
    ev_io_start(LibevTool::Instance().GetLoop(), &read_io_);
    ev_io_init(&write_io_, LibevTool::WritefdCB<WriteQueue>, fd_, EV_WRITE);

    return true;
}

bool ClientSocket::Free()
{
    if (close(fd_) == -1) {
        return false;
    }
    return true;
}

void ClientSocket::Write()
{
    // ev_io_init(&write_io_, WriteCB, fd_, EV_WRITE);
    ev_io_start(LibevTool::Instance().GetLoop(), &write_io_);
}

int ClientSocket::GetFd() { return fd_; }
	  // 返回套接字描述符

