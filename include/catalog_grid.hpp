#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <borealis.hpp>

#include "catalog.hpp"
#include "catalog_cache.hpp"
#include "catalog_transport.hpp"

class MiiGridTile;

// A controller-first, fixed-size gallery. Only the visible page exists and
// thumbnails are fetched serially so browsing never creates a network burst.
class MiiGrid : public brls::View {
  public:
    using SelectCallback = std::function<void(const CatalogMii&)>;

    MiiGrid(const std::vector<CatalogMii>& entries, SelectCallback onSelect);
    ~MiiGrid() override;

    void frame(brls::FrameContext* ctx) override;
    void draw(NVGcontext* vg, int x, int y, unsigned width, unsigned height,
              brls::Style* style, brls::FrameContext* ctx) override;
    void layout(NVGcontext* vg, brls::Style* style, brls::FontStash* stash) override;
    brls::View* getDefaultFocus() override;
    brls::View* getNextFocus(brls::FocusDirection direction, void* parentUserdata) override;
    void onChildFocusGained(brls::View* child) override;
    void willAppear(bool resetState = false) override;
    void willDisappear(bool resetState = false) override;
    void setThumbnailLoadingEnabled(bool enabled);

  private:
    static constexpr size_t COLUMNS = 4;
    static constexpr size_t NO_TILE = static_cast<size_t>(-1);
    static constexpr size_t RGBA_BYTES = 64 * 64 * 4;
    static constexpr uint64_t THUMBNAIL_TTL_SECONDS = 30ULL * 24 * 60 * 60;

    std::vector<MiiGridTile*> tiles;
    std::unique_ptr<CatalogTransport> thumbnailTransport;
    CatalogCache thumbnailCache;
    size_t focusedTile = 0;
    size_t activeThumbnail = NO_TILE;
    bool visible = false;
    bool thumbnailLoadingEnabled = true;

    void pumpThumbnail();
    size_t nextThumbnail() const;
    static std::string thumbnailCacheKey(const std::string& id);
};
