#ifndef _SERIAL_H_
#define _SERIAL_H_

extern "C" {

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
// #include "event.h"
#include <ev.h>
}

#include <string>
#include "libev_tool.h"

using std::string;

#define SERIAL_READ_BUFFER_SIZE 2048

namespace serial {

class Serial
{
public:
    bool Init(string port, string baudrate);
    bool Free();
    bool Write();

    int GetFd() { return fd_; }
private:
    bool Open();
    bool Flush();

    static void ReadCB(EV_P_ ev_io *w, int revents);
    static void WriteCB(EV_P_ ev_io *w, int revents);

private:
    int fd_;

    string port_;
    string baudrate_;

    ev_io read_io_;
    ev_io write_io_;
};
}

#endif  // _SERIAL_H_
