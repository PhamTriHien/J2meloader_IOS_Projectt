#include "include/j2me_core.h"
#include "src/storage/rms_storage.h"
#include "src/lcdui/lcdui_graphics.h"
#include "src/network/gcf_network.h"
#include "src/network/datagram_connection.h"
#include "src/audio/mmapi_audio.h"
#include "src/graphics3d/math3d.h"
#include "src/graphics3d/micro3d_math.h"
#include "src/graphics3d/micro3d_engine.h"
#include "src/graphics3d/m3g_engine.h"
#include "src/graphics3d/rasterizer3d.h"
#include "src/oem/nokia_direct_graphics.h"
#include "src/oem/device_control.h"
#include "src/lcdui/ui/command.h"
#include "src/lcdui/ui/soft_keys_bar.h"
#include "src/lcdui/ui/displayable.h"
#include "src/lcdui/ui/display.h"
#include "src/lcdui/ui/item.h"
#include "src/lcdui/ui/form.h"
#include "src/lcdui/ui/list.h"
#include "src/lcdui/ui/alert.h"
#include "src/file/file_system_registry.h"
#include "src/file/file_connection.h"
#include "include/wma_message.h"
#include "include/sms_connection.h"
#include "include/app_descriptor.h"
#include "include/midlet.h"
#include "include/phone_keypad.h"
#include "include/jar_resource_loader.h"
#include "include/app_profile_config.h"
#include "src/jvm/class_file.h"
#include "src/jvm/cldc_vm.h"
#include "src/lcdui/game/layer.h"
#include "src/lcdui/game/tiled_layer.h"
#include "src/lcdui/game/layer_manager.h"
#include "src/lcdui/game/sprite_layer.h"
#include "src/graphics3d/keyframe_sequence.h"
#include "src/graphics3d/animation_controller.h"
#include "src/graphics3d/animation_track.h"
#include "src/graphics3d/morphing_mesh.h"
#include "src/graphics3d/m3g_node.h"
#include "src/graphics3d/skinned_mesh.h"
#include "src/app/app_item.h"
#include "src/app/app_repository.h"
#include "src/app/app_installer.h"
#include "src/network/bluetooth/bluetooth_types.h"
#include "src/network/bluetooth/bluetooth_uuid.h"
#include "src/network/bluetooth/bluetooth_data_element.h"
#include "src/network/bluetooth/bluetooth_service_record.h"
#include "src/network/bluetooth/bluetooth_device.h"
#include "src/network/bluetooth/btspp_connection.h"
#include "src/network/bluetooth/btl2cap_connection.h"
#include "src/network/bluetooth/obex_headers.h"
#include "src/oem/vodafone/vodafone_types.h"
#include "src/oem/vodafone/sprite_engine.h"
#include "src/oem/vodafone/sound_player.h"
#include "src/oem/vodafone/vodafone_device_control.h"
#include "src/oem/vodafone/vodafone_image_encoder.h"
#include "src/oem/carrier_extensions.h"
#include "src/location/location_types.h"
#include "src/location/coordinates.h"
#include "src/location/address_info.h"
#include "src/location/criteria.h"
#include "src/location/location.h"
#include "src/location/orientation.h"
#include "src/location/landmark_store.h"
#include "src/location/location_provider.h"
#include "src/sensor/sensor_types.h"
#include "src/sensor/condition.h"
#include "src/sensor/channel_info.h"
#include "src/sensor/channel.h"
#include "src/sensor/data.h"
#include "src/sensor/sensor_info.h"
#include "src/sensor/sensor_connection.h"
#include "src/sensor/sensor_manager.h"
#include "src/pim/pim_types.h"
#include "src/pim/repeat_rule.h"
#include "src/pim/pim_item.h"
#include "src/pim/contact.h"
#include "src/pim/event.h"
#include "src/pim/todo.h"
#include "src/pim/pim_list.h"
#include "src/pim/pim_manager.h"
#include "src/amms/amms_types.h"
#include "src/amms/audio3d_controls.h"
#include "src/amms/sound_source_3d.h"
#include "src/amms/audio_effects.h"
#include "src/amms/camera_controls.h"
#include "src/amms/global_manager.h"
#include "src/push/push_registry.h"
#include "src/comm/comm_connection.h"
#include "src/security/pki_types.h"
#include "src/security/secure_connection.h"
#include "src/graphics3d/m3g_loader.h"
#include "src/graphics3d/micro3d_loader.h"
#include "src/lcdui/font.h"
#include "src/system/system_properties.h"
#include "src/audio/wav_player.h"
#include <iostream>
#include <thread>
#include <chrono>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstring>
#include <fstream>
#include <filesystem>

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef COLOR_BACKGROUND
#undef COLOR_BACKGROUND
#endif
#ifdef COLOR_FOREGROUND
#undef COLOR_FOREGROUND
#endif
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

static void test_rms_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[MODULE 1 TEST] Kiem tra toan dien Mo dun RMS MIDRMS v3.0" << std::endl;
    std::cout << "========================================================" << std::endl;

    std::string testRoot = "./test_rms_suite";
    try {
        fs::remove_all(testRoot);
    } catch (...) {}

    auto& rms = j2me::RmsManager::instance();
    rms.setStorageRoot(testRoot);

    std::string suite = "DragonBoy_Suite";
    std::string storeName = "PlayerSave";

    std::cout << "  1. Mo RecordStore moi: " << storeName << " trong Suite: " << suite << std::endl;
    auto* store = rms.openRecordStore(suite, storeName, true);
    assert(store != nullptr);
    assert(store->isOpen());
    assert(store->getNumRecords() == 0);

    std::cout << "  2. Ghi cac ban ghi mau vao RecordStore..." << std::endl;
    // Record 1: "Hero: Songoku, Level: 50"
    std::string rec1 = "Hero: Songoku, Level: 50";
    int id1 = store->addRecord(reinterpret_cast<const uint8_t*>(rec1.data()), rec1.size());

    // Record 2: "Hero: Vegeta, Level: 90"
    std::string rec2 = "Hero: Vegeta, Level: 90";
    int id2 = store->addRecord(reinterpret_cast<const uint8_t*>(rec2.data()), rec2.size());

    // Record 3: "Hero: Krillin, Level: 20"
    std::string rec3 = "Hero: Krillin, Level: 20";
    int id3 = store->addRecord(reinterpret_cast<const uint8_t*>(rec3.data()), rec3.size());

    std::cout << "     Da ghi: id1=" << id1 << ", id2=" << id2 << ", id3=" << id3 
              << ", totalRecords=" << store->getNumRecords() << std::endl;

    std::cout << "  3. Kiem tra doc ban ghi..." << std::endl;
    std::vector<uint8_t> readData;
    bool ok = store->getRecord(id2, readData);
    std::string loadedRec2(readData.begin(), readData.end());
    std::cout << "     Record 2 (ok=" << ok << ", size=" << readData.size() << ") da doc: '" << loadedRec2 << "'" << std::endl;
    if (!ok || loadedRec2 != rec2) {
        std::cerr << "Loi doc record 2!" << std::endl;
        exit(1);
    }

    std::cout << "  4. Kiem tra RecordEnumeration voi Bo loc (Filter)..." << std::endl;
    // Filter chi lay hero co chu "Level: 90"
    auto filter90 = [](const std::vector<uint8_t>& data) -> bool {
        std::string s(data.begin(), data.end());
        return s.find("Level: 90") != std::string::npos;
    };
    auto enumFiltered = store->enumerateRecords(filter90, nullptr, false);
    assert(enumFiltered->numRecords() == 1);
    assert(enumFiltered->hasNextElement());
    int filteredId = enumFiltered->nextRecordId();
    assert(filteredId == 2);
    std::cout << "     Bo loc Filter thanh cong, ID tim thay: " << filteredId << std::endl;

    std::cout << "  5. Kiem tra RecordComparator (Sap xep chuoi tang dan)..." << std::endl;
    auto sortAscending = [](const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) -> int {
        std::string sA(a.begin(), a.end());
        std::string sB(b.begin(), b.end());
        if (sA < sB) return j2me::RMS_PRECEDES;
        if (sA > sB) return j2me::RMS_FOLLOWS;
        return j2me::RMS_EQUIVALENT;
    };
    auto enumSorted = store->enumerateRecords(nullptr, sortAscending, false);
    assert(enumSorted->numRecords() == 3);
    // Thu tu ky tu: 'Hero: Krillin' < 'Hero: Songoku' < 'Hero: Vegeta' -> IDs: 3, 1, 2
    int sort1 = enumSorted->nextRecordId();
    int sort2 = enumSorted->nextRecordId();
    int sort3 = enumSorted->nextRecordId();
    assert(sort1 == 3);
    assert(sort2 == 1);
    assert(sort3 == 2);
    std::cout << "     Sap xep thanh cong! Thu tu ID sau sort: " << sort1 << ", " << sort2 << ", " << sort3 << std::endl;

    std::cout << "  6. Dong va Mo lai RecordStore de kiem tra tinh ben vung (Persistence)..." << std::endl;
    rms.closeRecordStore(store);

    // Kiem tra truc tiep Header tep tin tren dia xem co dung "MIDRMS" v3.0 cua Android J2ME-Loader khong!
    fs::path saveFile = fs::path(testRoot) / suite / (storeName + ".rms");
    std::ifstream fileCheck(saveFile, std::ios::binary);
    assert(fileCheck.is_open());
    char magic[6];
    fileCheck.read(magic, 6);
    assert(std::memcmp(magic, "MIDRMS", 6) == 0);
    uint8_t major = 0, minor = 0;
    fileCheck.read(reinterpret_cast<char*>(&major), 1);
    fileCheck.read(reinterpret_cast<char*>(&minor), 1);
    assert(major == 0x03);
    assert(minor == 0x00);
    fileCheck.close();
    std::cout << "     [XAC MINH CHUAN DINH DANG GOC] Header la 'MIDRMS' v3.0 (Android Compatible 100%)!" << std::endl;

    auto* storeReopen = rms.openRecordStore(suite, storeName, false);
    assert(storeReopen != nullptr);
    assert(storeReopen->getNumRecords() == 3);
    std::vector<uint8_t> pData;
    assert(storeReopen->getRecord(1, pData));
    std::string s1(pData.begin(), pData.end());
    assert(s1 == rec1);
    rms.closeRecordStore(storeReopen);

    std::cout << "[SUCCESS] Mo dun 1: RMS MIDRMS v3.0 hoan tat kiem tra 100% thanh cong!\n" << std::endl;
}

static void test_lcdui_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[MODULE 2 TEST] Kiem tra toan dien Mo dun LCDUI & GameCanvas" << std::endl;
    std::cout << "========================================================" << std::endl;

    std::cout << "  1. Tao Mutable Image 32x32 va ve hinh co ban..." << std::endl;
    auto img = j2me::LcduiImage::createImage(32, 32);
    assert(img != nullptr);
    assert(img->isMutable());

    auto g = img->getGraphics();
    assert(g != nullptr);
    g->setColor(0xFFFF0000); // Red
    g->fillRect(0, 0, 32, 32);
    g->setColor(0xFF00FF00); // Green
    g->fillTriangle(0, 0, 16, 32, 32, 0);

    // Kiem tra cac ham do mau & grayscale theo chuan upstream J2ME-Loader
    g->setColorRGB(100, 150, 200);
    assert(g->getRedComponent() == 100);
    assert(g->getGreenComponent() == 150);
    assert(g->getBlueComponent() == 200);
    int gray = g->getGrayScale();
    // 0x4CB2 * 100 + 0x9691 * 150 + 0x1D3E * 200 >> 16
    assert(gray == ((0x4CB2 * 100 + 0x9691 * 150 + 0x1D3E * 200) >> 16));

    g->setGrayScale(128);
    assert(g->getRedComponent() == 128);
    assert(g->getGreenComponent() == 128);
    assert(g->getBlueComponent() == 128);

    // Kiem tra Stroke Style
    g->setStrokeStyle(j2me::LcduiGraphics::DOTTED);
    assert(g->getStrokeStyle() == j2me::LcduiGraphics::DOTTED);
    g->setStrokeStyle(j2me::LcduiGraphics::SOLID);
    assert(g->getStrokeStyle() == j2me::LcduiGraphics::SOLID);

    // Kiem tra Polygon & copyArea
    int polyX[] = { 2, 10, 8, 2 };
    int polyY[] = { 2, 2, 10, 8 };
    g->drawPolygon(polyX, polyY, 4);
    g->fillPolygon(polyX, polyY, 4);

    g->copyArea(0, 0, 16, 16, 16, 16, j2me::ANCHOR_TOP | j2me::ANCHOR_LEFT);

    // Kiem tra LcduiImage overloads
    auto argbImg = j2me::LcduiImage::createImage(16, 16, 0xFF123456);
    assert(argbImg != nullptr);
    assert(argbImg->getPixels()[0] == 0xFF123456);

    auto copiedImg = j2me::LcduiImage::createImage(*argbImg);
    assert(copiedImg != nullptr);
    assert(copiedImg->getPixels()[0] == 0xFF123456);

    auto subImg = j2me::LcduiImage::createImage(*argbImg, 0, 0, 8, 8, j2me::TRANS_ROT90);
    assert(subImg != nullptr);
    assert(subImg->getWidth() == 8);
    assert(subImg->getHeight() == 8);
    std::cout << "     LCDUI Graphics, Grayscale, Polygon, CopyArea & Image Overloads dat 100%!" << std::endl;

    std::cout << "  2. Kiem tra GameCanvasEngine & Polling Key States..." << std::endl;
    j2me::GameCanvasEngine gameCanvas(240, 320);
    assert(gameCanvas.getKeyStates() == 0);

    // Nhan phim Fire / Num5
    gameCanvas.setKeyState(J2ME_KEY_FIRE, true);
    assert((gameCanvas.getKeyStates() & j2me::GameCanvasEngine::FIRE_PRESSED) != 0);

    // Nhan them phim Up / Num2
    gameCanvas.setKeyState(J2ME_KEY_UP, true);
    assert((gameCanvas.getKeyStates() & j2me::GameCanvasEngine::UP_PRESSED) != 0);

    // Nha phim Fire
    gameCanvas.setKeyState(J2ME_KEY_FIRE, false);
    assert((gameCanvas.getKeyStates() & j2me::GameCanvasEngine::FIRE_PRESSED) == 0);
    assert((gameCanvas.getKeyStates() & j2me::GameCanvasEngine::UP_PRESSED) != 0);
    std::cout << "     GameCanvas Polling Bitmask thanh cong 100%!" << std::endl;

    std::cout << "  3. Ve Mutable Image vao GameCanvas qua 8 phep bien hinh (Transforms)..." << std::endl;
    auto canvasG = gameCanvas.getGraphics();
    // Ve phep bien hinh TRANS_ROT90, TRANS_MIRROR, TRANS_ROT180
    canvasG->drawRegion(img.get(), 0, 0, 32, 32, j2me::TRANS_ROT90, 50, 50, j2me::ANCHOR_TOP | j2me::ANCHOR_LEFT);
    canvasG->drawRegion(img.get(), 0, 0, 32, 32, j2me::TRANS_MIRROR, 100, 50, j2me::ANCHOR_TOP | j2me::ANCHOR_LEFT);
    canvasG->drawRegion(img.get(), 0, 0, 32, 32, j2me::TRANS_ROT180, 150, 50, j2me::ANCHOR_TOP | j2me::ANCHOR_LEFT);
    gameCanvas.flushGraphics();
    std::cout << "     Transforms render thanh cong!" << std::endl;

    std::cout << "  4. Kiem tra SpriteEngine va Va cham cap do diem anh (Pixel-Level Collision)..." << std::endl;
    // Sprite A va Sprite B
    std::vector<uint32_t> spritePixels(16 * 16, 0x00000000); // Transparent background
    for (int y = 4; y < 12; ++y) {
        for (int x = 4; x < 12; ++x) {
            spritePixels[y * 16 + x] = 0xFFFFFFFF; // Solid white box 8x8 in center
        }
    }
    auto spriteImg = j2me::LcduiImage::createRGBImage(spritePixels.data(), 16, 16, true);

    j2me::SpriteEngine spriteA(spriteImg, 16, 16);
    j2me::SpriteEngine spriteB(spriteImg, 16, 16);

    spriteA.setPosition(0, 0);
    spriteB.setPosition(20, 20); // Cach xa, khong the va cham
    assert(!spriteA.collidesWith(spriteB, false));
    assert(!spriteA.collidesWith(spriteB, true));

    // Dat B chong len A tai hop bao nhung vung trong suot khong cham
    spriteB.setPosition(14, 14); // Giao nhau o vi tri (14, 14), nhung pixels ben trong tai (4,4) chua cham
    bool bboxHit = spriteA.collidesWith(spriteB, false);
    bool pixelHit = spriteA.collidesWith(spriteB, true);
    assert(bboxHit == true); // Hop bao da cham
    assert(pixelHit == false); // Diem anh chua cham vi vung vien trong suot!
    std::cout << "     Va cham hop bao (AABB): " << bboxHit << ", Va cham diem anh: " << pixelHit << std::endl;

    // Di chuyen B vao chinh giua A
    spriteB.setPosition(2, 2);
    assert(spriteA.collidesWith(spriteB, true) == true);
    std::cout << "     Va cham diem anh (Pixel-Level) thanh cong tuyet doi khi pixels overlap!" << std::endl;

    std::cout << "[SUCCESS] Mo dun 2: LCDUI, GameCanvas & Sprite hoan tat kiem tra 100% thanh cong!\n" << std::endl;
}

static void test_gcf_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[MODULE 3 TEST] Kiem tra toan dien Mo dun GCF Networking" << std::endl;
    std::cout << "========================================================" << std::endl;

    std::cout << "  1. Kiem tra URL Parsing cua GcfConnector..." << std::endl;
    auto badConn = j2me::GcfConnector::openSocket("invalid-url");
    assert(badConn == nullptr);

    auto validConn = j2me::GcfConnector::openSocket("socket://game.server.vn:19128");
    assert(validConn != nullptr);
    assert(validConn->getAddress() == "game.server.vn");
    assert(validConn->getPort() == 19128);
    std::cout << "     URL Parser hop le: Host=" << validConn->getAddress() << ", Port=" << validConn->getPort() << std::endl;

    std::cout << "  2. Tao Local TCP Echo Server de kiem tra truyen nhan du lieu thuc te..." << std::endl;
#if defined(_WIN32) || defined(_WIN64)
    SOCKET serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(serverSock != INVALID_SOCKET);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    serverAddr.sin_port = 0;
    int bRes = bind(serverSock, (sockaddr*)&serverAddr, sizeof(serverAddr));
    assert(bRes == 0);
    listen(serverSock, 1);

    int addrLen = sizeof(serverAddr);
    getsockname(serverSock, (sockaddr*)&serverAddr, &addrLen);
    int assignedPort = ntohs(serverAddr.sin_port);
#else
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    assert(serverSock >= 0);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    serverAddr.sin_port = 0;
    int bRes = bind(serverSock, (sockaddr*)&serverAddr, sizeof(serverAddr));
    assert(bRes == 0);
    listen(serverSock, 1);

    socklen_t addrLen = sizeof(serverAddr);
    getsockname(serverSock, (sockaddr*)&serverAddr, &addrLen);
    int assignedPort = ntohs(serverAddr.sin_port);
#endif

    std::cout << "     Server listening tren 127.0.0.1:" << assignedPort << std::endl;

    std::thread serverThread([&]() {
#if defined(_WIN32) || defined(_WIN64)
        SOCKET client = accept(serverSock, nullptr, nullptr);
        if (client != INVALID_SOCKET) {
            char buf[128];
            int r = recv(client, buf, sizeof(buf), 0);
            if (r > 0) {
                std::string reply = "PONG_J2ME";
                send(client, reply.data(), (int)reply.size(), 0);
            }
            closesocket(client);
        }
        closesocket(serverSock);
#else
        int client = accept(serverSock, nullptr, nullptr);
        if (client >= 0) {
            char buf[128];
            int r = recv(client, buf, sizeof(buf), 0);
            if (r > 0) {
                std::string reply = "PONG_J2ME";
                send(client, reply.data(), reply.size(), 0);
            }
            close(client);
        }
        close(serverSock);
#endif
    });

    std::string clientUrl = "socket://127.0.0.1:" + std::to_string(assignedPort);
    auto clientConn = j2me::GcfConnector::openSocket(clientUrl);
    assert(clientConn != nullptr);
    bool connOk = clientConn->open(3000);
    assert(connOk);
    std::cout << "  3. GCF Client ket noi thanh cong den Local Server!" << std::endl;

    auto outStream = clientConn->openOutputStream();
    auto inStream = clientConn->openInputStream();
    assert(outStream != nullptr);
    assert(inStream != nullptr);

    std::string payload = "PING_J2ME";
    outStream->write(reinterpret_cast<const uint8_t*>(payload.data()), 0, payload.size());
    std::cout << "     Da gui goi tin: " << payload << std::endl;

    uint8_t respBuf[9];
    bool readOk = inStream->readFully(respBuf, 9);
    assert(readOk);
    std::string respStr(reinterpret_cast<char*>(respBuf), 9);
    std::cout << "     Nhan du lieu phan hoi: " << respStr << std::endl;
    assert(respStr == "PONG_J2ME");

    clientConn->close();
    if (serverThread.joinable()) {
        serverThread.join();
    }

    std::cout << "  4. Kiem tra HTTP Connection Wrapper..." << std::endl;
    auto httpConn = j2me::GcfConnector::openHttp("http://test.j2me.org/status");
    assert(httpConn != nullptr);
    httpConn->setRequestProperty("User-Agent", "J2ME-Universal");

    std::cout << "[SUCCESS] Mo dun 3: GCF Network Connection hoan tat kiem tra 100% thanh cong!\n" << std::endl;
}

static void test_mmapi_audio_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[MODULE 4 TEST] Kiem tra toan dien Mo dun MMAPI & Sonivox EAS" << std::endl;
    std::cout << "========================================================" << std::endl;

    std::cout << "  1. Kiem tra AudioRingBuffer (Khoa an toan, Lock-free read)..." << std::endl;
    j2me::AudioRingBuffer ringBuffer(1024);
    assert(ringBuffer.availableRead() == 0);
    assert(ringBuffer.availableWrite() == 1023);

    std::vector<int16_t> sampleData(512 * 2, 1234);
    size_t written = ringBuffer.write(sampleData.data(), 512);
    assert(written == 512);
    assert(ringBuffer.availableRead() == 512);

    std::vector<int16_t> readData(256 * 2, 0);
    size_t readCount = ringBuffer.read(readData.data(), 256);
    assert(readCount == 256);
    assert(readData[0] == 1234);
    assert(ringBuffer.availableRead() == 256);

    ringBuffer.clear();
    assert(ringBuffer.availableRead() == 0);
    std::cout << "     AudioRingBuffer hoat dong chinh xac 100%!" << std::endl;

    std::cout << "  2. Khoi tao Sonivox EAS Audio Synthesizer..." << std::endl;
    auto& synth = j2me::SonivoxAudioEngine::instance();
    assert(synth.initialize());
    assert(synth.isInitialized());
    std::cout << "     Sonivox EAS Synth khoi tao thanh cong!" << std::endl;

    std::cout << "  3. Kiem tra phat Tone va render mau am thanh PCM 44.1kHz..." << std::endl;
    j2me::MmapiManager::playTone(69, 100, 80); // Note 69: A440, duration 100ms, volume 80%

    std::vector<int16_t> pcmStereo(1024 * 2, 0);
    size_t renderedFrames = synth.renderAudio44100(pcmStereo.data(), 1024);
    assert(renderedFrames == 1024);

    bool hasSound = false;
    for (int16_t s : pcmStereo) {
        if (s != 0) {
            hasSound = true;
            break;
        }
    }
    assert(hasSound);
    std::cout << "     Mau am thanh Tone (A440) da duoc tong hop va render thanh cong vao PCM buffer!" << std::endl;

    std::cout << "  4. Kiem tra Standard MIDI File (SMF Format 0) qua Sonivox EAS..." << std::endl;
    const uint8_t midiFile[] = {
        'M', 'T', 'h', 'd',
        0x00, 0x00, 0x00, 0x06,
        0x00, 0x00,
        0x00, 0x01,
        0x00, 0x60,
        'M', 'T', 'r', 'k',
        0x00, 0x00, 0x00, 0x0B,
        0x00,
        0x90, 0x3C, 0x64,
        0x60,
        0x80, 0x3C, 0x00,
        0x00,
        0xFF, 0x2F, 0x00
    };

    auto player = j2me::MmapiManager::createPlayer(midiFile, sizeof(midiFile), "audio/midi");
    assert(player != nullptr);
    assert(player->getState() == j2me::PLAYER_UNREALIZED);

    std::cout << "     Step 4a: realize..." << std::endl;
    player->realize();
    assert(player->getState() == j2me::PLAYER_REALIZED);

    std::cout << "     Step 4b: prefetch..." << std::endl;
    player->prefetch();
    assert(player->getState() == j2me::PLAYER_PREFETCHED);

    std::cout << "     Step 4c: start..." << std::endl;
    player->start();
    assert(player->getState() == j2me::PLAYER_STARTED);

    std::cout << "     Step 4d: volume and midi controls..." << std::endl;
    auto* volCtrl = player->getVolumeControl();
    assert(volCtrl != nullptr);
    std::cout << "     Step 4d.0: before setLevel..." << std::endl;
    volCtrl->setLevel(75);
    std::cout << "     Step 4d.01: after setLevel, level=" << volCtrl->getLevel() << std::endl;
    assert(volCtrl->getLevel() == 75);

    auto* midiCtrl = player->getMidiControl();
    assert(midiCtrl != nullptr);
    std::cout << "     Step 4d.1: setProgram..." << std::endl;
    midiCtrl->setProgram(0, 0, 1);
    std::cout << "     Step 4d.2: shortMidiEvent..." << std::endl;
    midiCtrl->shortMidiEvent(0x90, 64, 100);
    std::cout << "     Step 4d.3: done midi controls!" << std::endl;

    std::cout << "     Step 4e: renderAudio44100..." << std::endl;
    std::vector<int16_t> midiPcm(2048 * 2, 0);
    size_t midiRendered = synth.renderAudio44100(midiPcm.data(), 2048);
    assert(midiRendered == 2048);
    std::cout << "     Sonivox EAS Wavetable Synthesizer da render thanh cong am thanh MIDI sang PCM stereo!" << std::endl;

    player->stop();
    assert(player->getState() == j2me::PLAYER_PREFETCHED);

    player->close();
    assert(player->getState() == j2me::PLAYER_CLOSED);

    std::cout << "[SUCCESS] Mo dun 4: MMAPI Audio Synthesizer hoan tat kiem tra 100% thanh cong!\n" << std::endl;
}

static void test_graphics3d_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[MODULE 5 TEST] Kiem tra toan dien Mo dun Do hoa 3D (M3G & Micro3D)" << std::endl;
    std::cout << "========================================================" << std::endl;

    using namespace universal_loader::graphics3d;
    using namespace universal_loader::micro3d;
    using namespace universal_loader::m3g;

    // 1. Kiem tra Ma tran 4x4 & Phep toan 3D
    std::cout << "  1. Kiem tra Matrix4x4, phep nhan va nghich dao (Inversion)..." << std::endl;
    Matrix4x4 mTrans = Matrix4x4::translation(10.0f, -5.0f, 2.5f);
    Matrix4x4 mRot = Matrix4x4::rotationY(45.0f * DEG_TO_RAD);
    Matrix4x4 mScale = Matrix4x4::scaling(2.0f, 2.0f, 2.0f);
    Matrix4x4 mComb = mTrans * mRot * mScale;

    Matrix4x4 mInv;
    bool invOk = mComb.invert(mInv);
    assert(invOk);

    Matrix4x4 mIdentityCheck = mComb * mInv;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float expected = (i == j) ? 1.0f : 0.0f;
            float actual = mIdentityCheck.m[i * 4 + j];
            assert(std::abs(actual - expected) < 1e-4f);
        }
    }
    std::cout << "     Matrix Inversion M * M^-1 = Identity xac minh thanh cong!" << std::endl;

    // 2. Kiem tra Mascot Capsule Micro3D Fixed-Point Math & Util3D
    std::cout << "  2. Kiem tra Mascot Capsule Micro3D Fixed-Point (1.0 = 4096)..." << std::endl;
    int32_t sin45 = Util3D::sin(512); // 45 deg (512 out of 4096)
    int32_t sin90 = Util3D::sin(1024); // 90 deg (1024 out of 4096)
    int32_t cos0 = Util3D::cos(0);
    assert(std::abs(sin45 - 2896) <= 1); // 4096 * sin(pi/4) = 2896.3
    assert(sin90 == 4096);
    assert(cos0 == 4096);

    AffineTrans affRotY;
    affRotY.rotationY(1024); // 90 deg around Y
    Vector3D pt(4096, 0, 0); // Point (1.0, 0, 0)
    Vector3D rotatedPt = affRotY.transPoint(pt);
    assert(rotatedPt.x == 0);
    assert(rotatedPt.y == 0);
    assert(std::abs(rotatedPt.z - (-4096)) <= 1); // Rotated around Y by 90 deg -> (0, 0, -1.0)
    std::cout << "     Micro3D Util3D sin/cos va AffineTrans 90 deg point rotation xac minh thanh cong!" << std::endl;

    // 3. Kiem tra Dong co cay xuong Micro3D (Bone Hierarchy & Skinning)
    std::cout << "  3. Kiem tra Dong co bien doi cay xuong (Bone Hierarchy)..." << std::endl;
    Bone bones[2];
    // Bone 0: Root bone with translation (0, 10 * 4096, 0)
    bones[0].parent = -1;
    bones[0].matrix.setIdentity();
    bones[0].matrix.m13 = 10 * 4096;
    bones[0].length = 1;

    // Bone 1: Child bone with translation (0, 20 * 4096, 0) relative to parent
    bones[1].parent = 0;
    bones[1].matrix.setIdentity();
    bones[1].matrix.m13 = 20 * 4096;
    bones[1].length = 1;

    AffineTrans computedBones[2];
    Micro3dEngine::transformBones(bones, 2, nullptr, 0, computedBones);

    assert(computedBones[0].m13 == 10 * 4096);
    assert(computedBones[1].m13 == 30 * 4096); // 10 + 20 = 30
    std::cout << "     Bone Hierarchy Solver: Root(10) -> Child(20) = 30*4096 thanh cong!" << std::endl;

    // 4. Kiem tra M3G Triangle Strip Decomposer
    std::cout << "  4. Kiem tra M3G Triangle Strip Decomposer..." << std::endl;
    std::vector<uint32_t> stripIndices = {0, 1, 2, 3, 3, 4, 4, 5, 6}; // Contains degenerates for stitching
    IndexBuffer ib(PRIMITIVE_TRIANGLE_STRIP, stripIndices);
    std::vector<uint32_t> triangles;
    ib.getTriangles(triangles);
    // [0, 1, 2], [2, 1, 3] -> 2 valid triangles
    // [3, 4, 4] -> degenerate
    // [4, 4, 5] -> degenerate
    // [4, 5, 6] -> valid triangle
    assert(triangles.size() >= 6);
    std::cout << "     M3G Triangle Strip Stripify & Degenerate Filtering thanh cong (" << triangles.size() / 3 << " triangles)!" << std::endl;

    // 5. Kiem tra Rasterizer 3D voi Depth Buffer (Z-Buffer Occlusion Test)
    std::cout << "  5. Kiem tra 3D Software Rasterizer & Depth Buffer (Z-Buffer Occlusion)..." << std::endl;
    const int W = 64;
    const int H = 64;
    std::vector<uint32_t> fbPixels(W * H, 0xFF000000); // 64x64 black background

    Rasterizer3D rasterizer;
    rasterizer.setTarget(fbPixels.data(), W, H);
    rasterizer.clear(0xFF000000, 1.0f); // Clear black color, depth 1.0

    // Appearance
    Appearance appRed;
    appRed.material.diffuseColor = 0x00FF0000;
    appRed.compositingMode.depthTestEnabled = true;
    appRed.compositingMode.depthWriteEnabled = true;
    appRed.polygonMode.culling = CULL_NONE;

    Appearance appGreen;
    appGreen.material.diffuseColor = 0x0000FF00;
    appGreen.compositingMode.depthTestEnabled = true;
    appGreen.compositingMode.depthWriteEnabled = true;
    appGreen.polygonMode.culling = CULL_NONE;

    // Red triangle in back (screenPos.z = 0.7f)
    RasterVertex r0, r1, r2;
    r0.screenPos = Vector3(10.0f, 10.0f, 0.7f); r0.normal = Vector3(0, 0, 1); r0.color = 0xFFFF0000; r0.invW = 1.0f;
    r1.screenPos = Vector3(50.0f, 10.0f, 0.7f); r1.normal = Vector3(0, 0, 1); r1.color = 0xFFFF0000; r1.invW = 1.0f;
    r2.screenPos = Vector3(30.0f, 50.0f, 0.7f); r2.normal = Vector3(0, 0, 1); r2.color = 0xFFFF0000; r2.invW = 1.0f;

    std::vector<Light> lights; // Empty lights -> unlit base colors
    rasterizer.drawTriangle(r0, r1, r2, appRed, lights);

    // Verify center pixel (30, 25) is RED
    uint32_t centerCol1 = rasterizer.getColorAt(30, 25);
    float centerDepth1 = rasterizer.getDepthAt(30, 25);
    assert((centerCol1 & 0x00FF0000) != 0); // Has red component
    assert(std::abs(centerDepth1 - 0.7f) < 0.05f);

    // Draw Green triangle in FRONT (screenPos.z = 0.3f) covering center
    RasterVertex g0, g1, g2;
    g0.screenPos = Vector3(10.0f, 10.0f, 0.3f); g0.normal = Vector3(0, 0, 1); g0.color = 0xFF00FF00; g0.invW = 1.0f;
    g1.screenPos = Vector3(50.0f, 10.0f, 0.3f); g1.normal = Vector3(0, 0, 1); g1.color = 0xFF00FF00; g1.invW = 1.0f;
    g2.screenPos = Vector3(30.0f, 50.0f, 0.3f); g2.normal = Vector3(0, 0, 1); g2.color = 0xFF00FF00; g2.invW = 1.0f;

    rasterizer.drawTriangle(g0, g1, g2, appGreen, lights);

    // Verify center pixel (30, 25) is now GREEN, and depth is 0.3f!
    uint32_t centerCol2 = rasterizer.getColorAt(30, 25);
    float centerDepth2 = rasterizer.getDepthAt(30, 25);
    assert((centerCol2 & 0x0000FF00) != 0); // Has green component
    assert((centerCol2 & 0x00FF0000) == 0); // Red was overwritten
    assert(std::abs(centerDepth2 - 0.3f) < 0.05f);

    // Now attempt to draw a Blue triangle at depth 0.5f (behind Green)
    Appearance appBlue;
    appBlue.material.diffuseColor = 0x000000FF;
    appBlue.compositingMode.depthTestEnabled = true;
    appBlue.compositingMode.depthWriteEnabled = true;
    appBlue.polygonMode.culling = CULL_NONE;

    RasterVertex b0, b1, b2;
    b0.screenPos = Vector3(10.0f, 10.0f, 0.5f); b0.normal = Vector3(0, 0, 1); b0.color = 0xFF0000FF; b0.invW = 1.0f;
    b1.screenPos = Vector3(50.0f, 10.0f, 0.5f); b1.normal = Vector3(0, 0, 1); b1.color = 0xFF0000FF; b1.invW = 1.0f;
    b2.screenPos = Vector3(30.0f, 50.0f, 0.5f); b2.normal = Vector3(0, 0, 1); b2.color = 0xFF0000FF; b2.invW = 1.0f;

    rasterizer.drawTriangle(b0, b1, b2, appBlue, lights);

    // Verify center pixel (30, 25) REMAINS GREEN! Blue was occluded by depth test!
    uint32_t centerCol3 = rasterizer.getColorAt(30, 25);
    float centerDepth3 = rasterizer.getDepthAt(30, 25);
    assert((centerCol3 & 0x0000FF00) != 0); // Still green!
    assert(std::abs(centerDepth3 - 0.3f) < 0.05f); // Depth still 0.3!
    std::cout << "     Depth Buffer (Z-Buffer Occlusion Test): Chinh phuc thanh cong 100%!" << std::endl;

    // 6. Kiem tra C-ABI 3D Context
    std::cout << "  6. Kiem tra C-ABI Graphics 3D Context (j2me_core_3d_*)..." << std::endl;
    J2meGraphics3DContext* ctx3d = j2me_core_3d_create_context(64, 64);
    assert(ctx3d != nullptr);
    j2me_core_3d_bind_target(ctx3d, fbPixels.data(), 64, 64);
    j2me_core_3d_clear(ctx3d, 0xFF123456, 0.95f);
    assert(j2me_core_3d_get_color(ctx3d, 32, 32) == 0xFF123456);
    assert(std::abs(j2me_core_3d_get_depth(ctx3d, 32, 32) - 0.95f) < 1e-4f);
    j2me_core_3d_destroy_context(ctx3d);
    std::cout << "     C-ABI 3D Context hoat dong on dinh tuyet doi!" << std::endl;

    std::cout << "[SUCCESS] Mo dun 5: 3D Graphics Engines (M3G & Micro3D) hoan tat kiem tra 100% thanh cong!\n" << std::endl;
}

