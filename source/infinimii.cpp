#include "infinimii.hpp"

#include <cctype>
#include <iomanip>
#include <sstream>
#include <vector>

namespace {
std::string sectionMode(CatalogSection section) {
    switch (section) {
        case CatalogSection::Trending: return "trending";
        case CatalogSection::New: return "recent";
        case CatalogSection::TopRated: return "top";
        case CatalogSection::Official: return "official";
        case CatalogSection::Random: return "random";
    }
    return "trending";
}

std::string urlEncode(const std::string& value) {
    std::ostringstream encoded;
    encoded << std::uppercase << std::hex;
    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') encoded << c;
        else encoded << '%' << std::setw(2) << std::setfill('0') << static_cast<unsigned>(c);
    }
    return encoded.str();
}

std::vector<std::string> split(const std::string& value, char separator) {
    std::vector<std::string> fields;
    size_t start = 0;
    while (true) {
        const size_t end = value.find(separator, start);
        fields.push_back(value.substr(start, end == std::string::npos ? end : end - start));
        if (!fields.back().empty() && fields.back().back() == '\r') fields.back().pop_back();
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return fields;
}

uint32_t number(const std::string& value) {
    try { return static_cast<uint32_t>(std::stoul(value)); }
    catch (...) { return 0; }
}
}

std::string InfiniMii::mode(CatalogSection section, const std::string& query) {
    return query.empty() ? sectionMode(section) : "search";
}

std::string InfiniMii::listUrl(CatalogSection section, const std::string& query, size_t start) {
    std::string url = "https://infinimii.com/api/console/list?mode=" + mode(section, query)
        + "&start=" + std::to_string(start) + "&limit=" + std::to_string(PAGE_SIZE);
    if (!query.empty()) url += "&q=" + urlEncode(query);
    return url;
}

std::string InfiniMii::thumbnailUrl(const std::string& id) {
    return "https://infinimii.com/api/console/mii/" + urlEncode(id) + ".rgba";
}

std::string InfiniMii::downloadUrl(const std::string& id) {
    return "https://infinimii.com/downloadMii?id=" + urlEncode(id) + "&format=charinfo";
}

bool InfiniMii::parseList(const std::string& body, CatalogSection section,
                          const std::string& query, size_t requestedStart, CatalogPage* page,
                          std::string* error) {
    if (page == nullptr) {
        if (error) *error = "No output page was supplied";
        return false;
    }
    const std::vector<std::string> rows = split(body, '\n');
    if (rows.empty() || rows[0] != "OK") {
        if (error) *error = "Catalog provider rejected the request";
        return false;
    }

    *page = {};
    page->section = section;
    page->query = query;
    page->start = requestedStart;
    for (size_t index = 2; index < rows.size(); ++index) {
        const std::vector<std::string> fields = split(rows[index], '\t');
        // The ninth CFSD preview column is optional and commonly empty.
        if (fields.size() < 9 || fields[0].empty() || fields[1].empty()) continue;
        CatalogMii entry{};
        entry.id = fields[0];
        entry.name = fields[1];
        entry.creator = fields[2].empty() ? fields[3] : fields[2];
        entry.source = fields[5] == "1" ? "InfiniMii Official" : "InfiniMii";
        entry.tags = fields[6];
        entry.score = number(fields[4]);
        entry.imageUrl = thumbnailUrl(entry.id);
        entry.downloadUrl = downloadUrl(entry.id);
        page->entries.push_back(std::move(entry));
        if (page->entries.size() == PAGE_SIZE) break;
    }

    if (rows.size() > 1) {
        for (const std::string& field : split(rows[1], '\t')) {
            if (field.rfind("start=", 0) == 0) page->start = number(field.substr(6));
            else if (field.rfind("total=", 0) == 0) page->total = number(field.substr(6));
        }
    }
    page->hasMore = page->start + page->entries.size() < page->total;
    if (page->entries.empty() && query.empty()) {
        if (error) *error = "The catalog returned no usable Mii records";
        return false;
    }
    return true;
}
