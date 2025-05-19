#pragma once

#include "notification_type.h"
#include <nlohmann/json.hpp>

namespace lansend {

struct Notification {
    NotificationType type;
    nlohmann::json data;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(Notification, type, data);
};

} // namespace lansend