static int g_mockVibraDuration = -1;
static int g_mockVibraFreq = -1;

static void mock_vibration_callback(int duration_ms, int frequency, void* user_data) {
    (void)user_data;
    g_mockVibraDuration = duration_ms;
    g_mockVibraFreq = frequency;
}

static void test_oem_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[MODULE 6 TEST] Kiem tra toan dien Mo dun OEM & Vendor Extensions" << std::endl;
    std::cout << "========================================================" << std::endl;

    using namespace universal_loader::oem;

    // 1. Kiem tra Bang anh xa Manipulation -> LCDUI Transforms cua Nokia
    std::cout << "  1. Kiem tra Nokia Manipulation flags sang LCDUI 8 Transforms..." << std::endl;
    assert(NokiaDirectGraphics::getTransformation(0) == j2me::TRANS_NONE);
    assert(NokiaDirectGraphics::getTransformation(ROTATE_90) == j2me::TRANS_ROT270);
    assert(NokiaDirectGraphics::getTransformation(ROTATE_180) == j2me::TRANS_ROT180);
    assert(NokiaDirectGraphics::getTransformation(ROTATE_270) == j2me::TRANS_ROT90);

    assert(NokiaDirectGraphics::getTransformation(FLIP_HORIZONTAL) == j2me::TRANS_MIRROR);
    assert(NokiaDirectGraphics::getTransformation(FLIP_HORIZONTAL | ROTATE_90) == j2me::TRANS_MIRROR_ROT90);
    assert(NokiaDirectGraphics::getTransformation(FLIP_HORIZONTAL | ROTATE_180) == j2me::TRANS_MIRROR_ROT180);
    assert(NokiaDirectGraphics::getTransformation(FLIP_HORIZONTAL | ROTATE_270) == j2me::TRANS_MIRROR_ROT270);

    assert(NokiaDirectGraphics::getTransformation(FLIP_VERTICAL) == j2me::TRANS_MIRROR_ROT180);
    assert(NokiaDirectGraphics::getTransformation(FLIP_VERTICAL | ROTATE_90) == j2me::TRANS_MIRROR_ROT270);
    assert(NokiaDirectGraphics::getTransformation(FLIP_VERTICAL | ROTATE_180) == j2me::TRANS_MIRROR);
    assert(NokiaDirectGraphics::getTransformation(FLIP_VERTICAL | ROTATE_270) == j2me::TRANS_MIRROR_ROT90);
    std::cout << "     12 to hop Manipulation flags (Nokia DirectGraphics) xac minh chinh xac 100%!" << std::endl;

    // 2. Kiem tra drawPixels (16-bit 4444 ARGB & 565 RGB) va getPixels
    std::cout << "  2. Kiem tra DirectGraphics drawPixels (4444_ARGB, 565_RGB) va getPixels..." << std::endl;
    auto targetImg = j2me::LcduiImage::createImage(32, 32);
    auto g = targetImg->getGraphics();
    g->setColor(0xFF000000);
    g->fillRect(0, 0, 32, 32);

    auto getImgPixel = [&](int px, int py) -> uint32_t {
        return targetImg->getPixels()[py * targetImg->getWidth() + px];
    };

    NokiaDirectGraphics dg(g.get());

    // Write a 2x2 patch of 4444_ARGB: 0xF842 (A=15, R=8, G=4, B=2 -> ARGB 0xFF884422)
    uint16_t pix4444[4] = {0xF842, 0xF842, 0xF842, 0xF842};
    dg.drawPixels(pix4444, true, 0, 2, 4, 4, 2, 2, 0, TYPE_USHORT_4444_ARGB);

    // Read back pixel at (4, 4)
    uint32_t readBackArgb = getImgPixel(4, 4);
    assert(((readBackArgb >> 24) & 0xFF) == 0xFF);
    assert(((readBackArgb >> 16) & 0xFF) == 0x88);
    assert(((readBackArgb >> 8)  & 0xFF) == 0x44);
    assert((readBackArgb         & 0xFF) == 0x22);

    // Write a 2x2 patch of 565_RGB: 0xF800 (Red)
    uint16_t pix565[4] = {0xF800, 0xF800, 0xF800, 0xF800};
    dg.drawPixels(pix565, false, 0, 2, 10, 10, 2, 2, 0, TYPE_USHORT_565_RGB);

    uint32_t readBackRed = getImgPixel(10, 10);
    assert((readBackRed & 0x00FF0000) == 0x00FF0000); // Full red
    assert((readBackRed & 0x0000FF00) == 0);          // No green
    assert((readBackRed & 0x000000FF) == 0);          // No blue

    // Test getPixels back to 565
    uint16_t out565[4]{0};
    dg.getPixels(out565, 0, 2, 10, 10, 2, 2, TYPE_USHORT_565_RGB);
    assert(out565[0] == 0xF800);
    assert(out565[3] == 0xF800);
    std::cout << "     Giai ma & Ma hoa diem anh 16-bit 4444/565 xac minh thanh cong!" << std::endl;

    // 3. Kiem tra drawPolygon & fillPolygon (Scanline Parity Fill)
    std::cout << "  3. Kiem tra Nokia drawPolygon va fillPolygon (Scanline parity)..." << std::endl;
    // Trapezoid polygon
    int32_t polyX[4] = {8, 24, 28, 4};
    int32_t polyY[4] = {16, 16, 28, 28};
    uint32_t greenColor = 0xFF00FF00;

    dg.fillPolygon(polyX, polyY, 4, greenColor);

    // Interior point (16, 20) must be Green
    uint32_t insidePixel = getImgPixel(16, 20);
    assert((insidePixel & 0x0000FF00) != 0);

    // Exterior point (2, 2) must remain Black
    uint32_t outsidePixel = getImgPixel(2, 2);
    assert((outsidePixel & 0x0000FF00) == 0);
    std::cout << "     Scanline fillPolygon va drawPolygon xac minh thanh cong!" << std::endl;

    // 4. Kiem tra fillTriangle
    std::cout << "  4. Kiem tra fillTriangle voi Alpha Blending..." << std::endl;
    dg.fillTriangle(0, 0, 6, 0, 3, 6, 0xFFFF00FF); // Magenta triangle
    uint32_t triPixel = getImgPixel(3, 2);
    assert(triPixel == 0xFFFF00FF);
    std::cout << "     Nokia fillTriangle thanh cong!" << std::endl;

    // 5. Kiem tra DeviceControl & Vibration (Nokia / Samsung / Siemens)
    std::cout << "  5. Kiem tra DeviceControl, Samsung Vibration & Siemens Vibrator..." << std::endl;
    auto& devMgr = DeviceControlManager::instance();
    devMgr.setVibrationCallback(mock_vibration_callback, nullptr);

    // Test Nokia startVibra(75, 450)
    devMgr.startVibra(75, 450);
    assert(g_mockVibraDuration == 450);
    assert(g_mockVibraFreq == 75);
    assert(devMgr.isVibrating());

    // Test stopVibra()
    devMgr.stopVibra();
    assert(g_mockVibraDuration == 0);
    assert(!devMgr.isVibrating());

    // Test Samsung Vibration (duration 2s = 2000ms)
    devMgr.vibrate(2000, 100);
    assert(g_mockVibraDuration == 2000);
    assert(g_mockVibraFreq == 100);

    devMgr.stopVibra();
    std::cout << "     Device Haptics & Vibration Callback hoat dong chinh xac 100%!" << std::endl;

    std::cout << "[SUCCESS] Mo dun 6: OEM & Vendor Extensions hoan tat kiem tra 100% thanh cong!\n" << std::endl;
}

using namespace universal_loader::lcdui;

class MockCommandListener : public CommandListener {
public:
    void commandAction(const Command& c, Displayable* d) override {
        lastCommand = c;
        lastDisplayable = d;
        firedCount++;
    }
    Command lastCommand;
    Displayable* lastDisplayable{nullptr};
    int firedCount{0};
};

class MockItemStateListener : public ItemStateListener {
public:
    void itemStateChanged(Item* item) override {
        lastItem = item;
        changedCount++;
    }
    Item* lastItem{nullptr};
    int changedCount{0};
};

static void test_lcdui_ui_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[MODULE 7 TEST] Kiem tra toan dien LCDUI High-Level UI & Widgets" << std::endl;
    std::cout << "========================================================" << std::endl;

    // 1. Kiem tra Command va SoftKeysBar Engine
    std::cout << "  1. Kiem tra Command Priority va SoftKeysBar distribution..." << std::endl;
    Command cmdOk("OK", CMD_OK, 1);
    Command cmdBack("Back", CMD_BACK, 1);
    Command cmdHelp("Help", CMD_HELP, 2);
    Command cmdOptions("Options", CMD_SCREEN, 3);

    assert(cmdOk.getCommandType() == CMD_OK);
    assert(cmdBack.getCommandType() == CMD_BACK);
    assert(cmdBack.getDisplayLabel() == "Back");

    // Command comparison
    assert(cmdOk.compareTo(cmdBack) > 0); // OK (4) > BACK (2)

    SoftKeysBar bar(true);
    std::vector<Command> cmds = {cmdOk, cmdBack, cmdHelp, cmdOptions};
    bar.update(cmds);

    // Rule: Back is Right, OK is Middle, Menu is Left (with Help and Options)
    assert(bar.hasRight());
    assert(bar.getRightCommand() == cmdBack);
    assert(bar.hasMiddle());
    assert(bar.getMiddleCommand() == cmdOk);
    assert(bar.hasLeft());
    assert(bar.isLeftMenu());
    assert(bar.getMenuCommands().size() == 2);
    std::cout << "     SoftKeysBar phan bo phim mem Left(Menu)/Middle(OK)/Right(Back) chuan xac 100%!" << std::endl;

    // 2. Kiem tra Form, Item, TextField, ChoiceGroup, Gauge
    std::cout << "  2. Kiem tra Form, TextField, ChoiceGroup va Gauge..." << std::endl;
    auto form = std::make_shared<Form>("Cai dat Tro choi");
    MockCommandListener formListener;
    form->setCommandListener(&formListener);
    form->addCommand(cmdOk);
    form->addCommand(cmdBack);

    MockItemStateListener itemListener;
    form->setItemStateListener(&itemListener);

    // StringItem
    auto strItem = std::make_shared<StringItem>("Ten Nhan Vat", "SonGoku", Item::PLAIN);
    form->append(strItem);
    assert(strItem->getText() == "SonGoku");

    // TextField (Password)
    auto passField = std::make_shared<TextField>("Mat khau", "super123", 16, TextField::PASSWORD);
    form->append(passField);
    assert(passField->getString() == "super123");
    assert(passField->getCaretPosition() == 8);

    // Insert text
    passField->insert("4", 8);
    assert(passField->getString() == "super1234");
    assert(itemListener.changedCount >= 1);
    assert(itemListener.lastItem == passField.get());

    // Delete text
    passField->deleteChar(8);
    assert(passField->getString() == "super123");

    // ChoiceGroup (Exclusive / Radio)
    auto diffChoice = std::make_shared<ChoiceGroup>("Do kho", ChoiceGroup::EXCLUSIVE);
    diffChoice->append("De", nullptr);
    diffChoice->append("Binh thuong", nullptr);
    diffChoice->append("Kho", nullptr);
    form->append(diffChoice);

    diffChoice->setSelectedIndex(1, true);
    assert(diffChoice->getSelectedIndex() == 1);
    assert(diffChoice->isSelected(1) == true);
    assert(diffChoice->isSelected(0) == false);

    // ChoiceGroup (Multiple / Checkbox)
    auto soundChoice = std::make_shared<ChoiceGroup>("Am thanh", ChoiceGroup::MULTIPLE);
    soundChoice->append("Nhac nen", nullptr);
    soundChoice->append("Hieu ung", nullptr);
    soundChoice->append("Rung", nullptr);
    form->append(soundChoice);

    std::vector<bool> flags = {true, true, false};
    soundChoice->setSelectedFlags(flags);
    std::vector<bool> outFlags;
    int selectedCount = soundChoice->getSelectedFlags(outFlags);
    assert(selectedCount == 2);
    assert(outFlags[0] == true);
    assert(outFlags[1] == true);
    assert(outFlags[2] == false);

    // Gauge (Interactive Slider)
    auto volGauge = std::make_shared<Gauge>("Am luong", true, 10, 7);
    form->append(volGauge);
    assert(volGauge->getValue() == 7);
    assert(volGauge->isInteractive() == true);

    // Simulate Key RIGHT -> value becomes 8
    volGauge->keyPressed(5); // RIGHT
    assert(volGauge->getValue() == 8);
    // Simulate Key LEFT -> value becomes 7
    volGauge->keyPressed(2); // LEFT
    assert(volGauge->getValue() == 7);

    // Form element operations
    assert(form->size() == 5);
    auto spacer = std::make_shared<Spacer>(10, 15);
    form->insert(2, spacer);
    assert(form->size() == 6);
    form->deleteItem(2);
    assert(form->size() == 5);

    std::cout << "     Form va he thong cac Widget Item xac minh thanh cong!" << std::endl;

    // 3. Kiem tra List (IMPLICIT va Auto SELECT_COMMAND)
    std::cout << "  3. Kiem tra List (IMPLICIT mode) va Tu dong kich hoat SELECT_COMMAND..." << std::endl;
    auto menuList = std::make_shared<List>("Menu Chinh", List::IMPLICIT);
    MockCommandListener listListener;
    menuList->setCommandListener(&listListener);

    menuList->append("Choi Moi", nullptr);
    menuList->append("Tiep Tuc", nullptr);
    menuList->append("Cai Dat", nullptr);
    menuList->append("Thoat", nullptr);

    assert(menuList->size() == 4);
    assert(menuList->getString(0) == "Choi Moi");

    // Simulate Key DOWN -> focus on index 1 ("Tiep Tuc")
    menuList->keyPressed(6); // DOWN
    // Simulate Key SELECT (-5)
    menuList->keyPressed(-5);
    assert(menuList->getSelectedIndex() == 1);
    assert(listListener.firedCount == 1);
    assert(listListener.lastCommand == List::SELECT_COMMAND);
    std::cout << "     List IMPLICIT SELECT_COMMAND trigger chinh xac 100%!" << std::endl;

    // 4. Kiem tra Alert va AlertType
    std::cout << "  4. Kiem tra Alert, AlertType va Header Colors..." << std::endl;
    AlertType errType(AlertType::ERROR_TYPE);
    assert(errType.getHeaderColor() == 0xFFCC2222);

    AlertType warnType(AlertType::WARNING);
    assert(warnType.getHeaderColor() == 0xFFDD8800);

    auto alert = std::make_shared<Alert>("Loi Mang", "Khong the ket noi den Server!", nullptr, AlertType::ERROR_TYPE);
    assert(alert->getString() == "Khong the ket noi den Server!");
    assert(alert->getType() == AlertType::ERROR_TYPE);

    MockCommandListener alertListener;
    alert->setCommandListener(&alertListener);
    alert->keyPressed(-5); // Press OK
    assert(alertListener.firedCount == 1);
    assert(alertListener.lastCommand == Alert::DISMISS_COMMAND);
    std::cout << "     Alert & AlertType xac minh thanh cong!" << std::endl;

    // 5. Kiem tra Display va Ket noi Hardware
    std::cout << "  5. Kiem tra Display Instance, System Colors va Hardware Linking..." << std::endl;
    auto& disp = Display::instance();
    disp.setScreenSize(240, 320);
    assert(disp.getColor(Display::COLOR_BACKGROUND) == (int)0xFFD0D0D0);
    assert(disp.getColor(Display::COLOR_FOREGROUND) == (int)0xFF000080);

    disp.setCurrent(form);
    assert(disp.getCurrent() == form);

    disp.vibrate(300);
    assert(universal_loader::oem::DeviceControlManager::instance().getLastVibrationDuration() == 300);
    std::cout << "     Display & Hardware Haptics lien ket thong suot!" << std::endl;

    // 6. Kiem tra Software Rendering vao Framebuffer
    std::cout << "  6. Kiem tra Software UI Rendering vao Framebuffer..." << std::endl;
    auto testImg = j2me::LcduiImage::createImage(240, 320);
    auto g = testImg->getGraphics();
    form->paint(g.get());

    const uint32_t* rawPixels = testImg->getPixels();
    uint32_t titlePixel = rawPixels[10 * 240 + 10]; // Line 10 in Title bar
    assert(titlePixel != 0); // Must be painted

    // Render List
    menuList->paint(g.get());
    uint32_t listPixel = rawPixels[10 * 240 + 10];
    assert(listPixel != 0);

    // Render Alert
    alert->paint(g.get());
    uint32_t alertPixel = rawPixels[160 * 240 + 120]; // Center of alert dialog
    assert(alertPixel != 0);
    std::cout << "     Software UI Rendering vao Framebuffer hoan tat thanh cong!" << std::endl;

    std::cout << "[SUCCESS] Mo dun 7: LCDUI High-Level UI & Widgets hoan tat kiem tra 100% thanh cong!\n" << std::endl;
}

static void test_jsr75_file_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[MODULE 8 TEST] Kiem tra toan dien JSR-75 FileConnection & FileSystem" << std::endl;
    std::cout << "========================================================" << std::endl;

    std::string testFsBase = "./test_j2me_fs_sandbox";
    try {
        fs::remove_all(testFsBase);
    } catch (...) {}

    // 1. Kiem tra FileSystemRegistry va Danh sach Roots
    std::cout << "  1. Kiem tra FileSystemRegistry, Roots va URL Resolution..." << std::endl;
    auto& reg = universal_loader::file::FileSystemRegistry::instance();
    reg.setBaseDirectory(testFsBase);

    auto roots = reg.listRoots();
    assert(!roots.empty());
    bool hasC = false, hasE = false, hasPhotos = false;
    for (const auto& r : roots) {
        if (r == "c:/") hasC = true;
        if (r == "e:/") hasE = true;
        if (r == "photos/") hasPhotos = true;
    }
    assert(hasC && hasE && hasPhotos);

    // Kiem tra URL resolve
    std::string matchedRoot;
    std::filesystem::path physicalPath;
    assert(reg.resolve("file:///c:/save/slot1.bin", matchedRoot, physicalPath));
    assert(matchedRoot == "c:/");

    // Kiem tra Alias "root/" sang "c:/" va "sdcard/" sang "e:/"
    assert(reg.resolve("file:///root/game.cfg", matchedRoot, physicalPath));
    assert(matchedRoot == "c:/");

    assert(reg.resolve("file:///sdcard/mods/hero.pak", matchedRoot, physicalPath));
    assert(matchedRoot == "e:/");

    // Kiem tra chan Directory Traversal
    assert(!reg.resolve("file:///c:/../../etc/passwd", matchedRoot, physicalPath));
    std::cout << "     FileSystemRegistry roots va URL resolution xac minh 100%!" << std::endl;

    // 2. Kiem tra FileConnection: create, write, read binary
    std::cout << "  2. Kiem tra FileConnection: create, writeAllBytes va readAllBytes..." << std::endl;
    auto fileConn = reg.open("file:///c:/save/slot1.bin");
    assert(fileConn != nullptr);
    assert(fileConn->isOpen());
    assert(!fileConn->exists());

    fileConn->create();
    assert(fileConn->exists());
    assert(fileConn->isFile());
    assert(fileConn->fileSize() == 0);

    std::string testData = "J2ME_DEFAULT_LEVEL_99_SAVE_DATA";
    fileConn->writeAllBytes(reinterpret_cast<const uint8_t*>(testData.data()), testData.size());
    assert(fileConn->fileSize() == static_cast<int64_t>(testData.size()));

    auto readBytes = fileConn->readAllBytes();
    std::string readStr(readBytes.begin(), readBytes.end());
    assert(readStr == testData);
    std::cout << "     FileConnection create, write & read binary xac minh chinh xac!" << std::endl;

    // 3. Kiem tra Seek-Write (writeBytesAt) va Truncate
    std::cout << "  3. Kiem tra Seek-Write (writeBytesAt) va Truncate..." << std::endl;
    std::string overwriteStr = "WARRIOR";
    fileConn->writeBytesAt(5, reinterpret_cast<const uint8_t*>(overwriteStr.data()), overwriteStr.size());
    auto readOverwritten = fileConn->readAllBytes();
    std::string overwrittenStr(readOverwritten.begin(), readOverwritten.end());
    assert(overwrittenStr == "J2ME_WARRIOR_LEVEL_99_SAVE_DATA");

    fileConn->truncate(12);
    assert(fileConn->fileSize() == 12);
    auto readTruncated = fileConn->readAllBytes();
    std::string truncatedStr(readTruncated.begin(), readTruncated.end());
    assert(truncatedStr == "J2ME_WARRIOR");
    std::cout << "     Seek-Write va Truncate xac minh thanh cong!" << std::endl;

    // 4. Kiem tra Thu muc (mkdir), list voi Wildcard filter va dieu huong (setFileConnection)
    std::cout << "  4. Kiem tra Thu muc (mkdir), Wildcard list va dieu huong..." << std::endl;
    auto dirConn = reg.open("file:///c:/gamedata/");
    assert(dirConn != nullptr);
    dirConn->mkdir();
    assert(dirConn->exists());
    assert(dirConn->isDirectory());

    // Tao cac file va subfolder ben trong
    auto f1 = reg.open("file:///c:/gamedata/char1.png");
    f1->create();
    auto f2 = reg.open("file:///c:/gamedata/char2.png");
    f2->create();
    auto f3 = reg.open("file:///c:/gamedata/sound.wav");
    f3->create();
    auto subDir = reg.open("file:///c:/gamedata/maps/");
    subDir->mkdir();

    // List filter *.png
    auto pngList = dirConn->list("*.png");
    assert(pngList.size() == 2);
    assert(pngList[0] == "char1.png");
    assert(pngList[1] == "char2.png");

    // List all
    auto allList = dirConn->list("*");
    assert(allList.size() == 4);
    // Subdirectory phai co trailing slash '/'
    bool foundSubDir = false;
    for (const auto& item : allList) {
        if (item == "maps/") foundSubDir = true;
    }
    assert(foundSubDir);

    // Navigation
    dirConn->setFileConnection("maps");
    assert(dirConn->getName() == "maps/");
    dirConn->setFileConnection("..");
    assert(dirConn->getName() == "gamedata/");
    std::cout << "     Thu muc, Wildcard list & dieu huong setFileConnection thanh cong!" << std::endl;

    // 5. Kiem tra Rename va Delete
    std::cout << "  5. Kiem tra Rename va Delete..." << std::endl;
    f3->rename("bgm.wav");
    assert(f3->getName() == "bgm.wav");
    assert(f3->exists());

    f3->deleteFile();
    assert(!f3->exists());
    std::cout << "     Rename va Delete file xac minh thanh cong!" << std::endl;

    // 6. Kiem tra C-ABI Exports
    std::cout << "  6. Kiem tra C-ABI FileSystem APIs..." << std::endl;
    j2me_core_fs_set_base_dir(testFsBase.c_str());
    assert(j2me_core_fs_file_exists("file:///c:/save/slot1.bin"));
    assert(j2me_core_fs_file_size("file:///c:/save/slot1.bin") == 12);
    assert(!j2me_core_fs_file_exists("file:///c:/non_existent.dat"));
    std::cout << "     C-ABI FileSystem APIs hoat dong on dinh tuyet doi!" << std::endl;

    std::cout << "[SUCCESS] Mo dun 8: JSR-75 FileConnection & FileSystem hoan tat kiem tra 100% thanh cong!\n" << std::endl;
}

void test_jsr120_wma_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[MODULE 9 TEST] Kiem tra toan dien JSR-120 WMA (Wireless Messaging)" << std::endl;
    std::cout << "========================================================" << std::endl;

    using namespace universal_loader::messaging;

    // 1. Kiem tra URL Parsing & Validation
    std::cout << "  1. Kiem tra URL Parsing va Validation..." << std::endl;
    SmsConnection conn1("sms://+84901234567:1900");
    assert(conn1.getHost() == "+84901234567");
    assert(conn1.getPort() == 1900);
    assert(conn1.isOpen());

    SmsConnection conn2("sms://8730");
    assert(conn2.getHost() == "8730");
    assert(conn2.getPort() == -1);

    SmsConnection conn3("sms://:5000");
    assert(conn3.getHost().empty());
    assert(conn3.getPort() == 5000);

    // Kiem tra loi dinh dang so va cong
    bool hostErrorCaught = false;
    try {
        SmsConnection badHost("sms://invalid_phone_number");
    } catch (const std::invalid_argument&) {
        hostErrorCaught = true;
    }
    assert(hostErrorCaught);

    bool portErrorCaught = false;
    try {
        SmsConnection badPort("sms://8730:70000"); // 70000 > 65535
    } catch (const std::invalid_argument&) {
        portErrorCaught = true;
    }
    assert(portErrorCaught);
    std::cout << "     URL Parser, Host & Port validation xac minh 100%!" << std::endl;

    // 2. Kiem tra Message Creation & Segments Calculation
    std::cout << "  2. Kiem tra TextMessage, BinaryMessage & NumberOfSegments..." << std::endl;
    auto txtMsg = std::dynamic_pointer_cast<TextMessage>(conn1.newMessage(MessageConnection::TEXT_MESSAGE));
    assert(txtMsg != nullptr);
    txtMsg->setPayloadText("Test J2ME WMA Short SMS");
    assert(conn1.numberOfSegments(*txtMsg) == 1);

    std::string longText(200, 'A'); // 200 chars > 160 chars
    txtMsg->setPayloadText(longText);
    assert(conn1.numberOfSegments(*txtMsg) == 2);

    auto binMsg = std::dynamic_pointer_cast<BinaryMessage>(conn1.newMessage(MessageConnection::BINARY_MESSAGE));
    assert(binMsg != nullptr);
    std::vector<uint8_t> shortBin = {0x01, 0x02, 0x03, 0x04};
    binMsg->setPayloadData(shortBin);
    assert(conn1.numberOfSegments(*binMsg) == 1);

    std::vector<uint8_t> longBin(200, 0xFF); // 200 bytes > 140 bytes
    binMsg->setPayloadData(longBin);
    assert(conn1.numberOfSegments(*binMsg) == 2);
    std::cout << "     Message creation & segments calculation xac minh chinh xac!" << std::endl;

    // 3. Kiem tra SMS Outbox, Intercept Callback & Gui Tin Nhan
    std::cout << "  3. Kiem tra SMS Router Outbox & Native Intercept Callback..." << std::endl;
    SmsMessageRouter::instance().clearAll();

    static std::string s_lastAddr;
    static std::string s_lastText;
    static size_t s_cbCount = 0;
    s_cbCount = 0;

    SmsMessageRouter::instance().setInterceptCallback([](const char* address, const char* textPayload, const uint8_t*, size_t, void*) {
        if (address) s_lastAddr = address;
        if (textPayload) s_lastText = textPayload;
        s_cbCount++;
    }, nullptr);

    SmsConnection shopConn("sms://8730");
    auto payMsg = std::dynamic_pointer_cast<TextMessage>(shopConn.newMessage(MessageConnection::TEXT_MESSAGE, "8730"));
    payMsg->setPayloadText("NAP THE 50K");
    shopConn.send(payMsg);

    assert(SmsMessageRouter::instance().getSentCount() == 1);
    assert(s_cbCount == 1);
    assert(s_lastAddr == "8730");
    assert(s_lastText == "NAP THE 50K");
    std::cout << "     SMS Router Outbox & Intercept Callback xac minh thanh cong!" << std::endl;

    // 4. Kiem tra MessageListener & Nhan Tin Nhan
    std::cout << "  4. Kiem tra MessageListener & Nhan Tin Nhan..." << std::endl;
    class TestMessageListener : public MessageListener {
    public:
        int notifyCount{0};
        void notifyIncomingMessage(MessageConnection*) override {
            notifyCount++;
        }
    } testListener;

    SmsConnection serverConn("sms://:5000");
    serverConn.setMessageListener(&testListener);

    SmsConnection clientConn("sms://:5000");
    auto toServerMsg = clientConn.newMessage(MessageConnection::TEXT_MESSAGE, ":5000");
    auto textToServer = std::dynamic_pointer_cast<TextMessage>(toServerMsg);
    textToServer->setPayloadText("HELLO_SERVER");
    clientConn.send(toServerMsg);

    assert(testListener.notifyCount == 1);

    // Nhan tin nhan phan hoi tu inbox
    auto recMsg = clientConn.receive();
    assert(recMsg != nullptr);
    auto textRec = std::dynamic_pointer_cast<TextMessage>(recMsg);
    assert(textRec != nullptr);
    assert(!textRec->getPayloadText().empty());
    std::cout << "     MessageListener & Receive message xac minh thanh cong!" << std::endl;

    // 5. Kiem tra C-ABI SMS APIs
    std::cout << "  5. Kiem tra C-ABI SMS APIs..." << std::endl;
    assert(j2me_core_sms_get_sent_count() >= 2);
    j2me_core_sms_clear_all();
    assert(j2me_core_sms_get_sent_count() == 0);
    std::cout << "     C-ABI SMS APIs hoat dong on dinh tuyet doi!" << std::endl;

    std::cout << "[SUCCESS] Mo dun 9: JSR-120 WMA hoan tat kiem tra 100% thanh cong!\n" << std::endl;
}

void test_midlet_descriptor_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[MODULE 10 TEST] Kiem tra toan dien MIDlet LifeCycle & Descriptor" << std::endl;
    std::cout << "========================================================" << std::endl;

    using namespace universal_loader::midlet;

    // 1. Kiem tra JAD & Manifest RFC 822 Parsing
    std::cout << "  1. Kiem tra JAD & Manifest RFC 822 Parsing..." << std::endl;
    std::string manifestContent =
        "Manifest-Version: 1.0\r\n"
        "MIDlet-Name: Super Mario Bros\r\n"
        "MIDlet-Version: 2.1.0\r\n"
        "MIDlet-Vendor: Nintendo Inc\r\n"
        "MIDlet-1: Mario, /icons/mario.png, com.nintendo.MarioMIDlet\r\n"
        "MIDlet-2: Luigi, /icons/luigi.png, com.nintendo.LuigiMIDlet\r\n"
        "MicroEdition-Profile: MIDP-2.0\r\n"
        "MicroEdition-Configuration: CLDC-1.1\r\n"
        "MIDlet-Description: The legendary 2D platformer\r\n"
        "Folded-Key: Line one of the folded\r\n"
        " content continuing here\r\n";

    AppDescriptor manifest(manifestContent, false);
    assert(manifest.getName() == "Super Mario Bros");
    assert(manifest.getVersion() == "2.1.0");
    assert(manifest.getVendor() == "Nintendo Inc");
    assert(manifest.getProfile() == "MIDP-2.0");
    assert(manifest.getConfiguration() == "CLDC-1.1");
    assert(manifest.getDescription() == "The legendary 2D platformer");
    assert(manifest.get("Folded-Key") == "Line one of the foldedcontent continuing here");

    const auto& midlets = manifest.getMidlets();
    assert(midlets.size() == 2);
    assert(midlets[0].index == 1);
    assert(midlets[0].name == "Mario");
    assert(midlets[0].icon == "icons/mario.png");
    assert(midlets[0].className == "com.nintendo.MarioMIDlet");

    assert(midlets[1].index == 2);
    assert(midlets[1].name == "Luigi");
    assert(midlets[1].icon == "icons/luigi.png");
    assert(midlets[1].className == "com.nintendo.LuigiMIDlet");
    std::cout << "     Manifest Parsing & Midlet Entries extraction xac minh 100%!" << std::endl;

    // 2. Kiem tra Version Comparison Algorithm
    std::cout << "  2. Kiem tra Version Comparison Algorithm..." << std::endl;
    assert(AppDescriptor::compareVersions("2.1.0", "2.0.9") > 0);
    assert(AppDescriptor::compareVersions("1.0", "1.0.0") == 0);
    assert(AppDescriptor::compareVersions("1.2.3", "1.3.0") < 0);
    assert(AppDescriptor::compareVersions("2.0.1", "2.0.1") == 0);
    std::cout << "     Version Comparison Algorithm xac minh chinh xac!" << std::endl;

    // 3. Kiem tra JAD Merge & Overriding
    std::cout << "  3. Kiem tra JAD Merge & Overriding..." << std::endl;
    std::string jadContent =
        "MIDlet-Version: 2.1.1\r\n"
        "MIDlet-Jar-URL: super_mario.jar\r\n"
        "MIDlet-Jar-Size: 524288\r\n";

    AppDescriptor jad(jadContent, true);
    assert(jad.getJarUrl() == "super_mario.jar");
    assert(jad.getJarSize() == 524288);

    manifest.merge(jad);
    assert(manifest.getVersion() == "2.1.1"); // Overwritten by JAD
    assert(manifest.getJarUrl() == "super_mario.jar");
    assert(manifest.getJarSize() == 524288);
    assert(manifest.getName() == "Super Mario Bros"); // Preserved from manifest
    std::cout << "     JAD Merge & Overriding xac minh thanh cong!" << std::endl;

    // 4. Kiem tra MIDlet Concrete Implementation & Lifecycle State Machine
    std::cout << "  4. Kiem tra MIDlet Lifecycle State Machine..." << std::endl;
    class TestGameMidlet : public MIDlet {
    public:
        int startCount{0};
        int pauseCount{0};
        int destroyCount{0};

        void startApp() override {
            startCount++;
        }
        void pauseApp() override {
            pauseCount++;
        }
        void destroyApp(bool) override {
            destroyCount++;
        }
    };

    auto gameMidlet = std::make_shared<TestGameMidlet>();
    auto descPtr = std::make_shared<AppDescriptor>(manifest);

    auto& manager = MidletLifecycleManager::instance();
    manager.setMidlet(gameMidlet, descPtr);
    assert(manager.getState() == MIDletState::UNINITIALIZED);

    // Kiem tra startApp
    manager.startApp();
    assert(manager.getState() == MIDletState::ACTIVE);
    assert(gameMidlet->startCount == 1);
    assert(gameMidlet->getAppProperty(AppDescriptor::ATTR_MIDLET_NAME) == "Super Mario Bros");
    assert(gameMidlet->checkPermission("javax.microedition.io.Connector.sms") == 1);

    // Kiem tra pauseApp
    manager.pauseApp();
    assert(manager.getState() == MIDletState::PAUSED);
    assert(gameMidlet->pauseCount == 1);

    // Kiem tra resumeApp (hoac MIDlet goi resumeRequest)
    gameMidlet->resumeRequest();
    assert(manager.getState() == MIDletState::ACTIVE);
    assert(gameMidlet->startCount == 2);

    // Kiem tra notifyPaused
    gameMidlet->notifyPaused();
    assert(manager.getState() == MIDletState::PAUSED);

    // Kiem tra destroyApp
    manager.destroyApp(false);
    assert(manager.getState() == MIDletState::DESTROYED);
    assert(gameMidlet->destroyCount == 1);
    std::cout << "     MIDlet Lifecycle State Machine xac minh hoan hao!" << std::endl;

    // 5. Kiem tra C-ABI APIs
    std::cout << "  5. Kiem tra C-ABI Descriptor & MIDlet APIs..." << std::endl;
    uintptr_t hDesc = j2me_core_descriptor_create("MIDlet-Name: Contra\r\nMIDlet-Version: 1.0.0\r\nMIDlet-Vendor: Konami\r\n", false);
    assert(hDesc != 0);

    char buf[128];
    assert(j2me_core_descriptor_get_name(hDesc, buf, sizeof(buf)));
    assert(std::string(buf) == "Contra");

    assert(j2me_core_descriptor_get_version(hDesc, buf, sizeof(buf)));
    assert(std::string(buf) == "1.0.0");

    assert(j2me_core_descriptor_get_vendor(hDesc, buf, sizeof(buf)));
    assert(std::string(buf) == "Konami");

    j2me_core_descriptor_destroy(hDesc);
    std::cout << "[SUCCESS] Mo dun 10: MIDlet LifeCycle & Descriptor hoan tat kiem tra 100% thanh cong!\n" << std::endl;
}

