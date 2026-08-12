#include "catalog_transport.hpp"

#include <curl/curl.h>

namespace {
size_t writeResponse(char* data, size_t size, size_t count, void* userData) {
    auto* response = static_cast<CatalogHttpResponse*>(userData);
    const size_t bytes = size * count;
    if (bytes > CatalogTransport::MAX_RESPONSE_BYTES ||
        response->body.size() > CatalogTransport::MAX_RESPONSE_BYTES - bytes) {
        response->error = "Catalog response exceeded 96 KiB";
        return 0;
    }
    response->body.append(data, bytes);
    return bytes;
}

bool isHttps(const std::string& url) {
    return url.rfind("https://", 0) == 0;
}
}

CatalogTransport::CatalogTransport() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    multi = curl_multi_init();
}

CatalogTransport::~CatalogTransport() {
    if (easy != nullptr && multi != nullptr) curl_multi_remove_handle(static_cast<CURLM*>(multi), static_cast<CURL*>(easy));
    if (easy != nullptr) curl_easy_cleanup(static_cast<CURL*>(easy));
    if (multi != nullptr) curl_multi_cleanup(static_cast<CURLM*>(multi));
}

bool CatalogTransport::start(const std::string& url) {
    if (requestState == CatalogTransportState::Loading || !isHttps(url) || multi == nullptr) return false;

    response = {};
    easy = curl_easy_init();
    if (easy == nullptr) {
        requestState = CatalogTransportState::Failed;
        response.error = "Unable to create network request";
        return false;
    }

    CURL* handle = static_cast<CURL*>(easy);
    curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, writeResponse);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "MiiPortGallery/0.1");
    curl_easy_setopt(handle, CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, 3000L);
    curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, 8000L);
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);

    if (curl_multi_add_handle(static_cast<CURLM*>(multi), handle) != CURLM_OK) {
        curl_easy_cleanup(handle);
        easy = nullptr;
        requestState = CatalogTransportState::Failed;
        response.error = "Unable to schedule network request";
        return false;
    }
    requestState = CatalogTransportState::Loading;
    return true;
}

void CatalogTransport::pump() {
    if (requestState != CatalogTransportState::Loading) return;

    int running = 0;
    curl_multi_perform(static_cast<CURLM*>(multi), &running);

    int messages = 0;
    if (CURLMsg* message = curl_multi_info_read(static_cast<CURLM*>(multi), &messages)) {
        if (message->msg != CURLMSG_DONE) return;
        curl_easy_getinfo(message->easy_handle, CURLINFO_RESPONSE_CODE, &response.status);
        if (message->data.result != CURLE_OK && response.error.empty()) response.error = curl_easy_strerror(message->data.result);
        finish(response.error);
    }
}

CatalogTransportState CatalogTransport::state() const {
    return requestState;
}

CatalogHttpResponse CatalogTransport::takeResponse() {
    CatalogHttpResponse result = std::move(response);
    response = {};
    if (requestState == CatalogTransportState::Ready || requestState == CatalogTransportState::Failed) requestState = CatalogTransportState::Idle;
    return result;
}

void CatalogTransport::finish(const std::string& error) {
    if (!error.empty()) response.error = error;
    if (easy != nullptr) {
        curl_multi_remove_handle(static_cast<CURLM*>(multi), static_cast<CURL*>(easy));
        curl_easy_cleanup(static_cast<CURL*>(easy));
        easy = nullptr;
    }
    requestState = response.error.empty() && response.status >= 200 && response.status < 300
        ? CatalogTransportState::Ready : CatalogTransportState::Failed;
}
