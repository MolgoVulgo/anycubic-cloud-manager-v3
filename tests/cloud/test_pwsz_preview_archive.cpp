#include "infra/cloud/archive/PwszPreviewArchive.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void put16(std::ostream& out, std::uint16_t v) {
    const std::array<char, 2> b{static_cast<char>(v & 0xffU), static_cast<char>((v >> 8U) & 0xffU)};
    out.write(b.data(), static_cast<std::streamsize>(b.size()));
}

void put32(std::ostream& out, std::uint32_t v) {
    const std::array<char, 4> b{static_cast<char>(v & 0xffU), static_cast<char>((v >> 8U) & 0xffU),
                                static_cast<char>((v >> 16U) & 0xffU), static_cast<char>((v >> 24U) & 0xffU)};
    out.write(b.data(), static_cast<std::streamsize>(b.size()));
}

std::uint16_t get16(const std::vector<char>& data, std::size_t offset) {
    require(offset + 2 <= data.size(), "lecture ZIP 16 bits hors limites");
    return static_cast<std::uint16_t>(static_cast<unsigned char>(data[offset])) |
           static_cast<std::uint16_t>(static_cast<unsigned char>(data[offset + 1]) << 8U);
}

std::uint32_t get32(const std::vector<char>& data, std::size_t offset) {
    require(offset + 4 <= data.size(), "lecture ZIP 32 bits hors limites");
    return static_cast<std::uint32_t>(static_cast<unsigned char>(data[offset])) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(data[offset + 1])) << 8U) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(data[offset + 2])) << 16U) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(data[offset + 3])) << 24U);
}

std::uint32_t crc32(const std::string& data) {
    std::uint32_t crc = 0xffffffffU;
    for (unsigned char c : data) {
        crc ^= c;
        for (int i = 0; i < 8; ++i) crc = (crc >> 1U) ^ (0xedb88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}

struct Entry {
    std::string name;
    std::string data;
    std::uint32_t offset{};
    std::uint32_t crc{};
};

void writeZip(const std::filesystem::path& path, std::vector<Entry> entries) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    require(out.good(), "impossible de créer l'archive de test");

    for (auto& e : entries) {
        e.offset = static_cast<std::uint32_t>(out.tellp());
        e.crc = crc32(e.data);
        put32(out, 0x04034b50U);
        put16(out, 20);
        put16(out, 0);
        put16(out, 0);
        put16(out, 0);
        put16(out, 0);
        put32(out, e.crc);
        put32(out, static_cast<std::uint32_t>(e.data.size()));
        put32(out, static_cast<std::uint32_t>(e.data.size()));
        put16(out, static_cast<std::uint16_t>(e.name.size()));
        put16(out, 0);
        out.write(e.name.data(), static_cast<std::streamsize>(e.name.size()));
        out.write(e.data.data(), static_cast<std::streamsize>(e.data.size()));
    }

    const auto centralOffset = static_cast<std::uint32_t>(out.tellp());
    for (const auto& e : entries) {
        put32(out, 0x02014b50U);
        put16(out, 20);
        put16(out, 20);
        put16(out, 0);
        put16(out, 0);
        put16(out, 0);
        put16(out, 0);
        put32(out, e.crc);
        put32(out, static_cast<std::uint32_t>(e.data.size()));
        put32(out, static_cast<std::uint32_t>(e.data.size()));
        put16(out, static_cast<std::uint16_t>(e.name.size()));
        put16(out, 0);
        put16(out, 0);
        put16(out, 0);
        put16(out, 0);
        put32(out, 0);
        put32(out, e.offset);
        out.write(e.name.data(), static_cast<std::streamsize>(e.name.size()));
    }

    const auto centralSize = static_cast<std::uint32_t>(static_cast<std::uint32_t>(out.tellp()) - centralOffset);
    put32(out, 0x06054b50U);
    put16(out, 0);
    put16(out, 0);
    put16(out, static_cast<std::uint16_t>(entries.size()));
    put16(out, static_cast<std::uint16_t>(entries.size()));
    put32(out, centralSize);
    put32(out, centralOffset);
    put16(out, 0);
    require(out.good(), "écriture de l'archive de test incomplète");
}

std::vector<char> bytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    require(in.good(), "impossible de lire le fichier de test");
    return {std::istreambuf_iterator<char>(in), {}};
}

