#ifndef _SINGLETON_H_
#define _SINGLETON_H_

#include "noncopyable.h"

namespace utils {

template <typename T>
class Singleton : private NonCopyable // 与《Effective C++》item 06 一样
{
public:
    static inline T& Instance() //《effective C++》 item 04
    {
        // 虽然返回的是值而非指针，但其继承了noncopyable类可以有效防止拷贝，其构造函数应该不能被设计为private,让T的父类Singleton可以构造对象
        // 父类Singleton需要访问子类的构造函数，而子类的构造函数必须是public，连protect都不行，那不就向外部泄露构造函数了？！
        //这里的父类子类备注写反了

        // 返回的是类型的引用，其拷贝构造函数和拷贝赋值运算符是private阻止了对象复制，而其默认构造函数是protected，可以被子类访问。另外是private继承，外部代码无法访问父类的成员。
        static T instance_;
        return instance_;
    }
};

}  // end namespace utils

#endif  // _SINGLETON_H_
