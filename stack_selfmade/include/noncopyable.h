#ifndef _NON_COPYABLE_H_
#define _NON_COPYABLE_H_

namespace utils{

class NonCopyable
{
protected:
    NonCopyable() {}
    ~NonCopyable() {}
private:
    NonCopyable(const NonCopyable &);
    NonCopyable &operator=(const NonCopyable &);
};

}

#endif