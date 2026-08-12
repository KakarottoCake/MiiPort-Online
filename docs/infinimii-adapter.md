# InfiniMii adapter

The Gallery uses InfiniMii's machine-readable console endpoint, not its HTML
pages:

```text
GET https://infinimii.com/api/console/list?mode=trending&start=0&limit=12
GET https://infinimii.com/api/console/mii/{id}.rgba
GET https://infinimii.com/downloadMii?id={id}&format=charinfo
```

Supported list modes are `trending`, `recent`, `top`, `official`, `random`,
and `search`. The first endpoint returns a small tab-separated response with a
maximum provider-side list size of 24. Gallery deliberately asks for 12.

## Client etiquette

- Never prefetch categories, pages, thumbnails, or search suggestions.
- Load only the visible page's thumbnails, serially, after its grid appears.
- Allow exactly one network operation across catalog pages, thumbnails, and
  character downloads at a time.
- Cache each category/search result for ten minutes on SD.
- Cache validated 64×64 RGBA previews for 30 days and prune the thumbnail
  directory to 240 entries.
- Use HTTPS only; redirect following is disabled.
- Fail after an 8-second total timeout (3 seconds to connect).
- Download a character only after the user confirms it. The response must be
  exactly 88 bytes before it is written atomically to
  `sd:/MiiPort/miis/Downloads/` and imported.

The UI labels the provider's `top` mode as **Top Miis**. It does not show a
misleading Most Downloaded category because the API exposes votes, not an
all-time download metric.
