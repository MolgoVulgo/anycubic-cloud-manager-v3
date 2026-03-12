#pragma once

#ifdef ACCLOUD_WITH_QT

#include <optional>

namespace accloud::cloud::core {

enum class HttpMethod {
    Get,
    Post,
};

enum class EndpointId {
    AuthCheckSession,
    AuthLoginWithAccessToken,
    FilesList,
    FilesListFallback,
    FilesDelete,
    FilesDownloadUrl,
    PrintersList,
    PrintersStatus,
    PrintersDetails,
    ProjectsListByPrinter,
    ReasonCatalog,
    OrdersSend,
};

struct EndpointDefinition {
    EndpointId endpointId{};
    HttpMethod method{HttpMethod::Get};
    const char* path{nullptr};
    bool requiresBearer{true};
    bool requiresWorkbenchSignature{true};
    const char* contentType{nullptr};
    int timeoutMs{10000};
};

class EndpointRegistry {
public:
    static const EndpointRegistry& instance();
    std::optional<EndpointDefinition> find(EndpointId id) const;
};

} // namespace accloud::cloud::core

#endif // ACCLOUD_WITH_QT
