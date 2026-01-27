#pragma once

#include <builtin.hpp>

namespace radapter::can
{

struct CanFrame {
    QVariant frame_id;
    QVariant payload;
    WithDefault<bool> extended_id = false;
    WithDefault<bool> can_fd = false;
};

RAD_DESCRIBE(CanFrame) {
    RAD_MEMBER(frame_id);
    RAD_MEMBER(payload);
    RAD_MEMBER(extended_id);
    RAD_MEMBER(can_fd);
}

}