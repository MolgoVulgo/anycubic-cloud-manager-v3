#include "PwszPreviewArchive.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <string_view>
#include <system_error>
#include <vector>

namespace accloud::cloud::archive {
namespace {

constexpr std::uint32_t kLocalHeaderSignature = 0x04034b50U;
constexpr std::uint32_t kCentralHeaderSignature = 0x02014b50U;
constexpr std::uint32_t kEndOfCentralDirectorySignature = 0x06054b50U;
constexpr std::size_t kLocalHeaderSize = 30;
constexpr std::size_t kCentralHeaderSize = 46;
constexpr std::size_t kEndOfCentralDirectorySize = 22;
constexpr std::size_t kMaxZipCommentSize = 65535;
constexpr std::string_view kPreview1Name = "preview_images/preview_1.png";
constexpr std::string_view kPreview2Name = "preview_images/preview_2.png";

std::uint16_t readLe16(const std::uint8_t* bytes) {
    return static_cast<std::uint16_t>(bytes[0])
           | (static_cast<std::uint16_t>(bytes[1]) << 8U);
}

std::uint32_t readLe32(const std::uint8_t* bytes) {
    return static_cast<std::uint32_t>(bytes[0])
           | (static_cast<std::uint32_t>(bytes[1]) << 8U)
           | (static_cast<std::uint32_t>(bytes[2]) << 16U)
           | (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

void writeLe16(std::uint8_t* bytes, std::uint16_t value) {
    bytes[0] = static_cast<std::uint8_t>(value & 0xffU);
    bytes[1] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
}

void writeLe32(std::uint8_t* bytes, std::uint32_t value) {
    bytes[0] = static_cast<std::uint8_t>(value & 0xffU);
    bytes[1] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
    bytes[2] = static_cast<std::uint8_t>((value >> 16U) & 0xffU);
    bytes[3] = static_cast<std::uint8_t>((value >> 24U) & 0xffU);
}

struct CentralEntry {
    std::array<std::uint8_t, kCentralHeaderSize> fixed{};
    std::string name;
    std::vector<std::uint8_t> extra;
    std::vector<std::uint8_t> comment;
    std::uint16_t flags{0};
    std::uint32_t crc32{0};
    std::uint32_t compressedSize{0};
    std::uint32_t uncompressedSize{0};
    std::uint32_t localHeaderOffset{0};
};

struct ZipDirectory {
    std::uint16_t entryCount{0};
    std::uint32_t centralDirectorySize{0};
    std::uint32_t centralDirectoryOffset{0};
    std::vector<std::uint8_t> centralDirectoryBytes;
    std::vector<std::uint8_t> archiveComment;
    std::vector<CentralEntry> entries;
};

struct ParseResult {
    bool ok{false};
    ZipDirectory directory;
    std::string error;
};

bool readExact(std::istream& input, void* destination, std::size_t size) {
    if (size == 0) {
        return true;
    }
    input.read(static_cast<char*>(destination), static_cast<std::streamsize>(size));
    return input.good() || input.gcount() == static_cast<std::streamsize>(size);
}

bool writeExact(std::ostream& output, const void* source, std::size_t size) {
    if (size == 0) {
        return true;
    }
    output.write(static_cast<const char*>(source), static_cast<std::streamsize>(size));
    return output.good();
}

bool copyRange(std::ifstream& input,
               std::ofstream& output,
               std::uint64_t offset,
               std::uint64_t size) {
    input.clear();
    input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!input.good()) {
        return false;
    }

    std::array<char, 1024 * 1024> buffer{};
    std::uint64_t remaining = size;
    while (remaining > 0) {
        const std::size_t chunk = static_cast<std::size_t>(
            std::min<std::uint64_t>(remaining, buffer.size()));
        input.read(buffer.data(), static_cast<std::streamsize>(chunk));
        if (input.gcount() != static_cast<std::streamsize>(chunk)) {
            return false;
        }
        output.write(buffer.data(), static_cast<std::streamsize>(chunk));
        if (!output.good()) {
            return false;
        }
        remaining -= chunk;
    }
    return true;
}

ParseResult parseZipDirectory(const std::filesystem::path& archivePath) {
    ParseResult result;
    std::error_code ec;
    const std::uintmax_t fileSize = std::filesystem::file_size(archivePath, ec);
    if (ec || fileSize < kEndOfCentralDirectorySize) {
        result.error = "Archive ZIP introuvable ou trop courte.";
        return result;
    }
    if (fileSize > static_cast<std::uintmax_t>(std::numeric_limits<std::uint32_t>::max())) {
        result.error = "Archive ZIP64 non prise en charge pour la préparation PWSZ.";
        return result;
    }

    std::ifstream input(archivePath, std::ios::binary);
    if (!input) {
        result.error = "Impossible d'ouvrir l'archive PWSZ.";
        return result;
    }

    const std::uintmax_t tailSize = std::min<std::uintmax_t>(
        fileSize, kEndOfCentralDirectorySize + kMaxZipCommentSize);
    const std::uintmax_t tailOffset = fileSize - tailSize;
    std::vector<std::uint8_t> tail(static_cast<std::size_t>(tailSize));
    input.seekg(static_cast<std::streamoff>(tailOffset), std::ios::beg);
    if (!readExact(input, tail.data(), tail.size())) {
        result.error = "Lecture de fin d'archive ZIP impossible.";
        return result;
    }

    std::size_t eocdIndex = std::numeric_limits<std::size_t>::max();
    for (std::size_t index = tail.size() - kEndOfCentralDirectorySize + 1; index-- > 0;) {
        if (readLe32(tail.data() + index) != kEndOfCentralDirectorySignature) {
            continue;
        }
        const std::uint16_t commentLength = readLe16(tail.data() + index + 20);
        if (index + kEndOfCentralDirectorySize + commentLength == tail.size()) {
            eocdIndex = index;
            break;
        }
    }
    if (eocdIndex == std::numeric_limits<std::size_t>::max()) {
        result.error = "Fin de répertoire ZIP introuvable.";
        return result;
    }

    const std::uint8_t* eocd = tail.data() + eocdIndex;
    const std::uint16_t diskNumber = readLe16(eocd + 4);
    const std::uint16_t centralDisk = readLe16(eocd + 6);
    const std::uint16_t entriesOnDisk = readLe16(eocd + 8);
    const std::uint16_t totalEntries = readLe16(eocd + 10);
    const std::uint32_t centralSize = readLe32(eocd + 12);
    const std::uint32_t centralOffset = readLe32(eocd + 16);
    const std::uint16_t commentLength = readLe16(eocd + 20);

    if (diskNumber != 0 || centralDisk != 0 || entriesOnDisk != totalEntries) {
        result.error = "Archives ZIP multi-disques non prises en charge.";
        return result;
    }
    if (totalEntries == std::numeric_limits<std::uint16_t>::max()
        || centralSize == std::numeric_limits<std::uint32_t>::max()
        || centralOffset == std::numeric_limits<std::uint32_t>::max()) {
        result.error = "Archive ZIP64 non prise en charge pour la préparation PWSZ.";
        return result;
    }
    if (static_cast<std::uint64_t>(centralOffset) + centralSize
        > tailOffset + eocdIndex) {
        result.error = "Répertoire central ZIP incohérent.";
        return result;
    }

    ZipDirectory directory;
    directory.entryCount = totalEntries;
    directory.centralDirectorySize = centralSize;
    directory.centralDirectoryOffset = centralOffset;
    directory.archiveComment.assign(eocd + kEndOfCentralDirectorySize,
                                    eocd + kEndOfCentralDirectorySize + commentLength);
    directory.centralDirectoryBytes.resize(centralSize);
    input.clear();
    input.seekg(static_cast<std::streamoff>(centralOffset), std::ios::beg);
    if (!readExact(input,
                   directory.centralDirectoryBytes.data(),
                   directory.centralDirectoryBytes.size())) {
        result.error = "Lecture du répertoire central ZIP impossible.";
        return result;
    }

    std::size_t cursor = 0;
    directory.entries.reserve(totalEntries);
    for (std::uint16_t index = 0; index < totalEntries; ++index) {
        if (cursor + kCentralHeaderSize > directory.centralDirectoryBytes.size()
            || readLe32(directory.centralDirectoryBytes.data() + cursor)
                   != kCentralHeaderSignature) {
            result.error = "Entrée de répertoire central ZIP invalide.";
            return result;
        }

        const std::uint8_t* fixed = directory.centralDirectoryBytes.data() + cursor;
        const std::uint16_t fileNameLength = readLe16(fixed + 28);
        const std::uint16_t extraLength = readLe16(fixed + 30);
        const std::uint16_t commentEntryLength = readLe16(fixed + 32);
        const std::size_t entrySize = kCentralHeaderSize
                                      + fileNameLength
                                      + extraLength
                                      + commentEntryLength;
        if (cursor + entrySize > directory.centralDirectoryBytes.size()) {
            result.error = "Entrée ZIP tronquée dans le répertoire central.";
            return result;
        }

        CentralEntry entry;
        std::copy_n(fixed, kCentralHeaderSize, entry.fixed.begin());
        entry.flags = readLe16(fixed + 8);
        entry.crc32 = readLe32(fixed + 16);
        entry.compressedSize = readLe32(fixed + 20);
        entry.uncompressedSize = readLe32(fixed + 24);
        entry.localHeaderOffset = readLe32(fixed + 42);
        entry.name.assign(reinterpret_cast<const char*>(fixed + kCentralHeaderSize),
                          fileNameLength);
        const std::uint8_t* extraStart = fixed + kCentralHeaderSize + fileNameLength;
        entry.extra.assign(extraStart, extraStart + extraLength);
        const std::uint8_t* commentStart = extraStart + extraLength;
        entry.comment.assign(commentStart, commentStart + commentEntryLength);
        directory.entries.push_back(std::move(entry));
        cursor += entrySize;
    }

    result.ok = true;
    result.directory = std::move(directory);
    return result;
}

const CentralEntry* findEntry(const ZipDirectory& directory, std::string_view name) {
    const auto found = std::find_if(directory.entries.begin(),
                                    directory.entries.end(),
                                    [name](const CentralEntry& entry) {
                                        return entry.name == name;
                                    });
    return found == directory.entries.end() ? nullptr : &*found;
}

std::filesystem::path makeTemporaryPath(const std::filesystem::path& archivePath) {
    std::random_device randomDevice;
    std::mt19937_64 generator(randomDevice());
    for (int attempt = 0; attempt < 32; ++attempt) {
        std::ostringstream suffix;
        suffix << std::hex << std::setw(16) << std::setfill('0') << generator();
        const std::filesystem::path candidate = archivePath.parent_path()
            / ("." + archivePath.filename().string()
               + ".accloud-preview-" + suffix.str() + ".tmp");
        std::error_code ec;
        if (!std::filesystem::exists(candidate, ec) && !ec) {
            return candidate;
        }
    }
    return {};
}

bool writePreparedArchive(const std::filesystem::path& sourcePath,
                          const ZipDirectory& directory,
                          const CentralEntry& sourceEntry,
                          const std::filesystem::path& destinationPath,
                          std::string& error) {
    if ((sourceEntry.flags & 0x0001U) != 0U) {
        error = "L'aperçu source est chiffré dans l'archive ZIP.";
        return false;
    }
    if (directory.entryCount == std::numeric_limits<std::uint16_t>::max()) {
        error = "Nombre maximal d'entrées ZIP atteint.";
        return false;
    }

    std::ifstream input(sourcePath, std::ios::binary);
    if (!input) {
        error = "Impossible de relire l'archive PWSZ.";
        return false;
    }

    input.seekg(static_cast<std::streamoff>(sourceEntry.localHeaderOffset), std::ios::beg);
    std::array<std::uint8_t, kLocalHeaderSize> localHeader{};
    if (!readExact(input, localHeader.data(), localHeader.size())
        || readLe32(localHeader.data()) != kLocalHeaderSignature) {
        error = "En-tête local de preview_1.png invalide.";
        return false;
    }
    const std::uint16_t localNameLength = readLe16(localHeader.data() + 26);
    const std::uint16_t localExtraLength = readLe16(localHeader.data() + 28);
    const std::uint64_t compressedDataOffset = static_cast<std::uint64_t>(sourceEntry.localHeaderOffset)
                                               + kLocalHeaderSize
                                               + localNameLength
                                               + localExtraLength;
    if (compressedDataOffset + sourceEntry.compressedSize
        > directory.centralDirectoryOffset) {
        error = "Données compressées de preview_1.png hors limites.";
        return false;
    }

    std::vector<std::uint8_t> localName(localNameLength);
    input.seekg(static_cast<std::streamoff>(sourceEntry.localHeaderOffset
                                            + kLocalHeaderSize),
                std::ios::beg);
    if (!readExact(input, localName.data(), localName.size())
        || std::string_view(reinterpret_cast<const char*>(localName.data()), localName.size())
               != sourceEntry.name) {
        error = "Nom local de preview_1.png incohérent.";
        return false;
    }

    std::vector<std::uint8_t> localExtra(localExtraLength);
    input.seekg(static_cast<std::streamoff>(sourceEntry.localHeaderOffset
                                            + kLocalHeaderSize
                                            + localNameLength),
                std::ios::beg);
    if (!readExact(input, localExtra.data(), localExtra.size())) {
        error = "Champ supplémentaire local de preview_1.png illisible.";
        return false;
    }

    std::ofstream output(destinationPath, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = "Impossible de créer la version PWSZ temporaire.";
        return false;
    }

    if (!copyRange(input,
                   output,
                   0,
                   directory.centralDirectoryOffset)) {
        error = "Copie de l'archive PWSZ incomplète.";
        return false;
    }

    const std::streampos localOffsetPosition = output.tellp();
    if (localOffsetPosition < 0
        || static_cast<std::uint64_t>(localOffsetPosition)
               > std::numeric_limits<std::uint32_t>::max()) {
        error = "Offset ZIP temporaire hors limite.";
        return false;
    }
    const std::uint32_t newLocalOffset = static_cast<std::uint32_t>(localOffsetPosition);

    std::array<std::uint8_t, kLocalHeaderSize> newLocalHeader = localHeader;
    const std::uint16_t newFlags = static_cast<std::uint16_t>(sourceEntry.flags & ~0x0008U);
    writeLe16(newLocalHeader.data() + 6, newFlags);
    writeLe32(newLocalHeader.data() + 14, sourceEntry.crc32);
    writeLe32(newLocalHeader.data() + 18, sourceEntry.compressedSize);
    writeLe32(newLocalHeader.data() + 22, sourceEntry.uncompressedSize);
    writeLe16(newLocalHeader.data() + 26, static_cast<std::uint16_t>(kPreview2Name.size()));
    writeLe16(newLocalHeader.data() + 28, localExtraLength);

    if (!writeExact(output, newLocalHeader.data(), newLocalHeader.size())
        || !writeExact(output, kPreview2Name.data(), kPreview2Name.size())
        || !writeExact(output, localExtra.data(), localExtra.size())
        || !copyRange(input,
                      output,
                      compressedDataOffset,
                      sourceEntry.compressedSize)) {
        error = "Écriture de preview_2.png dans l'archive temporaire impossible.";
        return false;
    }

    const std::streampos centralOffsetPosition = output.tellp();
    if (centralOffsetPosition < 0
        || static_cast<std::uint64_t>(centralOffsetPosition)
               > std::numeric_limits<std::uint32_t>::max()) {
        error = "Répertoire central ZIP temporaire hors limite.";
        return false;
    }
    const std::uint32_t newCentralOffset = static_cast<std::uint32_t>(centralOffsetPosition);

    if (!writeExact(output,
                    directory.centralDirectoryBytes.data(),
                    directory.centralDirectoryBytes.size())) {
        error = "Réécriture du répertoire central ZIP impossible.";
        return false;
    }

    std::array<std::uint8_t, kCentralHeaderSize> newCentralHeader = sourceEntry.fixed;
    writeLe16(newCentralHeader.data() + 8, newFlags);
    writeLe16(newCentralHeader.data() + 28, static_cast<std::uint16_t>(kPreview2Name.size()));
    writeLe32(newCentralHeader.data() + 42, newLocalOffset);
    if (!writeExact(output, newCentralHeader.data(), newCentralHeader.size())
        || !writeExact(output, kPreview2Name.data(), kPreview2Name.size())
        || !writeExact(output, sourceEntry.extra.data(), sourceEntry.extra.size())
        || !writeExact(output, sourceEntry.comment.data(), sourceEntry.comment.size())) {
        error = "Ajout de l'entrée centrale preview_2.png impossible.";
        return false;
    }

    const std::uint64_t newEntrySize = kCentralHeaderSize
                                       + kPreview2Name.size()
                                       + sourceEntry.extra.size()
                                       + sourceEntry.comment.size();
    const std::uint64_t newCentralSize = static_cast<std::uint64_t>(directory.centralDirectorySize)
                                         + newEntrySize;
    if (newCentralSize > std::numeric_limits<std::uint32_t>::max()) {
        error = "Répertoire central ZIP temporaire trop grand.";
        return false;
    }

    std::array<std::uint8_t, kEndOfCentralDirectorySize> eocd{};
    writeLe32(eocd.data(), kEndOfCentralDirectorySignature);
    writeLe16(eocd.data() + 4, 0);
    writeLe16(eocd.data() + 6, 0);
    writeLe16(eocd.data() + 8, static_cast<std::uint16_t>(directory.entryCount + 1));
    writeLe16(eocd.data() + 10, static_cast<std::uint16_t>(directory.entryCount + 1));
    writeLe32(eocd.data() + 12, static_cast<std::uint32_t>(newCentralSize));
    writeLe32(eocd.data() + 16, newCentralOffset);
    writeLe16(eocd.data() + 20,
              static_cast<std::uint16_t>(directory.archiveComment.size()));
    if (!writeExact(output, eocd.data(), eocd.size())
        || !writeExact(output,
                       directory.archiveComment.data(),
                       directory.archiveComment.size())) {
        error = "Finalisation de l'archive ZIP temporaire impossible.";
        return false;
    }

    output.flush();
    if (!output.good()) {
        error = "Écriture finale de l'archive ZIP temporaire impossible.";
        return false;
    }
    output.close();

    std::error_code permissionError;
    const auto permissions = std::filesystem::status(sourcePath, permissionError).permissions();
    if (!permissionError) {
        std::filesystem::permissions(destinationPath,
                                     permissions,
                                     std::filesystem::perm_options::replace,
                                     permissionError);
    }
    return true;
}

} // namespace

PwszPreviewInspection inspectPwszPreviewArchive(const std::filesystem::path& archivePath) {
    const ParseResult parsed = parseZipDirectory(archivePath);
    if (!parsed.ok) {
        return {false, false, false, false, parsed.error};
    }

    const bool hasPreview1 = findEntry(parsed.directory, kPreview1Name) != nullptr;
    const bool hasPreview2 = findEntry(parsed.directory, kPreview2Name) != nullptr;
    PwszPreviewInspection result;
    result.ok = true;
    result.hasPreview1 = hasPreview1;
    result.hasPreview2 = hasPreview2;
    result.needsCompletion = !hasPreview2 && hasPreview1;
    if (hasPreview2) {
        result.message = "preview_images/preview_2.png est déjà présent.";
    } else if (hasPreview1) {
        result.message = "preview_images/preview_2.png est absent et peut être copié depuis preview_1.png.";
    } else {
        result.message = "preview_images/preview_1.png et preview_2.png sont absents.";
    }
    return result;
}

PwszPreviewPreparation preparePwszPreview2Copy(const std::filesystem::path& archivePath) {
    const ParseResult parsed = parseZipDirectory(archivePath);
    if (!parsed.ok) {
        return {false, false, {}, parsed.error};
    }
    if (findEntry(parsed.directory, kPreview2Name) != nullptr) {
        return {true, false, archivePath, "Aucune préparation nécessaire."};
    }
    const CentralEntry* preview1 = findEntry(parsed.directory, kPreview1Name);
    if (preview1 == nullptr) {
        return {false,
                false,
                {},
                "preview_images/preview_1.png est absent; preview_2.png ne peut pas être créé."};
    }

    const std::filesystem::path temporaryPath = makeTemporaryPath(archivePath);
    if (temporaryPath.empty()) {
        return {false, false, {}, "Impossible de réserver un fichier temporaire PWSZ."};
    }

    std::string writeError;
    if (!writePreparedArchive(archivePath,
                              parsed.directory,
                              *preview1,
                              temporaryPath,
                              writeError)) {
        discardPreparedFile(temporaryPath);
        return {false, false, {}, writeError};
    }

    const PwszPreviewInspection validation = inspectPwszPreviewArchive(temporaryPath);
    if (!validation.ok || !validation.hasPreview1 || !validation.hasPreview2) {
        discardPreparedFile(temporaryPath);
        return {false, false, {}, "Validation de l'archive PWSZ préparée échouée."};
    }
    return {true,
            true,
            temporaryPath,
            "preview_images/preview_1.png a été copié vers preview_2.png."};
}

PwszPreviewCommitResult replaceOriginalWithPrepared(
    const std::filesystem::path& originalPath,
    const std::filesystem::path& preparedPath) {
    if (originalPath.empty() || preparedPath.empty()) {
        return {false, "Chemin original ou préparé invalide."};
    }
    if (originalPath.parent_path() != preparedPath.parent_path()) {
        return {false, "Le remplacement atomique exige un fichier temporaire dans le même dossier."};
    }
    if (std::rename(preparedPath.c_str(), originalPath.c_str()) != 0) {
        return {false, "Remplacement atomique du fichier PWSZ local impossible."};
    }
    return {true, "Fichier PWSZ local remplacé par la version envoyée."};
}

void discardPreparedFile(const std::filesystem::path& preparedPath) noexcept {
    if (preparedPath.empty()) {
        return;
    }
    std::error_code ec;
    std::filesystem::remove(preparedPath, ec);
}

} // namespace accloud::cloud::archive
