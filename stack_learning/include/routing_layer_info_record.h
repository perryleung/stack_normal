#ifndef _ROUTING_LAYER_INFO_RECORD_H_
#define _ROUTING_LAYER_INFO_RECORD_H_

#include <fstream>
#include "singleton.h"
#include <string>
#include "config.h"
#include "tools.h"
#include <ctime>
#include <sstream>

using namespace std;

class RoutingLayerInfoRecord : public utils::Singleton<RoutingLayerInfoRecord> {
    public:
        ~RoutingLayerInfoRecord();
        string log(string info);
        void Init();
    private:
        uint64_t getCurrentTime();
    private:
        ofstream fout;
        bool isTestMode;
};
#endif
