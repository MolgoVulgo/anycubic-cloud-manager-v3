#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <string_view>

namespace accloud::config {

namespace detail {

inline std::string trim(std::string value) {
  const auto notSpace = [](unsigned char c) { return c != ' ' && c != '\t' && c != '\r' && c != '\n'; };
  std::size_t start = 0;
  while (start < value.size() && !notSpace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }
  std::size_t end = value.size();
  while (end > start && !notSpace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }
  return value.substr(start, end - start);
}

inline std::filesystem::path defaultRoot() {
  if (const char* home = std::getenv("HOME"); home != nullptr && *home != '\0') {
    return std::filesystem::path(home) / ".local" / "share" / "accloud";
  }
  return std::filesystem::path(".local/share/accloud");
}

inline std::filesystem::path configPath() {
  if (const char* env = std::getenv("ACCLOUD_PATHS_INI"); env != nullptr && *env != '\0') {
    return std::filesystem::path(env);
  }
  return defaultRoot() / "accloud.ini";
}

inline std::map<std::string, std::string> defaultEntries() {
  const auto root = defaultRoot();
  return {
      {"root", root.string()},
      {"session", (root / "session.json").string()},
      {"settings", (root / "settings.ini").string()},
      {"db", (root / "accloud_cache.db").string()},
      {"tmp", (root / "tmp").string()},
      {"thumbnails", (root / "thumbnails").string()},
      {"openssl_compat", (root / "tmp" / "accloud_openssl_seclevel0.cnf").string()},
      {"logs", "/var/log/accloud"},
  };
}

inline std::map<std::string, std::string> loadOrCreateIni() {
  const auto path = configPath();
  std::map<std::string, std::string> entries = defaultEntries();

  std::ifstream in(path);
  if (in.is_open()) {
    std::string line;
    while (std::getline(in, line)) {
      const auto eq = line.find('=');
      if (eq == std::string::npos) {
        continue;
      }
      std::string key = trim(line.substr(0, eq));
      std::string val = trim(line.substr(eq + 1));
      if (!key.empty() && !val.empty()) {
        entries[key] = val;
      }
    }
    return entries;
  }

  std::error_code ec;
  if (path.has_parent_path()) {
    std::filesystem::create_directories(path.parent_path(), ec);
  }
  std::ofstream out(path, std::ios::out | std::ios::trunc);
  if (out.is_open()) {
    out << "# accloud paths configuration\n";
    out << "# Generated automatically when missing.\n";
    for (const auto& [key, value] : entries) {
      out << key << '=' << value << '\n';
    }
  }
  return entries;
}

inline std::filesystem::path entryPath(std::string_view key) {
  const auto entries = loadOrCreateIni();
  const auto it = entries.find(std::string(key));
  if (it == entries.end() || it->second.empty()) {
    return defaultRoot();
  }
  return std::filesystem::path(it->second);
}

inline std::filesystem::path envOrConfig(const char* envKey, std::string_view configKey) {
  if (const char* env = std::getenv(envKey); env != nullptr && *env != '\0') {
    return std::filesystem::path(env);
  }
  return entryPath(configKey);
}

} // namespace detail

inline std::filesystem::path rootDir() { return detail::entryPath("root"); }
inline std::filesystem::path settingsPath() { return detail::entryPath("settings"); }
inline std::filesystem::path sessionPath() { return detail::envOrConfig("ACCLOUD_SESSION_PATH", "session"); }
inline std::filesystem::path dbPath() { return detail::envOrConfig("ACCLOUD_DB_PATH", "db"); }
inline std::filesystem::path tmpDir() { return detail::entryPath("tmp"); }
inline std::filesystem::path thumbnailDir() {
  return detail::envOrConfig("ACCLOUD_THUMBNAIL_DIR", "thumbnails");
}
inline std::filesystem::path opensslCompatPath() {
  return detail::envOrConfig("ACCLOUD_MQTT_OPENSSL_CONF_PATH", "openssl_compat");
}
inline std::filesystem::path logDir() { return detail::envOrConfig("ACCLOUD_LOG_DIR", "logs"); }

} // namespace accloud::config
