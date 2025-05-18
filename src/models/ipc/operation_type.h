#pragma once

#include <nlohmann/json.hpp>

namespace lansend {

enum class OperationType {
    kSendFile,
    kCancelWaitForConfirmation,
    kCancelSend,
    kConfirmReceive,
    kCancelReceive,
    kModifySettings,
    kConnectToDevice,
    kExitApp,
};

NLOHMANN_JSON_SERIALIZE_ENUM(OperationType,
                             {
                                 {OperationType::kSendFile, "SendFile"},
                                 {OperationType::kCancelWaitForConfirmation,
                                  "CancelWaitForConfirmation"},
                                 {OperationType::kCancelSend, "CancelSend"},
                                 {OperationType::kConfirmReceive, "ConfirmReceive"},
                                 {OperationType::kCancelReceive, "CancelReceive"},
                                 {OperationType::kModifySettings, "ModifySettings"},
                                 {OperationType::kConnectToDevice, "ConnectToDevice"},
                                 {OperationType::kExitApp, "ExitApp"},
                             });

} // namespace lansend