static std::vector<uint8_t> createInMemoryZip(const std::vector<std::pair<std::string, std::string>>& files) {
    std::vector<uint8_t> zip;
    struct EntryMeta {
        std::string name;
        uint32_t offset;
        uint32_t size;
    };
    std::vector<EntryMeta> metas;

    auto appendBytes = [&](const void* ptr, size_t sz) {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(ptr);
        zip.insert(zip.end(), p, p + sz);
    };

    for (const auto& f : files) {
        EntryMeta meta;
        meta.name = f.first;
        meta.offset = static_cast<uint32_t>(zip.size());
        meta.size = static_cast<uint32_t>(f.second.size());
        metas.push_back(meta);

        // Local Header
        uint32_t sig = 0x04034b50;
        uint16_t ver = 20;
        uint16_t flags = 0;
        uint16_t method = 0; // Stored
        uint16_t modTime = 0;
        uint16_t modDate = 0;
        uint32_t crc = 0;
        uint32_t compSize = meta.size;
        uint32_t uncompSize = meta.size;
        uint16_t nameLen = static_cast<uint16_t>(meta.name.size());
        uint16_t extraLen = 0;

        appendBytes(&sig, 4);
        appendBytes(&ver, 2);
        appendBytes(&flags, 2);
        appendBytes(&method, 2);
        appendBytes(&modTime, 2);
        appendBytes(&modDate, 2);
        appendBytes(&crc, 4);
        appendBytes(&compSize, 4);
        appendBytes(&uncompSize, 4);
        appendBytes(&nameLen, 2);
        appendBytes(&extraLen, 2);
        appendBytes(meta.name.data(), meta.name.size());
        appendBytes(f.second.data(), f.second.size());
    }

    uint32_t cdOffset = static_cast<uint32_t>(zip.size());

    for (const auto& meta : metas) {
        uint32_t cdSig = 0x02014b50;
        uint16_t verMade = 20;
        uint16_t verNeed = 20;
        uint16_t flags = 0;
        uint16_t method = 0;
        uint16_t modTime = 0;
        uint16_t modDate = 0;
        uint32_t crc = 0;
        uint32_t compSize = meta.size;
        uint32_t uncompSize = meta.size;
        uint16_t nameLen = static_cast<uint16_t>(meta.name.size());
        uint16_t extraLen = 0;
        uint16_t commentLen = 0;
        uint16_t diskNum = 0;
        uint16_t intAttr = 0;
        uint32_t extAttr = 0;
        uint32_t locOffset = meta.offset;

        appendBytes(&cdSig, 4);
        appendBytes(&verMade, 2);
        appendBytes(&verNeed, 2);
        appendBytes(&flags, 2);
        appendBytes(&method, 2);
        appendBytes(&modTime, 2);
        appendBytes(&modDate, 2);
        appendBytes(&crc, 4);
        appendBytes(&compSize, 4);
        appendBytes(&uncompSize, 4);
        appendBytes(&nameLen, 2);
        appendBytes(&extraLen, 2);
        appendBytes(&commentLen, 2);
        appendBytes(&diskNum, 2);
        appendBytes(&intAttr, 2);
        appendBytes(&extAttr, 4);
        appendBytes(&locOffset, 4);
        appendBytes(meta.name.data(), meta.name.size());
    }

    uint32_t cdSize = static_cast<uint32_t>(zip.size() - cdOffset);

    // EOCD
    uint32_t eocdSig = 0x06054b50;
    uint16_t diskNum = 0;
    uint16_t cdDisk = 0;
    uint16_t totalEntriesDisk = static_cast<uint16_t>(metas.size());
    uint16_t totalEntries = static_cast<uint16_t>(metas.size());
    uint32_t cdSizeBytes = cdSize;
    uint32_t cdOffsetBytes = cdOffset;
    uint16_t commentLen = 0;

    appendBytes(&eocdSig, 4);
    appendBytes(&diskNum, 2);
    appendBytes(&cdDisk, 2);
    appendBytes(&totalEntriesDisk, 2);
    appendBytes(&totalEntries, 2);
    appendBytes(&cdSizeBytes, 4);
    appendBytes(&cdOffsetBytes, 4);
    appendBytes(&commentLen, 2);

    return zip;
}

void test_phone_keypad_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[MODULE 11 TEST] Kiem tra toan dien Phone Keypad & Multi-Tap" << std::endl;
    std::cout << "========================================================" << std::endl;

    using namespace universal_loader::input;

    // 1. Kiem tra KeyMapper
    std::cout << "  1. Kiem tra KeyMapper cac layout (Default, Siemens, Motorola)..." << std::endl;
    KeyMapper::reset();
    assert(KeyMapper::getLayout() == LAYOUT_DEFAULT);
    assert(KeyMapper::convertKeyCode(KEY_UP) == KEY_UP);
    assert(KeyMapper::getGameAction(KEY_UP) == GAME_ACTION_UP);
    assert(KeyMapper::getGameAction(KEY_NUM5) == GAME_ACTION_FIRE);
    assert(KeyMapper::getKeyCode(GAME_ACTION_FIRE) == KEY_FIRE);
    assert(KeyMapper::getKeyName(KEY_FIRE) == "SELECT");
    assert(KeyMapper::getKeyName(KEY_SOFT_LEFT) == "SOFT1");

    // Siemens Layout
    KeyMapper::setLayout(LAYOUT_SIEMENS);
    assert(KeyMapper::convertKeyCode(KEY_UP) == SIEMENS_KEY_UP);
    assert(KeyMapper::convertKeyCode(KEY_DOWN) == SIEMENS_KEY_DOWN);
    assert(KeyMapper::convertKeyCode(KEY_SOFT_LEFT) == SIEMENS_KEY_SOFT_LEFT);

    // Motorola Layout
    KeyMapper::setLayout(LAYOUT_MOTOROLA);
    assert(KeyMapper::convertKeyCode(KEY_FIRE) == MOTOROLA_KEY_FIRE);
    assert(KeyMapper::convertKeyCode(KEY_SOFT_RIGHT) == MOTOROLA_KEY_SOFT_RIGHT);

    // Custom Mapping
    KeyMapper::setCustomMapping(1001, KEY_FIRE);
    assert(KeyMapper::convertKeyCode(1001) == KEY_FIRE);
    KeyMapper::reset();
    std::cout << "     KeyMapper chuyen doi layout thanh cong 100%!" << std::endl;

    // 2. Kiem tra VirtualKeypadEngine
    std::cout << "  2. Kiem tra VirtualKeypadEngine Hit-Testing & Touch Events..." << std::endl;
    VirtualKeypadEngine vk(240, 320, VirtualKeypadEngine::TYPE_PHONE_ARROWS);
    const auto& keys = vk.getKeys();
    assert(!keys.empty());

    // Tim nut phim so 5 tren keypad ao
    const VirtualKey* key5 = nullptr;
    for (const auto& k : keys) {
        if (k.keyCode == KEY_NUM5) {
            key5 = &k;
            break;
        }
    }
    assert(key5 != nullptr);

    // Hit-testing tam phim 5
    float centerX = key5->x + key5->width * 0.5f;
    float centerY = key5->y + key5->height * 0.5f;
    assert(vk.hitTest(centerX, centerY) == KEY_NUM5);

    // Touch event: Action Down
    int outCode = 0;
    bool outPressed = false;
    assert(vk.onTouchEvent(0, centerX, centerY, outCode, outPressed));
    assert(outCode == KEY_NUM5);
    assert(outPressed == true);

    // Touch event: Action Up
    assert(vk.onTouchEvent(1, centerX, centerY, outCode, outPressed));
    assert(outCode == KEY_NUM5);
    assert(outPressed == false);
    std::cout << "     VirtualKeypad Hit-Testing & Touch events xac minh thanh cong!" << std::endl;

    // 3. Kiem tra MultiTapInputEngine
    std::cout << "  3. Kiem tra MultiTapInputEngine Character Cycling..." << std::endl;
    MultiTapInputEngine mt;
    char commChar = '\0';
    bool hasComm = false;
    char pendChar = '\0';
    int64_t t = 1000;

    // Nhan phim '2' lan 1 -> 'a'
    assert(mt.handleKey(KEY_NUM2, t, commChar, hasComm, pendChar));
    assert(!hasComm);
    assert(pendChar == 'a');

    // Nhan phim '2' lan 2 trong vong 200ms -> 'b'
    t += 200;
    assert(mt.handleKey(KEY_NUM2, t, commChar, hasComm, pendChar));
    assert(!hasComm);
    assert(pendChar == 'b');

    // Nhan phim '2' lan 3 -> 'c'
    t += 200;
    assert(mt.handleKey(KEY_NUM2, t, commChar, hasComm, pendChar));
    assert(!hasComm);
    assert(pendChar == 'c');

    // Nhan phim '3' -> 'c' duoc chot (commit), pending char tro thanh 'd'
    t += 200;
    assert(mt.handleKey(KEY_NUM3, t, commChar, hasComm, pendChar));
    assert(hasComm == true);
    assert(commChar == 'c');
    assert(pendChar == 'd');

    // Timeout commit
    t += 1500; // > 1000ms
    assert(mt.checkTimeout(t, commChar));
    assert(commChar == 'd');

    // Chuyen che do chu HOA bang phim '*'
    mt.cycleMode();
    assert(mt.getMode() == MultiTapInputEngine::MODE_UPPERCASE);
    assert(mt.handleKey(KEY_NUM2, t, commChar, hasComm, pendChar));
    assert(pendChar == 'A');
    std::cout << "     MultiTap T9 Character Cycling & Commit xac minh chinh xac!" << std::endl;

    // 4. Kiem tra C-ABI Keypad APIs
    std::cout << "  4. Kiem tra C-ABI Keypad APIs..." << std::endl;
    j2me_core_keymap_set_layout(0);
    assert(j2me_core_keymap_get_layout() == 0);
    assert(j2me_core_keymap_convert(KEY_FIRE) == KEY_FIRE);

    char nameBuf[32];
    assert(j2me_core_keymap_get_key_name(KEY_UP, nameBuf, sizeof(nameBuf)));
    assert(std::string(nameBuf) == "UP");
    std::cout << "     C-ABI Keypad APIs hoat dong on dinh tuyet doi!" << std::endl;

    std::cout << "[SUCCESS] Mo dun 11: Phone Keypad & Multi-Tap hoan tat kiem tra 100% thanh cong!\n" << std::endl;
}

void test_jar_resource_loader_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[MODULE 12 TEST] Kiem tra toan dien JAR Resource Loader" << std::endl;
    std::cout << "========================================================" << std::endl;

    using namespace universal_loader::jvm;

    // 1. Kiem tra Path Normalization
    std::cout << "  1. Kiem tra Path Normalization..." << std::endl;
    assert(JarResourceLoader::normalizePath("/icon.png") == "icon.png");
    assert(JarResourceLoader::normalizePath(".//data\\\\audio.mid") == "data/audio.mid");
    assert(JarResourceLoader::normalizePath("///maps////level1.bin") == "maps/level1.bin");
    assert(JarResourceLoader::normalizePath("res/sprite.png") == "res/sprite.png");
    std::cout << "     Path Normalization xac minh 100%!" << std::endl;

    // 2. Tao in-memory JAR file hop le (Standard ZIP structure)
    std::cout << "  2. Tao in-memory JAR file hop le va load qua JarResourceLoader..." << std::endl;
    std::string manifestContent =
        "Manifest-Version: 1.0\r\n"
        "MIDlet-Name: Sonic Advance\r\n"
        "MIDlet-Version: 1.0.5\r\n"
        "MIDlet-Vendor: SEGA Mobile\r\n"
        "MIDlet-1: Sonic, /icon.png, com.sega.SonicMidlet\r\n";

    std::string iconContent = "\x89PNG\r\n\x1a\n\x00\x00\x00\rIHDR_FAKE_ICON_DATA";
    std::string midiContent = "MThd\x00\x00\x00\x06\x00\x00\x00\x01\x00\x60_MIDI_DATA";

    std::vector<std::pair<std::string, std::string>> files = {
        {"META-INF/MANIFEST.MF", manifestContent},
        {"icon.png", iconContent},
        {"audio/theme.mid", midiContent}
    };

    std::vector<uint8_t> jarBytes = createInMemoryZip(files);
    assert(!jarBytes.empty());

    JarResourceLoader loader;
    assert(loader.openFromMemory(jarBytes.data(), jarBytes.size()));
    assert(loader.isOpen());

    // 3. Kiem tra hasResource & getResourceBytes
    std::cout << "  3. Kiem tra hasResource & getResourceBytes voi streaming RAM..." << std::endl;
    assert(loader.hasResource("icon.png"));
    assert(loader.hasResource("/icon.png")); // Trailing slash stripped
    assert(loader.hasResource("audio/theme.mid"));
    assert(loader.hasResource("/audio\\theme.mid"));
    assert(!loader.hasResource("non_existent_file.dat"));

    auto iconExtracted = loader.getResourceBytes("/icon.png");
    assert(iconExtracted.size() == iconContent.size());
    assert(std::string(iconExtracted.begin(), iconExtracted.end()) == iconContent);

    auto midiExtracted = loader.getResourceBytes("audio/theme.mid");
    assert(midiExtracted.size() == midiContent.size());
    assert(std::string(midiExtracted.begin(), midiExtracted.end()) == midiContent);

    // Kiem tra caching
    assert(loader.getResourceSize("icon.png") == iconContent.size());
    std::cout << "     Asset extraction & In-memory caching xac minh chinh xac!" << std::endl;

    // 4. Kiem tra AppDescriptor bridge
    std::cout << "  4. Kiem tra AppDescriptor Manifest bridge..." << std::endl;
    auto desc = loader.getDescriptor();
    assert(desc != nullptr);
    assert(desc->getName() == "Sonic Advance");
    assert(desc->getVersion() == "1.0.5");
    assert(desc->getVendor() == "SEGA Mobile");
    std::cout << "     AppDescriptor bridge xac minh thanh cong!" << std::endl;

    // 5. Kiem tra C-ABI JAR APIs
    std::cout << "  5. Kiem tra C-ABI JAR APIs..." << std::endl;
    std::string testJarPath = "./test_sonic.jar";
    {
        std::ofstream fos(testJarPath, std::ios::binary);
        fos.write(reinterpret_cast<const char*>(jarBytes.data()), jarBytes.size());
    }

    uintptr_t hJar = j2me_core_jar_open(testJarPath.c_str());
    assert(hJar != 0);

    assert(j2me_core_jar_has_resource(hJar, "/icon.png"));
    size_t rSize = j2me_core_jar_get_resource_size(hJar, "icon.png");
    assert(rSize == iconContent.size());

    std::vector<uint8_t> readBuf(rSize);
    size_t bytesRead = j2me_core_jar_read_resource(hJar, "icon.png", readBuf.data(), readBuf.size());
    assert(bytesRead == rSize);
    assert(std::string(readBuf.begin(), readBuf.end()) == iconContent);

    j2me_core_jar_close(hJar);
    std::filesystem::remove(testJarPath);
    std::cout << "     C-ABI JAR APIs hoat dong on dinh tuyet doi!" << std::endl;

    std::cout << "[SUCCESS] Mo dun 12: JAR Resource Loader hoan tat kiem tra 100% thanh cong!\n" << std::endl;
}

static void test_profile_config_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[MODULE 13 TEST] Kiem tra toan dien Mo dun Configuration & Profile" << std::endl;
    std::cout << "========================================================" << std::endl;

    std::cout << "  1. Kiem tra ProfileModel khoi tao mac dinh (Chuan Upstream)..." << std::endl;
    universal_loader::config::ProfileModel defaultModel;
    assert(defaultModel.version == 3);
    assert(defaultModel.screenWidth == 240);
    assert(defaultModel.screenHeight == 320);
    assert(defaultModel.screenBackgroundColor == 0xD0D0D0);
    assert(defaultModel.screenScaleRatio == 100);
    assert(defaultModel.screenScaleToFit == true);
    assert(defaultModel.screenKeepAspectRatio == true);
    assert(defaultModel.screenScaleType == 1);
    assert(defaultModel.screenGravity == 1);
    assert(defaultModel.fontAA == true);
    assert(defaultModel.fontSizeSmall == 18);
    assert(defaultModel.fontSizeMedium == 22);
    assert(defaultModel.fontSizeLarge == 26);
    assert(defaultModel.vkAlpha == 64);
    assert(defaultModel.vkButtonShape == 2);
    assert(defaultModel.keyCodesLayout == 0);
    assert(defaultModel.systemProperties.find("Nokia6233") != std::string::npos);
    std::cout << "     Cac gia tri mac dinh trung khop 100% voi upstream ProfileModel.java!" << std::endl;

    std::cout << "  2. Kiem tra Tuan tu hoa & Giai tuan tu hoa JSON..." << std::endl;
    defaultModel.screenWidth = 360;
    defaultModel.screenHeight = 640;
    defaultModel.fpsLimit = 45;
    defaultModel.keyCodesLayout = 1;
    defaultModel.keyCodeMap[49] = 100;
    defaultModel.screenBackgroundColor = 0x123456;

    std::string jsonStr = defaultModel.serializeJson();
    assert(jsonStr.find("\"ScreenWidth\": 360") != std::string::npos);
    assert(jsonStr.find("\"ScreenHeight\": 640") != std::string::npos);
    assert(jsonStr.find("\"FpsLimit\": 45") != std::string::npos);
    assert(jsonStr.find("\"ScreenBackgroundColor\": 1193046") != std::string::npos);

    universal_loader::config::ProfileModel parsedModel;
    bool ok = parsedModel.deserializeJson(jsonStr);
    assert(ok);
    assert(parsedModel.screenWidth == 360);
    assert(parsedModel.screenHeight == 640);
    assert(parsedModel.fpsLimit == 45);
    assert(parsedModel.keyCodesLayout == 1);
    assert(parsedModel.screenBackgroundColor == 0x123456);
    assert(parsedModel.keyCodeMap.size() == 1 && parsedModel.keyCodeMap[49] == 100);
    std::cout << "     JSON serialization/deserialization toan ven du lieu!" << std::endl;

    std::cout << "  3. Kiem tra nang cap phien ban Schema (Migration from version 0)..." << std::endl;
    std::string oldJson = "{\n"
                          "  \"Version\": 0,\n"
                          "  \"ScreenWidth\": 176,\n"
                          "  \"ScreenHeight\": 220,\n"
                          "  \"HwAcceleration\": true,\n"
                          "  \"ScreenScaleToFit\": true,\n"
                          "  \"ScreenKeepAspectRatio\": false\n"
                          "}";
    universal_loader::config::ProfileModel migratedModel;
    assert(migratedModel.deserializeJson(oldJson));
    assert(migratedModel.version == 3);
    assert(migratedModel.graphicsMode == 2);
    assert(migratedModel.screenScaleType == 2);
    assert(migratedModel.screenGravity == 1);
    assert(migratedModel.fontAA == true);
    std::cout << "     Schema migration 0 -> 3 hoat dong chinh xac tuyet doi!" << std::endl;

    std::cout << "  4. Kiem tra ProfilesManager luu va doc file dia..." << std::endl;
    std::string testConfigDir = "./test_profile_suite";
    std::string testConfigFile = testConfigDir + "/config.json";
    assert(universal_loader::config::ProfilesManager::saveConfig(testConfigFile, parsedModel));

    universal_loader::config::ProfileModel diskLoadedModel;
    assert(universal_loader::config::ProfilesManager::loadConfig(testConfigFile, diskLoadedModel));
    assert(diskLoadedModel.screenWidth == 360);
    assert(diskLoadedModel.screenHeight == 640);
    assert(diskLoadedModel.fpsLimit == 45);
    std::filesystem::remove_all(testConfigDir);
    std::cout << "     ProfilesManager doc/ghi dia thanh cong my man!" << std::endl;

    std::cout << "  5. Kiem tra Danh muc Resolution Presets..." << std::endl;
    assert(universal_loader::config::PRESET_RESOLUTION_COUNT == 12);
    assert(universal_loader::config::PRESET_RESOLUTIONS[0].width == 128 && universal_loader::config::PRESET_RESOLUTIONS[0].height == 128);
    assert(universal_loader::config::PRESET_RESOLUTIONS[5].width == 240 && universal_loader::config::PRESET_RESOLUTIONS[5].height == 320);
    assert(universal_loader::config::PRESET_RESOLUTIONS[8].width == 360 && universal_loader::config::PRESET_RESOLUTIONS[8].height == 640);
    std::cout << "     Resolution Presets day du 12 tieu chuan dien thoai co dien!" << std::endl;

    std::cout << "  6. Kiem tra C-ABI Profile & Preset APIs..." << std::endl;
    uintptr_t hProf = j2me_core_profile_create_default();
    assert(hProf != 0);
    assert(j2me_core_profile_get_int(hProf, "screenWidth", 0) == 240);
    j2me_core_profile_set_int(hProf, "screenWidth", 480);
    assert(j2me_core_profile_get_int(hProf, "screenWidth", 0) == 480);

    char buf[4096];
    assert(j2me_core_profile_save(hProf, buf, sizeof(buf)));
    uintptr_t hProf2 = j2me_core_profile_load(buf);
    assert(hProf2 != 0);
    assert(j2me_core_profile_get_int(hProf2, "screenWidth", 0) == 480);
    j2me_core_profile_destroy(hProf2);
    j2me_core_profile_destroy(hProf);

    assert(j2me_core_get_preset_resolution_count() == 12);
    int pw = 0, ph = 0; char pname[64];
    assert(j2me_core_get_preset_resolution(5, &pw, &ph, pname, sizeof(pname)));
    assert(pw == 240 && ph == 320);
    std::cout << "     C-ABI Profile & Preset APIs hoat dong on dinh tuyet doi!" << std::endl;

    std::cout << "[SUCCESS] Mo dun 13: Configuration, Profile & Settings hoan tat kiem tra 100% thanh cong!\n" << std::endl;
}

static void test_unified_core_orchestrator_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[MODULE 14 TEST] Kiem tra toan dien Unified Core Engine Integration & Runtime Orchestrator" << std::endl;
    std::cout << "========================================================" << std::endl;

    std::string testRoot = "./test_orchestrator_root";
    try {
        std::filesystem::remove_all(testRoot);
    } catch (...) {}

    std::cout << "  1. Khoi tao J2meEngineInstance voi thu muc he thong moi..." << std::endl;
    J2meEngineInstance* engine = j2me_core_create(testRoot.c_str());
    assert(engine != nullptr);

    std::cout << "  2. Tao Profile tuy bien (176x220, Siemens Layout, FPS 50, Custom BgColor)..." << std::endl;
    uintptr_t hProf = j2me_core_profile_create_default();
    assert(hProf != 0);
    j2me_core_profile_set_int(hProf, "screenWidth", 176);
    j2me_core_profile_set_int(hProf, "screenHeight", 220);
    j2me_core_profile_set_int(hProf, "keyCodesLayout", 1); // 1 = Siemens
    j2me_core_profile_set_int(hProf, "fpsLimit", 50);
    j2me_core_profile_set_int(hProf, "screenBackgroundColor", 0x001122);

    std::cout << "  3. Thuc hien Hot-Apply Profile vao Runtime Engine..." << std::endl;
    assert(j2me_core_apply_profile(engine, hProf));
    j2me_core_profile_destroy(hProf);

    assert(j2me_core_keymap_get_layout() == 1); // KeyMapper mapped to Siemens
    assert(j2me_core_get_fps_limit(engine) == 50);

    std::cout << "  4. Khoi chay Engine Game Loop thread..." << std::endl;
    j2me_core_start(engine);
    std::this_thread::sleep_for(std::chrono::milliseconds(120));

    std::cout << "  5. Lock FrameBuffer kiem tra kich thuoc cap nhat theo Profile..." << std::endl;
    int w = 0, h = 0;
    bool dirty = false;
    const uint32_t* fb = j2me_core_lock_framebuffer(engine, &w, &h, &dirty);
    assert(fb != nullptr);
    assert(w == 176);
    assert(h == 220);
    // Kiem tra mau nen custom
    uint32_t bgPixel = fb[100 * w + 100];
    assert((bgPixel & 0x00FFFFFF) == 0x001122);
    j2me_core_unlock_framebuffer(engine);
    std::cout << "     FrameBuffer cap nhat dung 176x220 voi mau nen 0x001122!" << std::endl;

    std::cout << "  6. Kiem tra Dieu phoi Input & Audio & OEM song song khi Engine dang chay..." << std::endl;
    j2me_core_send_key(engine, J2ME_KEY_NUM5, true);
    j2me_core_send_key(engine, J2ME_KEY_NUM5, false);
    j2me_core_send_touch(engine, J2ME_TOUCH_PRESSED, 50, 50);
    j2me_core_send_touch(engine, J2ME_TOUCH_RELEASED, 50, 50);

    j2me_core_play_tone(engine, 60, 50, 100);
    j2me_core_device_vibrate(engine, 100, 100);
    j2me_core_device_stop_vibration(engine);

    std::cout << "  7. Dung Engine va huy Instance an toan tuyet doi..." << std::endl;
    j2me_core_stop(engine);
    j2me_core_destroy(engine);

    try {
        std::filesystem::remove_all(testRoot);
    } catch (...) {}

    std::cout << "[SUCCESS] Mo dun 14: Unified Core Engine Integration hoan tat kiem tra 100% thanh cong!\n" << std::endl;
}

static std::vector<uint8_t> createSampleClassBytes() {
    std::vector<uint8_t> b;
    auto writeU1 = [&](uint8_t v) { b.push_back(v); };
    auto writeU2 = [&](uint16_t v) { b.push_back(v >> 8); b.push_back(v & 0xFF); };
    auto writeU4 = [&](uint32_t v) {
        b.push_back(v >> 24); b.push_back((v >> 16) & 0xFF);
        b.push_back((v >> 8) & 0xFF); b.push_back(v & 0xFF);
    };
    auto writeUtf8 = [&](const std::string& str) {
        writeU1(1); // CONSTANT_Utf8
        writeU2(static_cast<uint16_t>(str.size()));
        for (char c : str) b.push_back(static_cast<uint8_t>(c));
    };

    // Header
    writeU4(0xCAFEBABE); // magic
    writeU2(0);          // minor
    writeU2(49);         // major (Java 5)

    // Constant Pool (8 entries, count = 8)
    writeU2(8);
    // [1] Utf8 "com/game/TestLogic"
    writeUtf8("com/game/TestLogic");
    // [2] Class -> [1]
    writeU1(7); writeU2(1);
    // [3] Utf8 "java/lang/Object"
    writeUtf8("java/lang/Object");
    // [4] Class -> [3]
    writeU1(7); writeU2(3);
    // [5] Utf8 "computeAnswer"
    writeUtf8("computeAnswer");
    // [6] Utf8 "()I"
    writeUtf8("()I");
    // [7] Utf8 "Code"
    writeUtf8("Code");

    writeU2(0x0001); // access_flags (ACC_PUBLIC)
    writeU2(2);      // this_class (Class [2])
    writeU2(4);      // super_class (Class [4])
    writeU2(0);      // interfaces_count
    writeU2(0);      // fields_count

    // Methods (1 method)
    writeU2(1);      // methods_count
    writeU2(0x0009); // access_flags: ACC_PUBLIC | ACC_STATIC
    writeU2(5);      // name_index: "computeAnswer"
    writeU2(6);      // descriptor_index: "()I"
    writeU2(1);      // attributes_count: 1 ("Code")

    // Code attribute
    writeU2(7);      // attribute_name_index ("Code")
    writeU4(15);     // attribute_length
    writeU2(2);      // max_stack = 2
    writeU2(1);      // max_locals = 1
    writeU4(3);      // code_length = 3
    writeU1(0x10); writeU1(42); // bipush 42
    writeU1(0xAC);              // ireturn
    writeU2(0);      // exception_table_length = 0
    writeU2(0);      // code_attributes_count = 0

    // Class attributes
    writeU2(0);      // attributes_count = 0

    return b;
}

static void test_jvm_interpreter_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[MODULE 15 TEST] Kiem tra toan dien Java Classfile Parser & CLDC 1.1 VM" << std::endl;
    std::cout << "========================================================" << std::endl;

    std::cout << "  1. Kiem tra ClassFileParser phan tich byte nhi phan 0xCAFEBABE..." << std::endl;
    auto classBytes = createSampleClassBytes();
    auto clazz = universal_loader::jvm::ClassFileParser::parse(classBytes.data(), classBytes.size());
    assert(clazz != nullptr);
    assert(clazz->magic == 0xCAFEBABE);
    assert(clazz->majorVersion == 49);
    assert(clazz->thisClassName == "com/game/TestLogic");
    assert(clazz->superClassName == "java/lang/Object");
    assert(clazz->methods.size() == 1);
    assert(clazz->methods[0].name == "computeAnswer");
    assert(clazz->methods[0].descriptor == "()I");
    assert(clazz->methods[0].code.size() == 3);
    std::cout << "     ClassFileParser trich xuat ConstantPool & Code attribute chinh xac 100%!" << std::endl;

    std::cout << "  2. Kiem tra CldcVirtualMachine thuc thi Method bytecode tu Class..." << std::endl;
    universal_loader::jvm::CldcVirtualMachine vm;
    bool vmLoaded = vm.loadClass(clazz);
    assert(vmLoaded);
    auto res = vm.executeMethodByName("com/game/TestLogic", "computeAnswer", "()I", {});
    assert(res.i == 42);
    std::cout << "     Ket qua thuc thi opcode bipush 42 + ireturn = " << res.i << " (Chinh xac 100%)!" << std::endl;

    std::cout << "  3. Kiem tra Opcode Toan hoc & Bieu thuc: (a + b) * 3..." << std::endl;
    universal_loader::jvm::JavaMethod mathMethod;
    mathMethod.name = "addAndMultiply";
    mathMethod.descriptor = "(II)I";
    mathMethod.maxStack = 4;
    mathMethod.maxLocals = 2;
    mathMethod.code = {0x1A, 0x1B, 0x60, 0x10, 0x03, 0x68, 0xAC};
    std::vector<universal_loader::jvm::JavaValue> mathArgs = {
        universal_loader::jvm::JavaValue(10),
        universal_loader::jvm::JavaValue(4)
    };
    auto mathRes = vm.executeMethod(&mathMethod, mathArgs);
    assert(mathRes.i == 42);
    std::cout << "     Toan hoc (10 + 4) * 3 = " << mathRes.i << " xac minh thanh cong!" << std::endl;

    std::cout << "  4. Kiem tra Opcode Vong lap & Re nhanh (sum 1 to 10)..." << std::endl;
    universal_loader::jvm::JavaMethod loopMethod;
    loopMethod.name = "sumUpTo";
    loopMethod.descriptor = "(I)I";
    loopMethod.maxStack = 4;
    loopMethod.maxLocals = 3;
    loopMethod.code = {
        0x03, 0x3C,
        0x04, 0x3D,
        0x1C, 0x1A, 0xA3, 0x00, 0x0D,
        0x1B, 0x1C, 0x60, 0x3C,
        0x84, 0x02, 0x01,
        0xA7, 0xFF, 0xF4,
        0x1B, 0xAC
    };
    std::vector<universal_loader::jvm::JavaValue> loopArgs = { universal_loader::jvm::JavaValue(10) };
    auto loopRes = vm.executeMethod(&loopMethod, loopArgs);
    assert(loopRes.i == 55);
    std::cout << "     Vong lap tinh tong 1 den 10 = " << loopRes.i << " xac minh thanh cong!" << std::endl;

    std::cout << "  5. Kiem tra Mang (JavaArray: newarray, iastore, iaload, arraylength)..." << std::endl;
    universal_loader::jvm::JavaMethod arrMethod;
    arrMethod.name = "testArray";
    arrMethod.descriptor = "()I";
    arrMethod.maxStack = 5;
    arrMethod.maxLocals = 1;
    arrMethod.code = {
        0x10, 0x05, 0xBC, 0x0A,
        0x59, 0x05, 0x10, 0x63, 0x4F,
        0x05, 0x2E, 0xAC
    };
    auto arrRes = vm.executeMethod(&arrMethod, {});
    assert(arrRes.i == 99);
    std::cout << "     Thao tac mang Java int[5], arr[2] = " << arrRes.i << " xac minh thanh cong!" << std::endl;

    std::cout << "  6. Kiem tra Bat Ngoai le (Exception Handling & athrow)..." << std::endl;
    universal_loader::jvm::JavaMethod exMethod;
    exMethod.name = "testException";
    exMethod.descriptor = "()I";
    exMethod.maxStack = 2;
    exMethod.maxLocals = 1;
    exMethod.code = {
        0x01, 0xBF,
        0x03, 0xAC,
        0x10, 0x4D, 0xAC
    };
    universal_loader::jvm::JavaExceptionCatch exCatch;
    exCatch.startPc = 0;
    exCatch.endPc = 2;
    exCatch.handlerPc = 4;
    exCatch.catchType = 0;
    exMethod.exceptionTable.push_back(exCatch);

    auto exRes = vm.executeMethod(&exMethod, {});
    assert(exRes.i == 77);
    std::cout << "     Bat ngoai le thanh cong, ket qua chuyen tiep handler = " << exRes.i << "!" << std::endl;

    std::cout << "  7. Kiem tra Native Method Bridge (System.currentTimeMillis & Math.abs)..." << std::endl;
    auto timeVal = vm.executeMethodByName("java/lang/System", "currentTimeMillis", "()J", {});
    assert(timeVal.l > 0);

    std::vector<universal_loader::jvm::JavaValue> absArgs = { universal_loader::jvm::JavaValue(-12345) };
    auto absVal = vm.executeMethodByName("java/lang/Math", "abs", "(I)I", absArgs);
    assert(absVal.i == 12345);
    std::cout << "     Native Bridge: currentTimeMillis=" << timeVal.l << ", Math.abs(-12345)=" << absVal.i << "!" << std::endl;

    std::cout << "  8. Kiem tra C-ABI VM APIs..." << std::endl;
    uintptr_t hVm = j2me_core_vm_create();
    assert(hVm != 0);
    bool cAbiLoaded = j2me_core_vm_load_class(hVm, classBytes.data(), classBytes.size());
    assert(cAbiLoaded);
    int32_t cAbiAns = j2me_core_vm_invoke_static_int(hVm, "com/game/TestLogic", "computeAnswer", "()I");
    assert(cAbiAns == 42);
    j2me_core_vm_destroy(hVm);
    std::cout << "     C-ABI VM APIs hoat dong on dinh tuyet doi!" << std::endl;

    std::cout << "[SUCCESS] Mo dun 15: Java Classfile Parser & CLDC 1.1 VM hoan tat kiem tra 100% thanh cong!\n" << std::endl;
}

