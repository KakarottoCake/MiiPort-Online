#pragma once

#include <string>

// Response files are compact, atomically written, and constrained before they
// are read. Thumbnail cache uses the same key validation and size discipline.
class CatalogCache {
  public:
    static constexpr size_t MAX_ENTRY_BYTES = 96 * 1024;

    explicit CatalogCache(std::string root = "/MiiPort/cache/catalog");
    bool read(const std::string& key, std::string* out) const;
    bool readFresh(const std::string& key, uint64_t maxAgeSeconds, std::string* out) const;
    bool write(const std::string& key, const std::string& body) const;
    void prune(size_t maxEntries) const;

  private:
    std::string root;
    bool validKey(const std::string& key) const;
};
