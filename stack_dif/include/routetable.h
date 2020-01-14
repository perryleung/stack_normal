#ifndef ROUTETABLE_H
#define ROUTETABLE_H

#include <vector>
#include "schedule.h"
using std::vector;

struct Entry
{
    uint8_t destNode;
    uint8_t nextNode;
    uint8_t metric;
    uint8_t seqNum;
};
class RouteTable
{
    private:
        uint8_t nodeID;
        vector<Entry> entrys;
    public:
        RouteTable();
        void handleRouteTable(RouteTable &recv , RouteTable &incre);
        uint8_t findNextNode(uint8_t dest);
        vector<uint8_t> findNeighNode();
        vector<Entry> & getEntry(){return entrys;}
        uint8_t & getNodeID(){return nodeID;}
        void setNodeID(uint8_t nodeID){ this->nodeID=nodeID;}
        void clearEntry();
        bool updateEntry(vector<Entry>::iterator p, uint8_t n);
        void addEntry(Entry &e);
        void print();
        void toArray(char pArray[2][20]);
        uint16_t getSize(){return entrys.size();};
};

#endif
