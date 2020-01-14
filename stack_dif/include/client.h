#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <ev.h>
#include "libev_tool.h"
#include <netinet/in.h>
#include "schedule.h"

struct client;
extern client* clientLink;

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

template<typename QUEUETYPE>
void cliinit(client* cli) {
  if (cli->_write_watcher == NULL){
    cli->_write_watcher = new ev_io;
    // ev_io_init(cli->_write_watcher, clientwritecb, cli->_socketfd, EV_WRITE);
    ev_io_init(cli->_write_watcher, (LibevTool::WritefdCB<QUEUETYPE>), cli->_socketfd, EV_WRITE);
  }
}

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
