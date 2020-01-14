#ifndef _CLIENTSOCKET_H_
#define _CLIENTSOCKET_H_

#include <cstdlib>
#include <string>
#include <ev.h>
#include <map>
#include <utility>
#include <cstddef>
#include <memory>
#include "libev_tool.h"
#include "noncopyable.h"
#include "schedule.h"
#define BUFFER_SIZE 1032


using std::string;
using std::map;
using std::pair;
using std::shared_ptr;

class ClientSocket;
typedef pair<string, string> sock_type;
typedef shared_ptr<ClientSocket> ClientSocketPtr;

class ClientSocketMap : private utils::NonCopyable{
    typedef map<sock_type, ClientSocketPtr> s_to_c_type;
    typedef map<int, sock_type> f_to_s_type;
    typedef pair<sock_type, ClientSocketPtr> s_to_c_ele;
    typedef pair<int, sock_type> f_to_s_ele;
    typedef s_to_c_type::iterator s_to_c_type_iter;
public:
    ClientSocketMap(){};
    ~ClientSocketMap(){
        socketToClientMap.clear();
        fdToSocketMap.clear();
    };
    ClientSocketPtr Find(sock_type socket){
        s_to_c_type_iter target = socketToClientMap.find(socket);
        if (target != socketToClientMap.end()) {
            return target->second;
        }else{
            return NULL;
        }
    }

    void Insert(sock_type sock, int fd, ClientSocketPtr client){
        LOG(DEBUG) << "INSERT ClientSocketMap";
        socketToClientMap.insert(s_to_c_ele(sock, client));
        fdToSocketMap.insert(f_to_s_ele(fd, sock));
    }

    void Delete(int fd){
        LOG(DEBUG) << "DELETE ClientSocketMap";
        sock_type sock = fdToSocketMap.at(fd);
        fdToSocketMap.erase(fd);
        socketToClientMap.erase(sock);
    }
private:
    s_to_c_type socketToClientMap;
    f_to_s_type fdToSocketMap;

};

class ClientSocket
{
public:
    bool Init(string hostname, string port);
    bool Free();
    void Write();
    int GetFd();
    // bool IsConnected(){ return is_connected_; };
    ClientSocket():
        fd_(-1),
        server_addr_(""),
        server_port_("")
    {}
    static ClientSocketPtr NewClient(string hostname, string port);

private:
    static void ReadCB(EV_P_ ev_io *w, int revents);
    // static void WriteCB(EV_P_ ev_io *w, int revents);

private:
    int fd_;

    string server_addr_;
    string server_port_;

    ev_io read_io_;
    ev_io write_io_;

    // static bool is_connected_;
    static ClientSocketMap clientsocketmap_;
};

#endif  // _SIM_CHANNEL_H_