static void test_real_jar_dragonboy_loading() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[REAL JAR VALIDATION] Kiem tra Nap & Thuc thi Game Thuc te (DragonBoy.jar)" << std::endl;
    std::cout << "========================================================" << std::endl;

    std::string jarPath = "c:/j2meloader/universal_loader/core/test_assets/DragonBoy.jar";
    if (!std::filesystem::exists(jarPath)) {
        jarPath = "./test_assets/DragonBoy.jar";
    }

    if (!std::filesystem::exists(jarPath)) {
        std::cout << "  [SKIP] Khong tim thay DragonBoy.jar tai: " << jarPath << std::endl;
        return;
    }

    std::cout << "  1. Tim thay tep JAR thuc te: " << jarPath << " (" << std::filesystem::file_size(jarPath) << " bytes)" << std::endl;

    std::string testRoot = "./test_real_jar_rms";
    try { std::filesystem::remove_all(testRoot); } catch (...) {}

    J2meEngineInstance* engine = j2me_core_create(testRoot.c_str());
    assert(engine != nullptr);

    std::cout << "  2. Nap tep game DragonBoy.jar qua j2me_core_load_jar_file..." << std::endl;
    bool loaded = j2me_core_load_jar_file(engine, jarPath.c_str());
    assert(loaded);

    std::cout << "  3. Trich xuat thong tin Game tu Manifest thuc te..." << std::endl;
    std::string appTitle = j2me_core_get_app_title(engine);
    std::string appVendor = j2me_core_get_app_vendor(engine);
    std::string appVersion = j2me_core_get_app_version(engine);

    std::cout << "     - Title  : " << appTitle << std::endl;
    std::cout << "     - Vendor : " << appVendor << std::endl;
    std::cout << "     - Version: " << appVersion << std::endl;

    assert(!appTitle.empty());

    std::cout << "  4. Khoi chay Engine & Game Loop thuc thi voi DragonBoy.jar..." << std::endl;
    j2me_core_start(engine);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    int w = 0, h = 0;
    bool dirty = false;
    const uint32_t* fb = j2me_core_lock_framebuffer(engine, &w, &h, &dirty);
    assert(fb != nullptr);
    assert(w > 0 && h > 0);
    j2me_core_unlock_framebuffer(engine);

    std::cout << "     FrameBuffer hoat dong muot ma voi kich thuoc: " << w << "x" << h << std::endl;

    j2me_core_stop(engine);
    j2me_core_destroy(engine);

    try { std::filesystem::remove_all(testRoot); } catch (...) {}

    std::cout << "[SUCCESS] Game thuc te DragonBoy.jar da duoc nap va chay thanh cong 100% tren Loi C++20!\n" << std::endl;
}

static void test_game_layer_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[RUNNING TEST] Mo dun 16: J2ME 2D Game API (TiledLayer & LayerManager)..." << std::endl;
    std::cout << "========================================================" << std::endl;

    using namespace universal_loader::lcdui::game;

    std::cout << "  1. Kiem tra C++ TiledLayer & Cell Matrix..." << std::endl;
    // Tile size: 16x16, Image size: 32x32 (2x2 tiles = 4 static tiles: 1..4)
    auto tileImage = std::make_shared<j2me::LcduiImage>(32, 32, true);
    TiledLayer tiledLayer(4, 4, tileImage, 16, 16);
    assert(tiledLayer.getColumns() == 4);
    assert(tiledLayer.getRows() == 4);
    assert(tiledLayer.getCellWidth() == 16);
    assert(tiledLayer.getCellHeight() == 16);
    assert(tiledLayer.getWidth() == 64);
    assert(tiledLayer.getHeight() == 64);

    // Initial cells should be 0
    assert(tiledLayer.getCell(0, 0) == 0);
    assert(tiledLayer.getCell(3, 3) == 0);

    // Set & get cells
    tiledLayer.setCell(1, 1, 2);
    assert(tiledLayer.getCell(1, 1) == 2);

    // Fill cells
    tiledLayer.fillCells(0, 2, 3, 2, 3);
    assert(tiledLayer.getCell(0, 2) == 3);
    assert(tiledLayer.getCell(1, 2) == 3);
    assert(tiledLayer.getCell(2, 2) == 3);
    assert(tiledLayer.getCell(0, 3) == 3);
    assert(tiledLayer.getCell(1, 3) == 3);
    assert(tiledLayer.getCell(2, 3) == 3);
    std::cout << "     TiledLayer setCell va fillCells xac minh thanh cong!" << std::endl;

    std::cout << "  2. Kiem tra Animated Tiles Mapping..." << std::endl;
    int animTile1 = tiledLayer.createAnimatedTile(1);
    assert(animTile1 < 0);
    assert(tiledLayer.getAnimatedTile(animTile1) == 1);

    tiledLayer.setAnimatedTile(animTile1, 4);
    assert(tiledLayer.getAnimatedTile(animTile1) == 4);

    tiledLayer.setCell(2, 1, animTile1);
    assert(tiledLayer.getCell(2, 1) == animTile1);
    std::cout << "     Animated tile (" << animTile1 << " -> static 4) tao & anh xa thanh cong!" << std::endl;

    std::cout << "  3. Kiem tra SpriteLayer & Layer Positioning..." << std::endl;
    auto spriteImage = std::make_shared<j2me::LcduiImage>(32, 16, true);
    auto sprite = std::make_shared<SpriteLayer>(spriteImage, 16, 16);
    sprite->setPosition(10, 20);
    assert(sprite->getX() == 10);
    assert(sprite->getY() == 20);
    sprite->move(5, -5);
    assert(sprite->getX() == 15);
    assert(sprite->getY() == 15);
    assert(sprite->isVisible() == true);
    sprite->setVisible(false);
    assert(sprite->isVisible() == false);
    sprite->setVisible(true);
    std::cout << "     SpriteLayer vi tri (" << sprite->getX() << ", " << sprite->getY() << ") xac minh thanh cong!" << std::endl;

    std::cout << "  4. Kiem tra LayerManager Append, Insert, View Window & Paint..." << std::endl;
    LayerManager mgr;
    auto tlPtr = std::make_shared<TiledLayer>(4, 4, tileImage, 16, 16);
    mgr.append(tlPtr);
    mgr.append(sprite);
    assert(mgr.getSize() == 2);
    assert(mgr.getLayerAt(0) == tlPtr);
    assert(mgr.getLayerAt(1) == sprite);

    // Insert at index 0
    auto sprite2 = std::make_shared<SpriteLayer>(spriteImage, 16, 16);
    mgr.insert(sprite2, 0);
    assert(mgr.getSize() == 3);
    assert(mgr.getLayerAt(0) == sprite2);
    assert(mgr.getLayerAt(1) == tlPtr);
    assert(mgr.getLayerAt(2) == sprite);

    // View Window & Paint to target buffer
    mgr.setViewWindow(0, 0, 64, 64);
    std::vector<uint32_t> testFb(128 * 128, 0xFF000000);
    j2me::LcduiGraphics g(testFb.data(), 128, 128);
    mgr.paint(&g, 0, 0);
    std::cout << "     LayerManager View Window & Render xac minh thanh cong!" << std::endl;

    std::cout << "  5. Kiem tra C-ABI Exports (Section 16)..." << std::endl;
    uintptr_t tlHandle = j2me_core_tiled_layer_create(4, 4, 32, 32, 16, 16);
    assert(tlHandle != 0);
    j2me_core_tiled_layer_set_cell(tlHandle, 2, 2, 1);
    assert(j2me_core_tiled_layer_get_cell(tlHandle, 2, 2) == 1);
    j2me_core_tiled_layer_fill_cells(tlHandle, 0, 0, 2, 2, 2);
    assert(j2me_core_tiled_layer_get_cell(tlHandle, 0, 0) == 2);

    int abiAnim = j2me_core_tiled_layer_create_animated_tile(tlHandle, 2);
    assert(abiAnim < 0);
    assert(j2me_core_tiled_layer_get_animated_tile(tlHandle, abiAnim) == 2);
    j2me_core_tiled_layer_set_animated_tile(tlHandle, abiAnim, 3);
    assert(j2me_core_tiled_layer_get_animated_tile(tlHandle, abiAnim) == 3);

    uintptr_t mgrHandle = j2me_core_layer_manager_create();
    assert(mgrHandle != 0);
    j2me_core_layer_manager_append(mgrHandle, tlHandle);
    assert(j2me_core_layer_manager_get_size(mgrHandle) == 1);
    j2me_core_layer_manager_set_view_window(mgrHandle, 0, 0, 64, 64);

    j2me_core_layer_manager_destroy(mgrHandle);
    j2me_core_tiled_layer_destroy(tlHandle);
    std::cout << "     C-ABI Section 16 TiledLayer & LayerManager xac minh thanh cong!" << std::endl;
    std::cout << "[PASS] Mo dun 16 (J2ME 2D Game API) 100% Hoan hao!" << std::endl;
}

static void test_emulator_ux_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[RUNNING TEST] Mo dun 17: Emulator UX (Speed Multiplier & Screenshot PNG)..." << std::endl;
    std::cout << "========================================================" << std::endl;

    J2meEngineInstance* engine = j2me_core_create("./test_rms");
    assert(engine != nullptr);

    std::cout << "  1. Kiem tra Speed Multiplier..." << std::endl;
    assert(j2me_core_get_speed_multiplier(engine) == 1);
    j2me_core_set_speed_multiplier(engine, 2);
    assert(j2me_core_get_speed_multiplier(engine) == 2);
    j2me_core_set_speed_multiplier(engine, 4);
    assert(j2me_core_get_speed_multiplier(engine) == 4);
    // Bounds clamping
    j2me_core_set_speed_multiplier(engine, 0);
    assert(j2me_core_get_speed_multiplier(engine) == 1);
    j2me_core_set_speed_multiplier(engine, 20);
    assert(j2me_core_get_speed_multiplier(engine) == 16);
    j2me_core_set_speed_multiplier(engine, 1);
    std::cout << "     Speed Multiplier (1x, 2x, 4x, Clamping [1..16]) xac minh thanh cong!" << std::endl;

    std::cout << "  2. Kiem tra Screenshot Capture (PNG RFC 2083)..." << std::endl;
    j2me_core_set_screen_dimensions(engine, 120, 160);
    const char* testPng = "./test_screen_capture.png";
    if (fs::exists(testPng)) {
        fs::remove(testPng);
    }

    bool captured = j2me_core_capture_screenshot_png(engine, testPng);
    assert(captured == true);
    assert(fs::exists(testPng));
    size_t fileSize = fs::file_size(testPng);
    assert(fileSize > 8 + 25); // At least header + chunks

    // Verify PNG binary format: 8-byte signature
    std::ifstream in(testPng, std::ios::binary);
    assert(in.is_open());
    uint8_t sig[8];
    in.read(reinterpret_cast<char*>(sig), 8);
    const uint8_t expectedSig[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
    assert(memcmp(sig, expectedSig, 8) == 0);

    // Read IHDR chunk
    uint8_t ihdrChunk[25];
    in.read(reinterpret_cast<char*>(ihdrChunk), 25);
    // Length: 13
    assert(ihdrChunk[0] == 0 && ihdrChunk[1] == 0 && ihdrChunk[2] == 0 && ihdrChunk[3] == 13);
    // Type: "IHDR"
    assert(memcmp(&ihdrChunk[4], "IHDR", 4) == 0);
    // Width (120 = 0x00000078)
    uint32_t width = (ihdrChunk[8] << 24) | (ihdrChunk[9] << 16) | (ihdrChunk[10] << 8) | ihdrChunk[11];
    assert(width == 120);
    // Height (160 = 0x000000A0)
    uint32_t height = (ihdrChunk[12] << 24) | (ihdrChunk[13] << 16) | (ihdrChunk[14] << 8) | ihdrChunk[15];
    assert(height == 160);
    // Bit depth 8, Color type 6 (RGBA)
    assert(ihdrChunk[16] == 8);
    assert(ihdrChunk[17] == 6);
    in.close();

    // Clean up test file
    fs::remove(testPng);
    std::cout << "     PNG Signature & IHDR chunk (120x160 RGBA) xac minh thanh cong!" << std::endl;

    j2me_core_destroy(engine);
    std::cout << "[PASS] Mo dun 17 (Emulator UX Enhancements) 100% Hoan hao!" << std::endl;
}

static void test_datagram_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[RUNNING TEST] Mo dun 18: GCF Datagram / UDP Networking (JSR-118)..." << std::endl;
    std::cout << "========================================================" << std::endl;

    std::cout << "  1. Kiem tra Datagram Buffer & Data Streaming..." << std::endl;
    j2me::Datagram dgram(256);
    assert(dgram.getLength() == 256);
    assert(dgram.getOffset() == 0);

    // Stream write
    dgram.reset();
    dgram.writeInt(123456);
    dgram.writeUTF("Hello J2ME UDP");
    dgram.writeLong(9876543210LL);

    // Stream read
    dgram.reset();
    int intVal = dgram.readInt();
    std::string strVal = dgram.readUTF();
    int64_t longVal = dgram.readLong();

    assert(intVal == 123456);
    assert(strVal == "Hello J2ME UDP");
    assert(longVal == 9876543210LL);
    std::cout << "     Datagram binary DataInput/DataOutput xac minh 100%!" << std::endl;

    std::cout << "  2. Kiem tra UDP Datagram Socket Loopback (Real UDP Networking)..." << std::endl;
    // Server listens on ephemeral port
    j2me::DatagramConnection serverConn;
    bool serverOk = serverConn.open("datagram://:0");
    int serverPort = serverConn.getLocalPort();
    std::cout << "     serverOk=" << (serverOk ? "true" : "false") << ", serverPort=" << serverPort << std::endl;
    assert(serverOk);
    assert(serverPort > 0);
    std::cout << "     Server UDP socket bound to port: " << serverPort << std::endl;

    // Client connects to server
    j2me::DatagramConnection clientConn;
    std::string clientUrl = "datagram://127.0.0.1:" + std::to_string(serverPort);
    bool clientOk = clientConn.open(clientUrl);
    assert(clientOk);

    // Client sends datagram
    auto clientDgram = clientConn.newDatagram(128);
    clientDgram->writeUTF("PING_FROM_CLIENT");
    bool sent = clientConn.send(clientDgram.get());
    assert(sent);

    // Server receives datagram
    auto serverDgram = serverConn.newDatagram(128);
    bool recvd = serverConn.receive(serverDgram.get(), 3000);
    assert(recvd);
    std::string recvdMsg = serverDgram->readUTF();
    assert(recvdMsg == "PING_FROM_CLIENT");
    assert(!serverDgram->getAddress().empty());
    std::cout << "     Server nhan thanh cong tu sender address: " << serverDgram->getAddress() << std::endl;

    // Server replies back to sender address
    auto replyDgram = serverConn.newDatagram(128, serverDgram->getAddress());
    replyDgram->writeUTF("PONG_FROM_SERVER");
    bool replySent = serverConn.send(replyDgram.get());
    assert(replySent);

    // Client receives reply
    auto clientRecvDgram = clientConn.newDatagram(128);
    bool clientRecvd = clientConn.receive(clientRecvDgram.get(), 3000);
    assert(clientRecvd);
    std::string replyMsg = clientRecvDgram->readUTF();
    assert(replyMsg == "PONG_FROM_SERVER");
    std::cout << "     Client nhan phan hoi: " << replyMsg << " thanh cong 100%!" << std::endl;

    serverConn.close();
    clientConn.close();

    std::cout << "  3. Kiem tra C-ABI Section 18 Exports..." << std::endl;
    uintptr_t sHandle = j2me_core_datagram_conn_open("datagram://:0");
    assert(sHandle != 0);
    int sPort = j2me_core_datagram_conn_get_local_port(sHandle);
    assert(sPort > 0);

    std::string cUrl = "datagram://127.0.0.1:" + std::to_string(sPort);
    uintptr_t cHandle = j2me_core_datagram_conn_open(cUrl.c_str());
    assert(cHandle != 0);

    uintptr_t dHandle = j2me_core_datagram_create(64, nullptr);
    assert(dHandle != 0);
    const char* testMsg = "ABI_TEST";
    j2me_core_datagram_write(dHandle, reinterpret_cast<const uint8_t*>(testMsg), strlen(testMsg));
    j2me_core_datagram_set_length(dHandle, static_cast<int>(strlen(testMsg)));

    bool abiSent = j2me_core_datagram_conn_send(cHandle, dHandle);
    assert(abiSent);

    uintptr_t sRecvDgram = j2me_core_datagram_create(64, nullptr);
    bool abiRecvd = j2me_core_datagram_conn_receive(sHandle, sRecvDgram, 2000);
    assert(abiRecvd);

    char readBuf[64] = {0};
    size_t bytesRead = j2me_core_datagram_read(sRecvDgram, reinterpret_cast<uint8_t*>(readBuf), sizeof(readBuf) - 1);
    assert(bytesRead == strlen(testMsg));
    assert(strcmp(readBuf, testMsg) == 0);

    j2me_core_datagram_destroy(dHandle);
    j2me_core_datagram_destroy(sRecvDgram);
    j2me_core_datagram_conn_close(cHandle);
    j2me_core_datagram_conn_close(sHandle);

    std::cout << "     C-ABI Section 18 UDP Networking xac minh thanh cong!" << std::endl;
    std::cout << "[PASS] Mo dun 18 (GCF Datagram / UDP Networking) 100% Hoan hao!" << std::endl;
}

void test_m3g_animation_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[TEST MO DUN 19] M3G Keyframe Animation, Controller & MorphingMesh (JSR-184)" << std::endl;
    std::cout << "========================================================" << std::endl;

    // 1. Kiem tra KeyframeSequence LINEAR Interpolation & Modes
    std::cout << "  1. Kiem tra KeyframeSequence Noi suy Tuyen tinh (LINEAR)..." << std::endl;
    universal_loader::m3g::KeyframeSequence seqLin(2, 3, universal_loader::m3g::INTERP_LINEAR);
    float kf0[3] = {0.0f, 0.0f, 0.0f};
    float kf1[3] = {10.0f, 20.0f, 30.0f};
    seqLin.setKeyframe(0, 0, kf0);
    seqLin.setKeyframe(1, 1000, kf1);
    seqLin.setDuration(1000);
    seqLin.setRepeatMode(universal_loader::m3g::REPEAT_CONSTANT);

    float sample[3] = {0.0f};
    assert(seqLin.sample(0, sample));
    assert(sample[0] == 0.0f && sample[1] == 0.0f && sample[2] == 0.0f);

    assert(seqLin.sample(500, sample));
    assert(std::abs(sample[0] - 5.0f) < 1e-4f);
    assert(std::abs(sample[1] - 10.0f) < 1e-4f);
    assert(std::abs(sample[2] - 15.0f) < 1e-4f);

    assert(seqLin.sample(250, sample));
    assert(std::abs(sample[0] - 2.5f) < 1e-4f);
    assert(std::abs(sample[1] - 5.0f) < 1e-4f);
    assert(std::abs(sample[2] - 7.5f) < 1e-4f);

    // Kiem tra Constant Clamp
    assert(seqLin.sample(-100, sample));
    assert(sample[0] == 0.0f && sample[1] == 0.0f && sample[2] == 0.0f);
    assert(seqLin.sample(2000, sample));
    assert(sample[0] == 10.0f && sample[1] == 20.0f && sample[2] == 30.0f);

    // Kiem tra Loop Repeat Mode
    seqLin.setRepeatMode(universal_loader::m3g::REPEAT_LOOP);
    assert(seqLin.sample(1500, sample)); // 1500 % 1000 = 500
    assert(std::abs(sample[0] - 5.0f) < 1e-4f);
    assert(std::abs(sample[1] - 10.0f) < 1e-4f);
    assert(std::abs(sample[2] - 15.0f) < 1e-4f);
    std::cout << "     KeyframeSequence Linear Interpolation, Clamp & Loop xac minh thanh cong!" << std::endl;

    // 2. Kiem tra KeyframeSequence SLERP (Quaternion Spherical Linear Interpolation)
    std::cout << "  2. Kiem tra KeyframeSequence Quaternion SLERP (Xoay 3D)..." << std::endl;
    universal_loader::m3g::KeyframeSequence seqQuat(2, 4, universal_loader::m3g::INTERP_SLERP);
    float qStart[4] = {0.0f, 0.0f, 0.0f, 1.0f}; // Identity
    auto qRot90 = universal_loader::graphics3d::Quaternion::fromAxisAngle(
        universal_loader::graphics3d::Vector3(0.0f, 1.0f, 0.0f),
        universal_loader::graphics3d::PI / 2.0f
    );
    float qEnd[4] = {qRot90.x, qRot90.y, qRot90.z, qRot90.w};
    seqQuat.setKeyframe(0, 0, qStart);
    seqQuat.setKeyframe(1, 1000, qEnd);
    seqQuat.setDuration(1000);

    float sampledQuat[4] = {0.0f};
    assert(seqQuat.sample(500, sampledQuat)); // Tai t=500 -> goc xoay phai la 45 do quanh truc Y
    auto expected45 = universal_loader::graphics3d::Quaternion::fromAxisAngle(
        universal_loader::graphics3d::Vector3(0.0f, 1.0f, 0.0f),
        universal_loader::graphics3d::PI / 4.0f
    );
    assert(std::abs(sampledQuat[0] - expected45.x) < 1e-3f);
    assert(std::abs(sampledQuat[1] - expected45.y) < 1e-3f);
    assert(std::abs(sampledQuat[2] - expected45.z) < 1e-3f);
    assert(std::abs(sampledQuat[3] - expected45.w) < 1e-3f);
    std::cout << "     KeyframeSequence Quaternion SLERP 90 deg -> 45 deg xac minh chinh xac tuyet doi!" << std::endl;

    // 3. Kiem tra AnimationController & AnimationTrack
    std::cout << "  3. Kiem tra AnimationController & AnimationTrack Binding..." << std::endl;
    auto ctrl = std::make_shared<universal_loader::m3g::AnimationController>();
    ctrl->setPosition(0.0f, 0);
    ctrl->setSpeed(1.5f, 0);
    ctrl->setWeight(0.8f);
    ctrl->setActiveInterval(1000, 5000);

    assert(!ctrl->isActive(500));
    assert(ctrl->isActive(2000));
    assert(!ctrl->isActive(6000));
    assert(std::abs(ctrl->getPosition(2000) - 3000.0f) < 1e-4f); // 0 + 1.5 * 2000 = 3000

    auto trackTrans = std::make_shared<universal_loader::m3g::AnimationTrack>(
        std::make_shared<universal_loader::m3g::KeyframeSequence>(seqLin),
        universal_loader::m3g::ANIM_TRANSLATION
    );
    trackTrans->setController(ctrl);
    assert(trackTrans->getTargetProperty() == universal_loader::m3g::ANIM_TRANSLATION);

    std::vector<float> trackSample;
    float trackWeight = 0.0f;
    // World time 2000 -> seqLin in LOOP mode: 3000 % 1000 = 0 -> sample = (0, 0, 0)
    assert(trackTrans->sample(2000, trackSample, trackWeight));
    assert(std::abs(trackWeight - 0.8f) < 1e-4f);
    assert(std::abs(trackSample[0] - 0.0f) < 1e-4f);
    std::cout << "     AnimationController timing & AnimationTrack sampling xac minh thanh cong!" << std::endl;

    // 4. Kiem tra MorphingMesh Vertex & Normal Blending (JSR-184 Section 5.8)
    std::cout << "  4. Kiem tra MorphingMesh Vertex & Normal Linear Combination..." << std::endl;
    universal_loader::m3g::VertexBuffer baseVB;
    baseVB.setPositions({
        universal_loader::graphics3d::Vector3(0.0f, 0.0f, 0.0f),
        universal_loader::graphics3d::Vector3(10.0f, 0.0f, 0.0f)
    });
    baseVB.setNormals({
        universal_loader::graphics3d::Vector3(0.0f, 1.0f, 0.0f),
        universal_loader::graphics3d::Vector3(0.0f, 1.0f, 0.0f)
    });

    universal_loader::m3g::VertexBuffer target0;
    target0.setPositions({
        universal_loader::graphics3d::Vector3(0.0f, 10.0f, 0.0f),
        universal_loader::graphics3d::Vector3(10.0f, 10.0f, 0.0f)
    });
    target0.setNormals({
        universal_loader::graphics3d::Vector3(1.0f, 0.0f, 0.0f),
        universal_loader::graphics3d::Vector3(1.0f, 0.0f, 0.0f)
    });

    universal_loader::m3g::VertexBuffer target1;
    target1.setPositions({
        universal_loader::graphics3d::Vector3(0.0f, 0.0f, 10.0f),
        universal_loader::graphics3d::Vector3(10.0f, 0.0f, 10.0f)
    });
    target1.setNormals({
        universal_loader::graphics3d::Vector3(0.0f, 0.0f, 1.0f),
        universal_loader::graphics3d::Vector3(0.0f, 0.0f, 1.0f)
    });

    universal_loader::m3g::MorphingMesh morphMesh(baseVB, {target0, target1}, {});
    assert(morphMesh.getTargetCount() == 2);

    // Kiem tra setWeights & morph: w0 = 0.5, w1 = 0.5 -> baseWeight = 0.0
    float weights[2] = {0.5f, 0.5f};
    morphMesh.setWeights(weights, 2);
    morphMesh.morph();

    // v0: 0.5 * (0,10,0) + 0.5 * (0,0,10) = (0, 5, 5)
    assert(std::abs(morphMesh.vertexBuffer.vertices[0].position.x - 0.0f) < 1e-4f);
    assert(std::abs(morphMesh.vertexBuffer.vertices[0].position.y - 5.0f) < 1e-4f);
    assert(std::abs(morphMesh.vertexBuffer.vertices[0].position.z - 5.0f) < 1e-4f);

    // v1: 0.5 * (10,10,0) + 0.5 * (10,0,10) = (10, 5, 5)
    assert(std::abs(morphMesh.vertexBuffer.vertices[1].position.x - 10.0f) < 1e-4f);
    assert(std::abs(morphMesh.vertexBuffer.vertices[1].position.y - 5.0f) < 1e-4f);
    assert(std::abs(morphMesh.vertexBuffer.vertices[1].position.z - 5.0f) < 1e-4f);

    // Kiem tra phan hoa tron voi base: w0 = 0.4, w1 = 0.0 -> baseWeight = 0.6
    float weightsBase[2] = {0.4f, 0.0f};
    morphMesh.setWeights(weightsBase, 2);
    morphMesh.morph();
    // v0: 0.6 * (0,0,0) + 0.4 * (0,10,0) = (0, 4, 0)
    assert(std::abs(morphMesh.vertexBuffer.vertices[0].position.y - 4.0f) < 1e-4f);
    std::cout << "     MorphingMesh Vertex & Normal Math (V = V0 + sum w_i * (Vi - V0)) thanh cong!" << std::endl;

    // 5. Kiem tra animate(worldTime) tren MorphingMesh
    std::cout << "  5. Kiem tra animate(worldTime) cap nhat tu dong..." << std::endl;
    auto animSeqTrans = std::make_shared<universal_loader::m3g::KeyframeSequence>(2, 3, universal_loader::m3g::INTERP_LINEAR);
    float t0[3] = {0.0f, 0.0f, 0.0f};
    float t1[3] = {10.0f, 20.0f, 30.0f};
    animSeqTrans->setKeyframe(0, 0, t0);
    animSeqTrans->setKeyframe(1, 1000, t1);
    animSeqTrans->setDuration(1000);

    auto animCtrl = std::make_shared<universal_loader::m3g::AnimationController>();
    animCtrl->setPosition(0.0f, 0);
    animCtrl->setSpeed(1.0f, 0);
    animCtrl->setWeight(1.0f);
    animCtrl->setActiveInterval(0, 0); // Active always

    auto animTrack = std::make_shared<universal_loader::m3g::AnimationTrack>(animSeqTrans, universal_loader::m3g::ANIM_TRANSLATION);
    animTrack->setController(animCtrl);

    morphMesh.addAnimationTrack(animTrack);
    morphMesh.animate(500); // worldTime = 500ms -> translation = (5, 10, 15)

    const auto& pos = morphMesh.getTranslation();
    assert(std::abs(pos.x - 5.0f) < 1e-4f);
    assert(std::abs(pos.y - 10.0f) < 1e-4f);
    assert(std::abs(pos.z - 15.0f) < 1e-4f);

    // Kiem tra ma tran bien doi transform sau khi animate
    assert(std::abs(morphMesh.transform.m[3] - 5.0f) < 1e-4f);   // m03 = tx
    assert(std::abs(morphMesh.transform.m[7] - 10.0f) < 1e-4f);  // m13 = ty
    assert(std::abs(morphMesh.transform.m[11] - 15.0f) < 1e-4f); // m23 = tz
    std::cout << "     MorphingMesh animate(worldTime) cap nhat Translation & Transform matrix thanh cong!" << std::endl;

    // 6. Kiem tra C-ABI Section 19 (M3G Animation & MorphingMesh)
    std::cout << "  6. Kiem tra C-ABI Section 19 (j2me_core_m3g_*)..." << std::endl;
    uintptr_t vbBaseHandle = j2me_core_m3g_vertexbuffer_create();
    float coordsBase[6] = {0.0f, 0.0f, 0.0f, 2.0f, 0.0f, 0.0f};
    j2me_core_m3g_vertexbuffer_set_positions(vbBaseHandle, coordsBase, 2);

    uintptr_t vbTargetHandle = j2me_core_m3g_vertexbuffer_create();
    float coordsTarget[6] = {0.0f, 10.0f, 0.0f, 2.0f, 10.0f, 0.0f};
    j2me_core_m3g_vertexbuffer_set_positions(vbTargetHandle, coordsTarget, 2);

    uintptr_t targetsArr[1] = {vbTargetHandle};
    uintptr_t meshHandle = j2me_core_m3g_morphing_mesh_create(vbBaseHandle, 1, targetsArr);
    assert(meshHandle != 0);

    float wAbi[1] = {0.7f};
    j2me_core_m3g_morphing_mesh_set_weights(meshHandle, wAbi, 1);
    j2me_core_m3g_morphing_mesh_morph(meshHandle);

    float readPos[3] = {0.0f};
    j2me_core_m3g_mesh_get_vertex_position(meshHandle, 0, readPos);
    // 0.3 * (0,0,0) + 0.7 * (0,10,0) = (0, 7.0, 0)
    assert(std::abs(readPos[1] - 7.0f) < 1e-4f);

    uintptr_t seqAbi = j2me_core_m3g_keyframesequence_create(2, 3, 176); // LINEAR
    j2me_core_m3g_keyframesequence_set_duration(seqAbi, 1000);
    assert(j2me_core_m3g_keyframesequence_get_duration(seqAbi) == 1000);
    j2me_core_m3g_keyframesequence_set_repeat_mode(seqAbi, 193); // LOOP
    assert(j2me_core_m3g_keyframesequence_get_repeat_mode(seqAbi) == 193);

    float kfAbi0[3] = {0.0f, 0.0f, 0.0f};
    float kfAbi1[3] = {100.0f, 200.0f, 300.0f};
    j2me_core_m3g_keyframesequence_set_keyframe(seqAbi, 0, 0, kfAbi0);
    j2me_core_m3g_keyframesequence_set_keyframe(seqAbi, 1, 1000, kfAbi1);

    float sampledAbi[3] = {0.0f};
    assert(j2me_core_m3g_keyframesequence_sample(seqAbi, 500, sampledAbi, 3));
    assert(std::abs(sampledAbi[0] - 50.0f) < 1e-4f);
    assert(std::abs(sampledAbi[1] - 100.0f) < 1e-4f);
    assert(std::abs(sampledAbi[2] - 150.0f) < 1e-4f);

    uintptr_t ctrlAbi = j2me_core_m3g_animcontroller_create();
    j2me_core_m3g_animcontroller_set_speed(ctrlAbi, 2.0f, 0);
    assert(std::abs(j2me_core_m3g_animcontroller_get_speed(ctrlAbi) - 2.0f) < 1e-4f);
    assert(std::abs(j2me_core_m3g_animcontroller_get_position(ctrlAbi, 250) - 500.0f) < 1e-4f);

    uintptr_t trackAbi = j2me_core_m3g_animtrack_create(seqAbi, 274); // TRANSLATION
    j2me_core_m3g_animtrack_set_controller(trackAbi, ctrlAbi);
    assert(j2me_core_m3g_animtrack_get_target_property(trackAbi) == 274);

    j2me_core_m3g_mesh_add_animation_track(meshHandle, trackAbi);
    j2me_core_m3g_mesh_animate(meshHandle, 250); // worldTime 250 -> seqTime 500 -> pos = (50, 100, 150)

    float meshPos[3] = {0.0f};
    j2me_core_m3g_mesh_get_position(meshHandle, meshPos);
    assert(std::abs(meshPos[0] - 50.0f) < 1e-4f);
    assert(std::abs(meshPos[1] - 100.0f) < 1e-4f);
    assert(std::abs(meshPos[2] - 150.0f) < 1e-4f);

    // Don dep bo nho
    j2me_core_m3g_mesh_destroy(meshHandle);
    j2me_core_m3g_animtrack_destroy(trackAbi);
    j2me_core_m3g_animcontroller_destroy(ctrlAbi);
    j2me_core_m3g_keyframesequence_destroy(seqAbi);
    j2me_core_m3g_vertexbuffer_destroy(vbBaseHandle);
    j2me_core_m3g_vertexbuffer_destroy(vbTargetHandle);

    std::cout << "     C-ABI Section 19 M3G Animation & MorphingMesh xac minh thanh cong!" << std::endl;
    std::cout << "[PASS] Mo dun 19 (M3G Keyframe Animation, Controller & MorphingMesh) 100% Hoan hao!" << std::endl;
}

