#include <cstdio>
#include <functional>
#include <string>
#include <cstring>

#include <switch.h>
// needed to access scrollview private members in TopScrollList
// todo: don't do this. Investigate alternatives. Fork borealis? Maybe possible on newer version?
#define private protected
#include <borealis.hpp>
#undef private
#include <borealis/swkbd.hpp>

#include "miiport.hpp"
#include "catalog.hpp"
#include "catalog_client.hpp"
#include "catalog_grid.hpp"


const AppletType APPLET_TYPE = appletGetAppletType();

Result init() {
    Result res;
    res = setsysInitialize();
    if(R_FAILED(res)) return res;
    res = miiInitialize(MiiServiceType_System);
    if(R_FAILED(res)) return res;
    return 0;
}

void deinit() {
    miiExit();
    setsysExit();
}

// modify so that the list can be made unfocusable, useful if the list has no focusable children.
class FocusList : public brls::List {
    private:
        bool allowFocus = false;
    public:
        FocusList(bool focus) {
            allowFocus = focus;
        }
        void setAllowFocus(bool focus) {
            allowFocus = focus;
        }
        brls::View* getDefaultFocus() override {
            if(allowFocus) {
                return this->getContentView();
            }
            else {
                return nullptr;
            }
        }
};

// modify so that the view is scrolled so that focused item is at the top instead of the middle of the screen.
class TopScrollList : public brls::List {
    bool updateScrolling(bool animated) {
        // Don't scroll if layout hasn't been called yet
        if (!this->ready || !this->contentView)
            return false;

        float contentHeight = (float)this->contentView->getHeight();

        // Ensure content is laid out too
        if (contentHeight == 0)
            return false;

        brls::View* focusedView = brls::Application::getCurrentFocus();
        // Edited here so that the focused element is at the top of the view
        float newScroll = -(this->scrollY * contentHeight) - ((float)(focusedView->getY() - 15) - (float)this->getY());

        // Bottom boundary
        if ((float)this->y + newScroll + contentHeight < (float)this->bottomY)
            newScroll = (float)this->height - contentHeight;

        // Top boundary
        if (newScroll > 0.0f)
            newScroll = 0.0f;

        // Apply 0.0f -> 1.0f scale
        newScroll = abs(newScroll) / contentHeight;

        //Start animation
        this->startScrolling(animated, newScroll);

        return true;
    }

    // override so that my updateScrolling gets used
    void onChildFocusGained(brls::View* child) override {
        if (!this->ready)
            return;

        if (child != this->contentView)
            return;

        // Start scrolling
        updateScrolling(true);

        brls::View::onChildFocusGained(child);
    }
};

// modify brls::Header so that they can be focused, but don't highlight
class FocusHeader : public brls::Header {
    // make focusable
    brls::View* getDefaultFocus() override {
        return this;
    }
    public:
        FocusHeader(std::string label, bool separator = true, std::string sublabel = "") 
            : Header(label, separator, sublabel)
        {}

        // no highlight
        void onFocusGained() override {
            this->focused = true;

            this->focusEvent.fire(this);

            if (this->hasParent())
                this->getParent()->onChildFocusGained(this);
        }
};

const std::string TITLE = "MiiPort Online";

class CatalogResultsFrame : public brls::AppletFrame {
  public:
    using PageCallback = std::function<void(CatalogSection, const std::string&, size_t)>;
    using CloseCallback = std::function<void(CatalogResultsFrame*)>;

    CatalogResultsFrame(PageCallback onPage, MiiGrid::SelectCallback onSelect,
                        CloseCallback onClose)
        : brls::AppletFrame(true, true), onPage(std::move(onPage)),
          onSelect(std::move(onSelect)), onClose(std::move(onClose)) {
        registerAction("Previous page", brls::Key::L, [this] {
            if (page.start < CatalogClient::PAGE_SIZE) return false;
            this->onPage(page.section, page.query, page.start - CatalogClient::PAGE_SIZE);
            return true;
        });
        registerAction("Next page", brls::Key::R, [this] {
            if (!page.hasMore) return false;
            this->onPage(page.section, page.query, page.start + CatalogClient::PAGE_SIZE);
            return true;
        });
        setFooterText("B  Back     L/R  Change page");
    }

