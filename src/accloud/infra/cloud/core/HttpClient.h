#pragma once

#ifdef ACCLOUD_WITH_QT

#include "WorkbenchRequestBuilder.h"

#include <string>

namespace accloud::cloud::core {

struct HttpResponse {
    bool ok{false};
    int httpStatus{0};
    std::string body;
    std::string error;
};

class HttpClient {
public:
    HttpResponse execute(const BuiltRequest& request) const;
};

} // namespace accloud::cloud::core

#endif // ACCLOUD_WITH_QT
