#include "catalog_grid.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <utility>

namespace {
uint32_t fnv1a(const std::string& value) {
    uint32_t hash = 2166136261u;
    for (unsigned char c : value) hash = (hash ^ c) * 16777619u;
    return hash;
}
}

class MiiGridTile : public brls::View {
  public:
    MiiGridTile(CatalogMii mii, MiiGrid::SelectCallback onSelect)
        : mii(std::move(mii)), onSelect(std::move(onSelect)) {
        image = new brls::Image();
        image->setParent(this);
        image->setScaleType(brls::ImageScaleType::FIT);
        registerAction("View", brls::Key::A, [this] {
            if (this->onSelect) this->onSelect(this->mii);
            return true;
        });
    }

    ~MiiGridTile() override { delete image; }

    brls::View* getDefaultFocus() override { return this; }

    void draw(NVGcontext* vg, int x, int y, unsigned width, unsigned height,
              brls::Style*, brls::FrameContext* ctx) override {
        nvgBeginPath(vg);
        nvgFillColor(vg, a(ctx->theme->sidebarColor));
        nvgRoundedRect(vg, x, y, width, height, 10);
        nvgFill(vg);

        if (imageReady) {
            image->frame(ctx);
        } else {
            const float placeholderSize = std::min(width - 30U, height - 58U);
            const float placeholderX = x + (width - placeholderSize) / 2.0F;
            nvgBeginPath(vg);
            nvgFillColor(vg, a(ctx->theme->listItemValueColor));
            nvgRoundedRect(vg, placeholderX, y + 10, placeholderSize, placeholderSize, 8);
            nvgFill(vg);

            nvgFillColor(vg, a(ctx->theme->descriptionColor));
            nvgFontFaceId(vg, ctx->fontStash->regular);
            nvgFontSize(vg, 14);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgText(vg, x + width / 2.0F, y + 10 + placeholderSize / 2.0F,
                    attempted ? "Preview unavailable" : "Loading preview…", nullptr);
        }

        nvgSave(vg);
        nvgIntersectScissor(vg, x + 8, y, width - 16, height);
        nvgFontFaceId(vg, ctx->fontStash->regular);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        nvgFillColor(vg, a(ctx->theme->textColor));
        nvgFontSize(vg, 18);
        nvgText(vg, x + width / 2.0F, y + height - 45, mii.name.c_str(), nullptr);
        nvgFillColor(vg, a(ctx->theme->descriptionColor));
        nvgFontSize(vg, 14);
        const std::string creator = mii.creator.empty() ? mii.source : "by " + mii.creator;
        nvgText(vg, x + width / 2.0F, y + height - 22, creator.c_str(), nullptr);
        nvgRestore(vg);
    }

    void layout(NVGcontext*, brls::Style*, brls::FontStash*) override {
        const unsigned imageSide = std::min(getWidth() - 30U, getHeight() - 58U);
        image->setBoundaries(getX() + (getWidth() - imageSide) / 2, getY() + 10,
                             imageSide, imageSide);
    }

    const CatalogMii& entry() const { return mii; }
    bool needsThumbnail() const { return !attempted && !imageReady; }

    void markAttempted() {
        attempted = true;
        invalidate();
    }

    void retryThumbnail() {
        if (!imageReady) attempted = false;
    }

    void setThumbnail(const std::string& rgba) {
        if (rgba.size() != 64 * 64 * 4) return;
        image->setImageRGBA(reinterpret_cast<const unsigned char*>(rgba.data()), 64, 64);
        imageReady = true;
        attempted = true;
        image->invalidate();
        invalidate();
    }

    void releaseThumbnail() {
        delete image;
        image = new brls::Image();
        image->setParent(this);
        image->setScaleType(brls::ImageScaleType::FIT);
        imageReady = false;
        attempted = false;
        invalidate();
    }

  private:
    CatalogMii mii;
    MiiGrid::SelectCallback onSelect;
    brls::Image* image = nullptr;
    bool attempted = false;
    bool imageReady = false;
};

MiiGrid::MiiGrid(const std::vector<CatalogMii>& entries, SelectCallback onSelect)
    : thumbnailTransport(new CatalogTransport()), thumbnailCache("/MiiPort/cache/thumbnails") {
    // Scan infrequently: enough to bound long-term SD use without adding work
    // to each thumbnail response or every page change.
    static size_t gridsSincePrune = 0;
    if (gridsSincePrune++ % 10 == 0) thumbnailCache.prune(240);
    tiles.reserve(std::min(entries.size(), static_cast<size_t>(12)));
    for (size_t index = 0; index < entries.size() && index < 12; ++index) {
        auto* tile = new MiiGridTile(entries[index], onSelect);
        // Borealis owns and frees parent user data with free().
        auto* parentIndex = static_cast<size_t*>(std::malloc(sizeof(size_t)));
        if (parentIndex != nullptr) *parentIndex = index;
        tile->setParent(this, parentIndex);
        tiles.push_back(tile);
    }
}

MiiGrid::~MiiGrid() {
    for (MiiGridTile* tile : tiles) delete tile;
}

void MiiGrid::frame(brls::FrameContext* ctx) {
    if (visible) pumpThumbnail();
    brls::View::frame(ctx);
}

