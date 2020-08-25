#ifndef _SINGLETON_H_
#define _SINGLETON_H_

#include "noncopyable.h"

namespace utils {

template <typename T>
class Singleton : private NonCopyable
{
public:
    static inline T& Instance()
    {
        static T instance_;
        return instance_;
    }
};

}