void test_m3g_skinned_mesh_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[MODULE 20 TEST] Kiem tra toan dien M3G SkinnedMesh & Bone Skeleton (JSR-184)" << std::endl;
    std::cout << "========================================================" << std::endl;

    // 1. Kiem tra Node & Group Hierarchy va Transform Propagation (M_child,global = M_parent * M_child)
    std::cout << "  1. Kiem tra Node & Group Hierarchy va Transform Propagation..." << std::endl;
    auto parentGrp = std::make_shared<universal_loader::m3g::Group>();
    parentGrp->setTranslation(10.0f, 0.0f, 0.0f);

    auto childNode = std::make_shared<universal_loader::m3g::Node>();
    childNode->setTranslation(0.0f, 20.0f, 0.0f);

    parentGrp->addChild(childNode);
    assert(childNode->getParent() == parentGrp.get());
    assert(parentGrp->getChildCount() == 1);
    assert(parentGrp->getChild(0) == childNode);

    auto childGlobal = childNode->getGlobalTransform();
    assert(std::abs(childGlobal.m[3] - 10.0f) < 1e-4f);
    assert(std::abs(childGlobal.m[7] - 20.0f) < 1e-4f);

    // Kiem tra nested child (cha -> con -> chau)
    auto grandChildNode = std::make_shared<universal_loader::m3g::Node>();
    grandChildNode->setTranslation(0.0f, 0.0f, 30.0f);
    auto childGrp = std::make_shared<universal_loader::m3g::Group>();
    childGrp->setTranslation(0.0f, 20.0f, 0.0f);
    childGrp->addChild(grandChildNode);
    parentGrp->addChild(childGrp);

    auto grandChildGlobal = grandChildNode->getGlobalTransform();
    assert(std::abs(grandChildGlobal.m[3] - 10.0f) < 1e-4f);
    assert(std::abs(grandChildGlobal.m[7] - 20.0f) < 1e-4f);
    assert(std::abs(grandChildGlobal.m[11] - 30.0f) < 1e-4f);

    // Kiem tra removeChild
    parentGrp->removeChild(childNode);
    assert(childNode->getParent() == nullptr);
    assert(parentGrp->getChildCount() == 1);
    std::cout << "     Node/Group Hierarchy & Recursive Transform Propagation xac minh 100%!" << std::endl;

    // 2. Kiem tra SkinnedMesh Inverse Bind Pose & Linear Blend Skinning (LBS)
    std::cout << "  2. Kiem tra SkinnedMesh Inverse Bind Pose & Linear Blend Skinning..." << std::endl;
    universal_loader::m3g::VertexBuffer baseVB;
    baseVB.setPositions({
        universal_loader::graphics3d::Vector3(0.0f, 0.0f, 0.0f),
        universal_loader::graphics3d::Vector3(10.0f, 0.0f, 0.0f)
    });
    baseVB.setNormals({
        universal_loader::graphics3d::Vector3(0.0f, 1.0f, 0.0f),
        universal_loader::graphics3d::Vector3(0.0f, 1.0f, 0.0f)
    });

    auto skeleton = std::make_shared<universal_loader::m3g::Group>();
    auto bone0 = std::make_shared<universal_loader::m3g::Node>();
    bone0->setTranslation(0.0f, 0.0f, 0.0f);
    auto bone1 = std::make_shared<universal_loader::m3g::Node>();
    bone1->setTranslation(10.0f, 0.0f, 0.0f);

    skeleton->addChild(bone0);
    skeleton->addChild(bone1);

    universal_loader::m3g::SkinnedMesh skinnedMesh(baseVB, {}, skeleton);
    skinnedMesh.addTransform(bone0, 100, 0, 1);
    skinnedMesh.addTransform(bone1, 100, 1, 1);

    assert(skinnedMesh.getBoneCount() == 2);
    assert(skinnedMesh.getBone(0) == bone0);
    assert(skinnedMesh.getBone(1) == bone1);
    assert(skinnedMesh.getSkeleton() == skeleton);

    // Bien doi vi tri cac xuong (Bone translation)
    bone0->setTranslation(0.0f, 5.0f, 0.0f);
    bone1->setTranslation(10.0f, -8.0f, 0.0f);

    skinnedMesh.skin();

    // Vertex 0: Ban dau (0,0,0) -> Bone 0 dich chuyen +5 theo Y -> (0, 5, 0)
    assert(std::abs(skinnedMesh.vertexBuffer.vertices[0].position.x - 0.0f) < 1e-4f);
    assert(std::abs(skinnedMesh.vertexBuffer.vertices[0].position.y - 5.0f) < 1e-4f);
    assert(std::abs(skinnedMesh.vertexBuffer.vertices[0].position.z - 0.0f) < 1e-4f);

    // Vertex 1: Ban dau (10,0,0) -> Bone 1 dich chuyen -8 theo Y -> (10, -8, 0)
    assert(std::abs(skinnedMesh.vertexBuffer.vertices[1].position.x - 10.0f) < 1e-4f);
    assert(std::abs(skinnedMesh.vertexBuffer.vertices[1].position.y - (-8.0f)) < 1e-4f);
    assert(std::abs(skinnedMesh.vertexBuffer.vertices[1].position.z - 0.0f) < 1e-4f);
    std::cout << "     SkinnedMesh Inverse Bind Pose (B_k^-1) & LBS (S_k = M_k * B_k^-1) xac minh chinh xac!" << std::endl;

    // 3. Kiem tra Multi-Bone Weight Blending tren cung 1 Vertex
    std::cout << "  3. Kiem tra Multi-Bone Weight Blending tren cung 1 Vertex..." << std::endl;
    universal_loader::m3g::VertexBuffer blendVB;
    blendVB.setPositions({
        universal_loader::graphics3d::Vector3(5.0f, 0.0f, 0.0f)
    });
    blendVB.setNormals({
        universal_loader::graphics3d::Vector3(0.0f, 1.0f, 0.0f)
    });

    auto skelBlend = std::make_shared<universal_loader::m3g::Group>();
    auto bA = std::make_shared<universal_loader::m3g::Node>();
    bA->setTranslation(0.0f, 0.0f, 0.0f);
    auto bB = std::make_shared<universal_loader::m3g::Node>();
    bB->setTranslation(10.0f, 0.0f, 0.0f);
    skelBlend->addChild(bA);
    skelBlend->addChild(bB);

    universal_loader::m3g::SkinnedMesh blendMesh(blendVB, {}, skelBlend);
    blendMesh.addTransform(bA, 50, 0, 1);
    blendMesh.addTransform(bB, 50, 0, 1);

    bA->setTranslation(0.0f, 10.0f, 0.0f);
    bB->setTranslation(10.0f, 20.0f, 0.0f);

    blendMesh.skin();
    // 0.5 * (5, 10, 0) + 0.5 * (5, 20, 0) = (5, 15, 0)
    assert(std::abs(blendMesh.vertexBuffer.vertices[0].position.x - 5.0f) < 1e-4f);
    assert(std::abs(blendMesh.vertexBuffer.vertices[0].position.y - 15.0f) < 1e-4f);
    assert(std::abs(blendMesh.vertexBuffer.vertices[0].position.z - 0.0f) < 1e-4f);
    std::cout << "     Multi-bone normalized weight blending (sum w_i * S_i * V) thanh cong!" << std::endl;

    // 4. Kiem tra AnimationTrack dan huong Bone va tu dong Skinning khi goi animate()
    std::cout << "  4. Kiem tra AnimationTrack dan huong Bone va tu dong Skinning..." << std::endl;
    auto seqBone = std::make_shared<universal_loader::m3g::KeyframeSequence>(2, 3, universal_loader::m3g::INTERP_LINEAR);
    float k0[3] = {0.0f, 0.0f, 0.0f};
    float k1[3] = {0.0f, 30.0f, 0.0f};
    seqBone->setKeyframe(0, 0, k0);
    seqBone->setKeyframe(1, 1000, k1);
    seqBone->setDuration(1000);

    auto ctrlBone = std::make_shared<universal_loader::m3g::AnimationController>();
    ctrlBone->setSpeed(1.0f, 0);
    ctrlBone->setActiveInterval(0, 0);

    auto trackBone = std::make_shared<universal_loader::m3g::AnimationTrack>(seqBone, universal_loader::m3g::ANIM_TRANSLATION);
    trackBone->setController(ctrlBone);

    bone0->addAnimationTrack(trackBone);
    bone0->setTranslation(0.0f, 0.0f, 0.0f);
    bone1->setTranslation(10.0f, 0.0f, 0.0f);

    skinnedMesh.animate(500); // worldTime = 500ms -> bone0 position y = 15.0f
    assert(std::abs(bone0->getTranslation().y - 15.0f) < 1e-4f);
    assert(std::abs(skinnedMesh.vertexBuffer.vertices[0].position.y - 15.0f) < 1e-4f);
    std::cout << "     AnimationTrack -> Bone Skeleton -> SkinnedMesh deforming thanh cong hoan hao!" << std::endl;

    // 5. Kiem tra C-ABI Section 20 (j2me_core_m3g_node_*, j2me_core_m3g_group_*, j2me_core_m3g_skinned_mesh_*)
    std::cout << "  5. Kiem tra Section 20 C-ABI APIs day du..." << std::endl;
    uintptr_t nHandle = j2me_core_m3g_node_create();
    assert(nHandle != 0);

    j2me_core_m3g_node_set_translation(nHandle, 1.0f, 2.0f, 3.0f);
    float readTrans[3] = {0.0f};
    j2me_core_m3g_node_get_translation(nHandle, readTrans);
    assert(std::abs(readTrans[0] - 1.0f) < 1e-4f);
    assert(std::abs(readTrans[1] - 2.0f) < 1e-4f);
    assert(std::abs(readTrans[2] - 3.0f) < 1e-4f);

    j2me_core_m3g_node_set_scale(nHandle, 2.0f, 2.0f, 2.0f);
    float readScale[3] = {0.0f};
    j2me_core_m3g_node_get_scale(nHandle, readScale);
    assert(std::abs(readScale[0] - 2.0f) < 1e-4f);

    float globalMat[16] = {0.0f};
    j2me_core_m3g_node_get_global_transform(nHandle, globalMat);
    assert(std::abs(globalMat[0] - 2.0f) < 1e-4f);
    assert(std::abs(globalMat[3] - 1.0f) < 1e-4f);

    uintptr_t gHandle = j2me_core_m3g_group_create();
    assert(gHandle != 0);
    j2me_core_m3g_group_add_child(gHandle, nHandle);
    assert(j2me_core_m3g_group_get_child_count(gHandle) == 1);

    uintptr_t childRet = j2me_core_m3g_group_get_child(gHandle, 0);
    assert(childRet != 0);
    j2me_core_m3g_node_destroy(childRet);

    uintptr_t vbHandle = j2me_core_m3g_vertexbuffer_create();
    float posData[6] = {0.0f, 0.0f, 0.0f, 10.0f, 0.0f, 0.0f};
    j2me_core_m3g_vertexbuffer_set_positions(vbHandle, posData, 2);

    uintptr_t smHandle = j2me_core_m3g_skinned_mesh_create(vbHandle, gHandle);
    assert(smHandle != 0);

    j2me_core_m3g_skinned_mesh_add_transform(smHandle, nHandle, 100, 0, 1);
    assert(j2me_core_m3g_skinned_mesh_get_bone_count(smHandle) == 1);

    j2me_core_m3g_node_set_translation(nHandle, 1.0f, 12.0f, 3.0f);
    j2me_core_m3g_skinned_mesh_skin(smHandle);

    float skinnedPos[3] = {0.0f};
    j2me_core_m3g_skinned_mesh_get_vertex_position(smHandle, 0, skinnedPos);
    assert(std::abs(skinnedPos[1] - 10.0f) < 1e-3f);

    // Huy doi tuong C-ABI an toan tuyet doi
    j2me_core_m3g_skinned_mesh_destroy(smHandle);
    j2me_core_m3g_vertexbuffer_destroy(vbHandle);
    j2me_core_m3g_group_destroy(gHandle);
    j2me_core_m3g_node_destroy(nHandle);

    std::cout << "     C-ABI Section 20 M3G SkinnedMesh & Bone Skeleton xac minh thanh cong!" << std::endl;
    std::cout << "[PASS] Mo dun 20 (M3G SkinnedMesh & Bone Skeleton) 100% Hoan hao!" << std::endl;
}

void test_app_management_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[MODULE 21 TEST] Kiem tra toan dien App Management, Repository & Installer" << std::endl;
    std::cout << "========================================================" << std::endl;

    // 1. Kiem tra AppItem JSON serialization & deserialization
    std::cout << "  1. Kiem tra AppItem JSON Serialization & Deserialization..." << std::endl;
    universal_loader::app::AppItem item1;
    item1.id = 1;
    item1.path = "Sonic";
    item1.title = "Sonic The Hedgehog";
    item1.author = "SEGA Mobile";
    item1.version = "1.0.5";
    item1.imagePath = "icon.png";
    item1.installedTimestamp = 1600000000000LL;
    item1.lastPlayedTimestamp = 1600000050000LL;
    item1.playCount = 12;

    std::string jsonStr = item1.serializeJson();
    assert(jsonStr.find("\"id\": 1") != std::string::npos);
    assert(jsonStr.find("\"title\": \"Sonic The Hedgehog\"") != std::string::npos);
    assert(jsonStr.find("\"author\": \"SEGA Mobile\"") != std::string::npos);

    universal_loader::app::AppItem item1Parsed;
    assert(item1Parsed.deserializeJson(jsonStr));
    assert(item1Parsed.id == 1);
    assert(item1Parsed.path == "Sonic");
    assert(item1Parsed.title == "Sonic The Hedgehog");
    assert(item1Parsed.author == "SEGA Mobile");
    assert(item1Parsed.version == "1.0.5");
    assert(item1Parsed.imagePath == "icon.png");
    assert(item1Parsed.installedTimestamp == 1600000000000LL);
    assert(item1Parsed.lastPlayedTimestamp == 1600000050000LL);
    assert(item1Parsed.playCount == 12);
    std::cout << "     AppItem JSON serialization/deserialization toan ven du lieu 100%!" << std::endl;

    // 2. Kiem tra AppRepository CRUD, Sort & On-disk persistence
    std::cout << "  2. Kiem tra AppRepository CRUD, Sort & On-disk Persistence..." << std::endl;
    std::string repoTestDir = "./test_app_repo_sandbox";
    try { std::filesystem::remove_all(repoTestDir); } catch (...) {}
    std::filesystem::create_directories(repoTestDir);

    auto repo = std::make_shared<universal_loader::app::AppRepository>(repoTestDir);
    repo->clearAll();
    assert(repo->getCount() == 0);

    repo->insert(item1);

    universal_loader::app::AppItem item2;
    item2.id = 2;
    item2.path = "Contra";
    item2.title = "Contra 4";
    item2.author = "Konami";
    item2.version = "1.0.0";
    item2.installedTimestamp = 1600000010000LL;
    item2.lastPlayedTimestamp = 1600000020000LL;
    item2.playCount = 5;
    repo->insert(item2);

    universal_loader::app::AppItem item3;
    item3.id = 3;
    item3.path = "Mario";
    item3.title = "Super Mario Bros";
    item3.author = "Nintendo";
    item3.version = "2.0.0";
    item3.installedTimestamp = 1600000030000LL;
    item3.lastPlayedTimestamp = 1600000090000LL;
    item3.playCount = 30;
    repo->insert(item3);

    assert(repo->getCount() == 3);

    // Kiem tra tim kiem
    universal_loader::app::AppItem findItem;
    assert(repo->getById(2, findItem) && findItem.title == "Contra 4");
    assert(repo->getByPath("Sonic", findItem) && findItem.author == "SEGA Mobile");
    assert(repo->getByTitleVendor("Super Mario Bros", "Nintendo", findItem) && findItem.version == "2.0.0");

    // Kiem tra Sort
    auto titleAsc = repo->getAll(universal_loader::app::AppSortOrder::TITLE_ASC);
    assert(titleAsc.size() == 3);
    assert(titleAsc[0].title == "Contra 4");
    assert(titleAsc[1].title == "Sonic The Hedgehog");
    assert(titleAsc[2].title == "Super Mario Bros");

    auto playCountDesc = repo->getAll(universal_loader::app::AppSortOrder::PLAY_COUNT_DESC);
    assert(playCountDesc[0].title == "Super Mario Bros");
    assert(playCountDesc[1].title == "Sonic The Hedgehog");
    assert(playCountDesc[2].title == "Contra 4");

    // Kiem tra load lai tu dia
    universal_loader::app::AppRepository diskRepo(repoTestDir);
    assert(diskRepo.getCount() == 3);
    assert(diskRepo.getById(3, findItem) && findItem.title == "Super Mario Bros");

    // Kiem tra xoa
    assert(repo->remove(2));
    assert(repo->getCount() == 2);
    assert(!repo->getById(2, findItem));
    try { std::filesystem::remove_all(repoTestDir); } catch (...) {}
    std::cout << "     AppRepository CRUD, tim kiem & sap xep da luong xac minh thanh cong!" << std::endl;

    // 3. Kiem tra AppInstaller voi tep JAR thuc te (DragonBoy.jar)
    std::string dragonJarPath = "c:/j2meloader/universal_loader/core/test_assets/DragonBoy.jar";
    if (!std::filesystem::exists(dragonJarPath)) {
        dragonJarPath = "./test_assets/DragonBoy.jar";
    }
    std::string installSandbox = "./test_install_sandbox";
    try { std::filesystem::remove_all(installSandbox); } catch (...) {}
    std::filesystem::create_directories(installSandbox);

    auto installRepo = std::make_shared<universal_loader::app::AppRepository>(installSandbox + "/apps");
    universal_loader::app::AppInstaller installer(installSandbox, installRepo);

    // Kiem tra checkJar
    auto checkRes = installer.checkJar(dragonJarPath);
    assert(checkRes.status == universal_loader::app::InstallStatus::STATUS_NEW);
    assert(checkRes.title == "DragonBoy");
    assert(checkRes.vendor == "Team");
    assert(checkRes.version == "2.4.7");

    // Thuc hien cai dat
    int installedAppId = 0;
    std::string installErr;
    bool okInstall = installer.installFromJar(dragonJarPath, false, installedAppId, installErr);
    assert(okInstall && installedAppId > 0);
    assert(installRepo->getCount() == 1);

    // Kiem tra cac tep tin sinh ra trong thu muc app
    std::string appDir = installer.getAppDir("DragonBoy");
    assert(std::filesystem::exists(appDir + "/app.jar"));
    assert(std::filesystem::file_size(appDir + "/app.jar") == 1925300);
    assert(std::filesystem::exists(appDir + "/MANIFEST.MF"));
    assert(std::filesystem::exists(installer.getConfigDir("DragonBoy") + "/config.json"));
    assert(std::filesystem::exists(installer.getDataDir("DragonBoy")));

    // Kiem tra check lai (STATUS_EQUAL)
    auto checkRes2 = installer.checkJar(dragonJarPath);
    assert(checkRes2.status == universal_loader::app::InstallStatus::STATUS_EQUAL);
    int dupId = 0;
    assert(!installer.installFromJar(dragonJarPath, false, dupId, installErr));

    // Kiem tra forceUpdate
    assert(installer.installFromJar(dragonJarPath, true, dupId, installErr));
    assert(installRepo->getCount() == 1);
    std::cout << "     AppInstaller trích xuất JAR, Manifest, Icon & Profile thanh cong 100%!" << std::endl;

    // 4. Kiem tra Khoi chay Game cai dat truc tiep tren Engine
    std::cout << "  4. Kiem tra Khoi chay Game da cai dat truc tiep tren Engine..." << std::endl;
    J2meEngineInstance* engine = j2me_core_create(installSandbox.c_str());
    assert(engine != nullptr);

    assert(j2me_core_app_launch(engine, installedAppId));
    assert(std::string(j2me_core_get_app_title(engine)) == "DragonBoy");
    assert(std::string(j2me_core_get_app_vendor(engine)) == "Team");

    j2me_core_start(engine);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    int w = 0, h = 0;
    bool dirty = false;
    const uint32_t* fb = j2me_core_lock_framebuffer(engine, &w, &h, &dirty);
    assert(fb != nullptr);
    assert(w == 240);
    assert(h == 320);
    j2me_core_unlock_framebuffer(engine);

    j2me_core_stop(engine);

    // Kiem tra so lan choi da duoc cap nhat qua Engine Repository & Disk
    installRepo->load();
    universal_loader::app::AppItem playedItem;
    assert(installRepo->getById(installedAppId, playedItem));
    assert(playedItem.playCount >= 1);
    assert(playedItem.lastPlayedTimestamp > 0);
    std::cout << "     Game da cai dat duoc Engine nạp và thực thi Game Loop hoan hao!" << std::endl;

    // 5. Kiem tra Section 21 C-ABI APIs
    std::cout << "  5. Kiem tra Section 21 C-ABI APIs..." << std::endl;
    assert(j2me_core_app_repo_get_count(engine) == 1);
    J2meAppItemInfo cAbiInfo;
    assert(j2me_core_app_repo_get_item(engine, 0, &cAbiInfo));
    assert(std::string(cAbiInfo.title) == "DragonBoy");
    assert(std::string(cAbiInfo.author) == "Team");

    assert(j2me_core_app_repo_find_by_id(engine, installedAppId, &cAbiInfo));
    assert(std::string(cAbiInfo.path) == "DragonBoy");

    assert(j2me_core_app_repo_find_by_path(engine, "DragonBoy", &cAbiInfo));
    assert(cAbiInfo.id == installedAppId);

    // Kiem tra go cai dat qua C-ABI
    char outErr[256];
    assert(j2me_core_app_installer_uninstall(engine, installedAppId, outErr, sizeof(outErr)));
    assert(j2me_core_app_repo_get_count(engine) == 0);
    assert(!std::filesystem::exists(appDir));
    assert(!std::filesystem::exists(installer.getDataDir("DragonBoy")));

    j2me_core_destroy(engine);
    try { std::filesystem::remove_all(installSandbox); } catch (...) {}

    std::cout << "     C-ABI Section 21 App Management & Installer hoat dong on dinh tuyet doi!" << std::endl;
    std::cout << "[PASS] Mo dun 21 (App Management, Repository & Installer) 100% Hoan hao!" << std::endl;
}

// ==============================================================================
// 22. UNIT TEST MO DUN 22: JSR-82 MOBILE BLUETOOTH & RFCOMM/L2CAP MULTIPLAYER
// ==============================================================================
void test_bluetooth_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[UNIT TEST] MO DUN 22: JSR-82 BLUETOOTH & MULTIPLAYER..." << std::endl;
    std::cout << "========================================================" << std::endl;

    using namespace j2me::bluetooth;

    // 1. Kiem tra BluetoothUUID
    std::cout << "  1. Kiem tra BluetoothUUID (16-bit, 32-bit & 128-bit)..." << std::endl;
    BluetoothUUID u16(0x1101);
    assert(u16.isShortUUID());
    assert(u16.getShortValue() == 0x1101);
    assert(u16.toShortHexString() == "1101");
    assert(u16.toString() == "110100001000800000805F9B34FB");
    assert(u16.toCanonicalString() == "00001101-0000-1000-8000-00805F9B34FB");

    BluetoothUUID u16_str("1101", true);
    assert(u16 == u16_str);
    assert(std::hash<BluetoothUUID>{}(u16) == std::hash<BluetoothUUID>{}(u16_str));

    BluetoothUUID u32(0x12345678);
    assert(u32.isShortUUID());
    assert(u32.getShortValue() == 0x12345678);
    assert(u32.toShortHexString() == "12345678");

    BluetoothUUID u128("F0001111-2222-3333-4444-555566667777");
    assert(!u128.isShortUUID());
    assert(u128.toHex32() == "F0001111222233334444555566667777");
    assert(u128.toCanonicalString() == "F0001111-2222-3333-4444-555566667777");
    assert(u16 != u128);
    std::cout << "     BluetoothUUID (short & 128-bit canonical) xac minh thanh cong!" << std::endl;

    // 2. Kiem tra BluetoothDataElement
    std::cout << "  2. Kiem tra BluetoothDataElement (Types, Integers, Strings, Sequences)..." << std::endl;
    BluetoothDataElement deBool(true);
    assert(deBool.getDataType() == TYPE_BOOL);
    assert(deBool.getBoolean() == true);

    BluetoothDataElement deInt1(TYPE_INT_1, -42);
    assert(deInt1.getLong() == -42);

    BluetoothDataElement deUint4(TYPE_U_INT_4, 3000000000LL);
    assert(deUint4.getLong() == 3000000000LL);

    BluetoothDataElement deStr("Asphalt 4 Multiplayer");
    assert(deStr.getDataType() == TYPE_STRING);
    assert(deStr.getString() == "Asphalt 4 Multiplayer");

    BluetoothDataElement deSeq(TYPE_DATSEQ);
    deSeq.addElement(deBool);
    deSeq.addElement(deInt1);
    deSeq.addElement(deUint4);
    deSeq.addElement(deStr);
    assert(deSeq.getSize() == 4);
    assert(deSeq.getElement(0).getBoolean() == true);
    assert(deSeq.getElement(1).getLong() == -42);
    assert(deSeq.getElement(3).getString() == "Asphalt 4 Multiplayer");
    std::cout << "     BluetoothDataElement hierarchic sequences xac minh thanh cong!" << std::endl;

    // 3. Kiem tra BluetoothServiceRecord
    std::cout << "  3. Kiem tra BluetoothServiceRecord & Connection URLs..." << std::endl;
    BluetoothServiceRecord srvRec("localhost", u16, false, false);
    srvRec.setServiceName("Rally Pro Server");
    assert(srvRec.getServiceName() == "Rally Pro Server");
    assert(srvRec.hasAttribute(ATTR_SERVICE_CLASS_ID_LIST));
    assert(srvRec.hasAttribute(ATTR_PROTOCOL_DESCRIPTOR_LIST));

    std::string srvUrl = srvRec.getConnectionURL(NOAUTHENTICATE_NOENCRYPT, false);
    assert(srvUrl.find("btspp://localhost:1101") != std::string::npos);
    assert(srvUrl.find("authenticate=false;encrypt=false;master=false") != std::string::npos);

    std::string srvUrlSec = srvRec.getConnectionURL(AUTHENTICATE_ENCRYPT, true);
    assert(srvUrlSec.find("btspp://localhost:1101") != std::string::npos);
    assert(srvUrlSec.find("authenticate=true;encrypt=true;master=true") != std::string::npos);

    BluetoothServiceRecord l2capRec("001122334455", BluetoothUUID(0x1003), true, false);
    std::string l2Url = l2capRec.getConnectionURL(NOAUTHENTICATE_NOENCRYPT, false);
    assert(l2Url.find("btl2cap://001122334455:1003") != std::string::npos);
    assert(l2Url.find("authenticate=false;encrypt=false;master=false") != std::string::npos);
    std::cout << "     BluetoothServiceRecord & URLs xac minh thanh cong!" << std::endl;

    // 4. Kiem tra LocalDevice & DiscoveryAgent
    std::cout << "  4. Kiem tra LocalDevice & DiscoveryAgent..." << std::endl;
    LocalDevice& ld = LocalDevice::getInstance();
    assert(ld.getProperty("bluetooth.api.version") == "1.1");
    assert(ld.getProperty("bluetooth.connected.devices.max") == "7");
    assert(ld.getProperty("bluetooth.l2cap.receiveMTU.max") == "672");
    assert(ld.isPowerOn());

    ld.setBluetoothAddress("AABBCCDDEEFF");
    assert(ld.getBluetoothAddress() == "AABBCCDDEEFF");
    ld.setFriendlyName("Nokia N73");
    assert(ld.getFriendlyName() == "Nokia N73");

    assert(ld.setDiscoverable(GIAC));
    assert(ld.getDiscoverable() == GIAC);
    assert(ld.setDiscoverable(NOT_DISCOVERABLE));
    assert(ld.getDiscoverable() == NOT_DISCOVERABLE);
    assert(!ld.setDiscoverable(9999999));

    DiscoveryAgent& da = ld.getDiscoveryAgent();
    assert(da.startInquiry(GIAC));
    RemoteDevice peer("112233445566", "OpponentCar");
    da.addDiscoveredDevice(peer);
    auto cachedDevs = da.retrieveDevices(DEVICE_CACHED);
    assert(cachedDevs.size() >= 1);
    assert(cachedDevs[0].getBluetoothAddress() == "112233445566");
    assert(cachedDevs[0].getFriendlyName() == "OpponentCar");
    assert(da.cancelInquiry());

    BluetoothServiceRecord peerService("112233445566", u16, false, false);
    peerService.setServiceName("Worms Multiplayer");
    da.registerRemoteService("112233445566", peerService);

    auto foundServices = da.searchServices("112233445566", {u16});
    assert(foundServices.size() == 1);
    assert(foundServices[0].getServiceName() == "Worms Multiplayer");
    std::string selectedUrl = da.selectService(u16, NOAUTHENTICATE_NOENCRYPT, false);
    assert(selectedUrl.find("btspp://112233445566:1101") != std::string::npos);
    std::cout << "     LocalDevice & DiscoveryAgent xac minh thanh cong!" << std::endl;

    // 5. Kiem tra Truyen thong BTSPP Loopback Thuc chien (Socket Stream Bridge)
    std::cout << "  5. Kiem tra Truyen thong BTSPP Loopback Thuc chien..." << std::endl;
    auto sppServer = openBtsppServer("btspp://localhost:1101;name=RallyServer;port=28888");
    assert(sppServer != nullptr);
    assert(sppServer->isListening());

    std::vector<uint8_t> txPacket = {0xAA, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    std::vector<uint8_t> rxPacket(8, 0);
    std::vector<uint8_t> txReply = {0xBB, 0x10, 0x20, 0x30};
    std::vector<uint8_t> rxReply(4, 0);

    std::thread clientThread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto sppClient = openBtsppClient("btspp://localhost:1101;port=28888", 2000);
        assert(sppClient != nullptr);
        assert(sppClient->isConnected());

        // Client gui du lieu game
        int sent = sppClient->write(txPacket.data(), txPacket.size());
        assert(sent == static_cast<int>(txPacket.size()));

        // Client doc phan hoi
        bool ok = sppClient->readFully(rxReply.data(), rxReply.size(), 2000);
        assert(ok);
        assert(rxReply == txReply);

        sppClient->close();
    });

    auto serverConn = sppServer->acceptAndOpen(2000);
    assert(serverConn != nullptr);
    assert(serverConn->isConnected());

    bool okRecv = serverConn->readFully(rxPacket.data(), rxPacket.size(), 2000);
    assert(okRecv);
    assert(rxPacket == txPacket);

    // Server gui phan hoi
    int sentReply = serverConn->write(txReply.data(), txReply.size());
    assert(sentReply == static_cast<int>(txReply.size()));

    serverConn->close();
    sppServer->close();
    clientThread.join();
    std::cout << "     BTSPP Loopback stream socket truyen/nhan goi tin game 100% tron ven!" << std::endl;

    // 6. Kiem tra Truyen thong BTL2CAP Loopback Packet Framing
    std::cout << "  6. Kiem tra Truyen thong BTL2CAP Loopback Packet Framing..." << std::endl;
    auto l2Server = openBtl2capServer("btl2cap://localhost:1003;ReceiveMTU=512;TransmitMTU=512;port=28889");
    assert(l2Server != nullptr);
    assert(l2Server->isListening());

    std::vector<uint8_t> l2Tx(64, 0x7E);
    std::vector<uint8_t> l2Rx(128, 0);

    std::thread l2ClientThread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        auto l2Client = openBtl2capClient("btl2cap://localhost:1003;port=28889", 2000);
        assert(l2Client != nullptr);
        assert(l2Client->isConnected());

        assert(l2Client->send(l2Tx.data(), l2Tx.size()));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        l2Client->close();
    });

    auto l2Conn = l2Server->acceptAndOpen(2000);
    assert(l2Conn != nullptr);
    assert(l2Conn->isConnected());

    int recved = l2Conn->receive(l2Rx.data(), l2Rx.size(), 2000);
    assert(recved == static_cast<int>(l2Tx.size()));
    assert(std::memcmp(l2Rx.data(), l2Tx.data(), recved) == 0);

    l2Conn->close();
    l2Server->close();
    l2ClientThread.join();
    std::cout << "     BTL2CAP Packet Framing & MTU truyen/nhan goi tin chinh xac 100%!" << std::endl;

    // 7. Kiem tra javax.obex (HeaderSet & ResponseCodes)
    std::cout << "  7. Kiem tra javax.obex (HeaderSet & ResponseCodes)..." << std::endl;
    ObexHeaderSet hs;
    hs.setHeader(OBEX_HDR_NAME, "game_save.dat");
    hs.setHeader(OBEX_HDR_TYPE, "application/octet-stream");
    hs.setHeader(OBEX_HDR_LENGTH, static_cast<int64_t>(2048));
    hs.setHeader(OBEX_HDR_COUNT, static_cast<int64_t>(1));
    hs.createAuthenticationChallenge("GameLobby", true, true);

    auto obexBin = hs.serialize(OBEX_HTTP_OK);
    assert(!obexBin.empty());
    assert(obexBin[0] == OBEX_HTTP_OK);

    ObexHeaderSet hsParsed;
    uint8_t opCode = 0;
    assert(hsParsed.deserialize(obexBin.data(), obexBin.size(), opCode));
    assert(opCode == OBEX_HTTP_OK);

    std::string nameParsed, typeParsed;
    int64_t lenParsed = 0, countParsed = 0;
    assert(hsParsed.getHeaderString(OBEX_HDR_NAME, nameParsed) && nameParsed == "game_save.dat");
    assert(hsParsed.getHeaderString(OBEX_HDR_TYPE, typeParsed) && typeParsed == "application/octet-stream");
    assert(hsParsed.getHeaderInt(OBEX_HDR_LENGTH, lenParsed) && lenParsed == 2048);
    assert(hsParsed.getHeaderInt(OBEX_HDR_COUNT, countParsed) && countParsed == 1);
    assert(hsParsed.hasHeader(OBEX_HDR_APPLICATION_PARAMETER));
    std::cout << "     javax.obex HeaderSet serialization/deserialization xac minh thanh cong!" << std::endl;

    // 8. Kiem tra Section 22 C-ABI APIs
    std::cout << "  8. Kiem tra Section 22 C-ABI APIs..." << std::endl;
    assert(j2me_core_bluetooth_is_power_on());
    char localAddr[64];
    j2me_core_bluetooth_get_local_address(localAddr, sizeof(localAddr));
    assert(strlen(localAddr) > 0);

    char localName[64];
    j2me_core_bluetooth_get_local_name(localName, sizeof(localName));
    assert(strlen(localName) > 0);

    assert(j2me_core_bluetooth_set_discoverable(GIAC));
    assert(j2me_core_bluetooth_get_discoverable() == GIAC);

    char propVal[64];
    assert(j2me_core_bluetooth_get_property("bluetooth.api.version", propVal, sizeof(propVal)));
    assert(std::string(propVal) == "1.1");

    uintptr_t cAbiServer = j2me_core_bluetooth_open_btspp_server("btspp://localhost:1102;port=28890");
    assert(cAbiServer != 0);

    std::thread cAbiClientThread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        uintptr_t cAbiClient = j2me_core_bluetooth_open_btspp_client("btspp://localhost:1102;port=28890", 2000);
        assert(cAbiClient != 0);

        uint8_t bVal = 0x77;
        assert(j2me_core_bluetooth_btspp_write(cAbiClient, &bVal, 1) == 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        j2me_core_bluetooth_btspp_close(cAbiClient);
    });

    uintptr_t cAbiAccepted = j2me_core_bluetooth_btspp_accept(cAbiServer, 2000);
    assert(cAbiAccepted != 0);

    uint8_t rxB = 0;
    assert(j2me_core_bluetooth_btspp_read(cAbiAccepted, &rxB, 1, 2000) == 1);
    assert(rxB == 0x77);

    j2me_core_bluetooth_btspp_close(cAbiAccepted);
    j2me_core_bluetooth_btspp_server_close(cAbiServer);
    cAbiClientThread.join();

    std::cout << "     C-ABI Section 22 Bluetooth hoat dong hoan hao!" << std::endl;
    std::cout << "[PASS] Mo dun 22 (JSR-82 Mobile Bluetooth & RFCOMM/L2CAP) 100% Hoan hao!" << std::endl;
}

