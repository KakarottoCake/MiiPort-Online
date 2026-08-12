#pragma once

#include <string>
#include <vector>

#include "catalog.hpp"
#include "catalog_cache.hpp"
#include "catalog_transport.hpp"

enum class CatalogRequestState { Idle, Loading, Ready, Failed };

// A frame-friendly request boundary. The live adapter advances one curl-multi
// handle in bounded slices and commits a validated page only when it is ready.
class CatalogClient {
  public:
    static constexpr size_t MAX_QUERY_BYTES = 64;
    static constexpr size_t PAGE_SIZE = 12;
    static constexpr uint64_t CACHE_TTL_SECONDS = 10 * 60;

    explicit CatalogClient(std::string cacheRoot = "/MiiPort/cache/catalog");
    void request(CatalogSection section, const std::string& query = "", size_t start = 0);
    void pump();
    CatalogRequestState state() const;
    const CatalogPage& page() const;
    const std::string& error() const;

  private:
    CatalogRequestState requestState = CatalogRequestState::Idle;
    CatalogPage pending;
    CatalogPage current;
    CatalogTransport transport;
    CatalogCache cache;
    bool transportStarted = false;
    std::string lastError;

    std::string cacheKey() const;
    std::string requestUrl() const;
    bool parseInfiniMiiResponse(const std::string& body, std::string* error);
};