    ~CatalogResultsFrame() override { notifyClosed(); }

    bool onCancel() override {
        notifyClosed();
        return brls::AppletFrame::onCancel();
    }

    void setPage(const CatalogPage& nextPage, bool focusGrid = true) {
        if (focusGrid) brls::Application::giveFocus(nullptr);
        MiiGrid* previousGrid = grid;
        page = nextPage;
        grid = new MiiGrid(page.entries, onSelect);
        setContentView(grid);
        if (previousGrid != nullptr) {
            previousGrid->willDisappear(true);
            delete previousGrid;
        }

        const size_t pageNumber = page.start / CatalogClient::PAGE_SIZE + 1;
        const size_t pageCount = std::max<size_t>(1,
            (page.total + CatalogClient::PAGE_SIZE - 1) / CatalogClient::PAGE_SIZE);
        const std::string context = page.query.empty() ? CatalogStore::title(page.section) : page.query;
        setTitle(page.query.empty() ? CatalogStore::title(page.section) : "Search Results");
        setSubtitle("InfiniMii", context + " · Page " + std::to_string(pageNumber) +
                    " of " + std::to_string(pageCount));
        setActionAvailable(brls::Key::L, page.start >= CatalogClient::PAGE_SIZE);
        setActionAvailable(brls::Key::R, page.hasMore);
        invalidate();
        if (focusGrid) brls::Application::giveFocus(grid);
    }

    void setNetworkBusy(bool busy) {
        if (grid != nullptr) grid->setThumbnailLoadingEnabled(!busy);
    }

  private:
    CatalogPage page;
    MiiGrid* grid = nullptr;
    PageCallback onPage;
    MiiGrid::SelectCallback onSelect;
    CloseCallback onClose;
    bool closeNotified = false;

    void notifyClosed() {
        if (closeNotified) return;
        closeNotified = true;
        if (onClose) onClose(this);
    }
};