void MiiGrid::draw(NVGcontext* vg, int x, int y, unsigned width, unsigned height,
                   brls::Style*, brls::FrameContext* ctx) {
    if (tiles.empty()) {
        nvgFillColor(vg, a(ctx->theme->descriptionColor));
        nvgFontFaceId(vg, ctx->fontStash->regular);
        nvgFontSize(vg, 23);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(vg, x + width / 2.0F, y + height / 2.0F,
                "No Miis matched. Press B and try a broader search.", nullptr);
        return;
    }
    for (MiiGridTile* tile : tiles) tile->frame(ctx);
}

void MiiGrid::layout(NVGcontext*, brls::Style*, brls::FontStash*) {
    constexpr unsigned horizontalMargin = 12;
    constexpr unsigned verticalMargin = 10;
    constexpr unsigned horizontalGap = 16;
    constexpr unsigned verticalGap = 12;
    constexpr size_t rows = 3;

    const unsigned usableWidth = getWidth() - horizontalMargin * 2 - horizontalGap * (COLUMNS - 1);
    const unsigned usableHeight = getHeight() - verticalMargin * 2 - verticalGap * (rows - 1);
    const unsigned tileWidth = usableWidth / COLUMNS;
    const unsigned tileHeight = usableHeight / rows;

    for (size_t index = 0; index < tiles.size(); ++index) {
        const size_t row = index / COLUMNS;
        const size_t column = index % COLUMNS;
        tiles[index]->setBoundaries(
            getX() + horizontalMargin + column * (tileWidth + horizontalGap),
            getY() + verticalMargin + row * (tileHeight + verticalGap),
            tileWidth, tileHeight);
    }
}

brls::View* MiiGrid::getDefaultFocus() {
    if (tiles.empty()) return this;
    return tiles.front();
}

brls::View* MiiGrid::getNextFocus(brls::FocusDirection direction, void* parentUserdata) {
    if (parentUserdata == nullptr || tiles.empty()) return nullptr;
    const size_t current = *static_cast<size_t*>(parentUserdata);
    size_t next = current;
    switch (direction) {
        case brls::FocusDirection::LEFT:
            if (current % COLUMNS == 0) return nullptr;
            next = current - 1;
            break;
        case brls::FocusDirection::RIGHT:
            if (current % COLUMNS == COLUMNS - 1) return nullptr;
            next = current + 1;
            break;
        case brls::FocusDirection::UP:
            if (current < COLUMNS) return nullptr;
            next = current - COLUMNS;
            break;
        case brls::FocusDirection::DOWN:
            next = current + COLUMNS;
            break;
    }
    return next < tiles.size() ? tiles[next]->getDefaultFocus() : nullptr;
}

void MiiGrid::onChildFocusGained(brls::View* child) {
    if (child != nullptr && child->getParentUserData() != nullptr)
        focusedTile = *static_cast<size_t*>(child->getParentUserData());
    brls::View::onChildFocusGained(child);
}

void MiiGrid::willAppear(bool) {
    visible = true;
    if (thumbnailLoadingEnabled && !thumbnailTransport)
        thumbnailTransport.reset(new CatalogTransport());
}

void MiiGrid::willDisappear(bool) {
    visible = false;
    thumbnailTransport.reset();
    activeThumbnail = NO_TILE;
    // Hidden pages should not retain GPU textures. The disk cache makes a
    // return visit cheap without increasing provider traffic.
    for (MiiGridTile* tile : tiles) tile->releaseThumbnail();
}

void MiiGrid::setThumbnailLoadingEnabled(bool enabled) {
    thumbnailLoadingEnabled = enabled;
    if (!enabled) {
        if (activeThumbnail < tiles.size()) tiles[activeThumbnail]->retryThumbnail();
        thumbnailTransport.reset();
        activeThumbnail = NO_TILE;
    } else if (visible && !thumbnailTransport) {
        thumbnailTransport.reset(new CatalogTransport());
    }
}

void MiiGrid::pumpThumbnail() {
    if (!thumbnailLoadingEnabled || !thumbnailTransport) return;
    if (activeThumbnail != NO_TILE) {
        thumbnailTransport->pump();
        if (thumbnailTransport->state() == CatalogTransportState::Loading) return;
        CatalogHttpResponse response = thumbnailTransport->takeResponse();
        MiiGridTile* tile = tiles[activeThumbnail];
        if (response.error.empty() && response.status >= 200 && response.status < 300 &&
            response.body.size() == RGBA_BYTES) {
            tile->setThumbnail(response.body);
            thumbnailCache.write(thumbnailCacheKey(tile->entry().id), response.body);
        } else {
            tile->markAttempted();
        }
        activeThumbnail = NO_TILE;
        return;
    }

    const size_t next = nextThumbnail();
    if (next == NO_TILE) return;
    MiiGridTile* tile = tiles[next];
    std::string cached;
    if (thumbnailCache.readFresh(thumbnailCacheKey(tile->entry().id),
                                 THUMBNAIL_TTL_SECONDS, &cached) &&
        cached.size() == RGBA_BYTES) {
        tile->setThumbnail(cached);
        return;
    }
    tile->markAttempted();
    if (thumbnailTransport->start(tile->entry().imageUrl)) activeThumbnail = next;
}

size_t MiiGrid::nextThumbnail() const {
    if (focusedTile < tiles.size() && tiles[focusedTile]->needsThumbnail()) return focusedTile;
    for (size_t index = 0; index < tiles.size(); ++index) {
        if (tiles[index]->needsThumbnail()) return index;
    }
    return NO_TILE;
}

std::string MiiGrid::thumbnailCacheKey(const std::string& id) {
    std::ostringstream key;
    key << "mii_" << std::hex << std::setw(8) << std::setfill('0') << fnv1a(id);
    return key.str();
}