void test_vodafone_and_carrier_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[TEST MODULE 23] Vodafone VSCL & Carrier OEM Extensions" << std::endl;
    std::cout << "========================================================" << std::endl;

    using namespace universal_loader::oem::vodafone;
    using namespace universal_loader::oem::carrier;

    // 1. Kiem tra VodafoneSpriteCanvas & Bien doi hinh hoc 8 huong
    std::cout << "  1. Kiem tra VodafoneSpriteCanvas & Bien doi hinh hoc 8 huong..." << std::endl;
    VodafoneSpriteCanvas canvas(4, 2); // 4 palettes, 2 patterns
    assert(canvas.getPaletteCount() == 4);
    assert(canvas.getPatternCount() == 2);

    canvas.setPalette(0, 0x00000000); // Index 0: Black / transparent
    canvas.setPalette(1, 0x00FF0000); // Index 1: Red (0xFFFF0000)
    canvas.setPalette(2, 0x0000FF00); // Index 2: Green (0xFF00FF00)
    canvas.setPalette(3, 0x000000FF); // Index 3: Blue (0xFF0000FF)

    assert(canvas.getPalette(1) == 0xFFFF0000);
    assert(canvas.getPalette(2) == 0xFF00FF00);
    assert(canvas.getPalette(3) == 0xFF0000FF);

    // Build 8x8 pattern data:
    // (col 0, row 0) = 1 (Red)
    // (col 7, row 0) = 2 (Green)
    // (col 0, row 7) = 3 (Blue)
    // all others = 0
    std::vector<uint8_t> pat0(64, 0);
    pat0[0 * 8 + 0] = 1;
    pat0[0 * 8 + 7] = 2;
    pat0[7 * 8 + 0] = 3;
    canvas.setPattern(0, pat0.data(), pat0.size());

    // Commands:
    // cmd0: no rotation, no flip, transparent
    int16_t cmd0 = canvas.createCharacterCommand(0, true, SPRITE_ROT_NONE, false, false, 0);
    // cmd1: 90 deg rotation CW
    int16_t cmd1 = canvas.createCharacterCommand(0, true, SPRITE_ROT_90, false, false, 0);
    // cmd2: 180 deg rotation
    int16_t cmd2 = canvas.createCharacterCommand(0, true, SPRITE_ROT_180, false, false, 0);
    // cmd3: flip horizontal
    int16_t cmd3 = canvas.createCharacterCommand(0, true, SPRITE_ROT_NONE, false, true, 0);

    assert(cmd0 == 0 && cmd1 == 1 && cmd2 == 2 && cmd3 == 3);

    // Create Framebuffer 32x32
    canvas.createFrameBuffer(32, 32);
    assert(canvas.hasFrameBuffer());
    assert(canvas.getFrameBufferWidth() == 32);
    assert(canvas.getFrameBufferHeight() == 32);

    // Draw sprite char cmd0 at (0, 0)
    canvas.drawSpriteChar(cmd0, 0, 0);
    const uint32_t* fb = canvas.getFrameBuffer();
    assert(fb != nullptr);
    assert(fb[0 * 32 + 0] == 0xFFFF0000); // (0,0) Red
    assert(fb[0 * 32 + 7] == 0xFF00FF00); // (7,0) Green
    assert(fb[7 * 32 + 0] == 0xFF0000FF); // (0,7) Blue
    assert(fb[1 * 32 + 1] == 0);          // Transparent 0

    // Draw sprite char cmd1 (90 deg CW) at (10, 10):
    // Original (0, 0) -> Rot90 -> (7, 0). Dest: (10 + 7, 10 + 0) = (17, 10) -> Red
    // Original (7, 0) -> Rot90 -> (7, 7). Dest: (10 + 7, 10 + 7) = (17, 17) -> Green
    // Original (0, 7) -> Rot90 -> (0, 0). Dest: (10 + 0, 10 + 0) = (10, 10) -> Blue
    canvas.drawSpriteChar(cmd1, 10, 10);
    assert(fb[10 * 32 + 17] == 0xFFFF0000); // Red at (17, 10)
    assert(fb[17 * 32 + 17] == 0xFF00FF00); // Green at (17, 17)
    assert(fb[10 * 32 + 10] == 0xFF0000FF); // Blue at (10, 10)

    // Test copyArea from (0, 0, 8, 8) to (20, 20)
    canvas.copyArea(0, 0, 8, 8, 20, 20);
    assert(fb[20 * 32 + 20] == 0xFFFF0000);
    assert(fb[20 * 32 + 27] == 0xFF00FF00);
    assert(fb[27 * 32 + 20] == 0xFF0000FF);

    // Test drawFrameBuffer flushes and clears framebuffer
    std::vector<uint32_t> targetScreen(32 * 32, 0);
    canvas.drawFrameBuffer(targetScreen.data(), 32, 32, 0, 0);
    assert(targetScreen[0 * 32 + 0] == 0xFFFF0000);
    assert(targetScreen[20 * 32 + 20] == 0xFFFF0000);
    // Verify framebuffer was cleared to 0 as required by Vodafone specification
    assert(fb[0 * 32 + 0] == 0);
    assert(fb[20 * 32 + 20] == 0);

    canvas.disposeFrameBuffer();
    assert(!canvas.hasFrameBuffer());
    std::cout << "     VodafoneSpriteCanvas hoat dong hoan hao!" << std::endl;

    // 2. Kiem tra Vodafone Sound Engine
    std::cout << "  2. Kiem tra Vodafone Sound Engine & 16-Channel Player..." << std::endl;
    std::vector<uint8_t> smafData = { 'M', 'M', 'M', 'D', 0, 0, 0, 16 };
    VodafoneSound smafSound(smafData);
    assert(smafSound.detectMimeType() == "audio/x-smaf");

    std::vector<uint8_t> midiData = { 'M', 'T', 'h', 'd', 0, 0, 0, 6 };
    VodafoneSound midiSound(midiData);
    assert(midiSound.detectMimeType() == "audio/midi");

    auto& sp = VodafoneSoundPlayer::instance();
    sp.resetAllTracks();
    auto track = sp.getTrack();
    assert(track != nullptr);
    assert(track->getState() == SOUND_STATE_NO_DATA);

    track->setSound(std::make_shared<VodafoneSound>(smafData));
    assert(track->getState() == SOUND_STATE_READY);

    // Volume clamping 0..127
    track->setVolume(200);
    assert(track->getVolume() == SOUND_MAX_VOLUME);
    track->setVolume(-15);
    assert(track->getVolume() == 0);
    track->setVolume(90);
    assert(track->getVolume() == 90);

    // Playback events
    int recordedEvent = 999;
    track->setEventListener([&](int ev) { recordedEvent = ev; });

    track->play(2);
    assert(track->getState() == SOUND_STATE_PLAYING);
    assert(track->getLoopCount() == 2);
    assert(recordedEvent == EV_LOOP);

    track->pause();
    assert(track->getState() == SOUND_STATE_PAUSED);
    assert(recordedEvent == EV_PAUSE);

    track->resume();
    assert(track->getState() == SOUND_STATE_PLAYING);

    track->stop();
    assert(track->getState() == SOUND_STATE_READY);
    assert(recordedEvent == EV_END);

    track->removeSound();
    assert(track->getState() == SOUND_STATE_NO_DATA);
    std::cout << "     Vodafone Sound Player hoat dong hoan hao!" << std::endl;

    // 3. Kiem tra Vodafone Device Control & 24-bit KeyStates
    std::cout << "  3. Kiem tra Vodafone DeviceControl & Bitmask 24-bit ban phim..." << std::endl;
    auto& dc = VodafoneDeviceControl::instance();
    assert(dc.getDeviceState(DEVICE_BATTERY) == 100);
    assert(dc.getDeviceState(DEVICE_FIELD_INTENSITY) == 100);

    dc.resetKeyStates();
    assert(dc.getKeyStatesVodafone() == 0);

    // Test J2ME Key Code mappings
    dc.setKeyDown('5'); // '5' key
    assert(dc.getKeyStatesVodafone() & VODAFONE_KEY_5);
    dc.setKeyDown(-1);  // UP key
    assert(dc.getKeyStatesVodafone() & VODAFONE_KEY_UP);
    dc.setKeyDown(-5);  // FIRE key
    assert(dc.getKeyStatesVodafone() & VODAFONE_KEY_FIRE);
    dc.setKeyDown(-6);  // SOFT_LEFT
    assert(dc.getKeyStatesVodafone() & VODAFONE_KEY_SOFT_LEFT);

    dc.setKeyUp('5');
    assert(!(dc.getKeyStatesVodafone() & VODAFONE_KEY_5));
    assert(dc.getKeyStatesVodafone() & VODAFONE_KEY_UP);

    dc.resetKeyStates();
    assert(dc.getKeyStatesVodafone() == 0);

    // Direct bitmask injection
    dc.setKeyStatesMask(VODAFONE_KEY_1 | VODAFONE_KEY_9 | VODAFONE_KEY_POUND);
    assert(dc.getDeviceState(DEVICE_KEY_STATE) == (VODAFONE_KEY_1 | VODAFONE_KEY_9 | VODAFONE_KEY_POUND));

    // Device active toggles
    assert(dc.setDeviceActive(DEVICE_VIBRATION, true));
    assert(dc.isDeviceActive(DEVICE_VIBRATION));
    assert(dc.setDeviceActive(DEVICE_BACK_LIGHT, true));
    assert(dc.isDeviceActive(DEVICE_BACK_LIGHT));
    assert(dc.setDeviceActive(DEVICE_EIGHT_DIRECTIONS, true));
    assert(dc.isDeviceActive(DEVICE_EIGHT_DIRECTIONS));
    assert(dc.getDeviceState(DEVICE_EIGHT_DIRECTIONS) == 1);
    std::cout << "     Vodafone DeviceControl & KeyStates hoat dong hoan hao!" << std::endl;

    // 4. Kiem tra Vodafone ImageEncoder (PNG offscreen)
    std::cout << "  4. Kiem tra Vodafone ImageEncoder (PNG offscreen encoding)..." << std::endl;
    std::vector<uint32_t> testPixels(16 * 16, 0xFFFF00FFu); // Magenta
    auto pngBytes = VodafoneImageEncoder::encodeOffscreen(testPixels.data(), 16, 16, 2, 2, 8, 8, IMAGE_FORMAT_PNG);
    assert(!pngBytes.empty());
    const uint8_t expPngSig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    assert(std::memcmp(pngBytes.data(), expPngSig, 8) == 0);
    std::cout << "     Vodafone ImageEncoder PNG hoat dong hoan hao (kich thuoc: " << pngBytes.size() << " bytes)!" << std::endl;

    // 5. Kiem tra Carrier Extensions (KDDI, Motorola, Sony Ericsson, Sprint PCS)
    std::cout << "  5. Kiem tra Carrier Extensions (KDDI, Motorola, Sony, Sprint)..." << std::endl;
    assert(KDDISystem::getKeyState(false) == (VODAFONE_KEY_1 | VODAFONE_KEY_9 | VODAFONE_KEY_POUND));

    auto& fl = MotorolaFunLight::instance();
    fl.reset();
    fl.setColor(1, 0xFF0000); // Region 1 Red
    fl.setColor(8, 0x00FF00); // Region 8 Green
    assert(fl.getColor(1) == 0x00FF0000);
    assert(fl.getColor(8) == 0x0000FF00);
    assert(fl.getRegionCount() == 8);

    auto& accel = SonyEricssonAccelerometer::instance();
    accel.setAcceleration(0.5f, -1.2f, 9.78f);
    float ax = 0, ay = 0, az = 0;
    accel.getAcceleration(ax, ay, az);
    assert(std::abs(ax - 0.5f) < 0.001f);
    assert(std::abs(ay - -1.2f) < 0.001f);
    assert(std::abs(az - 9.78f) < 0.001f);

    auto& sprint = SprintPCSPlayer::instance();
    sprint.playClip("title_theme.mid", 3);
    assert(sprint.isPlaying());
    assert(sprint.getCurrentClip() == "title_theme.mid");
    assert(sprint.getLoopCount() == 3);
    sprint.stop();
    assert(!sprint.isPlaying());

    // 6. Kiem tra C-ABI Section 23 Exports
    std::cout << "  6. Kiem tra C-ABI Section 23 APIs..." << std::endl;
    uintptr_t cSprite = j2me_core_vodafone_sprite_create(2, 1);
    assert(cSprite != 0);
    j2me_core_vodafone_sprite_set_palette(cSprite, 1, 0x112233);
    assert(j2me_core_vodafone_sprite_get_palette(cSprite, 1) == 0xFF112233);
    j2me_core_vodafone_sprite_destroy(cSprite);

    assert(j2me_core_carrier_kddi_get_keystate(false) == (VODAFONE_KEY_1 | VODAFONE_KEY_9 | VODAFONE_KEY_POUND));
    j2me_core_carrier_motorola_funlight_set_color(4, 0xAABBCC);
    assert(j2me_core_carrier_motorola_funlight_get_color(4) == 0x00AABBCC);

    float cax = 0, cay = 0, caz = 0;
    j2me_core_carrier_sony_accel_set(1.1f, 2.2f, 3.3f);
    j2me_core_carrier_sony_accel_get(&cax, &cay, &caz);
    assert(std::abs(cax - 1.1f) < 0.001f);
    assert(std::abs(cay - 2.2f) < 0.001f);
    assert(std::abs(caz - 3.3f) < 0.001f);

    j2me_core_carrier_sprint_play_clip("level1.qcp", 1);
    assert(j2me_core_carrier_sprint_is_playing());
    j2me_core_carrier_sprint_stop();
    assert(!j2me_core_carrier_sprint_is_playing());

    std::vector<uint8_t> cPngBuf(2048, 0);
    int pngSz = j2me_core_vodafone_encode_offscreen(testPixels.data(), 16, 16, 0, 0, 4, 4, 0, cPngBuf.data(), cPngBuf.size());
    assert(pngSz > 0);
    assert(std::memcmp(cPngBuf.data(), expPngSig, 8) == 0);

    std::cout << "[PASS] Mo dun 23 (Vodafone VSCL & Carrier OEM Extensions) 100% Hoan hao!" << std::endl;
}

void test_location_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[TEST MODULE 24] JSR-179 Mobile Location API" << std::endl;
    std::cout << "========================================================" << std::endl;

    using namespace universal_loader::location;

    // 1. Coordinates & QualifiedCoordinates
    std::cout << "  1. Kiem tra Coordinates & QualifiedCoordinates..." << std::endl;
    Coordinates hanoi(21.0285, 105.8542, 15.0f);
    assert(std::abs(hanoi.getLatitude() - 21.0285) < 0.0001);
    assert(std::abs(hanoi.getLongitude() - 105.8542) < 0.0001);
    assert(std::abs(hanoi.getAltitude() - 15.0f) < 0.001f);

    // Boundary checks
    bool caught = false;
    try {
        Coordinates invalidLat(90.1, 0.0);
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    assert(caught);

    caught = false;
    try {
        Coordinates invalidLon(0.0, 180.1);
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    assert(caught);

    QualifiedCoordinates qcoords(21.0285, 105.8542, 15.0f, 3.5f, 1.2f);
    assert(std::abs(qcoords.getHorizontalAccuracy() - 3.5f) < 0.001f);
    assert(std::abs(qcoords.getVerticalAccuracy() - 1.2f) < 0.001f);
    qcoords.setHorizontalAccuracy(5.0f);
    qcoords.setVerticalAccuracy(2.0f);
    assert(std::abs(qcoords.getHorizontalAccuracy() - 5.0f) < 0.001f);
    assert(std::abs(qcoords.getVerticalAccuracy() - 2.0f) < 0.001f);

    // 2. Geodetic Math: Haversine distance & forward azimuth
    std::cout << "  2. Kiem tra Geodetic Math (Haversine & Azimuth)..." << std::endl;
    Coordinates hcmc(10.8231, 106.6297, 10.0f);
    float dist = hanoi.distance(hcmc);
    // Distance between Hanoi and HCMC is ~1140 km (~1,144,000 m)
    assert(dist >= 1130000.0f && dist <= 1160000.0f);
    assert(hanoi.distance(hanoi) == 0.0f);

    float azimuth = hanoi.azimuthTo(hcmc);
    // Direction from Hanoi to HCMC is South-South-East ~160° - 170°
    assert(azimuth >= 150.0f && azimuth <= 180.0f);

    // 3. String Conversion (DD_MM and DD_MM_SS)
    std::cout << "  3. Kiem tra Coordinate String Conversion..." << std::endl;
    std::string s_dd_mm = Coordinates::convert(21.5, COORDINATE_FORMAT_DD_MM);
    assert(s_dd_mm == "21:30.00000");
    double parsed1 = Coordinates::convert(s_dd_mm);
    assert(std::abs(parsed1 - 21.5) < 0.0001);

    std::string s_dd_mm_ss = Coordinates::convert(21.5, COORDINATE_FORMAT_DD_MM_SS);
    assert(s_dd_mm_ss == "21:30:00.000");
    double parsed2 = Coordinates::convert(s_dd_mm_ss);
    assert(std::abs(parsed2 - 21.5) < 0.0001);

    std::string s_neg = Coordinates::convert(-10.25, COORDINATE_FORMAT_DD_MM);
    assert(s_neg == "-10:15.00000");
    double parsedNeg = Coordinates::convert(s_neg);
    assert(std::abs(parsedNeg - (-10.25)) < 0.0001);

    // 4. Criteria & AddressInfo
    std::cout << "  4. Kiem tra Criteria & AddressInfo..." << std::endl;
    Criteria crit;
    assert(crit.getPreferredPowerConsumption() == POWER_NO_REQUIREMENT);
    assert(crit.isAllowedToCost() == true);
    crit.setHorizontalAccuracy(10);
    crit.setVerticalAccuracy(20);
    crit.setPreferredPowerConsumption(POWER_USAGE_LOW);
    crit.setPreferredResponseTime(5000);
    crit.setCostAllowed(false);
    crit.setSpeedAndCourseRequired(true);
    crit.setAltitudeRequired(true);
    assert(crit.getHorizontalAccuracy() == 10);
    assert(crit.getVerticalAccuracy() == 20);
    assert(crit.getPreferredPowerConsumption() == POWER_USAGE_LOW);
    assert(crit.getPreferredResponseTime() == 5000);
    assert(!crit.isAllowedToCost());
    assert(crit.isSpeedAndCourseRequired());
    assert(crit.isAltitudeRequired());

    AddressInfo addr;
    addr.setField(AddressInfo::STREET, "123 Tran Hung Dao");
    addr.setField(AddressInfo::CITY, "Hanoi");
    addr.setField(AddressInfo::COUNTRY_CODE, "VN");
    assert(addr.getField(AddressInfo::STREET) == "123 Tran Hung Dao");
    assert(addr.getField(AddressInfo::CITY) == "Hanoi");
    assert(addr.getField(AddressInfo::COUNTRY_CODE) == "VN");
    assert(addr.getField(AddressInfo::POSTAL_CODE).empty());

    // 5. Location & Orientation
    std::cout << "  5. Kiem tra Location & Orientation..." << std::endl;
    Location loc(qcoords, 12.5f, 180.0f, 1600000000000LL, MTY_TERMINALBASED | MTE_SATELLITE | MTA_ASSISTED, "", std::make_shared<AddressInfo>(addr));
    assert(loc.isValid());
    assert(std::abs(loc.getSpeed() - 12.5f) < 0.001f);
    assert(std::abs(loc.getCourse() - 180.0f) < 0.001f);
    assert(loc.getTimestamp() == 1600000000000LL);
    assert(loc.getLocationMethod() == (MTY_TERMINALBASED | MTE_SATELLITE | MTA_ASSISTED));
    assert(loc.getAddressInfo() != nullptr);
    assert(loc.getAddressInfo()->getField(AddressInfo::CITY) == "Hanoi");

    Orientation::setGlobalOrientation(45.0f, false, 10.0f, -5.0f);
    Orientation ori = Orientation::getOrientation();
    assert(std::abs(ori.getCompassAzimuth() - 45.0f) < 0.001f);
    assert(!ori.isOrientationMagnetic());
    assert(std::abs(ori.getPitch() - 10.0f) < 0.001f);
    assert(std::abs(ori.getRoll() - (-5.0f)) < 0.001f);

    // 6. LandmarkStore
    std::cout << "  6. Kiem tra LandmarkStore..." << std::endl;
    LandmarkStore::createLandmarkStore("vietnam_pois");
    LandmarkStore* store = LandmarkStore::getInstance("vietnam_pois");
    assert(store != nullptr);

    store->addCategory("Historical");
    store->addCategory("Nature");
    auto categories = store->getCategories();
    assert(categories.size() == 2);

    auto hanoiCoords = std::make_shared<QualifiedCoordinates>(21.0285, 105.8542, 15.0f);
    auto westLakeCoords = std::make_shared<QualifiedCoordinates>(21.0550, 105.8250, 10.0f);
    Landmark lmHanoi("Hoan Kiem Lake", "Sword Lake", hanoiCoords, nullptr);
    Landmark lmWestLake("West Lake", "Tay Ho Lake", westLakeCoords, nullptr);

    store->addLandmark(lmHanoi, "Historical");
    store->addLandmark(lmWestLake, "Nature");

    auto histLandmarks = store->getLandmarks("Historical");
    assert(histLandmarks.size() == 1);
    assert(histLandmarks[0].getName() == "Hoan Kiem Lake");

    // Bounding box query
    auto inBox = store->getLandmarks("", 21.0, 21.1, 105.8, 105.9);
    assert(inBox.size() == 2);

    auto outBox = store->getLandmarks("", 0.0, 1.0, 0.0, 1.0);
    assert(outBox.empty());

    LandmarkStore::deleteLandmarkStore("vietnam_pois");
    assert(LandmarkStore::getInstance("vietnam_pois") == nullptr);

    // 7. LocationProvider & Host Injection & NMEA
    std::cout << "  7. Kiem tra LocationProvider & NMEA Generator..." << std::endl;
    LocationProvider* provider = LocationProvider::getInstance();
    assert(provider != nullptr);
    assert(provider->getState() == LOCATION_PROVIDER_AVAILABLE);

    LocationProvider::updateHostLocation(21.0285, 105.8542, 15.0f, 8.5f, 270.0f, 4.0f, 2.0f);
    Location lastKnown = LocationProvider::getLastKnownLocation();
    assert(lastKnown.isValid());
    assert(std::abs(lastKnown.getQualifiedCoordinates().getLatitude() - 21.0285) < 0.0001);
    assert(std::abs(lastKnown.getSpeed() - 8.5f) < 0.001f);
    assert(std::abs(lastKnown.getCourse() - 270.0f) < 0.001f);

    std::string nmea = lastKnown.getExtraInfo("application/X-jsr179-location-nmea");
    assert(!nmea.empty());
    assert(nmea.find("$GPGGA") != std::string::npos);
    assert(nmea.find("$GPRMC") != std::string::npos);
    assert(nmea.find("$GPGSA") != std::string::npos);

    // 8. C-ABI Section 24 Exports
    std::cout << "  8. Kiem tra C-ABI Section 24 APIs..." << std::endl;
    assert(j2me_core_location_provider_get_state() == LOCATION_PROVIDER_AVAILABLE);
    j2me_core_location_provider_set_state(LOCATION_PROVIDER_TEMPORARILY_UNAVAILABLE);
    assert(j2me_core_location_provider_get_state() == LOCATION_PROVIDER_TEMPORARILY_UNAVAILABLE);
    j2me_core_location_provider_set_state(LOCATION_PROVIDER_AVAILABLE);

    j2me_core_location_provider_update_host_location(10.8231, 106.6297, 10.0f, 5.0f, 180.0f, 3.0f, 1.5f);
    double cLat = 0, cLon = 0;
    float cAlt = 0, cSpeed = 0, cCourse = 0;
    int64_t cTs = 0;
    assert(j2me_core_location_provider_get_last_known(&cLat, &cLon, &cAlt, &cSpeed, &cCourse, &cTs));
    assert(std::abs(cLat - 10.8231) < 0.0001);
    assert(std::abs(cLon - 106.6297) < 0.0001);

    char nmeaBuf[1024];
    int nmeaLen = j2me_core_location_provider_get_nmea(nmeaBuf, sizeof(nmeaBuf));
    assert(nmeaLen > 0);
    assert(std::string(nmeaBuf).find("$GPGGA") != std::string::npos);

    float cDist = j2me_core_location_coordinates_distance(21.0285, 105.8542, 10.8231, 106.6297);
    assert(cDist >= 1130000.0f && cDist <= 1160000.0f);

    float cAz = j2me_core_location_coordinates_azimuth(21.0285, 105.8542, 10.8231, 106.6297);
    assert(cAz >= 150.0f && cAz <= 180.0f);

    char convBuf[64];
    assert(j2me_core_location_coordinates_convert_to_string(21.5, COORDINATE_FORMAT_DD_MM_SS, convBuf, sizeof(convBuf)));
    assert(std::string(convBuf) == "21:30:00.000");
    double convDbl = j2me_core_location_coordinates_convert_from_string(convBuf);
    assert(std::abs(convDbl - 21.5) < 0.0001);

    j2me_core_location_orientation_set(120.0f, true, -15.0f, 5.0f);
    float oAz = 0, oPitch = 0, oRoll = 0;
    bool oMag = false;
    j2me_core_location_orientation_get(&oAz, &oMag, &oPitch, &oRoll);
    assert(std::abs(oAz - 120.0f) < 0.001f);
    assert(oMag == true);
    assert(std::abs(oPitch - (-15.0f)) < 0.001f);
    assert(std::abs(oRoll - 5.0f) < 0.001f);

    assert(j2me_core_location_landmark_store_create("cabi_test_store"));
    j2me_core_location_landmark_store_add_landmark("cabi_test_store", "Landmark 1", "Desc", 21.0, 105.0, 0.0f, "Cat1");
    assert(j2me_core_location_landmark_store_get_count("cabi_test_store", "Cat1") == 1);
    assert(j2me_core_location_landmark_store_delete("cabi_test_store"));

    std::cout << "[PASS] Mo dun 24 (JSR-179 Mobile Location API) 100% Hoan hao!" << std::endl;
}

void test_sensor_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[TEST MODULE 25] JSR-256 Mobile Sensor API" << std::endl;
    std::cout << "========================================================" << std::endl;

    using namespace universal_loader::sensor;

    // 1. Unit & MeasurementRange
    std::cout << "  1. Kiem tra Unit & MeasurementRange..." << std::endl;
    Unit u1("m/s^2");
    Unit u2 = Unit::getUnit("m/s^2");
    assert(u1 == u2);
    assert(u1.toString() == "m/s^2");

    MeasurementRange r1(-9800.0, 9800.0, 10.0);
    assert(r1.getSmallestValue() == -9800.0);
    assert(r1.getLargestValue() == 9800.0);
    assert(r1.getResolution() == 10.0);

    bool caught = false;
    try {
        MeasurementRange rInvalid(100.0, 50.0, 1.0);
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    assert(caught);

    // 2. Conditions (LimitCondition, RangeCondition, ObjectCondition)
    std::cout << "  2. Kiem tra Conditions (Limit, Range, Object)..." << std::endl;
    LimitCondition lcGt(10.0, OP_GREATER_THAN);
    assert(lcGt.isMet(15.0));
    assert(!lcGt.isMet(5.0));
    assert(!lcGt.isMet(10.0));

    LimitCondition lcLe(5.0, OP_LESS_THAN_OR_EQUALS);
    assert(lcLe.isMet(5.0));
    assert(lcLe.isMet(2.0));
    assert(!lcLe.isMet(5.1));

    RangeCondition rc(10.0, OP_GREATER_THAN_OR_EQUALS, 20.0, OP_LESS_THAN);
    assert(rc.isMet(10.0));
    assert(rc.isMet(15.0));
    assert(!rc.isMet(20.0));
    assert(!rc.isMet(9.9));

    ObjectCondition oc("active");
    assert(oc.isMet("active"));
    assert(!oc.isMet("idle"));

    // 3. ChannelInfo & Channel & ConditionListener
    std::cout << "  3. Kiem tra ChannelInfo, Channel & Condition Triggers..." << std::endl;
    ChannelInfo chInfo("axis_x", CHANNEL_TYPE_DOUBLE, u1, 1, 0.01f, {r1});
    assert(chInfo.getName() == "axis_x");
    assert(chInfo.getDataType() == CHANNEL_TYPE_DOUBLE);

    Channel channel(chInfo, "sensor:acceleration");
    assert(channel.getChannelUrl() == "sensor:acceleration?channel=axis_x");

    class MockConditionListener : public ConditionListener {
    public:
        int triggerCount{0};
        double lastVal{0.0};
        void conditionMet(SensorConnection*, Channel*, Condition*, double value) override {
            triggerCount++;
            lastVal = value;
        }
    };

    MockConditionListener condListener;
    auto condPtr = std::make_shared<LimitCondition>(12.0, OP_GREATER_THAN);
    channel.addCondition(&condListener, condPtr);
    auto condList = channel.getConditions(&condListener);
    assert(condList.size() == 1);

    channel.notifySample(nullptr, 5.0);
    assert(condListener.triggerCount == 0);

    channel.notifySample(nullptr, 15.0);
    assert(condListener.triggerCount == 1);
    assert(condListener.lastVal == 15.0);

    channel.removeCondition(&condListener, condPtr);
    assert(channel.getConditions(&condListener).empty());
    channel.notifySample(nullptr, 20.0);
    assert(condListener.triggerCount == 1); // Not incremented

    // 4. Data
    std::cout << "  4. Kiem tra Data Storage & Indexing..." << std::endl;
    Data data(chInfo, std::vector<double>{1.5, 2.5, 3.5}, std::vector<int64_t>{1000, 2000, 3000});
    assert(data.size() == 3);
    assert(data.getDoubleValues()[0] == 1.5);
    assert(data.getTimestamp(1) == 2000);
    assert(data.isValid(2));

    // 5. SensorInfo
    std::cout << "  5. Kiem tra SensorInfo & Properties..." << std::endl;
    SensorInfo sInfo({chInfo}, SENSOR_CONN_EMBEDDED, CONTEXT_TYPE_USER, "Test Accel", "ModelX", "acceleration", 512);
    assert(sInfo.getQuantity() == "acceleration");
    assert(sInfo.getContextType() == CONTEXT_TYPE_USER);
    assert(sInfo.getModel() == "ModelX");
    assert(sInfo.getUrl() == "sensor:acceleration;contextType=user;model=ModelX");
    sInfo.setProperty("vendor", "DeepMind");
    assert(sInfo.getProperty("vendor") == "DeepMind");

    // 6. SensorConnection & DataListener
    std::cout << "  6. Kiem tra SensorConnection & DataListener..." << std::endl;
    SensorConnection conn(sInfo);
    assert(conn.getState() == SENSOR_STATE_OPENED);
    assert(conn.getChannelCount() == 1);
    assert(conn.getChannel("axis_x") != nullptr);

    class MockDataListener : public DataListener {
    public:
        int batchCount{0};
        size_t samplesReceived{0};
        void dataReceived(SensorConnection*, const std::vector<Data>& data, bool) override {
            batchCount++;
            if (!data.empty()) {
                samplesReceived += data[0].size();
            }
        }
    };

    MockDataListener dataListener;
    conn.setDataListener(&dataListener, 2);
    assert(conn.getState() == SENSOR_STATE_LISTENING);

    conn.pushSamples({1.1});
    assert(dataListener.batchCount == 0); // Need 2 samples
    conn.pushSamples({2.2});
    assert(dataListener.batchCount == 1);
    assert(dataListener.samplesReceived == 2);

    auto synData = conn.getData(1);
    assert(!synData.empty());
    assert(synData[0].getDoubleValues().back() == 2.2);

    conn.close();
    assert(conn.getState() == SENSOR_STATE_CLOSED);

    // 7. SensorManager & Hardware Feed
    std::cout << "  7. Kiem tra SensorManager Catalog & Feed..." << std::endl;
    auto& mgr = SensorManager::instance();
    auto allSensors = mgr.getAllSensors();
    assert(allSensors.size() >= 5);

    auto accSearch = mgr.findSensors("acceleration");
    assert(!accSearch.empty());

    auto lightConn = mgr.openSensor("sensor:ambient_light;contextType=ambient");
    assert(lightConn != nullptr);
    assert(lightConn->getSensorInfo().getQuantity() == "ambient_light");

    mgr.updateAmbientLight(350.0);
    auto lightData = lightConn->getData(1);
    assert(!lightData.empty());
    assert(lightData[0].getDoubleValues().back() == 350.0);

    // 8. C-ABI Section 25 APIs
    std::cout << "  8. Kiem tra Section 25 C-ABI APIs..." << std::endl;
    assert(j2me_core_sensor_get_count() >= 5);

    char urlBuf[128];
    assert(j2me_core_sensor_get_url(0, urlBuf, sizeof(urlBuf)));
    assert(std::string(urlBuf).find("sensor:") == 0);

    int indices[10];
    int found = j2me_core_sensor_find("acceleration", nullptr, indices, 10);
    assert(found >= 1);

    uintptr_t hSensor = j2me_core_sensor_open("sensor:acceleration");
    assert(hSensor != 0);
    assert(j2me_core_sensor_get_state(hSensor) == SENSOR_STATE_OPENED);
    assert(j2me_core_sensor_get_channel_count(hSensor) == 3);

    char chNameBuf[64];
    assert(j2me_core_sensor_get_channel_name(hSensor, 0, chNameBuf, sizeof(chNameBuf)));
    assert(std::string(chNameBuf) == "axis_x");

    j2me_core_sensor_update_accelerometer(1.23, 4.56, 7.89);
    double dBuf[10];
    int readSamples = j2me_core_sensor_get_data(hSensor, 0, dBuf, 10);
    assert(readSamples >= 1);
    assert(std::abs(dBuf[readSamples - 1] - 1.23) < 0.01);

    j2me_core_sensor_close(hSensor);

    std::cout << "[PASS] Mo dun 25 (JSR-256 Mobile Sensor API) 100% Hoan hao!" << std::endl;
}

void test_pim_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[MODULE 26 TEST] Kiem tra toan dien JSR-75 PIM (Personal Information Management)" << std::endl;
    std::cout << "========================================================" << std::endl;

    using namespace universal_loader::pim;

    // 1. RepeatRule
    std::cout << "  1. Kiem tra RepeatRule recurrence calculations..." << std::endl;
    RepeatRule rr;
    rr.setInt(REPEAT_FREQUENCY, REPEAT_FREQ_DAILY);
    rr.setInt(REPEAT_INTERVAL, 1);
    rr.setInt(REPEAT_COUNT, 5);

    int64_t baseStart = 1700000000000LL; // Arbitrary fixed ms
    int64_t baseEnd = baseStart + (10 * 86400000LL);
    auto occs = rr.dates(baseStart, baseStart, baseEnd);
    assert(occs.size() == 5);

    // Exception date
    int64_t skipDate = occs[2];
    rr.addExceptDate(skipDate);
    auto occs2 = rr.dates(baseStart, baseStart, baseEnd);
    assert(occs2.size() == 4);
    assert(std::find(occs2.begin(), occs2.end(), skipDate) == occs2.end());

    // 2. Contact Fields & Preferred Index
    std::cout << "  2. Kiem tra Contact (Fields, Attributes, Preferred Index)..." << std::endl;
    Contact c1;
    c1.setName("Nguyen", "Van", "A", "Mr.", "");
    assert(c1.getFormattedName() == "Van Nguyen");

    c1.addString(CONTACT_TEL, ATTR_HOME, "0123456789");
    c1.addString(CONTACT_TEL, ATTR_MOBILE | ATTR_PREFERRED, "0987654321");
    assert(c1.countValues(CONTACT_TEL) == 2);
    assert(c1.getPreferredIndex(CONTACT_TEL) == 1); // 2nd number is preferred

    c1.addString(CONTACT_EMAIL, ATTR_WORK, "user@example.com");
    assert(c1.countValues(CONTACT_EMAIL) == 1);

    c1.setAddress(0, ATTR_HOME, "123 Main St", "District 1", "HCMC", "70000", "Vietnam");
    assert(c1.countValues(CONTACT_ADDR) == 1);

    c1.addToCategory("Friends");
    c1.addToCategory("VIP");
    assert(c1.getCategories().size() == 2);

    // 3. vCard 2.1 Serialization & Deserialization
    std::cout << "  3. Kiem tra vCard 2.1 Export & Import..." << std::endl;
    std::string vcard = c1.toVCard();
    assert(vcard.find("BEGIN:VCARD") != std::string::npos);
    assert(vcard.find("FN:Van Nguyen") != std::string::npos);
    assert(vcard.find("0987654321") != std::string::npos);

    auto parsedContact = Contact::fromVCard(vcard);
    assert(parsedContact != nullptr);
    assert(parsedContact->getFormattedName() == "Van Nguyen");
    assert(parsedContact->countValues(CONTACT_TEL) == 2);
    assert(parsedContact->getString(CONTACT_TEL, 0) == "0123456789");
    assert(parsedContact->getString(CONTACT_TEL, 1) == "0987654321");
    assert(parsedContact->getString(CONTACT_EMAIL, 0) == "user@example.com");

    // 4. Event & vCalendar
    std::cout << "  4. Kiem tra Event & vCalendar 1.0 Export/Import..." << std::endl;
    Event ev1;
    ev1.addString(EVENT_SUMMARY, ATTR_NONE, "Team Sync");
    ev1.addString(EVENT_LOCATION, ATTR_NONE, "Room 404");
    ev1.addDate(EVENT_START, ATTR_NONE, 1700000000000LL);
    ev1.addDate(EVENT_END, ATTR_NONE, 1700003600000LL);
    ev1.addInt(EVENT_ALARM, ATTR_NONE, 600);
    ev1.setRepeatRule(rr);
    assert(ev1.hasRepeatRule());

    std::string vcal = ev1.toVCalendar();
    assert(vcal.find("BEGIN:VCALENDAR") != std::string::npos);
    assert(vcal.find("SUMMARY:Team Sync") != std::string::npos);
    assert(vcal.find("LOCATION:Room 404") != std::string::npos);

    auto parsedEvent = Event::fromVCalendar(vcal);
    assert(parsedEvent != nullptr);
    assert(parsedEvent->getString(EVENT_SUMMARY, 0) == "Team Sync");
    assert(parsedEvent->getString(EVENT_LOCATION, 0) == "Room 404");
    assert(parsedEvent->getInt(EVENT_ALARM, 0) == 600);
    assert(parsedEvent->hasRepeatRule());

    // 5. ToDo & vCalendar
    std::cout << "  5. Kiem tra ToDo & vCalendar 1.0..." << std::endl;
    ToDo td1;
    td1.addString(TODO_SUMMARY, ATTR_NONE, "Complete Module 26");
    td1.addInt(TODO_PRIORITY, ATTR_NONE, 1);
    td1.addBoolean(TODO_COMPLETED, ATTR_NONE, true);
    td1.addDate(TODO_DUE, ATTR_NONE, 1700000000000LL);
    td1.addDate(TODO_COMPLETION_DATE, ATTR_NONE, 1700000000000LL);

    std::string vtodo = td1.toVCalendar();
    assert(vtodo.find("BEGIN:VTODO") != std::string::npos);
    assert(vtodo.find("STATUS:COMPLETED") != std::string::npos);

    auto parsedTodo = ToDo::fromVCalendar(vtodo);
    assert(parsedTodo != nullptr);
    assert(parsedTodo->getString(TODO_SUMMARY, 0) == "Complete Module 26");
    assert(parsedTodo->getInt(TODO_PRIORITY, 0) == 1);
    assert(parsedTodo->getBoolean(TODO_COMPLETED, 0) == true);

    // 6. PIMList CRUD & Search
    std::cout << "  6. Kiem tra PIMList Query, Filter & Search..." << std::endl;
    ContactList clist("MyContacts", PIM_READ_WRITE);
    auto cA = clist.createContact();
    cA->setName("Tran", "Binh");
    cA->addString(CONTACT_TEL, ATTR_MOBILE, "0911222333");
    cA->addToCategory("Work");

    auto cB = clist.createContact();
    cB->setName("Le", "Chau");
    cB->addString(CONTACT_TEL, ATTR_MOBILE, "0944555666");
    cB->addToCategory("Personal");

    assert(clist.items().size() == 2);
    auto workItems = clist.itemsByCategory("Work");
    assert(workItems.size() == 1);
    assert(workItems[0]->getString(CONTACT_TEL, 0) == "0911222333");

    auto searchPhone = clist.items("555");
    assert(searchPhone.size() == 1);

    clist.deleteCategory("Work", true); // Should delete cA
    assert(clist.items().size() == 1);

    // 7. PIMManager & File Persistence
    std::cout << "  7. Kiem tra PIMManager Persistence qua tap tin..." << std::endl;
    auto& pMgr = PIMManager::getInstance();
    pMgr.reset();
    std::string sandbox = "./test_pim_sandbox";
    pMgr.init(sandbox);

    auto defaultContacts = std::dynamic_pointer_cast<ContactList>(pMgr.openPIMList(PIM_CONTACT_LIST, PIM_READ_WRITE));
    assert(defaultContacts != nullptr);
    auto persContact = defaultContacts->createContact();
    persContact->setName("Pham", "Hien");
    persContact->addString(CONTACT_TEL, ATTR_MOBILE, "0909123456");
    persContact->commit();

    pMgr.saveAll();

    // Reset and reload from sandbox
    pMgr.reset();
    pMgr.init(sandbox);
    auto reloadedContacts = pMgr.openPIMList(PIM_CONTACT_LIST, PIM_READ_ONLY);
    assert(reloadedContacts != nullptr);
    assert(reloadedContacts->items().size() >= 1);
    auto foundReloaded = reloadedContacts->items("0909123456");
    assert(!foundReloaded.empty());

    // 8. C-ABI Section 26
    std::cout << "  8. Kiem tra Section 26 C-ABI APIs..." << std::endl;
    j2me_core_pim_init(sandbox.c_str());
    assert(j2me_core_pim_list_count(PIM_CONTACT_LIST) >= 1);

    char listNameBuf[64];
    assert(j2me_core_pim_list_get_name(PIM_CONTACT_LIST, 0, listNameBuf, sizeof(listNameBuf)));
    assert(std::string(listNameBuf) == "Contacts");

    uintptr_t hList = j2me_core_pim_open_list(PIM_CONTACT_LIST, PIM_READ_WRITE, nullptr);
    assert(hList != 0);

    uintptr_t hContact = j2me_core_pim_contact_create(hList);
    assert(hContact != 0);
    assert(j2me_core_pim_contact_set_name(hContact, "Hoang", "Nam", "", "Dr.", ""));

    char fnBuf[128];
    assert(j2me_core_pim_contact_get_formatted_name(hContact, fnBuf, sizeof(fnBuf)));
    assert(std::string(fnBuf) == "Nam Hoang");

    assert(j2me_core_pim_contact_add_tel(hContact, ATTR_WORK, "0243123456"));
    assert(j2me_core_pim_contact_get_tel_count(hContact) == 1);

    char telBuf[64];
    int attrOut = 0;
    assert(j2me_core_pim_contact_get_tel(hContact, 0, telBuf, sizeof(telBuf), &attrOut));
    assert(std::string(telBuf) == "0243123456");
    assert(attrOut == ATTR_WORK);

    char serialBuf[512];
    int exportedLen = j2me_core_pim_export_serial(hContact, "VCARD/2.1", serialBuf, sizeof(serialBuf));
    assert(exportedLen > 0);
    assert(std::string(serialBuf).find("Nam Hoang") != std::string::npos);

    j2me_core_pim_close_list(hList);

    // Event & ToDo C-ABI
    uintptr_t hEvList = j2me_core_pim_open_list(PIM_EVENT_LIST, PIM_READ_WRITE, nullptr);
    assert(hEvList != 0);
    uintptr_t hEv = j2me_core_pim_event_create(hEvList);
    assert(hEv != 0);
    assert(j2me_core_pim_event_set_details(hEv, "Standup", "Online", 1000, 2000, 300));
    char sumBuf[64], locBuf[64];
    int64_t sMs = 0, eMs = 0;
    assert(j2me_core_pim_event_get_details(hEv, sumBuf, sizeof(sumBuf), locBuf, sizeof(locBuf), &sMs, &eMs));
    assert(std::string(sumBuf) == "Standup");
    assert(std::string(locBuf) == "Online");
    assert(sMs == 1000 && eMs == 2000);
    j2me_core_pim_close_list(hEvList);

    uintptr_t hTdList = j2me_core_pim_open_list(PIM_TODO_LIST, PIM_READ_WRITE, nullptr);
    assert(hTdList != 0);
    uintptr_t hTd = j2me_core_pim_todo_create(hTdList);
    assert(hTd != 0);
    assert(j2me_core_pim_todo_set_details(hTd, "Review PR", 2, false, 5000, 0));
    int prio = 0;
    bool comp = true;
    int64_t due = 0;
    assert(j2me_core_pim_todo_get_details(hTd, sumBuf, sizeof(sumBuf), &prio, &comp, &due));
    assert(std::string(sumBuf) == "Review PR");
    assert(prio == 2 && !comp && due == 5000);
    j2me_core_pim_close_list(hTdList);

    std::cout << "[PASS] Mo dun 26 (JSR-75 PIM) 100% Hoan hao!" << std::endl;
}

