#include "catalog.hpp"

const char* CatalogStore::title(CatalogSection section) {
    switch (section) {
        case CatalogSection::Trending: return "Popular This Week";
        case CatalogSection::New: return "New";
        case CatalogSection::TopRated: return "Top Miis";
        case CatalogSection::Official: return "Official Miis";
        case CatalogSection::Random: return "Random Miis";
    }
    return "Miis";
}

const char* CatalogStore::subtitle(CatalogSection section) {
    switch (section) {
        case CatalogSection::Trending: return "Currently trending on InfiniMii";
        case CatalogSection::New: return "Recently approved additions";
        case CatalogSection::TopRated: return "Most liked community favorites";
        case CatalogSection::Official: return "Documented official characters";
        case CatalogSection::Random: return "A quick surprise from the catalog";
    }
    return "";
}
