#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <ev.h>
#include "libev_tool.h"
#include <netinet/in.h>
#include "schedule.h"
/*
client是用来与UI、Trace进行通信的
*/
struct client;
extern client* clientLink;
//一个文件描述符，一个读IO事件，一个写IO事件，还有一个保存另一个client结构体的指针（如果有的话）。
//个人认为，因为这是一个结构体而不是类，因此对client中成员进行操作的函数在结构体外直接写了。（cliwrite就是在类外，可以实现把写的事件插入循环的功能）
struct client
{
  struct sockaddr_in _clientaddr;
  int _socketfd;
  ev_io* _read_watcher;
  ev_io* _write_watcher;
  client* _next;

  client():
    _socketfd(-1),
    _read_watcher(NULL),
    _write_watcher(NULL),
    _next(NULL)
  {}
};



void cleanClient(int );
void clientwritecb(EV_P_ ev_io *w, int revents);
void cliwrite(client* );

/*注意：client对象中的写IO事件是addClient函数里，调用cliinit来初始化的。（这里会传入一个模板参数，如果是与UI通信则传入队列是UIWriteQueue，如果是与Trace通信则传入队列是TraceWriteQueue。通过传入模板初始化事件，当调用cliwrite的时候，就是把该client的写事件插入循环）*/
template<typename QUEUETYPE>
void cliinit(client* cli) {
  if (cli->_write_watcher == NULL){
    cli->_write_watcher = new ev_io;
    // ev_io_init(cli->_write_watcher, clientwritecb, cli->_socketfd, EV_WRITE);
    ev_io_init(cli->_write_watcher, (LibevTool::WritefdCB<QUEUETYPE>), cli->_socketfd, EV_WRITE);
  }
}
//把UIWriteQueue放入，监听套接字是否可写，然后调用WritefdCB
template<typename QUEUETYPE>
void addClient(client* cli) {
    if (clientLink == NULL) {
        clientLink = cli;
    }else {
        cli->_next = clientLink;
        clientLink = cli;
    }
    cliinit<QUEUETYPE>(cli);
}


#endif
