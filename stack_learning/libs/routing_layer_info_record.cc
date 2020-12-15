#include "routing_layer_info_record.h"
#include "config.h"
#include "tools.h"
#include <ctime>
#include <sys/time.h>
#include <sstream>

using namespace std;

RoutingLayerInfoRecord::~RoutingLayerInfoRecord() {
    fout.close();
}

uint64_t RoutingLayerInfoRecord::getCurrentTime() {
    struct timeval tv;    
    gettimeofday(&tv,NULL);    //该函数在sys/time.h头文件中
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void RoutingLayerInfoRecord::Init() {
    uint16_t isOpen = Config::Instance()["routingLayerTest"]["isOpen"].as<uint16_t>();
    if (isOpen != 0) {
        string recordPath = Config::Instance()["routingLayerTest"]["recordPath"].as<string>();
        if (mkdirfun(recordPath.c_str()) == 0) {
            string protocolName = Config::Instance()["routingLayerTest"]["protocolName"].as<string>();
            if (recordPath.rfind('/') != recordPath.size() - 1) {
                recordPath += string("/");
            }            
            fout.open(recordPath + protocolName, ios_base::out | ios_base::app);
	    time_t t=time(NULL);
	    fout<<"========================="<<ctime(&t)<<flush;
            isTestMode = true;
            return;
        }
    }
    isTestMode = false;
    return;
}
string RoutingLayerInfoRecord::log(string info) {
    if (isTestMode) {
        stringstream recordInfo;
        recordInfo << getCurrentTime() << ":" << info << endl;
        fout << recordInfo.str() << flush;
        return recordInfo.str();
    }
    return "";
}
