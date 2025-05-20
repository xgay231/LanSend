#pragma once

#include "model/transfer_progress.h"
#include <cstdint>
#include <string>

// struct TransferProgress {
//     uint64_t transfer_id;
//     uint64_t bytes_transferred;
//     uint64_t total_bytes;
//     double speed; // bytes per second
// };

class ProgressDisplay {
public:
    ProgressDisplay();
    ~ProgressDisplay();

    void UpdateProgress(const lansend::models::TransferProgress& progress);
    void ClearProgress();

private:
    void printProgress(const lansend::models::TransferProgress& progress);
};