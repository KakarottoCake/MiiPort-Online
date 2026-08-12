#pragma once

#include <cstdint>
#include <string>
#include <vector>

// The catalog deliberately has hard limits. The Switch UI only keeps the
// current page and a small thumbnail working set in memory.
enum class CatalogSection {
    Trending,
    New,
    TopRated,
    Official,
    Random,
};

struct CatalogMii {
    std::string id;
    std::string name;
    std::string creator;
    std::string source;
    std::string tags;
    uint32_t score;
    std::string imageUrl;
    std::string downloadUrl;
    std::string sha256;
};

struct CatalogPage {
    CatalogSection section = CatalogSection::Trending;
    std::string query;
    std::vector<CatalogMii> entries;
    bool hasMore = false;
    size_t start = 0;
    size_t total = 0;
};

class CatalogStore {
  public:
    static const char* title(CatalogSection section);
    static const char* subtitle(CatalogSection section);
};
