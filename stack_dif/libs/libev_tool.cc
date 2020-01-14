#include "libev_tool.h"

LibevTool::LibevTool()
{
  loop = EV_DEFAULT;
}

ssize_t
LibevTool::Writen(int fd, void* ptr, size_t n_size)
{
    const char* cptr;
    ssize_t nwrite(0);
    size_t nleft;

    nleft = n_size;
    cptr = (char*)ptr;

    while (nleft > 0) {
        if ((nwrite = write(fd, cptr, nleft)) < 0) {
            if (errno == EINTR) {
                nwrite = 0;
            }else if(errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }else{
                return -1;      // 出错
            }
        }
        nleft -= nwrite;
        cptr += nwrite;
    }

    return n_size;              // 一定是写完n_size才会退出循环的 因为没有引入buffer而做写出数据的标记

}

ssize_t
LibevTool::Readn(int fd, void* ptr, size_t n_size)
{
    char *cptr;                 // 步进为1的指针
    size_t nleft;                  // 剩余读数量
    ssize_t nread(0);                  // 已读数量

    cptr = (char*)ptr;
    nleft = n_size;

    while (nleft > 0) {
        if ((nread = read(fd, cptr, nleft)) < 0) {
            LOG(ERROR) << "the errornum is " << errno << " and the mes is " << strerror(errno);
            if (errno == EINTR) {
                nread = 0;
            }else if(errno == EAGAIN || errno == EWOULDBLOCK){
                // 一般结合epoll后，调用read函数一定有东西可以读的；
                // 那么什么时候会有这个错误出现呢？
                // 读完一个数据包再继续读的时候，因为接收缓存区为空，因此返回这个errno，此时可以退出循环了，因此用break。意味出现这个errno表示数据读取完毕
                break;
            }else{
                return (-1);
            }
        }else if(nread == 0){   // EOF结束符，收到FIN分节
            return (0);
            // break;
        }else{
            cptr += nread;
            nleft -= nread;
        }
    }

    return (n_size - nleft);
}

void
LibevTool::Setnonblock(int &fd)
{
    int flags;

    flags = fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
}

void
LibevTool:: IdleCB(EV_P_ ev_idle *w, int revents){
    ev_idle_stop(EV_A_ w);
    free(w);
}

void LibevTool::WakeLoop(){
    struct ev_idle *idle_watcher = new struct ev_idle;
    ev_idle_init(idle_watcher, IdleCB);
    ev_idle_start(loop, idle_watcher);
}
