#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <string>
#include <yaml-cpp/yaml.h>
#include "singleton.h"

class Config : public utils::Singleton<Config>
{
public:
    static inline void Load(const std::string &s)
    {
        Config::Instance().config_ = YAML::LoadFile(s);
    }

    inline YAML::Node operator[] (const std::string &s) const
    {
        return config_[s];
    }

private:
    YAML::Node config_;
};

#endif // _CONFIG_H_