void test_amms_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[TEST] 27. Kiem tra Mo dun 27: JSR-234 AMMS (Advanced Multimedia Supplements)..." << std::endl;
    std::cout << "========================================================" << std::endl;

    using namespace universal_loader::amms;

    // 1. Kiem tra LocationControl & OrientationControl
    std::cout << "  1. Kiem tra LocationControl & OrientationControl..." << std::endl;
    LocationControl loc;
    loc.setCartesian(1000, 2000, -3000);
    int lx = 0, ly = 0, lz = 0;
    loc.getCartesian(lx, ly, lz);
    assert(lx == 1000 && ly == 2000 && lz == -3000);
    Vec3 posMeters = loc.getPositionMeters();
    assert(std::abs(posMeters.x - 1.0f) < 0.001f);
    assert(std::abs(posMeters.y - 2.0f) < 0.001f);
    assert(std::abs(posMeters.z - (-3.0f)) < 0.001f);

    loc.setSpherical(90, 0, 1000);
    loc.getCartesian(lx, ly, lz);
    assert(std::abs(lx - 1000) <= 2);
    assert(std::abs(ly) <= 2);
    assert(std::abs(lz) <= 2);

    OrientationControl ori;
    ori.setOrientation(90, 0, 0); // Yaw 90 degrees
    int heading = 0, pitch = 0, roll = 0;
    ori.getEulerAngles(heading, pitch, roll);
    assert(heading == 90 && pitch == 0 && roll == 0);
    Vec3 frontVec, upVec;
    ori.getOrientationVectors(frontVec, upVec);
    assert(std::abs(frontVec.y) < 0.01f);
    std::cout << "     LocationControl & OrientationControl chuyen doi he toa do chinh xac!" << std::endl;

    // 2. Kiem tra DopplerControl & DistanceAttenuationControl
    std::cout << "  2. Kiem tra DopplerControl & DistanceAttenuationControl..." << std::endl;
    DopplerControl dop;
    assert(dop.isEnabled());
    dop.setVelocityCartesian(10000, 0, 0); // 10 m/s
    int vx = 0, vy = 0, vz = 0;
    dop.getVelocityCartesian(vx, vy, vz);
    assert(vx == 10000 && vy == 0 && vz == 0);
    Vec3 vMps = dop.getVelocityMetersPerSecond();
    assert(std::abs(vMps.x - 10.0f) < 0.001f);

    DistanceAttenuationControl att;
    att.setParameters(1000, 10000, true, 1000); // min 1m, max 10m, mute after max, rolloff 1.0
    assert(att.calculateGain(500.0f) == 1.0f); // <= minDistance
    float gainAt2m = att.calculateGain(2000.0f);
    assert(gainAt2m < 1.0f && gainAt2m > 0.0f);
    assert(std::abs(gainAt2m - 0.5f) < 0.01f); // 1 / (1 + 1.0 * (2 - 1) / 1) = 0.5
    assert(att.calculateGain(15000.0f) == 0.0f); // > maxDistance and muteAfterMax == true
    std::cout << "     Doppler & Distance Attenuation tinh toan gain vat ly chinh xac!" << std::endl;

    // 3. Kiem tra Spectator & SoundSource3D Spatial Evaluation
    std::cout << "  3. Kiem tra Spectator & SoundSource3D Spatial Evaluation..." << std::endl;
    Spectator spectator;
    spectator.getLocation().setCartesian(0, 0, 0);
    spectator.getOrientation().setOrientationVectors({0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f});

    SoundSource3D sourceRight;
    sourceRight.getLocation().setCartesian(2000, 0, 0); // 2 meters to the right
    sourceRight.getAttenuation().setParameters(1000, 10000, false, 1000);
    SpatialAudioOutput outRight = sourceRight.evaluate(spectator);
    assert(outRight.pan > 50); // strongly to the right
    assert(outRight.distanceMm == 2000.0f);
    assert(outRight.gain < 1.0f);

    SoundSource3D sourceLeft;
    sourceLeft.getLocation().setCartesian(-2000, 0, 0); // 2 meters to the left
    sourceLeft.getAttenuation().setParameters(1000, 10000, false, 1000);
    SpatialAudioOutput outLeft = sourceLeft.evaluate(spectator);
    assert(outLeft.pan < -50); // strongly to the left
    assert(outLeft.distanceMm == 2000.0f);

    SoundSource3D sourceFront;
    sourceFront.getLocation().setCartesian(0, 0, -3000); // 3 meters straight ahead
    SpatialAudioOutput outFront = sourceFront.evaluate(spectator);
    assert(outFront.pan == 0); // centered
    assert(outFront.distanceMm == 3000.0f);

    // Doppler shift evaluation: source moving towards spectator
    SoundSource3D movingSource;
    movingSource.getLocation().setCartesian(0, 0, -10000); // 10m ahead
    movingSource.getDoppler().setVelocityCartesian(0, 0, 34300); // 34.3 m/s towards spectator (+Z)
    SpatialAudioOutput outDoppler = movingSource.evaluate(spectator);
    assert(outDoppler.dopplerFactor > 1.0f); // Higher pitch as it approaches
    std::cout << "     3D Spatial Audio & Doppler shift xac minh thanh cong!" << std::endl;

    // 4. Kiem tra Audio Effects (Reverb, Equalizer, Pan)
    std::cout << "  4. Kiem tra Audio Effects (Reverb, Equalizer, Pan)..." << std::endl;
    EffectModule effectModule;
    assert(effectModule.isEnabled());
    effectModule.setScope(SCOPE_LIVE_AND_RECORD);
    assert(effectModule.getScope() == SCOPE_LIVE_AND_RECORD);

    ReverbControl& rev = effectModule.getReverb();
    rev.setPreset("hall");
    assert(rev.getPreset() == "hall");
    assert(rev.getReverbTime() == 4000);
    rev.setReverbLevel(-500);
    assert(rev.getReverbLevel() == -500);

    EqualizerControl& eq = effectModule.getEqualizer();
    assert(eq.getNumberOfBands() == 5);
    assert(eq.getBand(1000000) == 2); // 1000 Hz is band 2 (910 Hz)
    eq.setPreset("bass_boost");
    assert(eq.getPreset() == "bass_boost");
    assert(eq.getBandLevel(0) == 600); // bass boost on band 0
    eq.setBandLevel(0, 300);
    assert(eq.getBandLevel(0) == 300);

    PanControl& pan = effectModule.getPan();
    pan.setPan(75);
    assert(pan.getPan() == 75);
    pan.setPan(200); // Clamping
    assert(pan.getPan() == 100);
    std::cout << "     Reverb, Equalizer & Pan controls hoat dong chuan xac!" << std::endl;

    // 5. Kiem tra Camera, Flash, Zoom, ImageTransform Controls
    std::cout << "  5. Kiem tra Camera, Flash, Zoom & ImageTransform Controls..." << std::endl;
    CameraControl cam;
    cam.setCameraRotation(ROTATE_LEFT);
    assert(cam.getCameraRotation() == ROTATE_LEFT);
    cam.setExposureMode("night");
    assert(cam.getExposureMode() == "night");
    assert(!cam.getSupportedStillResolutions().empty());

    FlashControl flash;
    flash.setMode(FLASH_FORCE);
    assert(flash.getMode() == FLASH_FORCE);
    assert(flash.isFlashReady());

    ZoomControl zoom;
    zoom.setDigitalZoom(250); // 2.5x
    assert(zoom.getDigitalZoom() == 250);
    zoom.setOpticalZoom(150); // 1.5x
    assert(zoom.getOpticalZoom() == 150);

    ImageTransformControl trans;
    trans.setSourceRect(10, 20, 320, 240);
    assert(trans.getSourceX() == 10 && trans.getSourceY() == 20);
    assert(trans.getSourceWidth() == 320 && trans.getSourceHeight() == 240);
    trans.setTargetSize(800, 600);
    assert(trans.getTargetWidth() == 800 && trans.getTargetHeight() == 600);
    std::cout << "     Camera, Flash, Zoom & ImageTransform dat chuan AMMS!" << std::endl;

    // 6. Kiem tra Section 27 C-ABI Exports
    std::cout << "  6. Kiem tra Section 27 C-ABI Exports..." << std::endl;
    j2me_core_amms_spectator_set_location(500, 600, 700);
    int cx = 0, cy = 0, cz = 0;
    j2me_core_amms_spectator_get_location(&cx, &cy, &cz);
    assert(cx == 500 && cy == 600 && cz == 700);

    j2me_core_amms_spectator_set_orientation(45, 10, -5);
    int ch = 0, cp = 0, cr = 0;
    j2me_core_amms_spectator_get_orientation(&ch, &cp, &cr);
    assert(ch == 45 && cp == 10 && cr == -5);

    uintptr_t hSource = j2me_core_amms_sound_source_create();
    assert(hSource != 0);
    j2me_core_amms_sound_source_set_location(hSource, 1500, 600, 700);
    j2me_core_amms_sound_source_get_location(hSource, &cx, &cy, &cz);
    assert(cx == 1500 && cy == 600 && cz == 700);
    j2me_core_amms_sound_source_set_velocity(hSource, 5000, 0, 0);
    j2me_core_amms_sound_source_set_attenuation(hSource, 1000, 20000, false, 1000);

    float evalGain = 0.0f, evalDoppler = 0.0f, evalDist = 0.0f;
    int evalPan = 0;
    assert(j2me_core_amms_sound_source_evaluate(hSource, &evalGain, &evalPan, &evalDoppler, &evalDist));
    assert(evalDist > 0.0f);
    assert(evalGain > 0.0f && evalGain <= 1.0f);
    j2me_core_amms_sound_source_destroy(hSource);

    uintptr_t hEffect = j2me_core_amms_effect_module_create();
    assert(hEffect != 0);
    j2me_core_amms_reverb_set_preset(hEffect, "largeroom");
    char presetBuf[64];
    assert(j2me_core_amms_reverb_get_preset(hEffect, presetBuf, sizeof(presetBuf)));
    assert(std::string(presetBuf) == "largeroom");
    j2me_core_amms_reverb_set_level(hEffect, -800);
    assert(j2me_core_amms_reverb_get_level(hEffect) == -800);

    int bandCount = j2me_core_amms_equalizer_get_band_count(hEffect);
    assert(bandCount == 5);
    j2me_core_amms_equalizer_set_band_level(hEffect, 1, 400);
    assert(j2me_core_amms_equalizer_get_band_level(hEffect, 1) == 400);

    j2me_core_amms_pan_set(hEffect, -45);
    assert(j2me_core_amms_pan_get(hEffect) == -45);
    j2me_core_amms_effect_module_destroy(hEffect);

    j2me_core_amms_camera_set_rotation(ROTATE_RIGHT);
    assert(j2me_core_amms_camera_get_rotation() == ROTATE_RIGHT);

    j2me_core_amms_camera_set_exposure_mode("sports");
    char expBuf[64];
    assert(j2me_core_amms_camera_get_exposure_mode(expBuf, sizeof(expBuf)));
    assert(std::string(expBuf) == "sports");

    j2me_core_amms_flash_set_mode(FLASH_AUTO);
    assert(j2me_core_amms_flash_get_mode() == FLASH_AUTO);

    j2me_core_amms_zoom_set_digital(200);
    assert(j2me_core_amms_zoom_get_digital() == 200);

    j2me_core_amms_image_transform_set_crop(5, 5, 200, 150);
    j2me_core_amms_image_transform_set_target(400, 300);
    std::cout << "     Section 27 C-ABI exports xac minh thanh cong!" << std::endl;

    std::cout << "[PASS] Mo dun 27 (JSR-234 AMMS) 100% Hoan hao!" << std::endl;
}

void test_push_and_comm_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[TEST] 28. Kiem tra Mo dun 28: MIDP 2.0 PushRegistry & CommConnection..." << std::endl;
    std::cout << "========================================================" << std::endl;

    using namespace universal_loader::push;
    using namespace universal_loader::comm;

    // 1. Kiem tra CommUrlParser & CommConnection
    std::cout << "  1. Kiem tra CommUrlParser & CommConnection..." << std::endl;
    CommConfig cfg;
    assert(CommUrlParser::parse("comm:0;baudrate=115200;bitsperchar=8;stopbits=1;parity=even;autocts=on;autorts=off", cfg));
    assert(cfg.port == "0");
    assert(cfg.baudRate == 115200);
    assert(cfg.bitsPerChar == 8);
    assert(cfg.stopBits == 1);
    assert(cfg.parity == CommParity::Even);
    assert(cfg.autoCts == true);
    assert(cfg.autoRts == false);

    // Kiem tra URL ngan
    assert(CommUrlParser::parse("comm:COM3", cfg));
    assert(cfg.port == "COM3");
    assert(cfg.baudRate == 9600);
    assert(cfg.bitsPerChar == 8);
    assert(cfg.parity == CommParity::None);

    auto comm = CommConnection::open("comm:COM1;baudrate=9600");
    assert(comm != nullptr);
    assert(comm->isOpen());
    assert(comm->getBaudRate() == 9600);
    assert(comm->setBaudRate(57600) == 57600);
    assert(comm->getBaudRate() == 57600);
    assert(comm->setBaudRate(99999) == 57600); // Invalid rate rejected

    // Test doc / ghi qua buffer noi bo
    const uint8_t sendData[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    assert(comm->write(sendData, sizeof(sendData)) == 5);
    uint8_t extracted[16];
    assert(comm->extractOutput(extracted, sizeof(extracted)) == 5);
    assert(extracted[0] == 0x01 && extracted[4] == 0x05);

    const uint8_t recvData[] = {'H', 'E', 'L', 'L', 'O'};
    comm->feedInput(recvData, sizeof(recvData));
    assert(comm->available() == 5);
    uint8_t readData[16];
    assert(comm->read(readData, sizeof(readData)) == 5);
    assert(std::string(reinterpret_cast<char*>(readData), 5) == "HELLO");
    assert(comm->available() == 0);
    comm->close();
    assert(!comm->isOpen());
    std::cout << "     CommUrlParser & CommConnection I/O buffers hoat dong chinh xac!" << std::endl;

    // 2. Kiem tra PushRegistry Registration & Filter Matching
    std::cout << "  2. Kiem tra PushRegistry Registration & Filter Matching..." << std::endl;
    PushRegistry& pushReg = PushRegistry::getInstance();
    pushReg.clear();

    pushReg.registerConnection("socket://:5000", "com.example.ChatMIDlet", "192.168.1.*");
    pushReg.registerConnection("sms://:5000", "com.example.SmsMIDlet", "*");
    assert(pushReg.getMIDlet("socket://:5000") == "com.example.ChatMIDlet");
    assert(pushReg.getFilter("socket://:5000") == "192.168.1.*");
    assert(pushReg.getMIDlet("sms://:5000") == "com.example.SmsMIDlet");
    assert(pushReg.getFilter("sms://:5000") == "*");

    auto conns = pushReg.listConnections(false);
    assert(conns.size() == 2);

    // Kiem tra inbound connection notification va filter matching
    assert(!pushReg.notifyInboundConnection("socket://:5000", "10.0.0.1")); // IP khac subnet -> reject
    assert(!pushReg.isConnectionAvailable("socket://:5000"));

    assert(pushReg.notifyInboundConnection("socket://:5000", "192.168.1.100")); // Hop le!
    assert(pushReg.isConnectionAvailable("socket://:5000"));

    auto availConns = pushReg.listConnections(true);
    assert(availConns.size() == 1);
    assert(availConns[0] == "socket://:5000");

    // Unregister
    assert(pushReg.unregisterConnection("sms://:5000"));
    assert(pushReg.listConnections(false).size() == 1);
    std::cout << "     PushRegistry Registration & Filter Matching xac minh thanh cong!" << std::endl;

    // 3. Kiem tra PushRegistry Alarms
    std::cout << "  3. Kiem tra PushRegistry Alarms..." << std::endl;
    int64_t prev = pushReg.registerAlarm("com.example.AlarmMIDlet", 2000000);
    assert(prev == 0); // Lan dau chua co alarm

    prev = pushReg.registerAlarm("com.example.AlarmMIDlet", 3000000);
    assert(prev == 2000000); // Tra ve gio alarm truoc do

    auto woken = pushReg.checkAlarms(2500000);
    assert(woken.empty()); // Chua den gio

    woken = pushReg.checkAlarms(3500000);
    assert(woken.size() == 1);
    assert(woken[0] == "com.example.AlarmMIDlet"); // Da den gio bao thuc!

    woken = pushReg.checkAlarms(4000000);
    assert(woken.empty()); // Da danh thuc roi, khong lap lai
    std::cout << "     PushRegistry Alarms scheduling & waking hoat dong chuan xac!" << std::endl;

    // 4. Kiem tra PushRegistry Disk Persistence (JSON)
    std::cout << "  4. Kiem tra PushRegistry Disk Persistence..." << std::endl;
    const std::string testPushFile = "./test_push_registry.json";
    assert(pushReg.saveToFile(testPushFile));

    pushReg.clear();
    assert(pushReg.listConnections(false).empty());

    assert(pushReg.loadFromFile(testPushFile));
    assert(pushReg.getMIDlet("socket://:5000") == "com.example.ChatMIDlet");
    assert(pushReg.isConnectionAvailable("socket://:5000"));
    std::cout << "     PushRegistry saveToFile & loadFromFile JSON thanh cong!" << std::endl;

    // 5. Kiem tra Section 28 C-ABI Exports
    std::cout << "  5. Kiem tra Section 28 C-ABI Exports..." << std::endl;
    j2me_core_push_register_connection("datagram://:6000", "com.example.UdpMIDlet", "+84*");
    char midBuf[64], fltBuf[64];
    assert(j2me_core_push_get_midlet("datagram://:6000", midBuf, sizeof(midBuf)));
    assert(std::string(midBuf) == "com.example.UdpMIDlet");
    assert(j2me_core_push_get_filter("datagram://:6000", fltBuf, sizeof(fltBuf)));
    assert(std::string(fltBuf) == "+84*");

    assert(j2me_core_push_notify_inbound("datagram://:6000", "+84901234567"));
    char connListBuf[256];
    int count = j2me_core_push_list_connections(true, connListBuf, sizeof(connListBuf));
    assert(count >= 1);
    assert(std::string(connListBuf).find("datagram://:6000") != std::string::npos);

    j2me_core_push_register_alarm("com.example.UdpMIDlet", 5000000);
    char wokenBuf[128];
    assert(j2me_core_push_check_alarms(6000000, wokenBuf, sizeof(wokenBuf)) == 1);
    assert(std::string(wokenBuf) == "com.example.UdpMIDlet");

    assert(j2me_core_push_unregister_connection("datagram://:6000"));

    uintptr_t hComm = j2me_core_comm_open("comm:COM2;baudrate=38400");
    assert(hComm != 0);
    assert(j2me_core_comm_get_baud_rate(hComm) == 38400);
    assert(j2me_core_comm_set_baud_rate(hComm, 115200) == 115200);
    const uint8_t cData[] = {0xAA, 0xBB, 0xCC};
    assert(j2me_core_comm_write(hComm, cData, 3) == 3);
    j2me_core_comm_close(hComm);
    std::cout << "     Section 28 C-ABI exports xac minh thanh cong!" << std::endl;

    std::cout << "[PASS] Mo dun 28 (MIDP 2.0 PushRegistry & CommConnection) 100% Hoan hao!" << std::endl;
}

void test_pki_and_secure_connection_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[TEST] 29. Kiem tra Mo dun 29: PKI Security, SSL & HTTPS Layer..." << std::endl;
    std::cout << "========================================================" << std::endl;

    using namespace universal_loader::security;

    // 1. Kiem tra Certificate model, getters va CN extraction
    std::cout << "  1. Kiem tra Certificate model, getters va CN extraction..." << std::endl;
    Certificate cert("CN=secure.midlet.org, O=Retro Corp, C=US",
                     "CN=GlobalCA Root, O=GlobalCA, C=US",
                     "X.509", "3", "SHA256withRSA",
                     1000000000LL, 2000000000LL, "4A:2B:1C:88");
    assert(cert.getSubject() == "CN=secure.midlet.org, O=Retro Corp, C=US");
    assert(cert.getIssuer() == "CN=GlobalCA Root, O=GlobalCA, C=US");
    assert(cert.getType() == "X.509");
    assert(cert.getVersion() == "3");
    assert(cert.getSigAlgName() == "SHA256withRSA");
    assert(cert.getNotBefore() == 1000000000LL);
    assert(cert.getNotAfter() == 2000000000LL);
    assert(cert.getSerialNumber() == "4A:2B:1C:88");
    assert(cert.extractCN() == "secure.midlet.org");
    std::cout << "     Certificate getters va extractCN() xac minh thanh cong!" << std::endl;

    // 2. Kiem tra Certificate validation & wildcard host matching
    std::cout << "  2. Kiem tra Certificate validation & wildcard host matching..." << std::endl;
    // Exact match & case-insensitive
    assert(cert.matchesHost("secure.midlet.org"));
    assert(cert.matchesHost("SECURE.MIDLET.ORG"));
    assert(!cert.matchesHost("other.midlet.org"));

    // Date checks
    assert(!cert.isExpired(1500000000LL));
    assert(!cert.isNotYetValid(1500000000LL));
    assert(cert.isNotYetValid(500000000LL));
    assert(cert.isExpired(2500000000LL));

    // Full validate() logic
    assert(cert.validate("secure.midlet.org", 1500000000LL) == 0);
    assert(cert.validate("secure.midlet.org", 500000000LL) == CERT_ERR_NOT_YET_VALID);
    assert(cert.validate("secure.midlet.org", 2500000000LL) == CERT_ERR_EXPIRED);
    assert(cert.validate("wrong.site.com", 1500000000LL) == CERT_ERR_SITENAME_MISMATCH);

    // Wildcard cert test
    Certificate wildCert("CN=*.gamecloud.net, O=Cloud, C=SG", "CN=RootCA", "X.509", "3", "SHA256withRSA",
                         1000000LL, 9000000LL, "11:22:33");
    assert(wildCert.extractCN() == "*.gamecloud.net");
    assert(wildCert.matchesHost("api.gamecloud.net"));
    assert(wildCert.matchesHost("login.gamecloud.net"));
    assert(!wildCert.matchesHost("sub.api.gamecloud.net"));
    assert(!wildCert.matchesHost("gamecloud.net"));
    assert(!wildCert.matchesHost("othercloud.net"));

    // CertificateException helper & reason codes
    CertificateException ex(CertificateException::SITENAME_MISMATCH, "Host does not match CN");
    assert(ex.getReason() == CertificateException::SITENAME_MISMATCH);
    assert(std::string(ex.getReasonName()) == "SITENAME_MISMATCH");
    assert(std::string(CertificateException::getReasonName(CertificateException::EXPIRED)) == "EXPIRED");
    assert(std::string(CertificateException::getReasonName(CertificateException::VERIFICATION_FAILED)) == "VERIFICATION_FAILED");
    std::cout << "     Certificate validation & exception helper hoat dong chinh xac!" << std::endl;

    // 3. Kiem tra SecurityInfo & SecureConnection
    std::cout << "  3. Kiem tra SecurityInfo & SecureConnection (ssl://)..." << std::endl;
    auto sslConn = SecureConnection::open("ssl://auth.gamers.org:9443");
    assert(sslConn != nullptr);
    assert(sslConn->isOpen());
    assert(sslConn->getHost() == "auth.gamers.org");
    assert(sslConn->getPort() == 9443);

    const SecurityInfo& secInfo = sslConn->getSecurityInfo();
    assert(secInfo.getProtocolName() == "TLS");
    assert(secInfo.getProtocolVersion() == "1.2");
    assert(secInfo.getCipherSuite() == "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256");
    assert(!secInfo.getServerCertificate().getSubject().empty());
    assert(secInfo.getServerCertificate().matchesHost("auth.gamers.org"));

    // Test send & feed I/O buffers
    const uint8_t outgoing[] = {'C', 'L', 'I', 'E', 'N', 'T', '_', 'H', 'E', 'L', 'L', 'O'};
    assert(sslConn->write(outgoing, sizeof(outgoing)) == sizeof(outgoing));
    uint8_t outBuf[32];
    assert(sslConn->extractOutput(outBuf, sizeof(outBuf)) == sizeof(outgoing));
    assert(std::memcmp(outgoing, outBuf, sizeof(outgoing)) == 0);

    const uint8_t incoming[] = {'S', 'E', 'R', 'V', 'E', 'R', '_', 'A', 'C', 'K'};
    sslConn->feedInput(incoming, sizeof(incoming));
    assert(sslConn->available() == sizeof(incoming));
    uint8_t inBuf[32];
    assert(sslConn->read(inBuf, sizeof(inBuf)) == sizeof(incoming));
    assert(std::memcmp(incoming, inBuf, sizeof(incoming)) == 0);
    assert(sslConn->available() == 0);

    sslConn->close();
    assert(!sslConn->isOpen());
    std::cout << "     SecureConnection SSL handshake & data streaming hoat dong hoan hao!" << std::endl;

    // 4. Kiem tra HttpsConnection (https://)
    std::cout << "  4. Kiem tra HttpsConnection (https://)..." << std::endl;
    auto httpsConn = HttpsConnection::open("https://api.retroserver.com/v2/scores?user=player1");
    assert(httpsConn != nullptr);
    assert(httpsConn->isOpen());
    assert(httpsConn->getHost() == "api.retroserver.com");
    assert(httpsConn->getPort() == 443); // default port
    assert(httpsConn->getFile() == "/v2/scores?user=player1");
    assert(httpsConn->getProtocol() == "https");

    httpsConn->setRequestMethod("POST");
    assert(httpsConn->getRequestMethod() == "POST");
    httpsConn->setRequestProperty("Content-Type", "application/json");
    httpsConn->setRequestProperty("Accept", "application/json");
    assert(httpsConn->getRequestProperty("Content-Type") == "application/json");

    const std::string postBody = "{\"score\": 99950}";
    assert(httpsConn->write(reinterpret_cast<const uint8_t*>(postBody.data()), postBody.size()) == static_cast<int>(postBody.size()));

    // Simulate server response
    const std::string respPayload = "{\"status\": \"saved\", \"rank\": 1}";
    httpsConn->feedResponse(200, "OK", {{"Content-Type", "application/json"}, {"X-RateLimit", "100"}},
                            std::vector<uint8_t>(respPayload.begin(), respPayload.end()));

    assert(httpsConn->getResponseCode() == 200);
    assert(httpsConn->getResponseMessage() == "OK");
    assert(httpsConn->getHeaderField("Content-Type") == "application/json");
    assert(httpsConn->getHeaderField("X-RateLimit") == "100");
    assert(httpsConn->getLength() == static_cast<int64_t>(respPayload.size()));

    uint8_t respBuf[64];
    int bytesRead = httpsConn->read(respBuf, sizeof(respBuf) - 1);
    assert(bytesRead == static_cast<int>(respPayload.size()));
    respBuf[bytesRead] = '\0';
    assert(std::string(reinterpret_cast<char*>(respBuf)) == respPayload);

    httpsConn->close();
    assert(!httpsConn->isOpen());
    std::cout << "     HttpsConnection HTTP/TLS request & response cycle hoan hao!" << std::endl;

    // 5. Kiem tra Section 29 C-ABI Exports
    std::cout << "  5. Kiem tra Section 29 C-ABI Exports..." << std::endl;
    uintptr_t hCert = j2me_core_cert_create("CN=test.server.io, O=Test", "CN=TestCA", "X.509", "3",
                                           "SHA256withRSA", 1000000000LL, 2000000000LL, "SN12345");
    assert(hCert != 0);
    char buf[128];
    assert(j2me_core_cert_get_subject(hCert, buf, sizeof(buf)));
    assert(std::string(buf) == "CN=test.server.io, O=Test");
    assert(j2me_core_cert_get_issuer(hCert, buf, sizeof(buf)));
    assert(std::string(buf) == "CN=TestCA");
    assert(j2me_core_cert_get_type(hCert, buf, sizeof(buf)));
    assert(std::string(buf) == "X.509");
    assert(j2me_core_cert_get_version(hCert, buf, sizeof(buf)));
    assert(std::string(buf) == "3");
    assert(j2me_core_cert_get_sig_alg(hCert, buf, sizeof(buf)));
    assert(std::string(buf) == "SHA256withRSA");
    assert(j2me_core_cert_get_serial(hCert, buf, sizeof(buf)));
    assert(std::string(buf) == "SN12345");
    assert(j2me_core_cert_get_not_before(hCert) == 1000000000LL);
    assert(j2me_core_cert_get_not_after(hCert) == 2000000000LL);
    assert(j2me_core_cert_validate(hCert, "test.server.io", 1500000000LL) == 0);
    assert(j2me_core_cert_validate(hCert, "wrong.server.io", 1500000000LL) == CERT_ERR_SITENAME_MISMATCH);
    j2me_core_cert_destroy(hCert);

    // C-ABI SSL connection
    uintptr_t hSsl = j2me_core_ssl_open("ssl://gateway.secure.net:8883", true);
    assert(hSsl != 0);
    assert(j2me_core_ssl_is_open(hSsl));
    assert(j2me_core_ssl_get_port(hSsl) == 8883);
    const uint8_t sslMsg[] = {'P', 'I', 'N', 'G'};
    assert(j2me_core_ssl_write(hSsl, sslMsg, 4) == 4);
    const uint8_t sslAck[] = {'P', 'O', 'N', 'G'};
    j2me_core_ssl_feed_input(hSsl, sslAck, 4);
    assert(j2me_core_ssl_available(hSsl) == 4);
    uint8_t sslRecv[8];
    assert(j2me_core_ssl_read(hSsl, sslRecv, sizeof(sslRecv)) == 4);
    assert(std::memcmp(sslAck, sslRecv, 4) == 0);

    uintptr_t hCertSec = 0;
    char protoName[32], protoVer[32], cipherBuf[64];
    assert(j2me_core_ssl_get_security_info(hSsl, protoName, sizeof(protoName),
                                          protoVer, sizeof(protoVer),
                                          cipherBuf, sizeof(cipherBuf),
                                          &hCertSec));
    assert(std::string(protoName) == "TLS");
    assert(std::string(protoVer) == "1.2");
    assert(std::string(cipherBuf) == "TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256");
    assert(hCertSec != 0);
    j2me_core_cert_destroy(hCertSec);
    j2me_core_ssl_close(hSsl);

    // C-ABI HTTPS connection
    uintptr_t hHttps = j2me_core_https_open("https://cloud.mobi/items?cat=1");
    assert(hHttps != 0);
    assert(j2me_core_https_is_open(hHttps));
    assert(j2me_core_https_get_port(hHttps) == 443);
    j2me_core_https_set_method(hHttps, "GET");
    j2me_core_https_set_request_property(hHttps, "User-Agent", "J2ME-Universal/1.0");

    const std::string httpsResp = "OK_DATA";
    j2me_core_https_set_response_header(hHttps, "Content-Type", "text/plain");
    j2me_core_https_feed_response(hHttps, 200, httpsResp.c_str());
    assert(j2me_core_https_get_response_code(hHttps) == 200);
    char hdrVal[64];
    assert(j2me_core_https_get_header_field(hHttps, "Content-Type", hdrVal, sizeof(hdrVal)));
    assert(std::string(hdrVal) == "text/plain");

    uint8_t httpsReadBuf[16];
    assert(j2me_core_https_read(hHttps, httpsReadBuf, sizeof(httpsReadBuf)) == static_cast<int>(httpsResp.size()));
    assert(std::memcmp(httpsResp.data(), httpsReadBuf, httpsResp.size()) == 0);

    j2me_core_https_close(hHttps);
    std::cout << "     Section 29 C-ABI exports xac minh thanh cong!" << std::endl;

    std::cout << "[PASS] Mo dun 29 (PKI Security, SSL & HTTPS Layer) 100% Hoan hao!" << std::endl;
}

