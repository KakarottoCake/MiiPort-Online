#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include "catalog_client.hpp"
#include "infinimii.hpp"

namespace {
std::string readFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream contents;
    contents << input.rdbuf();
    return contents.str();
}

CatalogSection sectionFromName(const std::string& value) {
    if (value == "recent") return CatalogSection::New;
    if (value == "top") return CatalogSection::TopRated;
    if (value == "official") return CatalogSection::Official;
    if (value == "random") return CatalogSection::Random;
    return CatalogSection::Trending;
}

void printPage(const CatalogPage& page) {
    std::cout << "mode=" << InfiniMii::mode(page.section, page.query)
              << " start=" << page.start
              << " results=" << page.entries.size()
              << " total=" << page.total
              << " hasMore=" << (page.hasMore ? "yes" : "no") << '\n';
    for (size_t i = 0; i < page.entries.size(); ++i) {
        const CatalogMii& mii = page.entries[i];
        std::cout << i + 1 << ". " << mii.name << " [" << mii.id << "] by "
                  << mii.creator << " — " << mii.score << " likes\n";
    }
}

int offlineTest(const std::string& fixture) {
    CatalogPage page;
    std::string error;
    if (!InfiniMii::parseList(readFile(fixture), CatalogSection::Trending, "mario", 0, &page, &error)) {
        std::cerr << "FAIL parser: " << error << '\n';
        return 1;
    }
    if (page.entries.size() != 2 || page.entries[0].id != "yYmXg" ||
        page.entries[1].source != "InfiniMii Official" || page.start != 0 ||
        page.total != 2 || page.hasMore) {
        std::cerr << "FAIL parser assertions\n";
        return 1;
    }
    if (page.entries[0].downloadUrl.find("format=charinfo") == std::string::npos) {
        std::cerr << "FAIL download URL\n";
        return 1;
    }
    printPage(page);
    std::cout << "PASS offline production-parser test\n";
    return 0;
}

int liveTest(const std::string& cacheRoot, CatalogSection section, const std::string& query,
             size_t start) {
    CatalogClient client(cacheRoot);
    client.request(section, query, start);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (client.state() == CatalogRequestState::Loading && std::chrono::steady_clock::now() < deadline) {
        client.pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (client.state() != CatalogRequestState::Ready) {
        std::cerr << "FAIL live request: " << client.error() << '\n';
        return 1;
    }
    printPage(client.page());
    std::cout << "PASS one bounded live request\n";
    return 0;
}
}

int main(int argc, char** argv) {
    std::string fixture = "tests/macos/fixtures/list.tsv";
    std::string cacheRoot = "build-macos/cache";
    bool live = false;
    std::string section = "trending";
    std::string query;
    size_t start = 0;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--live") live = true;
        else if (arg == "--fixture" && i + 1 < argc) fixture = argv[++i];
        else if (arg == "--cache" && i + 1 < argc) cacheRoot = argv[++i];
        else if (arg == "--section" && i + 1 < argc) section = argv[++i];
        else if (arg == "--query" && i + 1 < argc) query = argv[++i];
        else if (arg == "--start" && i + 1 < argc) start = std::stoul(argv[++i]);
        else {
            std::cerr << "Usage: MiiPortMacTest [--live] [--section trending|recent|top|official|random]"
                         " [--query text] [--start offset] [--fixture path] [--cache path]\n";
            return 2;
        }
    }
    return live ? liveTest(cacheRoot, sectionFromName(section), query, start) : offlineTest(fixture);
}
