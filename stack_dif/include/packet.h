#ifndef _PACKET_H_
#define _PACKET_H_

#include <cassert>
#include <cstdlib>
#include <cstring>
#include "singleton.h"

/*
 * most work of a protocol is handling packet
 * which came from upper or lower, receive a packet,
 * modified it and send it up or down.
 * purpose of all class in this file is making packet
 * transform between two layer with zero copying.
 */

/*
 * protocols usually add data to head or tail of the packet,
 * if we do not want to copy data, we must alloc enough memory
 * before the first writing. there are two macros help us to 
 * do that, they *MUST* used in the .cc file of each protocol.
 * the macros define two static variable, which will be instantiated
 * before the main function run, and they add the length of header 
 * and tailer of protocols which we want to know.
 */

#define PROTOCOL_HEADER(type)                                                  \
    static pkt::PacketHeaderHelper<type> type_##PacketHeaderHelper;

#define PROTOCOL_TAILER(type)                                                  \
    static pkt::PacketTailerHelper<type> type_##PacketTailerHelper;

namespace pkt {

/*
 * A instance to store header(tailer) length of all protocols
 */
class PacketHeader : public utils::Singleton<PacketHeader>
{
public:
    PacketHeader() : len_(0) {}
    static void Add(std::size_t len) { PacketHeader::Instance().len_ += len; }
    static std::size_t Size() { return PacketHeader::Instance().len_; }
private:
    std::size_t len_;
};

class PacketTailer : public utils::Singleton<PacketTailer>
{
public:
    PacketTailer() : len_(0) {}
    static void Add(std::size_t len) { PacketTailer::Instance().len_ += len; }
    static std::size_t Size() { return PacketTailer::Instance().len_; }
private:
    std::size_t len_;
};

/*
 * define a variable of this template, add header length to
 * the PacketHeader(PacketTailer)
 */
template <typename T>
class PacketHeaderHelper
{
public:
    PacketHeaderHelper() { PacketHeader::Add(sizeof(T)); }
};

template <typename T>
class PacketTailerHelper
{
public:
    PacketTailerHelper() { PacketTailer::Add(sizeof(T)); }
};

/*
 * ++++++++++++++++++++++++++++++++++++++++++++++++++++
 * |        |                                 |       |
 * start   head                             tail    end
 * |        |                                 |       |
 * ++++++++++++++++++++++++++++++++++++++++++++++++++++
 * this is the memory layout of a packet, we use 4 pointer.
 * start: memory start, end: memory end.
 * in a protocl, packet has three part, header, data and tailer,
 * actually, it can not see the memory between start-head, and tail-end,
 * which is prepared for other protocol.
 * indeed class Packet does not hold the data but just a pointer to it,
 * and a counter of reference of data. in destructor function,
 * Packet check if any other pakcet refer to the data, if not ,delete it.
 * so Packet likes smart pointer with some other variable.
 */

enum Direction {DOWN, UP};

class Packet
{
public:
    // an empty packet
    Packet()
        : start_(nullptr), head_(0), tail_(0), end_(0), ref_(nullptr)
    {
    }

    // packet from up to low
    Packet(std::size_t size) : ref_(new unsigned(1))
    {
        std::size_t head_len = PacketHeader::Size();
        std::size_t tail_len = PacketTailer::Size();

        start_ = new char[size + head_len + tail_len];
        head_  = head_len;
        tail_  = head_ + size;
        end_   = tail_ + tail_len;
        dir_   = DOWN;
    }

    // packet from low to up
    Packet(char* start, std::size_t size)
        : start_(new char[size]),
          head_(0),
          tail_(size),
          end_(size),
          ref_(new unsigned(1)),
          dir_(UP)
    {
        std::memcpy(start_, start, size);
    }

    // construct form other pakcet, add reference
    Packet(const Packet& p)
        : start_(p.start_),
          head_(p.head_),
          tail_(p.tail_),
          end_(p.end_),
          ref_(p.ref_),
          dir_(p.dir_)
    {
        ++(*ref_);
    }

    // copy assignment, 
    // del reference of lhs packet, and add the rhs
    Packet& operator=(Packet& rhs)
    {
        // lhs may be an empty packet, skip
        // otherwise, delete
        if (start_ != nullptr && ref_ != nullptr && --(*ref_) == 0) {
            delete [] start_;
            delete ref_;
        }

        start_ = rhs.start_;
        head_  = rhs.head_;
        tail_  = rhs.tail_;
        end_   = rhs.end_;
        ref_   = rhs.ref_;
        dir_   = rhs.dir_;

        ++(*ref_);

        return *this;
    }

    inline bool operator==(const Packet& rhs)
    {
        return (start_ == rhs.start_ && head_ == rhs.head_ &&
                end_   == rhs.end_   && ref_  == rhs.ref_  &&
                tail_  == rhs.tail_  && dir_  == rhs.dir_);
    }

    ~Packet()
    {
        // skip empty packey
        if (start_ == nullptr && ref_ == nullptr) {
            ;
        } else if (--(*ref_) == 0) {
            // delete if not other reference
            delete [] start_;
            delete ref_;
        }
    }

    /*
     * at any time we only can see the header, data and tailer,
     * while receive a packet from upper, header is empty, 
     * need us to complete it, while from lowr, it has data,
     * we shoule not change it.
     */

    // get size of data
    inline std::size_t Size() const { return tail_ - head_; }
    inline std::size_t realDataSize() const { return tail_ - PacketHeader::Size(); }
    
    // change dir
    inline void ChangeDir() 
    { 
        if(dir_ == DOWN)
        {
            dir_ = UP;
        }else{
            dir_ = DOWN;
        } 
    }

    // for api and driver which create the Packet, not use in protocol
    inline char* Raw() { return start_ + head_; }
    inline char* realRaw() { return start_ + PacketHeader::Size(); }
    inline int Raw_Len() {return tail_ - head_; }

    inline char* MoveP(int d) { return start_ + head_ + d; }

    // get pointer to header, need to declare what kind it is
    template <typename T>
    inline T* Header() const
    {
        // if packet ↓, we don't know where is head
        if (DOWN == dir_) {
            return (T*)(start_ + head_ - sizeof(T));
        }

        return (T*)(start_ + head_);
    }

    // get pointer to data, need to declare the type of header
    template <typename T>
    inline char* Data() const
    {
        // if packet ↑, we don't know where is data
        if (UP == dir_) {
            return (char*)(start_ + head_ + sizeof(T));
        }

        return (char*)(start_ + head_);
    }

    // get pointer to tailer, need to declare what kind it is
    template <typename T>
    inline T* Tailer() const
    {
        // if packet ↑, we don't know where is tail
        if (UP == dir_) {
            return (T*)(start_ + tail_ - sizeof(T));
        }

        return (T*)(start_ + tail_);
    }

    // reseek the pointer of packet, with header but tailer
    template <typename H>
    inline void Move()
    {
        if (UP == dir_) {
            head_ += sizeof(H);
        } else {
            head_ -= sizeof(H);
        }
    }

    // reseek the pointer of packet, with header and tailer
    template <typename H, typename T>
    inline void Move()
    {
        if (UP == dir_) {
            head_ += sizeof(H);
            tail_ -= sizeof(T);
        } else {
            head_ -= sizeof(H);
            tail_ += sizeof(T);
        }
    }

private:
    char* start_;
    std::size_t head_;
    std::size_t tail_;
    std::size_t end_;
    unsigned* ref_;
    enum Direction dir_;
};

}  // end namespace pkt

#endif  // _PACKET_H_
