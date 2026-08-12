#include "catalog_client.hpp"
#include "infinimii.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace {
std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

uint32_t fnv1a(const std::string& value) {
    uint32_t hash = 2166136261u;
    for (unsigned char c : value) hash = (hash ^ c) * 16777619u;
    return hash;
}
}

CatalogClient::CatalogClient(std::string cacheRoot) : cache(std::move(cacheRoot)) {}

void CatalogClient::request(CatalogSection section, const std::string& query, size_t start) {
    pending = {};
    pending.section = section;
    pending.query = lower(query.substr(0, MAX_QUERY_BYTES));
    pending.start = start;
    transportStarted = false;
    lastError.clear();
    requestState = CatalogRequestState::Loading;
}

std::string CatalogClient::cacheKey() const {
    std::ostringstream key;
    key << "infinimii_" << std::hex << fnv1a(InfiniMii::mode(pending.section, pending.query)
        + "_" + pending.query + "_" + std::to_string(pending.start));
    return key.str();
}

std::string CatalogClient::requestUrl() const {
    return InfiniMii::listUrl(pending.section, pending.query, pending.start);
}

bool CatalogClient::parseInfiniMiiResponse(const std::string& body, std::string* error) {
    CatalogPage parsed;
    if (!InfiniMii::parseList(body, pending.section, pending.query, pending.start, &parsed, error)) return false;
    pending = std::move(parsed);
    return true;
}

void CatalogClient::pump() {
    if (requestState != CatalogRequestState::Loading) return;

    std::string body;
    if (!transportStarted && cache.readFresh(cacheKey(), CACHE_TTL_SECONDS, &body)) {
        std::string error;
        if (parseInfiniMiiResponse(body, &error)) {
            current = std::move(pending);
            requestState = CatalogRequestState::Ready;
        } else {
            lastError = error;
            requestState = CatalogRequestState::Failed;
        }
        return;
    }
    if (!transportStarted) {
        transportStarted = transport.start(requestUrl());
        if (!transportStarted) {
            lastError = "Could not start the catalog request";
            requestState = CatalogRequestState::Failed;
        }
        return;
    }

    transport.pump();
    if (transport.state() == CatalogTransportState::Loading) return;
    CatalogHttpResponse response = transport.takeResponse();
    std::string error;
    if (!response.error.empty() || !parseInfiniMiiResponse(response.body, &error)) {
        // A stale cache is preferable to a blank screen when the Switch is
        // temporarily offline. It does not create another provider request.
        std::string stale;
        if (cache.read(cacheKey(), &stale) && parseInfiniMiiResponse(stale, &error)) {
            current = std::move(pending);
            requestState = CatalogRequestState::Ready;
            return;
        }
        lastError = !response.error.empty() ? response.error : error;
        requestState = CatalogRequestState::Failed;
        return;
    }
    cache.write(cacheKey(), response.body);
    current = std::move(pending);
    requestState = CatalogRequestState::Ready;
}

CatalogRequestState CatalogClient::state() const { return requestState; }
const CatalogPage& CatalogClient::page() const { return current; }
const std::string& CatalogClient::error() const { return lastError; }
