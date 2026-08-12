#pragma once

#include <string>

struct CatalogHttpResponse {
    long status = 0;
    std::string body;
    std::string error;
};

enum class CatalogTransportState { Idle, Loading, Ready, Failed };

// A non-blocking, bounded HTTPS request. Call pump() once per frame while a
// request is active; it performs at most one curl-multi state transition.
class CatalogTransport {
  public:
    static constexpr size_t MAX_RESPONSE_BYTES = 96 * 1024;

    CatalogTransport();
    ~CatalogTransport();

    bool start(const std::string& url);
    void pump();
    CatalogTransportState state() const;
    CatalogHttpResponse takeResponse();

  private:
    void finish(const std::string& error = "");

    void* multi = nullptr;
    void* easy = nullptr;
    CatalogTransportState requestState = CatalogTransportState::Idle;
    CatalogHttpResponse response;
};
