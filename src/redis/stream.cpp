#include "redis/stream.hpp"

using namespace radapter::redis;

struct Stream::Impl
{

};

Stream::Stream(Settings settings) :
    Client(std::move(settings)),
    d(new Impl{})
{

}

Stream::~Stream()
{

}
