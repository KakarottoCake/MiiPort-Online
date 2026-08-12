#include <cstdio>
#include <string>
#include <cstring>
#include <filesystem>
namespace fs = std::filesystem;

#include <switch.h>

#include "mii_ext.h"
#include "convert_mii.h"
#if MII_PORT_ENABLE_QR
#include "mii_qr.hpp"
#endif
#include "errors.h"

void errorCodeNotify(Result res) {
    std::stringstream ss;
    ss << "Error: 0x" << std::hex << res;
    brls::Application::notify(ss.str());
}

void errorNotify(Result res, std::string success_message = "Imported!") {
    switch(res) {
        case 0: {
            brls::Application::notify(success_message);
            break;
        }
        case 0xa7e: {
            brls::Application::notify("Mii database is full");
            break;
        }
        // bad storedata format
        case 0xda7e:
        // bad nfif format
        case 0xe07e: {
            brls::Application::notify("Improper file format");
            break;
        }
        // debug only
        case 0x1987e: {
            brls::Application::notify("Must enable DB testing mode");
            break;
        }
        case SHOWING_POPUP: {
            break;
        }
        case UNSUPPORTED_EXT: {
            brls::Application::notify("File extension not recognized");
            break;
        }
        case NO_QR: {
            brls::Application::notify("No QR code found");
            break;
        }
        case QR_DECODE_FAIL: {
            brls::Application::notify("QR decoding failed");
            break;
        }
        case AES_CCM_FAILED: {
            brls::Application::notify("Mii data decryption failed");
            break;
        }
        case JPEG_DECODE_FAIL: {
            brls::Application::notify("Jpeg decoding failed");
            break;
        }
        case BAD_KEY_FILE: {
            brls::Application::notify("Incorrect Mii QR key.\nSee \"About\" tab.");
            break;
        }
        case MISSING_KEY_FILE: {
            brls::Application::notify("qrkey.txt not found.\nSee \"About\" tab.");
            break;
        }
        case FILE_READ_FAILED: {
            brls::Application::notify("Could not read the selected file");
            break;
        }
        case FILE_WRITE_FAILED: {
            brls::Application::notify("Could not write the selected file");
            break;
        }
        case INVALID_FILE_SIZE: {
            brls::Application::notify("File has an invalid size for this Mii format");
            break;
        }
        default: {
            errorCodeNotify(res);
            break;
        }
    }
}

template <typename T>
Result readFromFile(const char *path, T *out) {
    FILE* file = fopen(path, "rb");
    if(file == nullptr) {
        printf("File open error: %d\n", errno);
        return FILE_READ_FAILED;
    }

    const size_t size_read = fread(out, 1, sizeof(T), file);
    // Every supported binary Mii format has a fixed size. Reject both
    // truncated files and trailing data rather than importing a partial record.
    const bool has_extra_data = fgetc(file) != EOF;
    fclose(file);
    if (size_read != sizeof(T) || has_extra_data) return INVALID_FILE_SIZE;

    return 0;
}
template <typename T>
Result writeToFile(const char *path, const T *out) {
    FILE* file = fopen(path, "wb");
    if(file == nullptr) {
        printf("File open error: %d\n", errno);
        return FILE_WRITE_FAILED;
    }

    const size_t size_written = fwrite(out, 1, sizeof(T), file);
    fclose(file);
    return size_written == sizeof(T) ? 0 : FILE_WRITE_FAILED;
}

template <typename T>
std::string getHexStr(T *data) {
    std::stringstream ss;
    ss << std::uppercase << std::hex;
    for(u64 i = 0; i<sizeof(T); i++) {
        ss << std::setw(2) << std::setfill('0') << (u32)((u8*)data)[i];
    }
    return ss.str();
}

void stringToLower(std::string *str) {
    std::transform(str->begin(), str->end(), str->begin(),
        [](unsigned char c){ return std::tolower(c); });
}

int strToCreateId(const std::string& hex, MiiCreateId *id) {
    for (unsigned int i = 0; i < sizeof(MiiCreateId); i++) {
        const char* byteStr = hex.substr(i*2, 2).c_str();
        char* end = nullptr;
        u8 byte = (u8) strtol(byteStr, &end, 16);
        if (byteStr == end) return false;
        id->uuid.uuid[i] = byte;
    }
    return true;
}

Result addOrReplaceStoreData(const storeData *input) {
    MiiDatabase DbService;
    Result res;
    res = miiOpenDatabase(&DbService, MiiSpecialKeyCode_Special);
    if(R_FAILED(res)) return res;
    res = miiDatabaseAddOrReplace(&DbService, input);
    miiDatabaseClose(&DbService);
    return res;
}

void showDupeCreateIDPopup(storeData *input){
    brls::Dialog* dialog = new brls::Dialog("A Mii with the same Mii ID already exists on your switch.");

    brls::GenericEvent::Callback repalceCallback = [dialog, input{*input}](brls::View* view) {
        Result res = addOrReplaceStoreData(&input);

        errorNotify(res);
        dialog->close();
    };
    brls::GenericEvent::Callback randomCallback = [dialog, input{*input}](brls::View* view) mutable {
        makeRandCreateId(&input.create_id);
        // changed data, so re-generate storedata hashes
        setStoreDataCrc16(&input);
        Result res = addOrReplaceStoreData(&input);

        errorNotify(res);
        dialog->close();
    };

    dialog->addButton("Replace", repalceCallback);
    dialog->addButton("Use Random Mii ID", randomCallback);

    dialog->setCancelable(true);

    dialog->open();
}

