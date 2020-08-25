#include "serial.h"
#include "schedule.h"
#include "utils.h"

namespace serial {

bool Serial::Init(string port, string baudrate)
{
    port_     = port;
    baudrate_ = baudrate;

    if (!Open()) {
        PLOG(WARNING) << "Open serial port " << port << " failed";
        return false;
    }
    LOG(DEBUG) << "Open serial port " << port << " successed";

    if (!Flush()) {
        PLOG(WARNING) << "Flush serial port " << port << " failed";
        return false;
    }
    LOG(DEBUG) << "Flush serial port " << port << " successed";

    ev_io_init(&read_io_, ReadCB, fd_, EV_READ);
    ev_io_start(LibevTool::Instance().GetLoop(), &read_io_);

    return true;
}

bool Serial::Open()
{
    int bd, fd, rv;

    fd_ = -1;

    if ((fd = open(port_.c_str(), O_RDWR | O_NONBLOCK)) == -1) {
        return false;
    }

    switch (atoi(baudrate_.c_str())) {
        case 9600:
            bd = B9600;
            break;
        case 19200:
            bd = B19200;
            break;
        case 38400:
            bd = B38400;
            break;
        case 115200:
            bd = B115200;
            break;
        default:
            close(fd);
            return false;
    }

    struct termios tio;

    memset(&tio, 0, sizeof(tio));

    tio.c_iflag     = 0;
    tio.c_oflag     = 0;
    tio.c_cflag     = CS8 | CREAD | CLOCAL;
    tio.c_lflag     = 0;
    tio.c_cc[VMIN]  = 1;
    tio.c_cc[VTIME] = 5;

    rv = cfsetospeed(&tio, bd);
    rv |= cfsetispeed(&tio, bd);
    rv |= tcsetattr(fd, TCSANOW, &tio);

    if (rv) {
        close(fd);
        return false;
    }

    fd_ = fd;

    return true;
}

bool Serial::Free() { return close(fd_); }
bool Serial::Flush()
{
    if (tcflush(fd_, 0) == -1) {
        return false;
    }

    return true;
}

void Serial::ReadCB(EV_P_ ev_io *w, int revents)
{
    assert(w != NULL);
    assert(w->fd >= 0);

    IODataPtr data(new IOData);
    char *p = (char *)malloc(sizeof(char) * SERIAL_READ_BUFFER_SIZE);
    if (!p) {
        return;
    }

    ssize_t res = 0, len = 0;

    for (;;) {
        res = read(w->fd, p + len, SERIAL_READ_BUFFER_SIZE - len);
        if (res == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            return;
        } else if (res == 0) {
            free(p);
            close(w->fd);
            ev_io_stop(EV_A_ w);
            return;
        } else {
            len += res;
        }
    }
    p[len] = '\0';

    data->fd   = w->fd;
    data->data = p;
    data->len  = len;

    ReadQueue::Push(data);
    // sched::Scheduler::Instance().Sched();

    return;
}

bool Serial::Write()
{
    if (fd_ < 0) {
        LOG(WARNING) << "Can not send data to serial port";
        return false;
    }

    ev_io_init(&write_io_, WriteCB, fd_, EV_WRITE);
    ev_io_start(LibevTool::Instance().GetLoop(), &write_io_);

    return true;
}

void Serial::WriteCB(EV_P_ ev_io *w, int revents)
{
    assert(!WriteQueue::Empty());
    assert(w != NULL);
    assert(w->fd >= 0);

    IODataPtr data = WriteQueue::Front();

    ssize_t res = write(data->fd, data->data, data->len);

    LOG(DEBUG) << "Serial write data of len " << data->len << " to fd "
               << data->fd;

    if (res == -1) {
        return;
    }

    ev_io_stop(EV_A_ w);

    WriteQueue::Pop();

    return;
}
}
