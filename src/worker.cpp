#include "worker.hpp"
#include "pushpop.hpp"

namespace radapter
{

Worker::Worker(Instance *parent, const char *category) : QObject(parent), _category(category)
{
    _Inst = parent;
}

void Worker::Log(LogLevel lvl, fmt::string_view fmt, fmt::format_args args)
{
    _Inst->Log(lvl, _category, fmt, args);
}

void Worker::Shutdown() {
    emit ShutdownDone();
}

Worker::~Worker()
{
}

}
