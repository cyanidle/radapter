#pragma once

#include "client.hpp"

namespace radapter::redis
{



class Stream : public Client
{
    Q_OBJECT
public:
    struct Settings : Client::Settings {
        string streamKey;
    };
    Stream(Settings settings);
    ~Stream() override;
private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

DESCRIBE_INHERIT(
    redis::Stream::Settings, Client::Settings,
    &_::streamKey
    )


}
