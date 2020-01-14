#ifndef LIBEV_TOOL_H
#define LIBEV_TOOL_H

#include <ev.h>
#include "singleton.h"
#include "schedule.h"
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>


class LibevTool : public utils::Singleton<LibevTool>
{
public:
  struct ev_loop* GetLoop(){
    return loop;
  }

  void Setnonblock(int &fd);
  ssize_t Readn(int fd, void* ptr, size_t n_size);
  ssize_t Writen(int fd, void* ptr, size_t n_size);
  template<typename Queue>
  static void WritefdCB(EV_P_ ev_io *w, int revents){
    IODataPtr data = Queue::Front();
    // ssize_t res = write(data->fd, data->data, data->len); // 由于fd是非阻塞，其写操作会出现U1 P72所讲述的实际操作的字节数比要求的少

    ssize_t res = LibevTool::Instance().Writen(data->fd, data->data, data->len);
    LOG(DEBUG) << "Write " << data->len << " bytes data to fd " << data->fd;
    delete [] data->data;
    if (res == -1) {
      return;
    }
    ev_io_stop(EV_A_ w);

    Queue::Pop();

    if (!Queue::Empty()) {
      ev_io_start(EV_A_ w);
    }

    return;
  }
  static void IdleCB(EV_P_ ev_idle *w, int revents);
  void WakeLoop();


  LibevTool();
private:
  struct ev_loop *loop;
};




#endif
