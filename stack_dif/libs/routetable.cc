#include "routetable.h"

bool
RouteTable::updateEntry(vector<Entry>::iterator p, uint8_t n)
{
    bool find=false;
    for(vector<Entry>::iterator iter=entrys.begin(); iter!=entrys.end(); iter++)
    {

        if(iter->destNode==p->destNode)
        {
            if(iter->seqNum<p->seqNum)
            {
                iter->nextNode=n;
                iter->metric=p->metric+1;
                iter->seqNum=p->seqNum;
                return true;
            }else if(iter->seqNum==p->seqNum)
            {
                if(iter->metric>p->metric+1)
                {
                    iter->nextNode=n;
                    iter->metric=p->metric+1;
                    return true;
                }
            }
            find=true;
            break;
        }
    }
    if(!find)
    {
        entrys.push_back(*p);
        entrys[entrys.size()-1].nextNode=n;
        entrys[entrys.size()-1].metric++;
        return true;
    }
}
void
RouteTable::addEntry(Entry &e)
{
    entrys.push_back(e);
}
void
RouteTable::clearEntry()
{
    entrys.clear();
}

RouteTable::RouteTable()
{
    LOG(INFO)<<"Dynamic route table is constructed"<<nodeID<<endl;
    LOG(INFO)<<"The number of route table entry is "<<entrys.size()<<endl;
}

void
RouteTable::handleRouteTable(RouteTable &recv, RouteTable &incre)
{
    for(vector<Entry>::iterator iter=recv.getEntry().begin(); iter!=recv.getEntry().end(); iter++)
    {
        if(this->updateEntry(iter, recv.getNodeID()))
        {
            incre.updateEntry(iter, recv.getNodeID());
        }
    }
}

uint8_t
RouteTable::findNextNode(uint8_t dest)
{
    for(vector<Entry>::iterator iter=entrys.begin(); iter!=entrys.end(); iter++)
    {
        if(iter->destNode==dest)
        {
            return iter->nextNode;
        }
    }
    return 0;
}
vector<uint8_t>
RouteTable::findNeighNode()
{
    vector<uint8_t> neigh;
    for(vector<Entry>::iterator iter=entrys.begin(); iter!=entrys.end(); iter++)
    {
        neigh.push_back(iter->destNode);
    }
    return neigh;
}
/*
struct Entry
{
    uint8_t destNode;
    uint8_t nextNode;
    uint8_t metric;
    uint8_t seqNum;
};
*/
void
RouteTable::print()
{
    cout<<"nodeID: "<<int(nodeID)<<endl;
    int i=1;
    for(vector<Entry>::iterator iter=entrys.begin(); iter!=entrys.end(); iter++)
    {
        cout<<i<<". destNode: "<<int(iter->destNode)<<", nextNode: "<<int(iter->nextNode)
        <<", metric: "<<int(iter->metric)<<", seqNum: "<<int(iter->seqNum)<<endl;
        i++;
    }
}
void
RouteTable::toArray(char pArray[2][20])
{
    int i=0;
    //char** pArray = *ppArray;
    for(vector<Entry>::iterator iter=entrys.begin(); iter!=entrys.end(); iter++)
    {
        if(iter->metric>=2)
        {
            *(pArray[0]+i)=char(iter->destNode);
            *(pArray[1]+i)=char(0);
            i++;
        }else if(iter->metric==1)
        {
            *(pArray[0]+i)=char(iter->destNode);
            *(pArray[1]+i)=char(1);
            i++;
        }
    }
    *(pArray[0]+i)=char(127);
    *(pArray[1]+i)=char(127);
}
