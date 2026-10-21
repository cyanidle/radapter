#ifndef RADAPTER_LAZY_HPP
#define RADAPTER_LAZY_HPP

#include "radapter/common.hpp"
#include <QVariant>

namespace radapter {

/// A message tree kept as QVariant until Lua actually reads it. `glua::Push`
/// turns it into a lazy view userdata instead of materializing tables; `toQVar`
/// returns the retained tree unchanged, so forwarding a message is free.
struct RADAPTER_API LazyNode {
    QVariant value;
};

}

Q_DECLARE_METATYPE(radapter::LazyNode)

#endif // RADAPTER_LAZY_HPP
