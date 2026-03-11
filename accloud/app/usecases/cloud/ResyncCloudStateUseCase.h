#pragma once

#include <string>

namespace accloud::usecases::cloud {

struct ResyncCloudStateResult {
    bool ok{false};
    std::string message;
    bool filesOk{false};
    bool quotaOk{false};
    bool printersOk{false};
};

class ResyncCloudStateUseCase {
public:
    ResyncCloudStateResult execute() const;
};

} // namespace accloud::usecases::cloud

