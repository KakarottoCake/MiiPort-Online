#pragma once

#include <string>

#include "catalog.hpp"

class InfiniMii {
  public:
    static constexpr size_t PAGE_SIZE = 12;

    static std::string mode(CatalogSection section, const std::string& query);
    static std::string listUrl(CatalogSection section, const std::string& query, size_t start);
    static std::string downloadUrl(const std::string& id);
    static std::string thumbnailUrl(const std::string& id);
    static bool parseList(const std::string& body, CatalogSection section,
                          const std::string& query, size_t requestedStart, CatalogPage* page,
                          std::string* error);
};
