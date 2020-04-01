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
    //回调函数先创建一个IOData对象，然后读出1032个字节的数据，对对象的赋值，压入全局的读事件队列中，最后由调度器来调用。（IOData事件包含了数据和文件描述符，调度器通过文件描述符找到Dap，从而通知该协议收到了数据。）

    // ssize_t res = write(data->fd, data->data, data->len); // 由于fd是非阻塞，其写操作会出现U1 P72所讲述的实际操作的字节数比要求的少
    //监听写的事件只是初始化完成了，如果要写数据要手动操作，即调用ClientSocket类中的Write()函数。其中写的回调函数是LibevTool中的WritefdCB，需要传入事件队列，而读的回调函数使用ClientSocket自身类中的ReadCB

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
