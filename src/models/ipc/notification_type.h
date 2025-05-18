#pragma once

#include <nlohmann/json.hpp>

namespace lansend {

enum class NotificationType {
    kInfo,
    kError,
    kFindDevice,
    kLostDevice,
    kSettings,
    kConnectToDevice,
    kAcceptSend,
    kDeclineSend,
    kSendProgress,
    kSendVerified,
    kSendCompleted,
    kRequestReceive,
    kReceiveProgress,
    kVerifyReceive,
    kCompleteReceive,
    kReceiverCanceled,
    kSenderCanceled,
};

NLOHMANN_JSON_SERIALIZE_ENUM(NotificationType,
                             {
                                 {NotificationType::kInfo, "Info"},
                                 {NotificationType::kError, "Error"},
                                 {NotificationType::kFindDevice, "FindDevice"},
                                 {NotificationType::kLostDevice, "LostDevice"},
                                 {NotificationType::kSettings, "Settings"},
                                 {NotificationType::kConnectToDevice, "ConnectToDevice"},
                                 {NotificationType::kAcceptSend, "AcceptSend"},
                                 {NotificationType::kDeclineSend, "DeclineSend"},
                                 {NotificationType::kSendProgress, "SendProgress"},
                                 {NotificationType::kSendVerified, "SendVerified"},
                                 {NotificationType::kSendCompleted, "SendCompleted"},
                                 {NotificationType::kRequestReceive, "RequestReceive"},
                                 {NotificationType::kReceiveProgress, "ReceiveProgress"},
                                 {NotificationType::kVerifyReceive, "VerifyReceive"},
                                 {NotificationType::kCompleteReceive, "CompleteReceive"},
                                 {NotificationType::kReceiverCanceled, "ReceiverCanceled"},
                                 {NotificationType::kSenderCanceled, "SenderCanceled"},
                             });

} // namespace lansend