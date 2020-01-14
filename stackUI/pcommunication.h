#ifndef PCOMMUNICATION_H
#define PCOMMUNICATION_H

#include "usermainwindow.h"
#include <vector>

class PCommunication
{
private:
    static PCommunication* m_instance;
    PCommunication();
public:
    static PCommunication* getInstance()
    {
        return m_instance;
    }

private:
    bool IsListen;

public:
    UserMainWindow* p_UserMainWindow;
    std::vector<UserMainWindow *> user_list;

public:
    bool start_listen();
    bool stop_listen();
    bool package_write(char* p_buffer, int datalength, int Package_type, int DestinationID, int SourceID);
    void Build_UserMainWindow();
//    void handleRoutTable(struct App_DataPackage table);

};

void* package_read(void* args);
void* package_handler(void* args);

#endif // PCOMMUNICATION_H