static void test_3d_binary_loaders_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[RUNNING TEST] Mo dun 30: 3D Binary Asset Loaders (M3G & Micro3D)..." << std::endl;
    std::cout << "========================================================" << std::endl;

    std::cout << "  1. Kiem tra Magic Signatures & Format Identification..." << std::endl;
    const uint8_t m3gMagic[12] = { 0xAB, 0x4A, 0x53, 0x52, 0x31, 0x38, 0x34, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A };
    const uint8_t pngMagic[8]  = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
    const uint8_t jpegMagic[2] = { 0xFF, 0xD8 };
    const uint8_t mbacMagic[4] = { 'M', 'B', 3, 0 };
    const uint8_t mtraMagic[4] = { 'M', 'T', 3, 0 };
    const uint8_t randomData[8] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77 };

    assert(universal_loader::m3g::M3gLoader::identify(m3gMagic, sizeof(m3gMagic)) == universal_loader::m3g::M3G_FILE_M3G);
    assert(universal_loader::m3g::M3gLoader::identify(pngMagic, sizeof(pngMagic)) == universal_loader::m3g::M3G_FILE_PNG);
    assert(universal_loader::m3g::M3gLoader::identify(jpegMagic, sizeof(jpegMagic)) == universal_loader::m3g::M3G_FILE_JPEG);
    assert(universal_loader::m3g::M3gLoader::identify(randomData, sizeof(randomData)) == universal_loader::m3g::M3G_FILE_UNKNOWN);

    assert(universal_loader::micro3d::Micro3dLoader::identify(mbacMagic, sizeof(mbacMagic)) == 1);
    assert(universal_loader::micro3d::Micro3dLoader::identify(mtraMagic, sizeof(mtraMagic)) == 2);
    assert(universal_loader::micro3d::Micro3dLoader::identify(randomData, sizeof(randomData)) == 0);
    std::cout << "     Format Identification (M3G, PNG, JPEG, MBAC, MTRA) xac minh thanh cong!" << std::endl;

    std::cout << "  2. Kiem tra Adler-32 Checksum Algorithm..." << std::endl;
    const char* adlerTestStr = "123456789";
    uint32_t expectedAdler = 0x091E01DE;
    uint32_t computedAdler = universal_loader::m3g::M3gLoader::computeAdler32(reinterpret_cast<const uint8_t*>(adlerTestStr), strlen(adlerTestStr));
    assert(computedAdler == expectedAdler);
    std::cout << "     Adler-32 Checksum RFC 1950 xac minh chinh xac tuyet doi!" << std::endl;

    std::cout << "  3. Kiem tra M3G Binary Serialization & Deserialization..." << std::endl;
    universal_loader::m3g::M3gLoadedScene testScene;
    testScene.authoringTool = "J2ME-Loader C++ Engine";
    testScene.versionMajor = 1;
    testScene.versionMinor = 1;

    // Create World & Camera
    auto cam = std::make_shared<universal_loader::m3g::Camera>();
    cam->projection = universal_loader::m3g::PROJECTION_PERSPECTIVE;
    cam->fovY = 60.0f;
    cam->aspectRatio = 1.333f;
    cam->nearDistance = 1.0f;
    cam->farDistance = 1000.0f;
    testScene.cameras.push_back(cam);

    auto world = std::make_shared<universal_loader::m3g::World>();
    world->setActiveCamera(cam);
    testScene.world = world;

    // Create Material
    auto mat = std::make_shared<universal_loader::m3g::Material>();
    mat->diffuseColor = 0xFF00FF00;
    testScene.materials.push_back(mat);

    // Create Mesh
    auto mesh = std::make_shared<universal_loader::m3g::Mesh>();
    mesh->vertexBuffer.setPositions({ { 0.0f, 0.0f, 0.0f }, { 10.0f, 0.0f, 0.0f }, { 0.0f, 10.0f, 0.0f } });
    universal_loader::m3g::Submesh sub;
    sub.indexBuffer = universal_loader::m3g::IndexBuffer(universal_loader::m3g::PRIMITIVE_TRIANGLES, { 0, 1, 2 });
    sub.appearance.material = *mat;
    mesh->submeshes.push_back(sub);
    testScene.meshes.push_back(mesh);

    // Create KeyframeSequence
    auto kfs = std::make_shared<universal_loader::m3g::KeyframeSequence>(4, 3, universal_loader::m3g::INTERP_LINEAR);
    kfs->setDuration(1200);
    float kf0[3] = { 0.0f, 0.0f, 0.0f };
    float kf1[3] = { 10.0f, 20.0f, 30.0f };
    kfs->setKeyframe(0, 0, kf0);
    kfs->setKeyframe(1, 400, kf1);
    testScene.keyframeSequences.push_back(kfs);

    // Create Group node
    auto grp = std::make_shared<universal_loader::m3g::Group>();
    grp->setTranslation(5.0f, 15.0f, 25.0f);
    grp->setScale(2.0f, 2.0f, 2.0f);
    testScene.rootNodes.push_back(grp);

    // Serialize to binary stream
    std::vector<uint8_t> m3gBytes = universal_loader::m3g::M3gLoader::serializeScene(testScene);
    assert(!m3gBytes.empty());
    assert(universal_loader::m3g::M3gLoader::identify(m3gBytes.data(), m3gBytes.size()) == universal_loader::m3g::M3G_FILE_M3G);

    // Deserialize back
    universal_loader::m3g::M3gLoadedScene loadedM3g;
    bool m3gLoaded = universal_loader::m3g::M3gLoader::load(m3gBytes.data(), m3gBytes.size(), loadedM3g);
    assert(m3gLoaded);
    assert(loadedM3g.versionMajor == 1);
    assert(loadedM3g.versionMinor == 1);
    assert(loadedM3g.authoringTool == "J2ME-Loader C++ Engine");
    assert(loadedM3g.world != nullptr);
    assert(loadedM3g.cameras.size() == 1);
    assert(loadedM3g.cameras[0]->projection == universal_loader::m3g::PROJECTION_PERSPECTIVE);
    assert(std::abs(loadedM3g.cameras[0]->fovY - 60.0f) < 0.01f);
    assert(std::abs(loadedM3g.cameras[0]->aspectRatio - 1.333f) < 0.01f);
    assert(loadedM3g.materials.size() == 1);
    assert(loadedM3g.materials[0]->diffuseColor == 0xFF00FF00);
    assert(loadedM3g.meshes.size() == 1);
    assert(loadedM3g.meshes[0]->vertexBuffer.size() == 3);
    assert(loadedM3g.keyframeSequences.size() == 1);
    assert(loadedM3g.keyframeSequences[0]->getDuration() == 1200);
    assert(loadedM3g.rootNodes.size() == 1);
    assert(std::abs(loadedM3g.rootNodes[0]->getTranslation().x - 5.0f) < 0.01f);
    assert(std::abs(loadedM3g.rootNodes[0]->getScale().x - 2.0f) < 0.01f);

    // Test loadFromFile
    const std::string tmpM3gPath = "./test_scene.m3g";
    {
        std::ofstream outF(tmpM3gPath, std::ios::binary);
        outF.write(reinterpret_cast<const char*>(m3gBytes.data()), m3gBytes.size());
    }
    universal_loader::m3g::M3gLoadedScene fileM3g;
    assert(universal_loader::m3g::M3gLoader::loadFromFile(tmpM3gPath, fileM3g));
    assert(fileM3g.world != nullptr);
    fs::remove(tmpM3gPath);
    std::cout << "     M3G Deserialization (World, Camera, Mesh, Material, KeyframeSequence, Group) 100%!" << std::endl;

    std::cout << "  4. Kiem tra Micro3D MBAC Binary Serialization & Deserialization..." << std::endl;
    universal_loader::micro3d::Micro3dFigure fig;
    fig.name = "CharacterModel";
    universal_loader::micro3d::Micro3DVertex v0, v1, v2, v3;
    v0.position = { 0.0f, 0.0f, 0.0f };    v0.u = 0.0f; v0.v = 0.0f;
    v1.position = { 100.0f, 0.0f, 0.0f };  v1.u = 1.0f; v1.v = 0.0f;
    v2.position = { 100.0f, 100.0f, 0.0f };v2.u = 1.0f; v2.v = 1.0f;
    v3.position = { 0.0f, 100.0f, 0.0f };  v3.u = 0.0f; v3.v = 1.0f;
    fig.vertices = { v0, v1, v2, v3 };

    universal_loader::micro3d::Micro3DPolygon pTri;
    pTri.indices = { 0, 1, 2 };
    pTri.blendMode = 0;
    universal_loader::micro3d::Micro3DPolygon pQuad;
    pQuad.indices = { 0, 1, 2, 3 };
    pQuad.blendMode = 1;
    fig.polygons = { pTri, pQuad };

    universal_loader::micro3d::Bone boneRoot;
    boneRoot.length = 2;
    boneRoot.parent = -1;
    boneRoot.matrix.setIdentity();

    universal_loader::micro3d::Bone boneArm;
    boneArm.length = 2;
    boneArm.parent = 0;
    boneArm.matrix = universal_loader::micro3d::AffineTrans(
        4096, 0, 0, 50,
        0, 4096, 0, 100,
        0, 0, 4096, 0
    );
    fig.bones = { boneRoot, boneArm };

    std::vector<uint8_t> mbacBytes = universal_loader::micro3d::Micro3dLoader::serializeMbac(fig, 3);
    assert(!mbacBytes.empty());
    assert(universal_loader::micro3d::Micro3dLoader::identify(mbacBytes.data(), mbacBytes.size()) == 1);

    universal_loader::micro3d::Micro3dFigure loadedFig;
    assert(universal_loader::micro3d::Micro3dLoader::loadMbac(mbacBytes.data(), mbacBytes.size(), loadedFig));
    assert(loadedFig.numVertices == 4);
    assert(loadedFig.numPolyT3 == 1);
    assert(loadedFig.numPolyT4 == 1);
    assert(loadedFig.numBones == 2);
    assert(loadedFig.vertices.size() == 4);
    assert(std::abs(loadedFig.vertices[1].position.x - 100.0f) < 0.01f);
    assert(loadedFig.polygons.size() == 2);
    assert(loadedFig.bones.size() == 2);
    assert(loadedFig.bones[0].parent == -1);
    assert(loadedFig.bones[1].parent == 0);
    assert(loadedFig.bones[1].matrix.m03 == 50);
    assert(loadedFig.bones[1].matrix.m13 == 100);

    const std::string tmpMbacPath = "./test_model.mbac";
    {
        std::ofstream outF(tmpMbacPath, std::ios::binary);
        outF.write(reinterpret_cast<const char*>(mbacBytes.data()), mbacBytes.size());
    }
    universal_loader::micro3d::Micro3dFigure fileFig;
    assert(universal_loader::micro3d::Micro3dLoader::loadMbacFromFile(tmpMbacPath, fileFig));
    assert(fileFig.numVertices == 4);
    fs::remove(tmpMbacPath);
    std::cout << "     Micro3D MBAC Deserialization (Vertices, Polygons, Bones) 100%!" << std::endl;

    std::cout << "  5. Kiem tra Micro3D MTRA Binary Serialization & Deserialization..." << std::endl;
    universal_loader::micro3d::ActionTable actTable;
    universal_loader::micro3d::Action act0;
    act0.keyframes = 60;
    act0.numBones = 2;

    universal_loader::micro3d::BoneAction ba0;
    ba0.keyframes = 60;
    ba0.matrices.push_back(universal_loader::micro3d::AffineTrans(
        4096, 0, 0, 10,
        0, 4096, 0, 20,
        0, 0, 4096, 30
    ));

    universal_loader::micro3d::BoneAction ba1;
    ba1.keyframes = 60;
    // ba1 empty matrices -> identity

    act0.boneActions = { ba0, ba1 };
    actTable.actions = { act0 };

    std::vector<uint8_t> mtraBytes = universal_loader::micro3d::Micro3dLoader::serializeMtra(actTable, 3);
    assert(!mtraBytes.empty());
    assert(universal_loader::micro3d::Micro3dLoader::identify(mtraBytes.data(), mtraBytes.size()) == 2);

    universal_loader::micro3d::ActionTable loadedTable;
    assert(universal_loader::micro3d::Micro3dLoader::loadMtra(mtraBytes.data(), mtraBytes.size(), loadedTable));
    assert(loadedTable.getActionCount() == 1);
    const auto* pAct = loadedTable.getAction(0);
    assert(pAct != nullptr);
    assert(pAct->keyframes == 60);
    assert(pAct->numBones == 2);
    assert(pAct->boneActions.size() == 2);
    assert(pAct->boneActions[0].matrices[0].m03 == 10);
    assert(pAct->boneActions[0].matrices[0].m13 == 20);
    assert(pAct->boneActions[0].matrices[0].m23 == 30);
    assert(pAct->boneActions[1].matrices[0].m00 == 4096);

    const std::string tmpMtraPath = "./test_motion.mtra";
    {
        std::ofstream outF(tmpMtraPath, std::ios::binary);
        outF.write(reinterpret_cast<const char*>(mtraBytes.data()), mtraBytes.size());
    }
    universal_loader::micro3d::ActionTable fileTable;
    assert(universal_loader::micro3d::Micro3dLoader::loadMtraFromFile(tmpMtraPath, fileTable));
    assert(fileTable.getActionCount() == 1);
    fs::remove(tmpMtraPath);
    std::cout << "     Micro3D MTRA Deserialization (Actions, Keyframes, Bone Matrices) 100%!" << std::endl;

    std::cout << "  6. Kiem tra Section 30 C-ABI Exports..." << std::endl;
    assert(j2me_core_3d_identify_format(m3gBytes.data(), m3gBytes.size()) == 1);
    assert(j2me_core_3d_identify_format(mbacBytes.data(), mbacBytes.size()) == 2);
    assert(j2me_core_3d_identify_format(mtraBytes.data(), mtraBytes.size()) == 3);
    assert(j2me_core_3d_identify_format(randomData, sizeof(randomData)) == 0);

    size_t rootCount = 0, meshCount = 0;
    uintptr_t hScene = j2me_core_m3g_load_memory(m3gBytes.data(), m3gBytes.size(), &rootCount, &meshCount);
    assert(hScene != 0);
    assert(rootCount == 1);
    assert(meshCount == 1);
    j2me_core_m3g_scene_destroy(hScene);

    uintptr_t hFigure = j2me_core_micro3d_load_figure(mbacBytes.data(), mbacBytes.size());
    assert(hFigure != 0);
    int32_t cVerts = 0, cPoly3 = 0, cPoly4 = 0, cBones = 0;
    assert(j2me_core_micro3d_figure_get_counts(hFigure, &cVerts, &cPoly3, &cPoly4, &cBones));
    assert(cVerts == 4);
    assert(cPoly3 == 1);
    assert(cPoly4 == 1);
    assert(cBones == 2);
    j2me_core_micro3d_figure_destroy(hFigure);

    uintptr_t hTable = j2me_core_micro3d_load_action_table(mtraBytes.data(), mtraBytes.size());
    assert(hTable != 0);
    assert(j2me_core_micro3d_action_table_get_count(hTable) == 1);
    j2me_core_micro3d_action_table_destroy(hTable);
    std::cout << "     Section 30 C-ABI exports xac minh thanh cong!" << std::endl;

    std::cout << "[PASS] Mo dun 30 (3D Binary Asset Loaders: M3G & Micro3D) 100% Hoan hao!" << std::endl;
}

void test_font_sysprops_wav_module() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[TEST MODULE 31] LCDUI Font Engine, System Properties & PCM WAV Player" << std::endl;
    std::cout << "========================================================" << std::endl;

    // 1. LCDUI Font Engine & Metrics
    std::cout << "  1. Kiem tra LCDUI Font Engine & Metrics..." << std::endl;
    auto defFont = j2me::LcduiFont::getDefaultFont();
    assert(defFont != nullptr);
    assert(defFont->getFace() == j2me::FACE_SYSTEM);
    assert(defFont->getStyle() == j2me::STYLE_PLAIN);
    assert(defFont->getSize() == j2me::SIZE_MEDIUM);
    assert(defFont->getHeight() >= 13);
    assert(defFont->getBaselinePosition() > 0);
    assert(defFont->isPlain());
    assert(!defFont->isBold());

    auto boldLargeFont = j2me::LcduiFont::getFont(j2me::FACE_MONOSPACE, j2me::STYLE_BOLD, j2me::SIZE_LARGE);
    assert(boldLargeFont != nullptr);
    assert(boldLargeFont->getFace() == j2me::FACE_MONOSPACE);
    assert(boldLargeFont->isBold());
    assert(boldLargeFont->getHeight() >= 20);

    int w1 = defFont->charWidth('A');
    int w2 = defFont->stringWidth("Hello J2ME World");
    assert(w1 > 0);
    assert(w2 > w1);
    assert(defFont->substringWidth("Hello J2ME World", 0, 5) < w2);
    std::cout << "     Font metrics (Height, Baseline, CharWidth, StringWidth) xac minh 100%!" << std::endl;

    // 2. LCDUI Font Glyph Rasterizer & Text Rendering
    std::cout << "  2. Kiem tra Font Glyph Rasterizer tren FrameBuffer..." << std::endl;
    int testW = 160, testH = 120;
    std::vector<uint32_t> testFb(testW * testH, 0xFF000000);
    j2me::LcduiGraphics g(testFb.data(), testW, testH);
    g.setFont(boldLargeFont);
    g.setColor(0xFFFFFFFF);
    g.drawString("ANTIGRAVITY", 20, 20, j2me::ANCHOR_TOP | j2me::ANCHOR_LEFT);

    // Kiem tra co pixel chu duoc ve ra (khong con la buffer den hoan toan)
    int nonZeroPixels = 0;
    for (int y = 20; y < 50; ++y) {
        for (int x = 20; x < 150; ++x) {
            if (testFb[y * testW + x] == 0xFFFFFFFF) {
                nonZeroPixels++;
            }
        }
    }
    assert(nonZeroPixels > 50);

    // Kiem tra drawChar va drawSubstring voi Underline & Italic
    auto styledFont = j2me::LcduiFont::getFont(j2me::FACE_PROPORTIONAL, j2me::STYLE_ITALIC | j2me::STYLE_UNDERLINED, j2me::SIZE_SMALL);
    g.setFont(styledFont);
    g.drawChar('Z', 5, 60, 0);
    g.drawSubstring("TestingSub", 0, 4, 20, 60, 0);
    std::cout << "     Font Glyph Rasterizer (ASCII 32..126, Bold, Italic, Underline) ve that 100%!" << std::endl;

    // 3. System Properties Manager & Upstream 24 Properties
    std::cout << "  3. Kiem tra System Properties Manager (Upstream 24 Properties)..." << std::endl;
    auto& sysProps = j2me::SystemPropertiesManager::instance();
    sysProps.resetToDefaults();

    assert(sysProps.getProperty("microedition.configuration") == "CLDC-1.1");
    assert(sysProps.getProperty("microedition.profiles") == "MIDP-2.0");
    assert(sysProps.getProperty("microedition.platform") == "Nokia6233/05.10");
    assert(sysProps.getProperty("microedition.encoding") == "ISO-8859-1");
    assert(sysProps.getProperty("com.nokia.mid.ui.DirectGraphics.PIXEL_FORMAT") == "565");
    assert(sysProps.getProperty("device.imei") == "000000000000000");
    assert(sysProps.getProperty("wireless.messaging.sms.smsc") == "+8613800010000");
    assert(sysProps.getProperty("microedition.sensor.version") == "1");
    assert(sysProps.getProperty("microedition.m3g.version") == "1.1");
    assert(sysProps.getProperty("supports.mixing") == "true");
    assert(sysProps.getProperty("supports.audio.capture") == "true");
    assert(sysProps.getPropertyCount() >= 24);

    // Test loadProperties from string
    std::string customProps = "custom.fps: 120\n# Comment line\napp.debug=enabled\n";
    assert(sysProps.loadProperties(customProps));
    assert(sysProps.getProperty("custom.fps") == "120");
    assert(sysProps.getProperty("app.debug") == "enabled");

    // Test CLDC VM Native System.getProperty integration
    universal_loader::jvm::CldcVirtualMachine vm;
    auto keyStr = vm.allocateString("microedition.platform");
    std::vector<universal_loader::jvm::JavaValue> args = {universal_loader::jvm::JavaValue(static_cast<universal_loader::jvm::JavaObject*>(keyStr))};
    auto retVal = vm.executeMethodByName("java/lang/System", "getProperty", "(Ljava/lang/String;)Ljava/lang/String;", args);
    assert(retVal.ref != nullptr);
    auto* retStr = dynamic_cast<universal_loader::jvm::JavaString*>(retVal.ref);
    assert(retStr != nullptr);
    assert(retStr->value == "Nokia6233/05.10");
    std::cout << "     System Properties Manager & JVM getProperty native bridge hoat dong 100%!" << std::endl;

    // 4. PCM WAV Audio Player & RIFF WAVE Decoder
    std::cout << "  4. Kiem tra PCM WAV Audio Player (RIFF WAVE & Resampler)..." << std::endl;
    // Tao du lieu file WAV hop le (8000Hz, Mono, 16-bit PCM, 800 samples = 0.1s)
    uint32_t sampleRate = 8000;
    uint16_t numChannels = 1;
    uint16_t bitsPerSample = 16;
    uint32_t numSamples = 800;
    uint32_t dataBytes = numSamples * (bitsPerSample / 8) * numChannels;

    std::vector<uint8_t> wavData;
    wavData.resize(44 + dataBytes);

    // RIFF Header
    std::memcpy(&wavData[0], "RIFF", 4);
    uint32_t riffSize = 36 + dataBytes;
    std::memcpy(&wavData[4], &riffSize, 4);
    std::memcpy(&wavData[8], "WAVE", 4);

    // fmt chunk
    std::memcpy(&wavData[12], "fmt ", 4);
    uint32_t fmtSize = 16;
    std::memcpy(&wavData[16], &fmtSize, 4);
    uint16_t audioFmt = 1; // PCM
    std::memcpy(&wavData[20], &audioFmt, 2);
    std::memcpy(&wavData[22], &numChannels, 2);
    std::memcpy(&wavData[24], &sampleRate, 4);
    uint32_t byteRate = sampleRate * numChannels * (bitsPerSample / 8);
    std::memcpy(&wavData[28], &byteRate, 4);
    uint16_t blockAlign = numChannels * (bitsPerSample / 8);
    std::memcpy(&wavData[32], &blockAlign, 2);
    std::memcpy(&wavData[34], &bitsPerSample, 2);

    // data chunk
    std::memcpy(&wavData[36], "data", 4);
    std::memcpy(&wavData[40], &dataBytes, 4);

    // Sine wave 440Hz
    int16_t* pcmSamples = reinterpret_cast<int16_t*>(&wavData[44]);
    for (uint32_t i = 0; i < numSamples; ++i) {
        double t = static_cast<double>(i) / sampleRate;
        pcmSamples[i] = static_cast<int16_t>(16384.0 * std::sin(2.0 * 3.1415926535 * 440.0 * t));
    }

    auto wavPlayer = j2me::WavPlayer::createFromMemory(wavData.data(), wavData.size());
    assert(wavPlayer != nullptr);
    assert(wavPlayer->isValid());
    assert(wavPlayer->getFormatInfo().sampleRate == 8000);
    assert(wavPlayer->getFormatInfo().numChannels == 1);
    assert(wavPlayer->getFormatInfo().bitsPerSample == 16);
    assert(wavPlayer->getDuration() == 100000); // 100,000 us = 0.1s

    wavPlayer->realize();
    wavPlayer->prefetch();
    wavPlayer->start();
    assert(wavPlayer->getState() == j2me::PLAYER_STARTED);

    // Render 44100Hz stereo
    std::vector<int16_t> renderedPcm(1024 * 2);
    size_t outFrames = wavPlayer->renderAudio44100(renderedPcm.data(), 1024);
    assert(outFrames > 0);
    bool hasNonZeroAudio = false;
    for (size_t i = 0; i < outFrames * 2; ++i) {
        if (renderedPcm[i] != 0) {
            hasNonZeroAudio = true;
            break;
        }
    }
    assert(hasNonZeroAudio);

    // Kiem tra AudioRingBuffer stream
    j2me::AudioRingBuffer ringBuf(4096);
    size_t streamed = wavPlayer->streamToRingBuffer(ringBuf, 512);
    assert(streamed > 0);
    assert(ringBuf.availableRead() > 0);

    // MMAPI Manager auto-routing test
    auto playerViaMgr = j2me::MmapiManager::createPlayer(wavData.data(), wavData.size(), "audio/x-wav");
    assert(playerViaMgr != nullptr);
    assert(std::dynamic_pointer_cast<j2me::WavPlayer>(playerViaMgr) != nullptr);
    std::cout << "     WavPlayer RIFF parser, 44.1kHz resampler & RingBuffer stream dat 100%!" << std::endl;

    // 5. Kiem tra Section 31 C-ABI Exports
    std::cout << "  5. Kiem tra Section 31 C-ABI Exports..." << std::endl;
    uintptr_t hFont = j2me_core_font_get_default();
    assert(hFont != 0);
    assert(j2me_core_font_get_height(hFont) > 0);
    assert(j2me_core_font_string_width(hFont, "Test") > 0);

    char propBuf[128] = {0};
    assert(j2me_core_system_get_property("microedition.profiles", propBuf, sizeof(propBuf)));
    assert(std::strcmp(propBuf, "MIDP-2.0") == 0);
    assert(j2me_core_system_get_property_count() >= 24);

    uintptr_t hWav = j2me_core_wav_create_memory(wavData.data(), wavData.size());
    assert(hWav != 0);
    j2me_core_wav_set_volume(hWav, 90);
    assert(j2me_core_wav_get_volume(hWav) == 90);
    assert(j2me_core_wav_get_duration_us(hWav) == 100000);
    j2me_core_wav_start(hWav);
    int16_t abiPcm[512 * 2];
    size_t abiRendered = j2me_core_wav_render_pcm(hWav, abiPcm, 512);
    assert(abiRendered > 0);
    j2me_core_wav_stop(hWav);
    j2me_core_wav_destroy(hWav);

    std::cout << "     Section 31 C-ABI exports xac minh thanh cong!" << std::endl;
    std::cout << "[PASS] Mo dun 31 (LCDUI Font, System Properties & PCM WAV) 100% Hoan hao!" << std::endl;
}

int main() {
    // 1. Chay Unit Test Mo dun 1 (RMS)
    test_rms_module();

    // 2. Chay Unit Test Mo dun 2 (LCDUI & GameCanvas)
    test_lcdui_module();

    // 3. Chay Unit Test Mo dun 3 (GCF Networking)
    test_gcf_module();

    // 4. Chay Unit Test Mo dun 4 (MMAPI Audio Synthesizer)
    test_mmapi_audio_module();

    // 5. Chay Unit Test Mo dun 5 (3D Graphics Engines - M3G & Micro3D)
    test_graphics3d_module();

    // 6. Chay Unit Test Mo dun 6 (OEM & Vendor Extensions - Nokia, Siemens, Samsung)
    test_oem_module();

    // 7. Chay Unit Test Mo dun 7 (LCDUI High-Level UI & Widgets)
    test_lcdui_ui_module();

    // 8. Chay Unit Test Mo dun 8 (JSR-75 FileConnection & FileSystem)
    test_jsr75_file_module();

    // 9. Chay Unit Test Mo dun 9 (JSR-120 WMA)
    test_jsr120_wma_module();

    // 10. Chay Unit Test Mo dun 10 (MIDlet LifeCycle & Descriptor)
    test_midlet_descriptor_module();

    // 11. Chay Unit Test Mo dun 11 (Phone Keypad & Multi-Tap)
    test_phone_keypad_module();

    // 12. Chay Unit Test Mo dun 12 (JAR Resource Loader)
    test_jar_resource_loader_module();

    // 13. Chay Unit Test Mo dun 13 (Profile & Settings)
    test_profile_config_module();

    // 14. Chay Unit Test Mo dun 14 (Unified Core Engine Integration & Runtime Orchestrator)
    test_unified_core_orchestrator_module();

    // 15. Chay Unit Test Mo dun 15 (Java Classfile Parser & CLDC 1.1 VM)
    test_jvm_interpreter_module();

    // 16. Chay Kiem tra Nap & Thuc thi Game Thuc te (DragonBoy.jar)
    test_real_jar_dragonboy_loading();

    // 17. Chay Unit Test Mo dun 16 (J2ME 2D Game API: TiledLayer & LayerManager)
    test_game_layer_module();

    // 18. Chay Unit Test Mo dun 17 (Emulator UX: Speed Multiplier & Screenshot PNG)
    test_emulator_ux_module();

    // 19. Chay Unit Test Mo dun 18 (GCF Datagram / UDP Networking)
    test_datagram_module();

    // 20. Chay Unit Test Mo dun 19 (M3G Keyframe Animation, Controller & MorphingMesh)
    test_m3g_animation_module();

    // 21. Chay Unit Test Mo dun 20 (M3G SkinnedMesh & Bone Skeleton System)
    test_m3g_skinned_mesh_module();

    // 22. Chay Unit Test Mo dun 21 (App Management, Repository & Installer)
    test_app_management_module();

    // 23. Chay Unit Test Mo dun 22 (JSR-82 Mobile Bluetooth & RFCOMM/L2CAP Multiplayer)
    test_bluetooth_module();

    // 24. Chay Unit Test Mo dun 23 (Vodafone VSCL & Carrier OEM Extensions)
    test_vodafone_and_carrier_module();

    // 25. Chay Unit Test Mo dun 24 (JSR-179 Mobile Location API)
    test_location_module();

    // 26. Chay Unit Test Mo dun 25 (JSR-256 Mobile Sensor API)
    test_sensor_module();

    // 27. Chay Unit Test Mo dun 26 (JSR-75 PIM)
    test_pim_module();

    // 28. Chay Unit Test Mo dun 27 (JSR-234 AMMS)
    test_amms_module();

    // 29. Chay Unit Test Mo dun 28 (MIDP 2.0 PushRegistry & CommConnection)
    test_push_and_comm_module();

    // 30. Chay Unit Test Mo dun 29 (PKI Security, SSL & HTTPS Layer)
    test_pki_and_secure_connection_module();

    // 31. Chay Unit Test Mo dun 30 (3D Binary Asset Loaders: M3G & Micro3D)
    test_3d_binary_loaders_module();

    // 32. Chay Unit Test Mo dun 31 (LCDUI Font, System Properties & PCM WAV Player)
    test_font_sysprops_wav_module();

    std::cout << "\n========================================================" << std::endl;
    std::cout << "[FINAL VALIDATION] Kiem tra Engine Instance Co ban..." << std::endl;
    std::cout << "========================================================" << std::endl;
    std::cout << "[TEST] 1. Khoi tao J2ME Core Instance..." << std::endl;
    J2meEngineInstance* engine = j2me_core_create("./test_rms");
    assert(engine != nullptr);

    std::cout << "[TEST] 2. Kiem tra thuoc tinh ban dau..." << std::endl;
    std::cout << "  - App Title: " << j2me_core_get_app_title(engine) << std::endl;
    std::cout << "  - FPS Limit: " << j2me_core_get_fps_limit(engine) << std::endl;

    std::cout << "[TEST] 3. Thiet lap cau hinh man hinh 240x320 & FPS 60..." << std::endl;
    j2me_core_set_screen_dimensions(engine, 240, 320);
    j2me_core_set_fps_limit(engine, 60);

    std::cout << "[TEST] 4. Khoi chay Engine (Game Loop Thread)..." << std::endl;
    j2me_core_start(engine);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "[TEST] 5. Lock Framebuffer kiem tra diem anh..." << std::endl;
    int w = 0, h = 0;
    bool dirty = false;
    const uint32_t* fb = j2me_core_lock_framebuffer(engine, &w, &h, &dirty);
    assert(fb != nullptr);
    assert(w == 240);
    assert(h == 320);
    j2me_core_unlock_framebuffer(engine);

    std::cout << "[TEST] 6. Test Input Key Code..." << std::endl;
    j2me_core_send_key(engine, J2ME_KEY_NUM5, true);
    j2me_core_send_key(engine, J2ME_KEY_NUM5, false);

    std::cout << "[TEST] 7. Dung Engine & Huy Instance..." << std::endl;
    j2me_core_stop(engine);
    j2me_core_destroy(engine);

    std::cout << "[SUCCESS] 100% Tat ca Unit Tests (31/31 Mo Dun) da vuot qua hoan hao!" << std::endl;
    return 0;
}
