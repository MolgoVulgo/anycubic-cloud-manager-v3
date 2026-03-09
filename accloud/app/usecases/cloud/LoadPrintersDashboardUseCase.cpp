#include "LoadPrintersDashboardUseCase.h"

#include "infra/cloud/api/PrintersApi.h"
#include "infra/cloud/api/ProjectsApi.h"
#include "infra/cloud/core/SessionProvider.h"

#include <array>
#include <chrono>
#include <thread>

namespace accloud::usecases::cloud {
namespace {

void enrichPrinterFromDetails(accloud::cloud::CloudPrinterInfo& printer,
                              const accloud::cloud::CloudPrinterDetailsResult& details) {
    if (!details.ok) {
        return;
    }
    if (printer.currentFile.empty() && !details.currentFile.empty()) {
        printer.currentFile = details.currentFile;
    }
    if (printer.progress < 0 && details.progress >= 0) {
        printer.progress = details.progress;
    }
    if (printer.elapsedSec < 0 && details.elapsedSec >= 0) {
        printer.elapsedSec = details.elapsedSec;
    }
    if (printer.remainingSec < 0 && details.remainingSec >= 0) {
        printer.remainingSec = details.remainingSec;
    }
    if (printer.currentLayer < 0 && details.currentLayer >= 0) {
        printer.currentLayer = details.currentLayer;
    }
    if (printer.totalLayers < 0 && details.totalLayers >= 0) {
        printer.totalLayers = details.totalLayers;
    }
}

void enrichPrinterFromProjects(accloud::cloud::CloudPrinterInfo& printer,
                               const accloud::cloud::CloudPrinterProjectsResult& projects) {
    if (!projects.ok || projects.items.empty()) {
        return;
    }
    const auto& active = projects.items.front();
    if (printer.currentFile.empty()) {
        if (!active.currentFile.empty()) {
            printer.currentFile = active.currentFile;
        } else if (!active.gcodeName.empty()) {
            printer.currentFile = active.gcodeName;
        }
    }
    if (printer.progress < 0 && active.progress >= 0) {
        printer.progress = active.progress;
    }
    if (printer.elapsedSec < 0 && active.elapsedSec >= 0) {
        printer.elapsedSec = active.elapsedSec;
    }
    if (printer.remainingSec < 0 && active.remainingSec >= 0) {
        printer.remainingSec = active.remainingSec;
    }
    if (printer.currentLayer < 0 && active.currentLayer >= 0) {
        printer.currentLayer = active.currentLayer;
    }
    if (printer.totalLayers < 0 && active.totalLayers >= 0) {
        printer.totalLayers = active.totalLayers;
    }
}

} // namespace

LoadPrintersDashboardResult LoadPrintersDashboardUseCase::execute() const {
    const accloud::cloud::core::SessionProvider sessionProvider;
    const auto contextResult = sessionProvider.loadRequestContext();
    if (!contextResult.ok) {
        return {false, "Session invalide ou introuvable", {}, {}};
    }

    static constexpr std::array<std::chrono::milliseconds, 3> kDelays = {
        std::chrono::milliseconds(0),
        std::chrono::milliseconds(350),
        std::chrono::milliseconds(1200),
    };

    const accloud::cloud::api::PrintersApi printersApi;
    accloud::cloud::CloudPrintersResult listResult;
    for (const auto& delay : kDelays) {
        if (delay.count() > 0) {
            std::this_thread::sleep_for(delay);
        }
        listResult = printersApi.list(contextResult.context.accessToken,
                                      contextResult.context.xxToken);
        if (listResult.ok) {
            break;
        }
    }
    if (!listResult.ok) {
        return {false, listResult.message, listResult.rawJson, {}};
    }

    const accloud::cloud::api::ProjectsApi projectsApi;
    for (auto& printer : listResult.printers) {
        const auto details = printersApi.details(contextResult.context.accessToken,
                                                 contextResult.context.xxToken,
                                                 printer.id);
        enrichPrinterFromDetails(printer, details);

        const auto projects = projectsApi.listByPrinter(contextResult.context.accessToken,
                                                        contextResult.context.xxToken,
                                                        printer.id, 1, 20);
        enrichPrinterFromProjects(printer, projects);
    }

    return {true, listResult.message, listResult.rawJson, std::move(listResult.printers)};
}

} // namespace accloud::usecases::cloud
