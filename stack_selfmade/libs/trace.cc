#include "trace.h"
#include "config.h"
#include "tools.h"
#include "time.h"
#include "app_datapackage.h"
#include <string>
#include <sstream>

using std::string;

void
Trace::Init(){

  uint16_t tracemode = Config::Instance()["trace"]["tracemode"].as<uint16_t>();
  if (tracemode != 0) {
    string tracepath = Config::Instance()["trace"]["tracepath"].as<string>();
    if (mkdirfun(tracepath.c_str()) == 0) {
      string tracefile = Config::Instance()["trace"]["tracefile"].as<string>();
      if (tracepath.rfind('/') != tracepath.size() - 1) {
        tracepath += string("/");
      }

      string traceTime = Config::Instance()["trace"]["tracetime"].as<string>();
      string tracePathTime = tracepath + traceTime + "/trace";
      mkdirfun(tracePathTime.c_str());
      fout.open(tracePathTime + '/' +  tracefile, ios_base::out | ios_base::app);
      time_t t=time(NULL);
      fout<<"========================="<<ctime(&t)<<flush;
      isTrace = true;
      return;
    }
    isTrace = false;
    return;
  }
  isTrace = false;
  return;
}


Trace::~Trace(){
  fout.close();
}
//时间：层：协议：包序号：包大小：源节点(net)：本节点(net)：下一跳(net)：目的节点(net)：hello包序号(net)：源节点(mac)：目的节点(mac)：类型：状态
string
Trace::Log(int layerID, int protocolID, int packetID, int packetLen, int srcID, int lastID, 
			int nextID, int destID, int helloSeq, int mSrcID, int mDestID, int ackSeq, string type, string status)
{
  if (!isTrace) {
    return "";
  }
  // ofstream fout("trace" + nodeID + ".log", ios_base::out | ios_base::app);
  time_t t=time(NULL);
  //out to traceHandler
	stringstream ss;
    ss<<t<<":"<<layerID<<":"<<protocolID<<":"<<packetID<<":"<<packetLen<<":"<<srcID<<":"<<lastID<<":"<<nextID<<":"<<destID<<":"<<helloSeq
		<<":"<<mSrcID<<":"<<mDestID<<":"<<ackSeq<<":"<<type<<":"<<status<<endl;
  
  //out to file
  fout<<t<<":"<<layerID<<":"<<protocolID<<":"<<packetID<<":"<<packetLen<<":"<<srcID<<":"<<lastID<<":"<<nextID<<":"<<destID<<":"<<helloSeq
		<<":"<<mSrcID<<":"<<mDestID<<":"<<ackSeq<<":"<<type<<":"<<status<<endl;
	return ss.str();
}
string 
Trace::Log(int layerID, int protocolID, Packet pkt, int srcID, int lastID, 
			int nextID, int destID, int helloSeq, int mSrcID, int mDestID, int ackSeq, string type, string status)
{
	if (!isTrace) {
	    return "";
	  }
    time_t t=time(NULL);
 
			
	struct TestPackage pktInfo;
	memcpy(&pktInfo, pkt.realRaw(), sizeof(TestPackage));

	stringstream ss;
        uint8_t app_header[9];
        memcpy(&app_header, pkt.realRaw(), 9);
        if(app_header[0] == 100 || app_header[0] == 101)
        {
        uint16_t packetSerial = 0;
        memcpy(&packetSerial , app_header+7, 2);
        char packetSize = 0;
        memcpy(&packetSize , app_header+4, 1);
	    ss<<t<<":"<<layerID<<":"<<protocolID<<":"<<(int)packetSerial<<":"
	    <<(int)pkt.realDataSize()<<":"<<srcID<<":"<<lastID<<":"<<nextID<<":"<<destID<<":"<<helloSeq
		    <<":"<<mSrcID<<":"<<mDestID<<":"<<ackSeq<<":"<<type<<":"<<status<<endl;
  	    
	//out to file
	fout<<t<<":"<<layerID<<":"<<protocolID<<":"<<(int)packetSerial<<":"
	<<(int)pkt.realDataSize()<<":"<<srcID<<":"<<lastID<<":"<<nextID<<":"<<destID<<":"<<helloSeq
		<<":"<<mSrcID<<":"<<mDestID<<":"<<ackSeq<<":"<<type<<":"<<status<<endl;
        }else{
	    ss<<t<<":"<<layerID<<":"<<protocolID<<":"<<(int)pktInfo.SerialNum<<":"
	    <<(int)pktInfo.data_size<<":"<<srcID<<":"<<lastID<<":"<<nextID<<":"<<destID<<":"<<helloSeq
		    <<":"<<mSrcID<<":"<<mDestID<<":"<<ackSeq<<":"<<type<<":"<<status<<endl;
  	    
	//out to file
	fout<<t<<":"<<layerID<<":"<<protocolID<<":"<<(int)pktInfo.SerialNum<<":"
	<<(int)pktInfo.data_size<<":"<<srcID<<":"<<lastID<<":"<<nextID<<":"<<destID<<":"<<helloSeq
		<<":"<<mSrcID<<":"<<mDestID<<":"<<ackSeq<<":"<<type<<":"<<status<<endl;
        }
	//out to traceHandler
	return ss.str();
}
