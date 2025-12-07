#ifndef _WIN32
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#else
#pragma warning(push)
#pragma warning(disable:4200)
#endif
#include "async.h"
#ifndef _WIN32
#pragma GCC diagnostic pop
#else
#pragma warning(pop)
#endif