std::vector<char> storedEntryBytes(const std::filesystem::path& path, const std::string& targetName) {
    const auto data = bytes(path);
    std::size_t offset = 0;
    while (offset + 4 <= data.size()) {
        const auto signature = get32(data, offset);
        if (signature == 0x02014b50U || signature == 0x06054b50U) break;
        require(signature == 0x04034b50U, "signature d'entrée locale ZIP inattendue");

        const auto flags = get16(data, offset + 6);
        const auto method = get16(data, offset + 8);
        const auto compressedSize = get32(data, offset + 18);
        const auto nameLength = get16(data, offset + 26);
        const auto extraLength = get16(data, offset + 28);
        require((flags & 0x0008U) == 0U, "descripteur de données inattendu dans l'archive synthétique");
        require(method == 0U, "compression inattendue dans l'archive synthétique");

        const auto nameOffset = offset + 30;
        const auto payloadOffset = nameOffset + nameLength + extraLength;
        require(payloadOffset + compressedSize <= data.size(), "entrée ZIP tronquée");
        const std::string name(data.data() + nameOffset, nameLength);
        if (name == targetName) {
            return {data.begin() + static_cast<std::ptrdiff_t>(payloadOffset),
                    data.begin() + static_cast<std::ptrdiff_t>(payloadOffset + compressedSize)};
        }
        offset = payloadOffset + compressedSize;
    }
    throw std::runtime_error("entrée ZIP absente: " + targetName);
}

} // namespace

int main() {
    try {
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto dir = std::filesystem::temp_directory_path() /
                         ("accloud-pwsz-preview-test-" + std::to_string(nonce));
        std::filesystem::create_directories(dir);

        const auto original = dir / "lychee.pwsz";
        writeZip(original, {{"preview_images/preview_1.png", "PNG-UNCHANGED"}, {"data.bin", "abc"}});
        const auto originalBytes = bytes(original);

        auto inspection = accloud::cloud::archive::inspectPwszPreviewArchive(original);
        require(inspection.ok && inspection.hasPreview1 && !inspection.hasPreview2 && inspection.needsCompletion,
                "inspection initiale incorrecte");

        auto prepared = accloud::cloud::archive::preparePwszPreview2Copy(original);
        require(prepared.ok && prepared.changed && prepared.preparedPath != original,
                "préparation de preview_2 échouée");
        require(bytes(original) == originalBytes, "le fichier original a été modifié avant l'upload");
        inspection = accloud::cloud::archive::inspectPwszPreviewArchive(prepared.preparedPath);
        require(inspection.ok && inspection.hasPreview1 && inspection.hasPreview2 && !inspection.needsCompletion,
                "archive préparée invalide");
        require(storedEntryBytes(prepared.preparedPath, "preview_images/preview_1.png") ==
                    storedEntryBytes(prepared.preparedPath, "preview_images/preview_2.png"),
                "preview_2 ne reproduit pas exactement preview_1");

        auto commit = accloud::cloud::archive::replaceOriginalWithPrepared(original, prepared.preparedPath);
        require(commit.ok && !std::filesystem::exists(prepared.preparedPath),
                "remplacement atomique du fichier local échoué");
        inspection = accloud::cloud::archive::inspectPwszPreviewArchive(original);
        require(inspection.ok && inspection.hasPreview2, "preview_2 absente après remplacement local");

        prepared = accloud::cloud::archive::preparePwszPreview2Copy(original);
        require(prepared.ok && !prepared.changed && prepared.preparedPath == original,
                "un PWSZ déjà complet ne doit pas être réécrit");

        const auto missing = dir / "missing.pwsz";
        writeZip(missing, {{"data.bin", "abc"}});
        prepared = accloud::cloud::archive::preparePwszPreview2Copy(missing);
        require(!prepared.ok && !prepared.changed, "l'absence de preview_1 doit être signalée");

        std::filesystem::remove_all(dir);
        std::cout << "PWSZ preview archive tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "PWSZ preview archive test failed: " << error.what() << '\n';
        return 1;
    }
}
