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
        // 虽然返回的是值而非指针，但其继承了noncopyable类可以有效防止拷贝，其构造函数应该不能被设计为private,让T的父类Singleton可以构造对象
        // 父类Singleton需要访问子类的构造函数，而子类的构造函数必须是public，连protect都不行，那不就向外部泄露构造函数了？！
        static T instance_;
        return instance_;
    }
};

}  // end namespace utils

#endif  // _SINGLETON_H_
