#include "client.h"
// clientsocket与client不同，前者是用来与通信机进行通信的，后者是与UI、Trace进行通信的

void cleanClient(int fd)
{
  client* target = clientLink;
  client* pre = NULL;
  while (target != NULL && target->_socketfd != fd) {
    pre = target;
    target = target->_next;
  }
  if (target == NULL) {
    return;
  }
  if (pre == NULL) {
    clientLink = clientLink->_next;
  }else {
    pre = target->_next;
  }

  if (target->_read_watcher != NULL) {
    ev_io_stop(LibevTool::Instance().GetLoop(), target->_read_watcher);
    delete target->_read_watcher;
    target->_read_watcher = NULL;
  }

  if (target->_write_watcher != NULL) {
    ev_io_stop(LibevTool::Instance().GetLoop(), target->_write_watcher);
    delete target->_write_watcher;
    target->_write_watcher = NULL;
  }


  close(fd);
  delete target;
  target = NULL;
}

// void clientwritecb(EV_P_ ev_io *w, int revents)
// {
   
//     IODataPtr data = UIWriteQueue::Front();
//     // ssize_t res = write(data->fd, data->data, data->len); // 由于fd是非阻塞，其写操作会出现U1 P72所讲述的实际操作的字节数比要求的少
//     // struct App_DataPackage appBuff = static_cast<struct App_DataPackage>(data->data);
    
//     ssize_t res = LibevTool::Instance().Writen(data->fd, data->data, data->len);
//     delete [] data->data;
//     if (res == -1) {
//       return;
//     }
//     ev_io_stop(EV_A_ w);

//     UIWriteQueue::Pop();

//     if (!UIWriteQueue::Empty()) {
//       ev_io_start(EV_A_ w);
//     }

//     return;
// }

//client.cc中类外的一个函数，调用的时候会把client对象中的写IO事件插入到循环中
void cliwrite(client* cli){   
    ev_io_start(LibevTool::Instance().GetLoop(), cli->_write_watcher);
}
//（注意：clinet是一个结构体，而对结构体操作的函数都写在结构体外，因此不需要加命名空间或类名。）

