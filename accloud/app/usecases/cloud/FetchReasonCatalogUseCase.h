#pragma once

#include "infra/cloud/CloudClient.h"

namespace accloud::usecases::cloud {

class FetchReasonCatalogUseCase {
public:
    accloud::cloud::CloudReasonCatalogResult execute() const;
};

} // namespace accloud::usecases::cloud
