#include "EndpointRegistry.h"

#ifdef ACCLOUD_WITH_QT

namespace accloud::cloud::core {

const EndpointRegistry& EndpointRegistry::instance() {
    static EndpointRegistry registry;
    return registry;
}

std::optional<EndpointDefinition> EndpointRegistry::find(EndpointId id) const {
    switch (id) {
        case EndpointId::AuthCheckSession:
            return EndpointDefinition{EndpointId::AuthCheckSession, HttpMethod::Post,
                                      "/p/p/workbench/api/work/index/getUserStore", true, true,
                                      "application/json", 10000};
        case EndpointId::AuthLoginWithAccessToken:
            return EndpointDefinition{EndpointId::AuthLoginWithAccessToken, HttpMethod::Post,
                                      "/p/p/workbench/api/v3/public/loginWithAccessToken", true, true,
                                      "application/json", 10000};
        case EndpointId::FilesList:
            return EndpointDefinition{EndpointId::FilesList, HttpMethod::Post,
                                      "/p/p/workbench/api/work/index/files", true, true,
                                      "application/json", 10000};
        case EndpointId::FilesListFallback:
            return EndpointDefinition{EndpointId::FilesListFallback, HttpMethod::Post,
                                      "/p/p/workbench/api/work/index/userFiles", true, true,
                                      "application/json", 10000};
        case EndpointId::FilesDelete:
            return EndpointDefinition{EndpointId::FilesDelete, HttpMethod::Post,
                                      "/p/p/workbench/api/work/index/delFiles", true, true,
                                      "application/json", 10000};
        case EndpointId::FilesDownloadUrl:
            return EndpointDefinition{EndpointId::FilesDownloadUrl, HttpMethod::Post,
                                      "/p/p/workbench/api/work/index/getDowdLoadUrl", true, true,
                                      "application/json", 10000};
        case EndpointId::UploadLockStorage:
            return EndpointDefinition{EndpointId::UploadLockStorage, HttpMethod::Post,
                                      "/p/p/workbench/api/v2/cloud_storage/lockStorageSpace", true, true,
                                      "application/json", 15000};
        case EndpointId::UploadRegisterFile:
            return EndpointDefinition{EndpointId::UploadRegisterFile, HttpMethod::Post,
                                      "/p/p/workbench/api/v2/profile/newUploadFile", true, true,
                                      "application/json", 10000};
        case EndpointId::UploadStatus:
            return EndpointDefinition{EndpointId::UploadStatus, HttpMethod::Post,
                                      "/p/p/workbench/api/work/index/getUploadStatus", true, true,
                                      "application/json", 10000};
        case EndpointId::UploadUnlockStorage:
            return EndpointDefinition{EndpointId::UploadUnlockStorage, HttpMethod::Post,
                                      "/p/p/workbench/api/v2/cloud_storage/unlockStorageSpace", true, true,
                                      "application/json", 10000};
        case EndpointId::PrintersList:
            return EndpointDefinition{EndpointId::PrintersList, HttpMethod::Get,
                                      "/p/p/workbench/api/work/printer/getPrinters", true, true,
                                      nullptr, 10000};
        case EndpointId::PrintersStatus:
            return EndpointDefinition{EndpointId::PrintersStatus, HttpMethod::Get,
                                      "/p/p/workbench/api/v2/printer/printersStatus", true, true,
                                      nullptr, 10000};
        case EndpointId::PrintersDetails:
            return EndpointDefinition{EndpointId::PrintersDetails, HttpMethod::Get,
                                      "/p/p/workbench/api/v2/printer/info", true, true,
                                      nullptr, 10000};
        case EndpointId::ProjectsListByPrinter:
            return EndpointDefinition{EndpointId::ProjectsListByPrinter, HttpMethod::Get,
                                      "/p/p/workbench/api/work/project/getProjects", true, true,
                                      nullptr, 10000};
        case EndpointId::ReasonCatalog:
            return EndpointDefinition{EndpointId::ReasonCatalog, HttpMethod::Get,
                                      "/p/p/workbench/api/portal/index/reason", true, true,
                                      nullptr, 10000};
        case EndpointId::OrdersSend:
            return EndpointDefinition{EndpointId::OrdersSend, HttpMethod::Post,
                                      "/p/p/workbench/api/work/operation/sendOrder", true, true,
                                      "application/x-www-form-urlencoded", 10000};
    }
    return std::nullopt;
}

} // namespace accloud::cloud::core

#endif // ACCLOUD_WITH_QT
