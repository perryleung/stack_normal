#ifndef _TRACE_H_
#define _TRACE_H_

#include <fstream>
#include "packet.h"
#include "singleton.h"
#include <string>


using namespace std;
using namespace pkt;

extern string nodeID;
class Trace : public utils::Singleton<Trace>
{
public:
	string Log(int layerID, int protocolID, int packetID, int packetLen, int srcID, int lastID, 
			int nextID, int destID, int helloSeq, int mSrcID, int mDestID, int ackSeq, string type, string status);
	string Log(int layerID, int protocolID, Packet pkt, int srcID, int lastID, 
			int nextID, int destID, int helloSeq, int mSrcID, int mDestID, int ackSeq, string type, string status);
	void Init();
	~Trace();
private:
	ofstream fout;
	bool isTrace;

};
#endif