int main(int argc, char* argv[]) {
    brls::Logger::setLogLevel(brls::LogLevel::INFO);

    brls::Style custom_style = brls::Style::horizon();
    custom_style.Sidebar.width = 280;
    custom_style.Sidebar.marginLeft = 55;
    custom_style.Header.height = 44;
    custom_style.Header.fontSize = 20;
    custom_style.Header.rectangleWidth = 5;
    custom_style.List.Item.height = 69;
    custom_style.List.Item.heightWithSubLabel = 92;
    custom_style.List.spacing = 20;

    if (R_FAILED(init()) || !brls::Application::init(TITLE, custom_style, brls::Theme::horizon()))
    {
        brls::Logger::error("Unable to init Borealis application");
        return EXIT_FAILURE;
    }

    brls::TabFrame* rootFrame = new brls::TabFrame();
    rootFrame->setTitle(TITLE);
    rootFrame->setIcon(BOREALIS_ASSET("icon/MiiPort.png"));

    // The category screen remains alive for the entire app session. Results
    // open as a separate page, avoiding focused-view deletion during refreshes.
    TopScrollList* browseList = new TopScrollList();
    CatalogClient catalogClient;
    CatalogTransport downloadTransport;
    std::string pendingDownloadId;
    bool downloading = false;
    bool browseRequestPending = false;
    bool requestTargetsResultsFrame = false;
    CatalogResultsFrame* resultsFrame = nullptr;
    constexpr std::array<CatalogSection, 5> sections = {{
        CatalogSection::Trending, CatalogSection::New, CatalogSection::TopRated,
        CatalogSection::Official, CatalogSection::Random,
    }};
    auto beginBrowseRequest = [&](CatalogSection section, const std::string& query = "",
                                  size_t start = 0, bool fromResultsFrame = false) {
        if (browseRequestPending) {
            brls::Application::notify("Please wait for the current catalog request");
            return;
        }
        if (downloading) {
            brls::Application::notify("Please wait for the current Mii download");
            return;
        }
        if (fromResultsFrame && resultsFrame != nullptr) resultsFrame->setNetworkBusy(true);
        catalogClient.request(section, query, start);
        browseRequestPending = true;
        requestTargetsResultsFrame = fromResultsFrame;
        brls::Application::notify(query.empty() ? "Loading Miis…" : "Searching Miis…");
    };
    auto showMiiDetails = [&](const CatalogMii& mii) {
        std::stringstream details;
        details << mii.name << "\n\nCreator: " << mii.creator << "\nSource: " << mii.source
                << "\nDescription: " << mii.tags << "\nLikes: " << mii.score;
        brls::Dialog* dialog = new brls::Dialog(new brls::Label(brls::LabelStyle::REGULAR, details.str(), true));
        dialog->addButton("Download", [dialog, mii, &downloadTransport, &pendingDownloadId,
                                        &downloading, &browseRequestPending, &resultsFrame](brls::View*) {
            if (browseRequestPending) {
                brls::Application::notify("Please wait for the current catalog request");
                return;
            }
            if (downloading || !downloadTransport.start(mii.downloadUrl)) {
                brls::Application::notify("A download is already in progress");
                return;
            }
            if (resultsFrame != nullptr) resultsFrame->setNetworkBusy(true);
            pendingDownloadId = mii.id;
            downloading = true;
            brls::Application::notify("Downloading one CHARINFO file…");
            dialog->close();
        });
        dialog->addButton("Close", [dialog](brls::View*) { dialog->close(); });
        dialog->open();
    };
    auto openResults = [&] {
        const CatalogPage& page = catalogClient.page();
        if (requestTargetsResultsFrame) {
            if (resultsFrame != nullptr) resultsFrame->setPage(page);
            return;
        }
        resultsFrame = new CatalogResultsFrame(
            [&](CatalogSection section, const std::string& query, size_t start) {
                beginBrowseRequest(section, query, start, true);
            },
            showMiiDetails,
            [&](CatalogResultsFrame* closing) {
                if (resultsFrame == closing) resultsFrame = nullptr;
            });
        resultsFrame->setPage(page, false);
        brls::Application::pushView(resultsFrame, brls::ViewAnimation::SLIDE_LEFT);
    };

    browseList->addView(new brls::Header("Mii Gallery", false));
    browseList->addView(new brls::Label(brls::LabelStyle::REGULAR,
        "Browse InfiniMii on demand. Pages are cached for 10 minutes and nothing is prefetched.", true));
    auto* searchItem = new brls::ListItem("Search Miis", "", "Name, creator, description, or uploader");
    searchItem->getClickEvent()->subscribe([&](brls::View*) {
        if (browseRequestPending) {
            brls::Application::notify("Please wait for the current catalog request");
            return;
        }
        const bool opened = brls::Swkbd::openForText([&](std::string query) {
            if (!query.empty()) beginBrowseRequest(CatalogSection::Trending, query);
        }, "Search Miis", "Searches InfiniMii", CatalogClient::MAX_QUERY_BYTES);
        if (!opened) brls::Application::notify("Could not open the Switch keyboard");
    });
    browseList->addView(searchItem);
    for (CatalogSection section : sections) {
        auto* category = new brls::ListItem(CatalogStore::title(section), "", CatalogStore::subtitle(section));
        category->getClickEvent()->subscribe([&, section](brls::View*) { beginBrowseRequest(section); });
        browseList->addView(category);
    }

    TopScrollList* aboutList = new TopScrollList();

    aboutList->addView(new FocusHeader("About", false));
    aboutList->addView(new brls::Label(brls::LabelStyle::REGULAR, 
    "A tool to import and export Miis in a variety of formats.\n"
    "Supports importing the NFIF, charinfo, coredata and storedata formats.\n"
    "Exports full DBs in NFIF and individual characters in charinfo.\n"
    "Can also import jpeg images of Mii QR codes and generate new QR codes."
    , true));

    aboutList->addView(new FocusHeader("How to use", false));
    aboutList->addView(new brls::Label(brls::LabelStyle::REGULAR, 
    "Place Mii files in \"sd:/MiiPort/miis/\".\n"
    "Give files a file extension that corresponds to their format i.e. \".charinfo\" or \".jpg\".\n"
    "Currently exports to \"sd:/MiiPort/miis/exportedDB.NFIF\" and \"sd:/MiiPort/miis/[name].charinfo\" or \"sd:/MiiPort/miis/[Mii ID].charinfo\" if the name can not be used. This will overwrite an existing file.\n"
    "For cordata files, a Mii ID can be specified in hexadecimal in the file name, otherwise a random one will be used.\n"
    "For example \"7C118DA34ADB46CB8FFC083BD00DC111.coredata\"\n"
    , true));

    #if MII_PORT_ENABLE_QR
    aboutList->addView(new FocusHeader("QR key info", false));
    aboutList->addView(new brls::Label(brls::LabelStyle::REGULAR, 
    "In order to import Miis from a QR code or generate QR codes, you must supply the Mii QR key. This is needed to decrypt the Mii data stored in Mii QR codes.\n\n"
    "You can find this on the internet by searching for \"Mii QR key\" or \"slot0x31KeyN\".\n"
    "This program looks for the key in hex in the first line of the file \"/MiiPort/qrkey.txt\".\n"
    "It will accept it in a variety of formats such as:\n"
    "\"[0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA]\"\n"
    "or \"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\"\n"
    , true));
    #endif

    const fs::path import_path = "/MiiPort/miis";

    FocusList* fileList = new FocusList(true);

    fs::create_directories(import_path);
    std::vector<fs::directory_entry> dirEntVec;
    // iterator does not give unicode paths at all
    std::copy(fs::directory_iterator(import_path), fs::directory_iterator(), std::back_inserter(dirEntVec));
    std::sort(dirEntVec.begin(), dirEntVec.end());
    for(fs::path path: dirEntVec) {
        brls::ListItem* fileItem = new brls::ListItem(path.filename());
        if(path.extension() == ".jpg") {
            fileItem->setThumbnail(path);
        }
        fileItem->getClickEvent()->subscribe([path{std::move(path)}](brls::View* view) {
            Result res = importMiiFile(path);
            errorNotify(res);
        });
        fileList->addView(fileItem);
    }
    if(fileList->getViewsCount() == 0){
        fileList->setAllowFocus(false);
        std::stringstream ss;
        ss << "No mii files.\nAdd files to " << import_path;
        fileList->addView(new brls::Label(brls::LabelStyle::REGULAR, ss.str(), true));
    }
    
    brls::List* exportList = new brls::List();
    brls::ListItem* exportItem = new brls::ListItem("Export Mii database as NFIF");
    exportItem->getClickEvent()->subscribe([import_path](brls::View* view) {
        fs::path path = import_path / "exportedDB.NFIF";
        Result res = miiDbExportToFile(path.c_str());
        if(R_FAILED(res)) {
            errorNotify(res);
        }
        else {
            brls::Application::notify("Exported!");
        }
    });
    exportItem->setTextSize(28);
    exportList->addView(exportItem);
    brls::Label *note = new brls::Label(brls::LabelStyle::REGULAR, "Export individual Miis as charinfo", false);
    exportList->addView(note);

    {
        Result res;
        int count;
        const int max_miis = 100;
        charInfo miis[max_miis];
        res = getCharInfos(miis, max_miis, &count);
        if(R_FAILED(res)) {
            errorNotify(res);
        }
        else {
            for(int i = 0; i < count; i++) {
                std::u16string utf16_name = miis[i].nickname;
                std::string utf8_name = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>{}.to_bytes(utf16_name);
                fs::path export_path;
                // special characters seem poorly supported in paths. 
                // If the name uses any, use create ID for file name instead.
                // compare lengths to check for special characters.
                if(utf16_name.length() == utf8_name.length()) {
                    export_path = import_path / utf8_name += ".charinfo";
                }
                else {
                    export_path = import_path / getHexStr(&miis[i].create_id) += ".charinfo";
                }
                // todo: face icon for each Mii?
                brls::ListItem* miiItem = new brls::ListItem(utf8_name, "",getHexStr(&miis[i].create_id));
                miiItem->getClickEvent()->subscribe(
                [export_path{std::move(export_path)}, mii{miis[i]}]
                (brls::View* view) {
                    // todo: ask before replacing file?
                    errorNotify(writeToFile(export_path.c_str(), &mii), "Exported!");
                });
                #if MII_PORT_ENABLE_QR
                miiItem->registerAction("Show Mii QR", brls::Key::Y, [mii{miis[i]}, name{utf8_name}] {
                    ver3StoreData qr_data;
                    charInfoToVer3StoreData(&mii, &qr_data);
                    Result res = showQrPopup(&qr_data, name);
                    if(R_FAILED(res)) {
                        errorNotify(res);
                    }
                    return true;
                });
                #endif
                exportList->addView(miiItem);
            }
        }
    }

    rootFrame->addTab("Browse", browseList);
    rootFrame->addTab("Import", fileList);
    rootFrame->addTab("Export", exportList);
    rootFrame->addSeparator();
    rootFrame->addTab("About", aboutList);

    // This was not working in applet mode. I think this is a memory issue?
    if(APPLET_TYPE == AppletType_Application || APPLET_TYPE == AppletType_SystemApplication) {
        rootFrame->registerAction("Show Mii applet", brls::Key::X, [] {
            miiLaShowMiiEdit(MiiSpecialKeyCode_Special);
            return true;
        });
    }
    
    brls::Application::pushView(rootFrame);

    while (brls::Application::mainLoop()) {
        if (browseRequestPending) {
            catalogClient.pump();
            if (catalogClient.state() == CatalogRequestState::Ready) {
                browseRequestPending = false;
                openResults();
            } else if (catalogClient.state() == CatalogRequestState::Failed) {
                browseRequestPending = false;
                if (requestTargetsResultsFrame && resultsFrame != nullptr)
                    resultsFrame->setNetworkBusy(false);
                brls::Application::notify(catalogClient.error().empty()
                    ? "The catalog request failed" : catalogClient.error());
            }
        }
        if (downloading) {
            downloadTransport.pump();
            if (downloadTransport.state() != CatalogTransportState::Loading) {
                CatalogHttpResponse response = downloadTransport.takeResponse();
                downloading = false;
                if (resultsFrame != nullptr) resultsFrame->setNetworkBusy(false);
                if (response.status < 200 || response.status >= 300 || !response.error.empty() || response.body.size() != sizeof(charInfo)) {
                    brls::Application::notify("Download failed or returned an invalid CHARINFO file");
                    continue;
                }
                std::string safeId;
                for (unsigned char c : pendingDownloadId) {
                    safeId += (std::isalnum(c) || c == '-' || c == '_') ? static_cast<char>(c) : '_';
                }
                const fs::path downloads = "/MiiPort/miis/Downloads";
                const fs::path destination = downloads / (safeId + ".charinfo");
                const fs::path temporary = downloads / (safeId + ".tmp");
                std::error_code error;
                fs::create_directories(downloads, error);
                FILE* file = error ? nullptr : fopen(temporary.c_str(), "wb");
                const bool written = file != nullptr && fwrite(response.body.data(), 1, response.body.size(), file) == response.body.size();
                if (file != nullptr) fclose(file);
                if (!written) {
                    fs::remove(temporary, error);
                    brls::Application::notify("Could not save the downloaded CHARINFO file");
                    continue;
                }
                fs::remove(destination, error);
                error.clear();
                fs::rename(temporary, destination, error);
                if (error) {
                    fs::remove(temporary, error);
                    brls::Application::notify("Could not finalize the downloaded CHARINFO file");
                    continue;
                }
                errorNotify(miiDbAddOrReplaceCharInfoFromFile(destination.c_str()), "Downloaded and imported!");
            }
        }
    };

    // Exit
    deinit();
    return EXIT_SUCCESS;
}
