# Catalog API contract

The Switch client accepts only a small, provider-neutral response. A provider
adapter runs on a server controlled by the project; the Switch never scrapes a
third-party HTML page.

## Endpoint

`GET /v1/miis?section=trending&page=1&q=mario`

Response limits: 96 KiB body, 18 entries, 64 UTF-8 bytes for `q`.

```json
{
  "items": [{
    "id": "provider:stable-id",
    "name": "Example",
    "creator": "Creator",
    "source": "Approved provider",
    "tags": ["Video Games"],
    "score": 42,
    "image": "https://catalog.example/v1/images/provider:stable-id",
    "download": "https://catalog.example/v1/miis/provider:stable-id/charinfo",
    "sha256": "optional lowercase hex"
  }],
  "nextPage": 2
}
```

The client rejects unknown file sizes, oversized bodies, invalid UTF-8, missing
IDs, and a downloaded CHARINFO whose size is not exactly 88 bytes. Images are
requested only after text results are visible and are stored in a 12-item LRU
cache. HTTPS transport uses curl-multi polling: one state transition per frame,
no redirects, certificate verification enabled, 3-second connect timeout, and
8-second total timeout.