Result addOrReplaceStoreDataWithPrompt(storeData *input) {
    MiiDatabase DbService;
    Result res;
    int idx;
    res = miiOpenDatabase(&DbService, MiiSpecialKeyCode_Special);
    if(R_FAILED(res)) return res;
    res = miiDatabaseFindIndex(&DbService, &input->create_id, true, &idx);
    if(R_FAILED(res)) return res;
    // duplicate create ID found
    if(idx != -1) {
        showDupeCreateIDPopup(input);
        res = SHOWING_POPUP;
    }
    else {
        res = miiDatabaseAddOrReplace(&DbService, input);
    }
    miiDatabaseClose(&DbService);
    return res;
}

Result exportNFIF(NFIF *out) {
    MiiDatabase DbService;
    Result res;
    res = miiOpenDatabase(&DbService, MiiSpecialKeyCode_Special);
    if(R_FAILED(res)) return res;
    res = miiDatabaseExport(&DbService, out);
    miiDatabaseClose(&DbService);
    return res;
}

Result importNFIF(NFIF *input) {
    MiiDatabase DbService;
    Result res;
    res = miiOpenDatabase(&DbService, MiiSpecialKeyCode_Special);
    if(R_FAILED(res)) return res;
    res = miiDatabaseImport(&DbService, input);
    miiDatabaseClose(&DbService);
    return res;
}

Result getCharInfos(charInfo *out_array, int size, int *out_size) {
    MiiDatabase DbService;
    Result res;
    res = miiOpenDatabase(&DbService, MiiSpecialKeyCode_Special);
    if(R_FAILED(res)) return res;
    res = miiDatabaseGet1(&DbService, MiiSourceFlag_Database, (MiiCharInfo*)out_array, size, out_size);
    miiDatabaseClose(&DbService);
    return res;
}

Result miiDbExportToFile(const char* file_path) {
    NFIF Db;
    Result res = exportNFIF(&Db);
    if(R_FAILED(res)) return res;
    return writeToFile(file_path, &Db);
}

Result miiDbImportFromFile(const char* file_path) {
    NFIF Db;
    Result res = readFromFile(file_path, &Db);
    if (R_FAILED(res)) return res;
    return importNFIF(&Db);
}

Result miiDbAddOrReplaceStoreDataFromFile(const char* file_path) {
    storeData in_data;
    Result res = readFromFile(file_path, &in_data);
    if (R_FAILED(res)) return res;
    // run this to regenerate checksums
    setStoreDataCrc16(&in_data);
    return addOrReplaceStoreDataWithPrompt(&in_data);
}

Result miiDbAddOrReplaceCoreDataFromFile(const char* file_path) {
    coreData in_data;
    storeData new_data;
    MiiCreateId id;

    Result res = readFromFile(file_path, &in_data);
    if (R_FAILED(res)) return res;
    
    // get createID from file name, or use a random one
    std::string filename = fs::path(file_path).filename().string();
    if(!strToCreateId(filename, &id)) {
        printf("using random ID\n");
        makeRandCreateId(&id);
    }

    printf("Create ID: ");
    for(u64 i = 0; i<sizeof(MiiCreateId); i++) {
        printf("%02X", id.uuid.uuid[i]);
    }
    printf("\n");
    
    coreDataToStoreData(&in_data, &id, &new_data);
    return addOrReplaceStoreDataWithPrompt(&new_data);
}

Result miiDbAddOrReplaceCharInfoFromFile(const char* file_path) {
    charInfo in_data;
    coreData intermediate;
    storeData new_data;
    MiiCreateId id;

    Result res = readFromFile(file_path, &in_data);
    if (R_FAILED(res)) return res;

    charInfoToCoreData(&in_data, &intermediate, &id);
    coreDataToStoreData(&intermediate, &id, &new_data);

    return addOrReplaceStoreDataWithPrompt(&new_data);
}

#if MII_PORT_ENABLE_QR
Result showQrPopup(ver3StoreData* data, std::string name) {
    int qr_width = 0;
    std::unique_ptr<u32[]> qr_RGBA;
    Result res = generateMiiQr(data, 8, &qr_width, qr_RGBA);
    if(R_FAILED(res)) {
        return res;
    }
    brls::Image *qr_image = new brls::Image;
    qr_image->setScaleType(brls::ImageScaleType::NO_RESIZE);
    qr_image->setImageRGBA((u8*)qr_RGBA.get(), qr_width, qr_width);
    brls::AppletFrame* frame = new brls::AppletFrame(0,0);
    frame->setContentView(qr_image);
    brls::PopupFrame::open("QR Code", frame, "", name);
    return 0;
}

Result importMiiQr(const char* path) {
    ver3StoreData ver3mii;
    storeData mii;
    Result res;
    res = parseMiiQr(path, &ver3mii);
    if(R_FAILED(res)) {
        return res;
    }
    ver3StoreDataToStoreData(&ver3mii, &mii);
    return addOrReplaceStoreData(&mii);
}
#endif

Result importMiiFile(fs::path file_path) {
    std::string ext = file_path.extension().string();
    stringToLower(&ext);
    Result res = 0;
    
    if(ext == ".charinfo" || ext == ".bin") {
        res = miiDbAddOrReplaceCharInfoFromFile(file_path.c_str());
    }
    else if(ext == ".nfif" || ext == ".dat") {
        res = miiDbImportFromFile(file_path.c_str());
    }
    else if(ext == ".coredata") {
        res = miiDbAddOrReplaceCoreDataFromFile(file_path.c_str());
    }
    else if(ext == ".storedata") {
        res = miiDbAddOrReplaceStoreDataFromFile(file_path.c_str());
    }
    #if MII_PORT_ENABLE_QR
    else if(ext == ".jpg" || ext == ".jpeg") {
        res = importMiiQr(file_path.c_str());
    }
    #endif
    else {
        return UNSUPPORTED_EXT;
    }
    return res;
}
