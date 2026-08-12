#include "catalog_cache.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

CatalogCache::CatalogCache(std::string root) : root(std::move(root)) {}

bool CatalogCache::validKey(const std::string& key) const {
    if (key.empty() || key.size() > 96) return false;
    for (unsigned char c : key) {
        if (!(std::isalnum(c) || c == '_' || c == '-')) return false;
    }
    return true;
}

bool CatalogCache::read(const std::string& key, std::string* out) const {
    if (out == nullptr || !validKey(key)) return false;
    const fs::path path = fs::path(root) / key;
    std::error_code error;
    const auto size = fs::file_size(path, error);
    if (error || size > MAX_ENTRY_BYTES) return false;
    FILE* file = fopen(path.c_str(), "rb");
    if (file == nullptr) return false;
    out->assign(static_cast<size_t>(size), '\0');
    const bool ok = fread(out->data(), 1, out->size(), file) == out->size();
    fclose(file);
    if (!ok) out->clear();
    return ok;
}

bool CatalogCache::readFresh(const std::string& key, uint64_t maxAgeSeconds, std::string* out) const {
    if (!validKey(key)) return false;
    std::error_code error;
    const auto modified = fs::last_write_time(fs::path(root) / key, error);
    if (error) return false;
    const auto age = fs::file_time_type::clock::now() - modified;
    if (age > std::chrono::seconds(maxAgeSeconds)) return false;
    return read(key, out);
}

bool CatalogCache::write(const std::string& key, const std::string& body) const {
    if (!validKey(key) || body.size() > MAX_ENTRY_BYTES) return false;
    std::error_code error;
    fs::create_directories(root, error);
    if (error) return false;
    const fs::path target = fs::path(root) / key;
    const fs::path temporary = fs::path(root) / (key + ".tmp");
    FILE* file = fopen(temporary.c_str(), "wb");
    if (file == nullptr) return false;
    const bool ok = fwrite(body.data(), 1, body.size(), file) == body.size();
    fclose(file);
    if (!ok) {
        fs::remove(temporary, error);
        return false;
    }
    // Horizon's rename does not replace an existing target. Removing only
    // this validated cache key keeps refreshes working on Switch and Eden.
    fs::remove(target, error);
    error.clear();
    fs::rename(temporary, target, error);
    if (error) fs::remove(temporary, error);
    return !error;
}

void CatalogCache::prune(size_t maxEntries) const {
    struct Entry {
        fs::path path;
        fs::file_time_type modified;
    };
    std::error_code error;
    std::vector<Entry> entries;
    for (fs::directory_iterator item(root, error), end; !error && item != end; item.increment(error)) {
        if (!item->is_regular_file(error)) continue;
        const std::string name = item->path().filename().string();
        if (!validKey(name)) continue;
        entries.push_back({item->path(), item->last_write_time(error)});
        if (error) break;
    }
    if (error || entries.size() <= maxEntries) return;
    std::sort(entries.begin(), entries.end(), [](const Entry& left, const Entry& right) {
        return left.modified < right.modified;
    });
    for (size_t index = 0; index < entries.size() - maxEntries; ++index) {
        error.clear();
        fs::remove(entries[index].path, error);
    }
}
