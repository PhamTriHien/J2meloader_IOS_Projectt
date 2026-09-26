#ifndef J2ME_CORE_EXPORTS
#define J2ME_CORE_EXPORTS
#endif
#include "../include/j2me_core.h"
#include "lcdui/frame_buffer.h"
#include "lcdui/lcdui_graphics.h"
#include "lcdui/font.h"
#include "jvm/jar_reader.h"
#include "storage/rms_storage.h"
#include "audio/mmapi_audio.h"
#include "audio/wav_player.h"
#include "system/system_properties.h"
#include "graphics3d/rasterizer3d.h"
#include "oem/device_control.h"
#include "lcdui/ui/display.h"
#include "file/file_system_registry.h"
#include "file/file_connection.h"
#include "sms_connection.h"
#include "app_descriptor.h"
#include "midlet.h"
#include "phone_keypad.h"
#include "jar_resource_loader.h"
#include "app_profile_config.h"
#include "jvm/cldc_vm.h"
#include "lcdui/game/layer.h"
#include "lcdui/game/tiled_layer.h"
#include "lcdui/game/layer_manager.h"
#include "lcdui/game/sprite_layer.h"
#include "network/datagram_connection.h"
#include "graphics3d/keyframe_sequence.h"
#include "graphics3d/animation_controller.h"
#include "graphics3d/animation_track.h"
#include "graphics3d/morphing_mesh.h"
#include "graphics3d/m3g_node.h"
#include "graphics3d/skinned_mesh.h"
#include "app/app_item.h"
#include "app/app_repository.h"
#include "app/app_installer.h"
#include "network/bluetooth/bluetooth_types.h"
#include "network/bluetooth/bluetooth_uuid.h"
#include "network/bluetooth/bluetooth_data_element.h"
#include "network/bluetooth/bluetooth_service_record.h"
#include "network/bluetooth/bluetooth_device.h"
#include "network/bluetooth/btspp_connection.h"
#include "network/bluetooth/btl2cap_connection.h"
#include "network/bluetooth/obex_headers.h"
#include "oem/vodafone/vodafone_types.h"
#include "oem/vodafone/sprite_engine.h"
#include "oem/vodafone/sound_player.h"
#include "oem/vodafone/vodafone_device_control.h"
#include "oem/vodafone/vodafone_image_encoder.h"
#include "oem/carrier_extensions.h"
#include "location/location_types.h"
#include "location/coordinates.h"
#include "location/address_info.h"
#include "location/criteria.h"
#include "location/location.h"
#include "location/orientation.h"
#include "location/landmark_store.h"
#include "location/location_provider.h"
#include "sensor/sensor_manager.h"
#include "pim/pim_types.h"
#include "pim/pim_manager.h"
#include "pim/contact.h"
#include "pim/event.h"
#include "pim/todo.h"
#include "pim/repeat_rule.h"
#include "amms/amms_types.h"
#include "amms/global_manager.h"
#include "amms/sound_source_3d.h"
#include "amms/audio_effects.h"
#include "amms/camera_controls.h"
#include "push/push_registry.h"
#include "comm/comm_connection.h"
#include "security/secure_connection.h"
#include "graphics3d/m3g_loader.h"
#include "graphics3d/micro3d_loader.h"
#include <filesystem>

#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <chrono>
#include <mutex>
#include <cstring>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cmath>

struct J2meLayerWrapper {
    std::shared_ptr<universal_loader::lcdui::game::Layer> layer;
};

struct J2meEngineInstance {
    std::string storageRoot;
    std::string appTitle{"J2ME Application"};
    std::string appVendor{"Unknown"};
    std::string appVersion{"1.0.0"};
    std::string mainClass;

    universal_loader::config::ProfileModel profile;
    universal_loader::config::ConfigDirs   configDirs;
    universal_loader::jvm::CldcVirtualMachine vm;

    std::shared_ptr<universal_loader::app::AppRepository> appRepo;
    std::unique_ptr<universal_loader::app::AppInstaller>  appInstaller;

    j2me::JarReader jarReader;
    j2me::FrameBuffer frameBuffer{240, 320};

    std::atomic<bool> isRunning{false};
    std::atomic<bool> isPaused{false};
    std::atomic<int>  fpsLimit{60};
    std::atomic<int>  speedMultiplier{1};

    std::thread gameThread;
    std::mutex stateMutex;

    // Key states (bitmask or array)
    bool keyStates[256]{false};
    bool specialKeyStates[64]{false};

    bool isKeyPressed(int code) const {
        if (code >= 0 && code < 256) return keyStates[code];
        if (code < 0 && code >= -63) return specialKeyStates[-code];
        return false;
    }

    // Canvas title or splash animation state
    uint64_t frameCounter{0};

    std::shared_ptr<j2me::LcduiFont> currentFont;
};

// Thread thực thi vòng lặp Game Loop của Core J2ME
static void engine_game_loop(J2meEngineInstance* inst) {
    auto lastTick = std::chrono::steady_clock::now();

    if (!inst->mainClass.empty()) {
        try {
            inst->vm.executeMethodByName(inst->mainClass, "startApp", "()V", {});
        } catch (...) {}
    }

    while (inst->isRunning.load()) {
        if (inst->isPaused.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        int targetFps = inst->fpsLimit.load();
        if (targetFps <= 0) targetFps = 60;
        int mult = inst->speedMultiplier.load();
        if (mult <= 0) mult = 1;
        int frameIntervalMs = 1000 / (targetFps * mult);
        if (frameIntervalMs < 1) frameIntervalMs = 1;

        inst->frameCounter++;

        // Render frame nền tảng LCDUI
        uint32_t customBg = inst->profile.screenBackgroundColor & 0x00FFFFFF;
        uint32_t bgColor = (customBg != 0xD0D0D0 && customBg != 0) ? (0xFF000000 | customBg) : 0xFF000000;
        inst->frameBuffer.clear(bgColor);

        auto currentDisplayable = universal_loader::lcdui::Display::instance().getCurrent();
        if (currentDisplayable) {
            j2me::LcduiGraphics g(inst->frameBuffer.getRawDrawBuffer(), inst->frameBuffer.getWidth(), inst->frameBuffer.getHeight());
            currentDisplayable->paint(&g);
        }

        // Đẩy frame ra Display Buffer cho UI (Flutter / GPU Texture) lấy
        inst->frameBuffer.publishFrame();

        // Giữ nhịp FPS chính xác
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTick).count();
        if (elapsed < frameIntervalMs) {
            std::this_thread::sleep_for(std::chrono::milliseconds(frameIntervalMs - elapsed));
        }
        lastTick = std::chrono::steady_clock::now();
    }
}

extern "C" {

J2ME_API J2meEngineInstance* j2me_core_create(const char* storage_root_dir) {
    auto* inst = new J2meEngineInstance();
    if (storage_root_dir && std::strlen(storage_root_dir) > 0) {
        inst->storageRoot = storage_root_dir;
    } else {
        inst->storageRoot = "./j2me_data";
    }
    inst->configDirs.init(inst->storageRoot);
    j2me::RmsManager::instance().setStorageRoot(inst->storageRoot);
    universal_loader::file::FileSystemRegistry::instance().setBaseDirectory(inst->storageRoot);

    std::string appsDir = (std::filesystem::path(inst->storageRoot) / "apps").string();
    inst->appRepo = std::make_shared<universal_loader::app::AppRepository>(appsDir);
    inst->appInstaller = std::make_unique<universal_loader::app::AppInstaller>(inst->storageRoot, inst->appRepo);
    universal_loader::lcdui::Display::instance().setCurrent(nullptr);
    return inst;
}

J2ME_API bool j2me_core_load_jar(J2meEngineInstance* inst, const uint8_t* jar_bytes, size_t jar_size) {
    if (!inst || !jar_bytes || jar_size == 0) return false;

    if (!inst->jarReader.openFromMemory(jar_bytes, jar_size)) {
        return false;
    }

    // Đọc thông tin Manifest
    std::string name = inst->jarReader.getManifestProperty("MIDlet-Name");
    if (!name.empty()) inst->appTitle = name;

    std::string vendor = inst->jarReader.getManifestProperty("MIDlet-Vendor");
    if (!vendor.empty()) inst->appVendor = vendor;

    std::string ver = inst->jarReader.getManifestProperty("MIDlet-Version");
    if (!ver.empty()) inst->appVersion = ver;

    inst->mainClass = inst->jarReader.getMainMidletClass();

    // Preload .class files into VM
    for (const auto& entryName : inst->jarReader.listEntries()) {
        if (entryName.size() >= 6 && entryName.substr(entryName.size() - 6) == ".class") {
            std::vector<uint8_t> classBytes;
            if (inst->jarReader.extractEntry(entryName, classBytes)) {
                inst->vm.loadClass(classBytes.data(), classBytes.size());
            }
        }
    }

    return true;
}

J2ME_API bool j2me_core_load_jar_file(J2meEngineInstance* inst, const char* jar_file_path) {
    if (!inst || !jar_file_path) return false;

    if (!inst->jarReader.openFromFile(jar_file_path)) {
        return false;
    }

    std::string name = inst->jarReader.getManifestProperty("MIDlet-Name");
    if (!name.empty()) inst->appTitle = name;

    std::string vendor = inst->jarReader.getManifestProperty("MIDlet-Vendor");
    if (!vendor.empty()) inst->appVendor = vendor;

    std::string ver = inst->jarReader.getManifestProperty("MIDlet-Version");
    if (!ver.empty()) inst->appVersion = ver;

    inst->mainClass = inst->jarReader.getMainMidletClass();

    // Preload .class files into VM
    for (const auto& entryName : inst->jarReader.listEntries()) {
        if (entryName.size() >= 6 && entryName.substr(entryName.size() - 6) == ".class") {
            std::vector<uint8_t> classBytes;
            if (inst->jarReader.extractEntry(entryName, classBytes)) {
                inst->vm.loadClass(classBytes.data(), classBytes.size());
            }
        }
    }

    return true;
}

J2ME_API void j2me_core_start(J2meEngineInstance* inst) {
    if (!inst || inst->isRunning.load()) return;

    inst->isRunning.store(true);
    inst->isPaused.store(false);
    inst->gameThread = std::thread(engine_game_loop, inst);
}

J2ME_API void j2me_core_pause(J2meEngineInstance* inst) {
    if (inst) inst->isPaused.store(true);
}

J2ME_API void j2me_core_resume(J2meEngineInstance* inst) {
    if (inst) inst->isPaused.store(false);
}

J2ME_API void j2me_core_stop(J2meEngineInstance* inst) {
    if (!inst) return;
    if (inst->isRunning.load()) {
        inst->isRunning.store(false);
        if (inst->gameThread.joinable()) {
            inst->gameThread.join();
        }
    }
}

J2ME_API void j2me_core_destroy(J2meEngineInstance* inst) {
    if (!inst) return;
    j2me_core_stop(inst);
    universal_loader::lcdui::Display::instance().setCurrent(nullptr);
    delete inst;
}

J2ME_API const uint32_t* j2me_core_lock_framebuffer(J2meEngineInstance* inst, int* out_width, int* out_height, bool* out_dirty) {
    if (!inst) {
        if (out_dirty) *out_dirty = false;
        return nullptr;
    }
    return inst->frameBuffer.lockDisplayFrame(out_width, out_height, out_dirty);
}

J2ME_API void j2me_core_unlock_framebuffer(J2meEngineInstance* inst) {
    if (inst) {
        inst->frameBuffer.unlockDisplayFrame();
    }
}

J2ME_API void j2me_core_set_screen_dimensions(J2meEngineInstance* inst, int width, int height) {
    if (inst && width > 0 && height > 0) {
        inst->frameBuffer.resize(width, height);
    }
}

J2ME_API void j2me_core_send_key(J2meEngineInstance* inst, int key_code, bool is_pressed) {
    if (!inst) return;
    if (key_code >= 0 && key_code < 256) {
        inst->keyStates[key_code] = is_pressed;
    } else if (key_code < 0 && key_code >= -63) {
        inst->specialKeyStates[-key_code] = is_pressed;
    }

    auto currentDisplayable = universal_loader::lcdui::Display::instance().getCurrent();
    if (currentDisplayable) {
        if (is_pressed) {
            currentDisplayable->keyPressed(key_code);
        } else {
            currentDisplayable->keyReleased(key_code);
        }
    }
}

J2ME_API void j2me_core_send_touch(J2meEngineInstance* inst, int action, int x, int y) {
    if (!inst) return;
    auto currentDisplayable = universal_loader::lcdui::Display::instance().getCurrent();
    if (currentDisplayable) {
        if (action == 0) {
            currentDisplayable->pointerPressed(x, y);
        } else if (action == 1) {
            currentDisplayable->pointerReleased(x, y);
        } else if (action == 2) {
            currentDisplayable->pointerDragged(x, y);
        }
    }
}

J2ME_API size_t j2me_core_render_audio(J2meEngineInstance* inst, int16_t* pcm_stereo_buffer, size_t sample_count) {
    if (!inst || !pcm_stereo_buffer || sample_count == 0) return 0;
    // Mỗi khung hình stereo gồm 2 mẫu (L + R) -> frameCount = sample_count / 2
    size_t frameCount = sample_count / 2;
    j2me::SonivoxAudioEngine::instance().renderAudio44100(pcm_stereo_buffer, frameCount);
    return sample_count;
}

J2ME_API void j2me_core_play_tone(J2meEngineInstance* inst, int note, int duration_ms, int volume) {
    if (!inst) return;
    j2me::MmapiManager::playTone(note, duration_ms, volume);
}

J2ME_API bool j2me_core_play_midi(J2meEngineInstance* inst, const uint8_t* midi_bytes, size_t length) {
    if (!inst || !midi_bytes || length == 0) return false;
    return j2me::SonivoxAudioEngine::instance().playMidiData(midi_bytes, length);
}

J2ME_API void j2me_core_stop_midi(J2meEngineInstance* inst) {
    if (!inst) return;
    j2me::SonivoxAudioEngine::instance().stopMidi();
}

J2ME_API void j2me_core_set_volume(J2meEngineInstance* inst, int volume) {
    if (!inst) return;
    j2me::SonivoxAudioEngine::instance().setMasterVolume(volume);
}

J2ME_API const char* j2me_core_get_app_title(J2meEngineInstance* inst) {
    return inst ? inst->appTitle.c_str() : "";
}

J2ME_API const char* j2me_core_get_app_vendor(J2meEngineInstance* inst) {
    return inst ? inst->appVendor.c_str() : "";
}

J2ME_API const char* j2me_core_get_app_version(J2meEngineInstance* inst) {
    return inst ? inst->appVersion.c_str() : "";
}

J2ME_API int j2me_core_get_fps_limit(J2meEngineInstance* inst) {
    return inst ? inst->fpsLimit.load() : 60;
}

J2ME_API void j2me_core_set_fps_limit(J2meEngineInstance* inst, int fps) {
    if (inst && fps > 0) {
        inst->fpsLimit.store(fps);
    }
}

// --- 6. ĐỒ HỌA 3D (M3G & MASCOT CAPSULE) ---
struct J2meGraphics3DContext {
    universal_loader::graphics3d::Rasterizer3D rasterizer;
};

J2ME_API J2meGraphics3DContext* j2me_core_3d_create_context(int width, int height) {
    auto* ctx = new J2meGraphics3DContext();
    ctx->rasterizer.setViewport(0, 0, width, height);
    ctx->rasterizer.setClipRect(0, 0, width, height);
    return ctx;
}

J2ME_API void j2me_core_3d_destroy_context(J2meGraphics3DContext* ctx) {
    delete ctx;
}

J2ME_API void j2me_core_3d_bind_target(J2meGraphics3DContext* ctx, uint32_t* color_buffer, int width, int height) {
    if (ctx) {
        ctx->rasterizer.setTarget(color_buffer, width, height);
    }
}

J2ME_API void j2me_core_3d_set_viewport(J2meGraphics3DContext* ctx, int x, int y, int width, int height) {
    if (ctx) {
        ctx->rasterizer.setViewport(x, y, width, height);
        ctx->rasterizer.setClipRect(x, y, width, height);
    }
}

J2ME_API void j2me_core_3d_clear(J2meGraphics3DContext* ctx, uint32_t argb_color, float depth) {
    if (ctx) {
        ctx->rasterizer.clear(argb_color, depth);
    }
}

J2ME_API float j2me_core_3d_get_depth(J2meGraphics3DContext* ctx, int x, int y) {
    if (ctx) {
        return ctx->rasterizer.getDepthAt(x, y);
    }
    return 1.0f;
}

J2ME_API uint32_t j2me_core_3d_get_color(J2meGraphics3DContext* ctx, int x, int y) {
    if (ctx) {
        return ctx->rasterizer.getColorAt(x, y);
    }
    return 0;
}

// --- 7. OEM & VENDOR EXTENSIONS ---
J2ME_API void j2me_core_set_vibration_callback(J2meEngineInstance* inst, J2meDeviceVibrationCallback cb, void* user_data) {
    (void)inst;
    universal_loader::oem::DeviceControlManager::instance().setVibrationCallback(cb, user_data);
}

J2ME_API void j2me_core_device_vibrate(J2meEngineInstance* inst, int duration_ms, int frequency) {
    (void)inst;
    universal_loader::oem::DeviceControlManager::instance().startVibra(frequency, duration_ms);
}

J2ME_API void j2me_core_device_stop_vibration(J2meEngineInstance* inst) {
    (void)inst;
    universal_loader::oem::DeviceControlManager::instance().stopVibra();
}

// --- 8. LCDUI HIGH-LEVEL UI & EVENT SYSTEM ---
J2ME_API int j2me_core_display_get_system_color(int color_specifier) {
    return universal_loader::lcdui::Display::instance().getColor(color_specifier);
}

J2ME_API bool j2me_core_display_vibrate(int duration_ms) {
    return universal_loader::lcdui::Display::instance().vibrate(duration_ms);
}

J2ME_API bool j2me_core_display_flash_backlight(int duration_ms) {
    return universal_loader::lcdui::Display::instance().flashBacklight(duration_ms);
}

J2ME_API void j2me_core_display_send_key(int key_code, bool is_pressed) {
    auto cur = universal_loader::lcdui::Display::instance().getCurrent();
    if (cur) {
        if (is_pressed) {
            cur->keyPressed(key_code);
        } else {
            cur->keyReleased(key_code);
        }
    }
}

J2ME_API void j2me_core_display_send_pointer(int event_type, int x, int y) {
    auto cur = universal_loader::lcdui::Display::instance().getCurrent();
    if (cur) {
        if (event_type == 0) {
            cur->pointerPressed(x, y);
        } else if (event_type == 1) {
            cur->pointerReleased(x, y);
        } else if (event_type == 2) {
            cur->pointerDragged(x, y);
        }
    }
}

// --- 9. JSR-75 FILECONNECTION & FILESYSTEM ---
J2ME_API void j2me_core_fs_set_base_dir(const char* base_dir) {
    if (base_dir) {
        universal_loader::file::FileSystemRegistry::instance().setBaseDirectory(base_dir);
    }
}

J2ME_API bool j2me_core_fs_file_exists(const char* url) {
    if (!url) return false;
    auto conn = universal_loader::file::FileSystemRegistry::instance().open(url);
    return conn ? conn->exists() : false;
}

J2ME_API int64_t j2me_core_fs_file_size(const char* url) {
    if (!url) return -1;
    auto conn = universal_loader::file::FileSystemRegistry::instance().open(url);
    if (!conn || !conn->exists() || !conn->isFile()) return -1;
    return conn->fileSize();
}

// --- 10. JSR-120 WIRELESS MESSAGING API (SMS) ---
J2ME_API void j2me_core_sms_set_callback(j2me_sms_callback_t callback, void* user_data) {
    universal_loader::messaging::SmsMessageRouter::instance().setInterceptCallback(callback, user_data);
}

J2ME_API void j2me_core_sms_set_autoreply(bool enabled, const char* reply_text) {
    std::string text = reply_text ? reply_text : "OK";
    universal_loader::messaging::SmsMessageRouter::instance().setAutoReply(enabled, text);
}

J2ME_API size_t j2me_core_sms_get_sent_count() {
    return universal_loader::messaging::SmsMessageRouter::instance().getSentCount();
}

J2ME_API void j2me_core_sms_clear_all() {
    universal_loader::messaging::SmsMessageRouter::instance().clearAll();
}

// --- 11. MIDLET LIFECYCLE & DESCRIPTOR ---
J2ME_API uintptr_t j2me_core_descriptor_create(const char* content, bool is_jad) {
    if (!content) return 0;
    auto desc = new universal_loader::midlet::AppDescriptor(content, is_jad);
    return reinterpret_cast<uintptr_t>(desc);
}

J2ME_API bool j2me_core_descriptor_get(uintptr_t handle, const char* key, char* out_buf, size_t max_len) {
    if (!handle || !key || !out_buf || max_len == 0) return false;
    auto desc = reinterpret_cast<universal_loader::midlet::AppDescriptor*>(handle);
    std::string val = desc->get(key);
    strncpy(out_buf, val.c_str(), max_len - 1);
    out_buf[max_len - 1] = '\0';
    return !val.empty();
}

J2ME_API bool j2me_core_descriptor_get_name(uintptr_t handle, char* out_buf, size_t max_len) {
    if (!handle || !out_buf || max_len == 0) return false;
    auto desc = reinterpret_cast<universal_loader::midlet::AppDescriptor*>(handle);
    std::string val = desc->getName();
    strncpy(out_buf, val.c_str(), max_len - 1);
    out_buf[max_len - 1] = '\0';
    return !val.empty();
}

J2ME_API bool j2me_core_descriptor_get_version(uintptr_t handle, char* out_buf, size_t max_len) {
    if (!handle || !out_buf || max_len == 0) return false;
    auto desc = reinterpret_cast<universal_loader::midlet::AppDescriptor*>(handle);
    std::string val = desc->getVersion();
    strncpy(out_buf, val.c_str(), max_len - 1);
    out_buf[max_len - 1] = '\0';
    return !val.empty();
}

J2ME_API bool j2me_core_descriptor_get_vendor(uintptr_t handle, char* out_buf, size_t max_len) {
    if (!handle || !out_buf || max_len == 0) return false;
    auto desc = reinterpret_cast<universal_loader::midlet::AppDescriptor*>(handle);
    std::string val = desc->getVendor();
    strncpy(out_buf, val.c_str(), max_len - 1);
    out_buf[max_len - 1] = '\0';
    return !val.empty();
}

J2ME_API void j2me_core_descriptor_destroy(uintptr_t handle) {
    if (handle) {
        delete reinterpret_cast<universal_loader::midlet::AppDescriptor*>(handle);
    }
}

J2ME_API void j2me_core_midlet_start() {
    universal_loader::midlet::MidletLifecycleManager::instance().startApp();
}

J2ME_API void j2me_core_midlet_pause() {
    universal_loader::midlet::MidletLifecycleManager::instance().pauseApp();
}

J2ME_API void j2me_core_midlet_resume() {
    universal_loader::midlet::MidletLifecycleManager::instance().resumeApp();
}

J2ME_API void j2me_core_midlet_destroy(bool unconditional) {
    universal_loader::midlet::MidletLifecycleManager::instance().destroyApp(unconditional);
}

J2ME_API int j2me_core_midlet_get_state() {
    return static_cast<int>(universal_loader::midlet::MidletLifecycleManager::instance().getState());
}

// --- 12. PHONE KEYPAD, KEYMAPPER & VIRTUAL KEYBOARD ---
J2ME_API void j2me_core_keymap_set_layout(int layout_type) {
    universal_loader::input::KeyMapper::setLayout(static_cast<universal_loader::input::KeyLayoutType>(layout_type));
}

J2ME_API int j2me_core_keymap_get_layout() {
    return static_cast<int>(universal_loader::input::KeyMapper::getLayout());
}

J2ME_API int j2me_core_keymap_convert(int midp_key_code) {
    return universal_loader::input::KeyMapper::convertKeyCode(midp_key_code);
}

J2ME_API int j2me_core_keymap_get_game_action(int key_code) {
    return universal_loader::input::KeyMapper::getGameAction(key_code);
}

J2ME_API int j2me_core_keymap_get_key_code(int game_action) {
    return universal_loader::input::KeyMapper::getKeyCode(game_action);
}

J2ME_API bool j2me_core_keymap_get_key_name(int key_code, char* out_buf, size_t max_len) {
    if (!out_buf || max_len == 0) return false;
    std::string name = universal_loader::input::KeyMapper::getKeyName(key_code);
    strncpy(out_buf, name.c_str(), max_len - 1);
    out_buf[max_len - 1] = '\0';
    return !name.empty();
}

static universal_loader::input::VirtualKeypadEngine s_virtualKeypad(240, 320);

J2ME_API int j2me_core_vk_hit_test(float touch_x, float touch_y) {
    return s_virtualKeypad.hitTest(touch_x, touch_y);
}

// --- 13. JAR RESOURCE LOADER ---
J2ME_API uintptr_t j2me_core_jar_open(const char* file_path) {
    if (!file_path) return 0;
    auto loader = new universal_loader::jvm::JarResourceLoader();
    if (loader->openFromFile(file_path)) {
        return reinterpret_cast<uintptr_t>(loader);
    }
    delete loader;
    return 0;
}

J2ME_API bool j2me_core_jar_has_resource(uintptr_t handle, const char* resource_path) {
    if (!handle || !resource_path) return false;
    auto loader = reinterpret_cast<universal_loader::jvm::JarResourceLoader*>(handle);
    return loader->hasResource(resource_path);
}

J2ME_API size_t j2me_core_jar_get_resource_size(uintptr_t handle, const char* resource_path) {
    if (!handle || !resource_path) return 0;
    auto loader = reinterpret_cast<universal_loader::jvm::JarResourceLoader*>(handle);
    return loader->getResourceSize(resource_path);
}

J2ME_API size_t j2me_core_jar_read_resource(uintptr_t handle, const char* resource_path, uint8_t* out_buf, size_t max_len) {
    if (!handle || !resource_path || !out_buf || max_len == 0) return 0;
    auto loader = reinterpret_cast<universal_loader::jvm::JarResourceLoader*>(handle);
    auto bytes = loader->getResourceBytes(resource_path);
    size_t copyLen = std::min(max_len, bytes.size());
    if (copyLen > 0) {
        memcpy(out_buf, bytes.data(), copyLen);
    }
    return copyLen;
}

J2ME_API void j2me_core_jar_close(uintptr_t handle) {
    if (handle) {
        auto loader = reinterpret_cast<universal_loader::jvm::JarResourceLoader*>(handle);
        loader->close();
        delete loader;
    }
}

// --- 14. CONFIGURATION, PROFILE & SETTINGS ---
J2ME_API uintptr_t j2me_core_profile_create_default() {
    auto* p = new universal_loader::config::ProfileModel();
    return reinterpret_cast<uintptr_t>(p);
}

J2ME_API uintptr_t j2me_core_profile_load(const char* json_str) {
    if (!json_str) return 0;
    auto* p = new universal_loader::config::ProfileModel();
    if (p->deserializeJson(json_str)) {
        return reinterpret_cast<uintptr_t>(p);
    }
    delete p;
    return 0;
}

J2ME_API bool j2me_core_profile_save(uintptr_t profile_handle, char* out_buf, size_t max_len) {
    if (!profile_handle || !out_buf || max_len == 0) return false;
    auto* p = reinterpret_cast<universal_loader::config::ProfileModel*>(profile_handle);
    std::string json = p->serializeJson();
    strncpy(out_buf, json.c_str(), max_len - 1);
    out_buf[max_len - 1] = '\0';
    return true;
}

J2ME_API int j2me_core_profile_get_int(uintptr_t profile_handle, const char* key, int default_val) {
    if (!profile_handle || !key) return default_val;
    auto* p = reinterpret_cast<universal_loader::config::ProfileModel*>(profile_handle);
    std::string k(key);
    if (k == "screenWidth") return p->screenWidth;
    if (k == "screenHeight") return p->screenHeight;
    if (k == "screenBackgroundColor") return static_cast<int>(p->screenBackgroundColor);
    if (k == "screenScaleRatio") return p->screenScaleRatio;
    if (k == "orientation") return p->orientation;
    if (k == "screenScaleToFit") return p->screenScaleToFit ? 1 : 0;
    if (k == "screenKeepAspectRatio") return p->screenKeepAspectRatio ? 1 : 0;
    if (k == "screenScaleType") return p->screenScaleType;
    if (k == "screenGravity") return p->screenGravity;
    if (k == "screenFilter") return p->screenFilter ? 1 : 0;
    if (k == "immediateMode") return p->immediateMode ? 1 : 0;
    if (k == "hwAcceleration") return p->hwAcceleration ? 1 : 0;
    if (k == "graphicsMode") return p->graphicsMode;
    if (k == "parallelRedrawScreen") return p->parallelRedrawScreen ? 1 : 0;
    if (k == "showFps") return p->showFps ? 1 : 0;
    if (k == "fpsLimit") return p->fpsLimit;
    if (k == "forceFullscreen") return p->forceFullscreen ? 1 : 0;
    if (k == "fontSizeSmall") return p->fontSizeSmall;
    if (k == "fontSizeMedium") return p->fontSizeMedium;
    if (k == "fontSizeLarge") return p->fontSizeLarge;
    if (k == "fontApplyDimensions") return p->fontApplyDimensions ? 1 : 0;
    if (k == "fontAA") return p->fontAA ? 1 : 0;
    if (k == "touchInput") return p->touchInput ? 1 : 0;
    if (k == "showKeyboard") return p->showKeyboard ? 1 : 0;
    if (k == "vkType") return p->vkType;
    if (k == "vkButtonShape") return p->vkButtonShape;
    if (k == "vkAlpha") return p->vkAlpha;
    if (k == "vkForceOpacity") return p->vkForceOpacity ? 1 : 0;
    if (k == "vkFeedback") return p->vkFeedback ? 1 : 0;
    if (k == "vkHideDelay") return p->vkHideDelay;
    if (k == "vkBgColor") return static_cast<int>(p->vkBgColor);
    if (k == "vkBgColorSelected") return static_cast<int>(p->vkBgColorSelected);
    if (k == "vkFgColor") return static_cast<int>(p->vkFgColor);
    if (k == "vkFgColorSelected") return static_cast<int>(p->vkFgColorSelected);
    if (k == "vkOutlineColor") return static_cast<int>(p->vkOutlineColor);
    if (k == "layout" || k == "keyCodesLayout") return p->keyCodesLayout;
    if (k == "version") return p->version;
    return default_val;
}

J2ME_API void j2me_core_profile_set_int(uintptr_t profile_handle, const char* key, int value) {
    if (!profile_handle || !key) return;
    auto* p = reinterpret_cast<universal_loader::config::ProfileModel*>(profile_handle);
    std::string k(key);
    if (k == "screenWidth") p->screenWidth = value;
    else if (k == "screenHeight") p->screenHeight = value;
    else if (k == "screenBackgroundColor") p->screenBackgroundColor = static_cast<uint32_t>(value);
    else if (k == "screenScaleRatio") p->screenScaleRatio = value;
    else if (k == "orientation") p->orientation = value;
    else if (k == "screenScaleToFit") p->screenScaleToFit = (value != 0);
    else if (k == "screenKeepAspectRatio") p->screenKeepAspectRatio = (value != 0);
    else if (k == "screenScaleType") p->screenScaleType = value;
    else if (k == "screenGravity") p->screenGravity = value;
    else if (k == "screenFilter") p->screenFilter = (value != 0);
    else if (k == "immediateMode") p->immediateMode = (value != 0);
    else if (k == "hwAcceleration") p->hwAcceleration = (value != 0);
    else if (k == "graphicsMode") p->graphicsMode = value;
    else if (k == "parallelRedrawScreen") p->parallelRedrawScreen = (value != 0);
    else if (k == "showFps") p->showFps = (value != 0);
    else if (k == "fpsLimit") p->fpsLimit = value;
    else if (k == "forceFullscreen") p->forceFullscreen = (value != 0);
    else if (k == "fontSizeSmall") p->fontSizeSmall = value;
    else if (k == "fontSizeMedium") p->fontSizeMedium = value;
    else if (k == "fontSizeLarge") p->fontSizeLarge = value;
    else if (k == "fontApplyDimensions") p->fontApplyDimensions = (value != 0);
    else if (k == "fontAA") p->fontAA = (value != 0);
    else if (k == "touchInput") p->touchInput = (value != 0);
    else if (k == "showKeyboard") p->showKeyboard = (value != 0);
    else if (k == "vkType") p->vkType = value;
    else if (k == "vkButtonShape") p->vkButtonShape = value;
    else if (k == "vkAlpha") p->vkAlpha = value;
    else if (k == "vkForceOpacity") p->vkForceOpacity = (value != 0);
    else if (k == "vkFeedback") p->vkFeedback = (value != 0);
    else if (k == "vkHideDelay") p->vkHideDelay = value;
    else if (k == "vkBgColor") p->vkBgColor = static_cast<uint32_t>(value);
    else if (k == "vkBgColorSelected") p->vkBgColorSelected = static_cast<uint32_t>(value);
    else if (k == "vkFgColor") p->vkFgColor = static_cast<uint32_t>(value);
    else if (k == "vkFgColorSelected") p->vkFgColorSelected = static_cast<uint32_t>(value);
    else if (k == "vkOutlineColor") p->vkOutlineColor = static_cast<uint32_t>(value);
    else if (k == "layout" || k == "keyCodesLayout") p->keyCodesLayout = value;
    else if (k == "version") p->version = value;
}

J2ME_API bool j2me_core_profile_get_string(uintptr_t profile_handle, const char* key, char* out_buf, size_t max_len) {
    if (!profile_handle || !key || !out_buf || max_len == 0) return false;
    auto* p = reinterpret_cast<universal_loader::config::ProfileModel*>(profile_handle);
    std::string k(key);
    if (k == "systemProperties") {
        strncpy(out_buf, p->systemProperties.c_str(), max_len - 1);
        out_buf[max_len - 1] = '\0';
        return true;
    }
    return false;
}

J2ME_API void j2me_core_profile_set_string(uintptr_t profile_handle, const char* key, const char* value) {
    if (!profile_handle || !key || !value) return;
    auto* p = reinterpret_cast<universal_loader::config::ProfileModel*>(profile_handle);
    std::string k(key);
    if (k == "systemProperties") {
        p->systemProperties = value;
    }
}

J2ME_API void j2me_core_profile_destroy(uintptr_t profile_handle) {
    if (profile_handle) {
        delete reinterpret_cast<universal_loader::config::ProfileModel*>(profile_handle);
    }
}

J2ME_API bool j2me_core_apply_profile(J2meEngineInstance* inst, uintptr_t profile_handle) {
    if (!inst || !profile_handle) return false;
    auto* p = reinterpret_cast<universal_loader::config::ProfileModel*>(profile_handle);
    inst->profile = *p;
    inst->frameBuffer.resize(inst->profile.screenWidth, inst->profile.screenHeight);
    inst->fpsLimit.store(inst->profile.fpsLimit > 0 ? inst->profile.fpsLimit : 60);
    universal_loader::input::KeyMapper::setLayout(static_cast<universal_loader::input::KeyLayoutType>(inst->profile.keyCodesLayout));
    return true;
}

J2ME_API size_t j2me_core_get_preset_resolution_count() {
    return universal_loader::config::PRESET_RESOLUTION_COUNT;
}

J2ME_API bool j2me_core_get_preset_resolution(size_t index, int* out_w, int* out_h, char* out_name, size_t max_len) {
    if (index >= universal_loader::config::PRESET_RESOLUTION_COUNT) return false;
    const auto& preset = universal_loader::config::PRESET_RESOLUTIONS[index];
    if (out_w) *out_w = preset.width;
    if (out_h) *out_h = preset.height;
    if (out_name && max_len > 0) {
        strncpy(out_name, preset.name, max_len - 1);
        out_name[max_len - 1] = '\0';
    }
    return true;
}

// --- 15. JAVA CLDC BYTECODE VIRTUAL MACHINE ---
J2ME_API uintptr_t j2me_core_vm_create() {
    auto* vm = new universal_loader::jvm::CldcVirtualMachine();
    return reinterpret_cast<uintptr_t>(vm);
}

J2ME_API bool j2me_core_vm_load_class(uintptr_t vm_handle, const uint8_t* class_bytes, size_t size) {
    if (!vm_handle || !class_bytes || size == 0) return false;
    auto* vm = reinterpret_cast<universal_loader::jvm::CldcVirtualMachine*>(vm_handle);
    return vm->loadClass(class_bytes, size);
}

J2ME_API int32_t j2me_core_vm_invoke_static_int(uintptr_t vm_handle, const char* class_name, const char* method_name, const char* desc) {
    if (!vm_handle || !class_name || !method_name) return 0;
    auto* vm = reinterpret_cast<universal_loader::jvm::CldcVirtualMachine*>(vm_handle);
    std::string d = desc ? desc : "()I";
    auto ret = vm->executeMethodByName(class_name, method_name, d, {});
    return ret.i;
}

J2ME_API void j2me_core_vm_destroy(uintptr_t vm_handle) {
    if (vm_handle) {
        delete reinterpret_cast<universal_loader::jvm::CldcVirtualMachine*>(vm_handle);
    }
}

// --- 16. J2ME 2D GAME API (TILEDLAYER & LAYERMANAGER) ---
J2ME_API uintptr_t j2me_core_tiled_layer_create(int columns, int rows, int image_w, int image_h, int tile_w, int tile_h) {
    try {
        auto img = std::make_shared<j2me::LcduiImage>(image_w, image_h, true);
        auto tiledLayer = std::make_shared<universal_loader::lcdui::game::TiledLayer>(columns, rows, img, tile_w, tile_h);
        auto* wrapper = new J2meLayerWrapper{tiledLayer};
        return reinterpret_cast<uintptr_t>(wrapper);
    } catch (...) {
        return 0;
    }
}

J2ME_API void j2me_core_tiled_layer_set_cell(uintptr_t handle, int col, int row, int tile_index) {
    if (!handle) return;
    auto* wrapper = reinterpret_cast<J2meLayerWrapper*>(handle);
    auto* tl = dynamic_cast<universal_loader::lcdui::game::TiledLayer*>(wrapper->layer.get());
    if (tl) {
        try {
            tl->setCell(col, row, tile_index);
        } catch (...) {}
    }
}

J2ME_API int j2me_core_tiled_layer_get_cell(uintptr_t handle, int col, int row) {
    if (!handle) return 0;
    auto* wrapper = reinterpret_cast<J2meLayerWrapper*>(handle);
    auto* tl = dynamic_cast<universal_loader::lcdui::game::TiledLayer*>(wrapper->layer.get());
    if (tl) {
        try {
            return tl->getCell(col, row);
        } catch (...) {
            return 0;
        }
    }
    return 0;
}

J2ME_API void j2me_core_tiled_layer_fill_cells(uintptr_t handle, int col, int row, int num_cols, int num_rows, int tile_index) {
    if (!handle) return;
    auto* wrapper = reinterpret_cast<J2meLayerWrapper*>(handle);
    auto* tl = dynamic_cast<universal_loader::lcdui::game::TiledLayer*>(wrapper->layer.get());
    if (tl) {
        try {
            tl->fillCells(col, row, num_cols, num_rows, tile_index);
        } catch (...) {}
    }
}

J2ME_API int j2me_core_tiled_layer_create_animated_tile(uintptr_t handle, int static_tile_index) {
    if (!handle) return 0;
    auto* wrapper = reinterpret_cast<J2meLayerWrapper*>(handle);
    auto* tl = dynamic_cast<universal_loader::lcdui::game::TiledLayer*>(wrapper->layer.get());
    if (tl) {
        try {
            return tl->createAnimatedTile(static_tile_index);
        } catch (...) {
            return 0;
        }
    }
    return 0;
}

J2ME_API void j2me_core_tiled_layer_set_animated_tile(uintptr_t handle, int anim_tile_index, int static_tile_index) {
    if (!handle) return;
    auto* wrapper = reinterpret_cast<J2meLayerWrapper*>(handle);
    auto* tl = dynamic_cast<universal_loader::lcdui::game::TiledLayer*>(wrapper->layer.get());
    if (tl) {
        try {
            tl->setAnimatedTile(anim_tile_index, static_tile_index);
        } catch (...) {}
    }
}

J2ME_API int j2me_core_tiled_layer_get_animated_tile(uintptr_t handle, int anim_tile_index) {
    if (!handle) return 0;
    auto* wrapper = reinterpret_cast<J2meLayerWrapper*>(handle);
    auto* tl = dynamic_cast<universal_loader::lcdui::game::TiledLayer*>(wrapper->layer.get());
    if (tl) {
        try {
            return tl->getAnimatedTile(anim_tile_index);
        } catch (...) {
            return 0;
        }
    }
    return 0;
}

J2ME_API void j2me_core_tiled_layer_destroy(uintptr_t handle) {
    if (handle) {
        delete reinterpret_cast<J2meLayerWrapper*>(handle);
    }
}

J2ME_API uintptr_t j2me_core_layer_manager_create() {
    auto* mgr = new universal_loader::lcdui::game::LayerManager();
    return reinterpret_cast<uintptr_t>(mgr);
}

J2ME_API void j2me_core_layer_manager_append(uintptr_t mgr_handle, uintptr_t layer_handle) {
    if (!mgr_handle || !layer_handle) return;
    auto* mgr = reinterpret_cast<universal_loader::lcdui::game::LayerManager*>(mgr_handle);
    auto* wrapper = reinterpret_cast<J2meLayerWrapper*>(layer_handle);
    if (mgr && wrapper && wrapper->layer) {
        try {
            mgr->append(wrapper->layer);
        } catch (...) {}
    }
}

J2ME_API void j2me_core_layer_manager_insert(uintptr_t mgr_handle, uintptr_t layer_handle, int index) {
    if (!mgr_handle || !layer_handle) return;
    auto* mgr = reinterpret_cast<universal_loader::lcdui::game::LayerManager*>(mgr_handle);
    auto* wrapper = reinterpret_cast<J2meLayerWrapper*>(layer_handle);
    if (mgr && wrapper && wrapper->layer) {
        try {
            mgr->insert(wrapper->layer, index);
        } catch (...) {}
    }
}

J2ME_API int j2me_core_layer_manager_get_size(uintptr_t mgr_handle) {
    if (!mgr_handle) return 0;
    auto* mgr = reinterpret_cast<universal_loader::lcdui::game::LayerManager*>(mgr_handle);
    return mgr->getSize();
}

J2ME_API void j2me_core_layer_manager_remove(uintptr_t mgr_handle, uintptr_t layer_handle) {
    if (!mgr_handle || !layer_handle) return;
    auto* mgr = reinterpret_cast<universal_loader::lcdui::game::LayerManager*>(mgr_handle);
    auto* wrapper = reinterpret_cast<J2meLayerWrapper*>(layer_handle);
    if (mgr && wrapper && wrapper->layer) {
        mgr->remove(wrapper->layer);
    }
}

J2ME_API void j2me_core_layer_manager_set_view_window(uintptr_t mgr_handle, int x, int y, int width, int height) {
    if (!mgr_handle) return;
    auto* mgr = reinterpret_cast<universal_loader::lcdui::game::LayerManager*>(mgr_handle);
    try {
        mgr->setViewWindow(x, y, width, height);
    } catch (...) {}
}

J2ME_API void j2me_core_layer_manager_paint(uintptr_t mgr_handle, J2meEngineInstance* inst, int x, int y) {
    if (!mgr_handle || !inst) return;
    auto* mgr = reinterpret_cast<universal_loader::lcdui::game::LayerManager*>(mgr_handle);
    j2me::LcduiGraphics g(inst->frameBuffer.getRawDrawBuffer(), inst->frameBuffer.getWidth(), inst->frameBuffer.getHeight());
    mgr->paint(&g, x, y);
}

J2ME_API void j2me_core_layer_manager_destroy(uintptr_t mgr_handle) {
    if (mgr_handle) {
        delete reinterpret_cast<universal_loader::lcdui::game::LayerManager*>(mgr_handle);
    }
}

// --- 17. EMULATOR UX ENHANCEMENTS (SPEED MULTIPLIER & SCREENSHOT) ---
J2ME_API void j2me_core_set_speed_multiplier(J2meEngineInstance* inst, int multiplier) {
    if (!inst) return;
    if (multiplier < 1) multiplier = 1;
    if (multiplier > 16) multiplier = 16;
    inst->speedMultiplier.store(multiplier);
}

J2ME_API int j2me_core_get_speed_multiplier(J2meEngineInstance* inst) {
    if (!inst) return 1;
    return inst->speedMultiplier.load();
}

static uint32_t png_calc_crc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int k = 0; k < 8; ++k) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(int)(crc & 1)));
        }
    }
    return ~crc;
}

static uint32_t png_calc_adler32(const uint8_t* data, size_t length) {
    uint32_t s1 = 1;
    uint32_t s2 = 0;
    const uint32_t MOD_ADLER = 65521;
    for (size_t i = 0; i < length; ++i) {
        s1 = (s1 + data[i]) % MOD_ADLER;
        s2 = (s2 + s1) % MOD_ADLER;
    }
    return (s2 << 16) | s1;
}

static void png_write_u32_be(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
}

static void png_write_chunk(std::vector<uint8_t>& buf, const char* type, const uint8_t* data, uint32_t length) {
    png_write_u32_be(buf, length);
    size_t crc_start = buf.size();
    buf.insert(buf.end(), type, type + 4);
    if (data && length > 0) {
        buf.insert(buf.end(), data, data + length);
    }
    uint32_t crc = png_calc_crc32(buf.data() + crc_start, length + 4);
    png_write_u32_be(buf, crc);
}

J2ME_API bool j2me_core_capture_screenshot_png(J2meEngineInstance* inst, const char* out_png_path) {
    if (!inst || !out_png_path) return false;
    int w = inst->frameBuffer.getWidth();
    int h = inst->frameBuffer.getHeight();
    if (w <= 0 || h <= 0) return false;

    const uint32_t* pixels = inst->frameBuffer.getRawDrawBuffer();
    if (!pixels) return false;

    std::vector<uint8_t> pngData;
    // 1. Signature
    const uint8_t signature[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
    pngData.insert(pngData.end(), signature, signature + 8);

    // 2. IHDR Chunk
    std::vector<uint8_t> ihdr;
    png_write_u32_be(ihdr, static_cast<uint32_t>(w));
    png_write_u32_be(ihdr, static_cast<uint32_t>(h));
    ihdr.push_back(8); // Bit depth
    ihdr.push_back(6); // Color type: RGBA
    ihdr.push_back(0); // Compression
    ihdr.push_back(0); // Filter
    ihdr.push_back(0); // Interlace
    png_write_chunk(pngData, "IHDR", ihdr.data(), static_cast<uint32_t>(ihdr.size()));

    // 3. IDAT Chunk: Raw scanlines with filter byte 0 + Deflate uncompressed blocks
    std::vector<uint8_t> rawScanlines;
    rawScanlines.reserve(h * (1 + w * 4));
    for (int y = 0; y < h; ++y) {
        rawScanlines.push_back(0); // Filter: None
        for (int x = 0; x < w; ++x) {
            uint32_t px = pixels[y * w + x];
            rawScanlines.push_back(static_cast<uint8_t>((px >> 16) & 0xFF)); // R
            rawScanlines.push_back(static_cast<uint8_t>((px >> 8) & 0xFF));  // G
            rawScanlines.push_back(static_cast<uint8_t>(px & 0xFF));         // B
            rawScanlines.push_back(static_cast<uint8_t>((px >> 24) & 0xFF)); // A
        }
    }

    std::vector<uint8_t> zlibStream;
    zlibStream.push_back(0x78);
    zlibStream.push_back(0x01);

    size_t offset = 0;
    size_t totalRaw = rawScanlines.size();
    while (offset < totalRaw) {
        size_t chunk = std::min(totalRaw - offset, static_cast<size_t>(65535));
        bool isLast = (offset + chunk == totalRaw);
        zlibStream.push_back(isLast ? 0x01 : 0x00);
        uint16_t len = static_cast<uint16_t>(chunk);
        uint16_t nlen = ~len;
        zlibStream.push_back(static_cast<uint8_t>(len & 0xFF));
        zlibStream.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        zlibStream.push_back(static_cast<uint8_t>(nlen & 0xFF));
        zlibStream.push_back(static_cast<uint8_t>((nlen >> 8) & 0xFF));
        zlibStream.insert(zlibStream.end(), rawScanlines.begin() + offset, rawScanlines.begin() + offset + chunk);
        offset += chunk;
    }
    uint32_t adler = png_calc_adler32(rawScanlines.data(), rawScanlines.size());
    png_write_u32_be(zlibStream, adler);

    png_write_chunk(pngData, "IDAT", zlibStream.data(), static_cast<uint32_t>(zlibStream.size()));

    // 4. IEND Chunk
    png_write_chunk(pngData, "IEND", nullptr, 0);

    // Save to disk
    std::ofstream out(out_png_path, std::ios::binary);
    if (!out.is_open()) return false;
    out.write(reinterpret_cast<const char*>(pngData.data()), pngData.size());
    return out.good();
}

// --- 18. GCF DATAGRAM / UDP NETWORKING (JSR-118) ---
J2ME_API uintptr_t j2me_core_datagram_create(int size, const char* address) {
    try {
        std::string addr = address ? address : "";
        auto* dgram = new j2me::Datagram(size, addr);
        return reinterpret_cast<uintptr_t>(dgram);
    } catch (...) {
        return 0;
    }
}

J2ME_API bool j2me_core_datagram_get_address(uintptr_t dgram_handle, char* out_buf, size_t max_len) {
    if (!dgram_handle || !out_buf || max_len == 0) return false;
    auto* dgram = reinterpret_cast<j2me::Datagram*>(dgram_handle);
    std::string addr = dgram->getAddress();
    strncpy(out_buf, addr.c_str(), max_len - 1);
    out_buf[max_len - 1] = '\0';
    return true;
}

J2ME_API void j2me_core_datagram_set_address(uintptr_t dgram_handle, const char* address) {
    if (!dgram_handle) return;
    auto* dgram = reinterpret_cast<j2me::Datagram*>(dgram_handle);
    dgram->setAddress(address ? address : "");
}

J2ME_API int j2me_core_datagram_get_length(uintptr_t dgram_handle) {
    if (!dgram_handle) return 0;
    auto* dgram = reinterpret_cast<j2me::Datagram*>(dgram_handle);
    return dgram->getLength();
}

J2ME_API void j2me_core_datagram_set_length(uintptr_t dgram_handle, int length) {
    if (!dgram_handle) return;
    auto* dgram = reinterpret_cast<j2me::Datagram*>(dgram_handle);
    try {
        dgram->setLength(length);
    } catch (...) {}
}

J2ME_API int j2me_core_datagram_get_offset(uintptr_t dgram_handle) {
    if (!dgram_handle) return 0;
    auto* dgram = reinterpret_cast<j2me::Datagram*>(dgram_handle);
    return dgram->getOffset();
}

J2ME_API size_t j2me_core_datagram_get_data(uintptr_t dgram_handle, uint8_t* out_buf, size_t max_len) {
    if (!dgram_handle || !out_buf || max_len == 0) return 0;
    auto* dgram = reinterpret_cast<j2me::Datagram*>(dgram_handle);
    size_t toCopy = std::min(max_len, static_cast<size_t>(dgram->getLength()));
    std::memcpy(out_buf, dgram->getData() + dgram->getOffset(), toCopy);
    return toCopy;
}

J2ME_API void j2me_core_datagram_set_data(uintptr_t dgram_handle, const uint8_t* buf, int offset, int length) {
    if (!dgram_handle) return;
    auto* dgram = reinterpret_cast<j2me::Datagram*>(dgram_handle);
    try {
        dgram->setData(buf, offset, length);
    } catch (...) {}
}

J2ME_API void j2me_core_datagram_reset(uintptr_t dgram_handle) {
    if (!dgram_handle) return;
    auto* dgram = reinterpret_cast<j2me::Datagram*>(dgram_handle);
    dgram->reset();
}

J2ME_API void j2me_core_datagram_write(uintptr_t dgram_handle, const uint8_t* data, size_t length) {
    if (!dgram_handle || !data || length == 0) return;
    auto* dgram = reinterpret_cast<j2me::Datagram*>(dgram_handle);
    dgram->write(data, length);
}

J2ME_API size_t j2me_core_datagram_read(uintptr_t dgram_handle, uint8_t* out_buf, size_t length) {
    if (!dgram_handle || !out_buf || length == 0) return 0;
    auto* dgram = reinterpret_cast<j2me::Datagram*>(dgram_handle);
    return dgram->read(out_buf, length);
}

J2ME_API void j2me_core_datagram_destroy(uintptr_t dgram_handle) {
    if (dgram_handle) {
        delete reinterpret_cast<j2me::Datagram*>(dgram_handle);
    }
}

J2ME_API uintptr_t j2me_core_datagram_conn_open(const char* url) {
    if (!url) return 0;
    auto* conn = new j2me::DatagramConnection();
    if (!conn->open(url)) {
        delete conn;
        return 0;
    }
    return reinterpret_cast<uintptr_t>(conn);
}

J2ME_API bool j2me_core_datagram_conn_send(uintptr_t conn_handle, uintptr_t dgram_handle) {
    if (!conn_handle || !dgram_handle) return false;
    auto* conn = reinterpret_cast<j2me::DatagramConnection*>(conn_handle);
    auto* dgram = reinterpret_cast<j2me::Datagram*>(dgram_handle);
    return conn->send(dgram);
}

J2ME_API bool j2me_core_datagram_conn_receive(uintptr_t conn_handle, uintptr_t dgram_handle, int timeout_ms) {
    if (!conn_handle || !dgram_handle) return false;
    auto* conn = reinterpret_cast<j2me::DatagramConnection*>(conn_handle);
    auto* dgram = reinterpret_cast<j2me::Datagram*>(dgram_handle);
    return conn->receive(dgram, timeout_ms);
}

J2ME_API int j2me_core_datagram_conn_get_local_port(uintptr_t conn_handle) {
    if (!conn_handle) return 0;
    auto* conn = reinterpret_cast<j2me::DatagramConnection*>(conn_handle);
    return conn->getLocalPort();
}

J2ME_API void j2me_core_datagram_conn_close(uintptr_t conn_handle) {
    if (conn_handle) {
        auto* conn = reinterpret_cast<j2me::DatagramConnection*>(conn_handle);
        conn->close();
        delete conn;
    }
}

// --- 19. M3G KEYFRAME ANIMATION & MORPHING MESH (JSR-184) ---
J2ME_API uintptr_t j2me_core_m3g_vertexbuffer_create() {
    auto* vb = new universal_loader::m3g::VertexBuffer();
    return reinterpret_cast<uintptr_t>(vb);
}

J2ME_API void j2me_core_m3g_vertexbuffer_set_positions(uintptr_t vb_handle, const float* coords, int vertex_count) {
    if (!vb_handle || !coords || vertex_count <= 0) return;
    auto* vb = reinterpret_cast<universal_loader::m3g::VertexBuffer*>(vb_handle);
    std::vector<universal_loader::graphics3d::Vector3> positions;
    positions.reserve(vertex_count);
    for (int i = 0; i < vertex_count; ++i) {
        positions.emplace_back(coords[i * 3], coords[i * 3 + 1], coords[i * 3 + 2]);
    }
    vb->setPositions(positions);
}

J2ME_API void j2me_core_m3g_vertexbuffer_set_normals(uintptr_t vb_handle, const float* normals, int vertex_count) {
    if (!vb_handle || !normals || vertex_count <= 0) return;
    auto* vb = reinterpret_cast<universal_loader::m3g::VertexBuffer*>(vb_handle);
    std::vector<universal_loader::graphics3d::Vector3> norms;
    norms.reserve(vertex_count);
    for (int i = 0; i < vertex_count; ++i) {
        norms.emplace_back(normals[i * 3], normals[i * 3 + 1], normals[i * 3 + 2]);
    }
    vb->setNormals(norms);
}

J2ME_API void j2me_core_m3g_vertexbuffer_destroy(uintptr_t vb_handle) {
    if (vb_handle) {
        delete reinterpret_cast<universal_loader::m3g::VertexBuffer*>(vb_handle);
    }
}

J2ME_API uintptr_t j2me_core_m3g_keyframesequence_create(int num_keyframes, int num_components, int interpolation) {
    auto seq = std::make_shared<universal_loader::m3g::KeyframeSequence>(num_keyframes, num_components, interpolation);
    auto* wrapper = new std::shared_ptr<universal_loader::m3g::KeyframeSequence>(seq);
    return reinterpret_cast<uintptr_t>(wrapper);
}

J2ME_API void j2me_core_m3g_keyframesequence_set_duration(uintptr_t seq_handle, int duration) {
    if (!seq_handle) return;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::KeyframeSequence>*>(seq_handle);
    if (*wrapper) (*wrapper)->setDuration(duration);
}

J2ME_API int j2me_core_m3g_keyframesequence_get_duration(uintptr_t seq_handle) {
    if (!seq_handle) return 0;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::KeyframeSequence>*>(seq_handle);
    return (*wrapper) ? (*wrapper)->getDuration() : 0;
}

J2ME_API void j2me_core_m3g_keyframesequence_set_repeat_mode(uintptr_t seq_handle, int mode) {
    if (!seq_handle) return;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::KeyframeSequence>*>(seq_handle);
    if (*wrapper) (*wrapper)->setRepeatMode(mode);
}

J2ME_API int j2me_core_m3g_keyframesequence_get_repeat_mode(uintptr_t seq_handle) {
    if (!seq_handle) return 0;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::KeyframeSequence>*>(seq_handle);
    return (*wrapper) ? (*wrapper)->getRepeatMode() : 0;
}

J2ME_API void j2me_core_m3g_keyframesequence_set_keyframe(uintptr_t seq_handle, int index, int time, const float* value) {
    if (!seq_handle || !value) return;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::KeyframeSequence>*>(seq_handle);
    if (*wrapper) (*wrapper)->setKeyframe(index, time, value);
}

J2ME_API bool j2me_core_m3g_keyframesequence_sample(uintptr_t seq_handle, int sequence_time, float* out_val, int max_components) {
    if (!seq_handle || !out_val || max_components <= 0) return false;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::KeyframeSequence>*>(seq_handle);
    if (!*wrapper) return false;
    return (*wrapper)->sample(sequence_time, out_val);
}

J2ME_API void j2me_core_m3g_keyframesequence_destroy(uintptr_t seq_handle) {
    if (seq_handle) {
        delete reinterpret_cast<std::shared_ptr<universal_loader::m3g::KeyframeSequence>*>(seq_handle);
    }
}

J2ME_API uintptr_t j2me_core_m3g_animcontroller_create() {
    auto ctrl = std::make_shared<universal_loader::m3g::AnimationController>();
    auto* wrapper = new std::shared_ptr<universal_loader::m3g::AnimationController>(ctrl);
    return reinterpret_cast<uintptr_t>(wrapper);
}

J2ME_API void j2me_core_m3g_animcontroller_set_active_interval(uintptr_t ctrl_handle, int start, int end) {
    if (!ctrl_handle) return;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::AnimationController>*>(ctrl_handle);
    if (*wrapper) (*wrapper)->setActiveInterval(start, end);
}

J2ME_API int j2me_core_m3g_animcontroller_get_active_interval_start(uintptr_t ctrl_handle) {
    if (!ctrl_handle) return 0;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::AnimationController>*>(ctrl_handle);
    return (*wrapper) ? (*wrapper)->getActiveIntervalStart() : 0;
}

J2ME_API int j2me_core_m3g_animcontroller_get_active_interval_end(uintptr_t ctrl_handle) {
    if (!ctrl_handle) return 0;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::AnimationController>*>(ctrl_handle);
    return (*wrapper) ? (*wrapper)->getActiveIntervalEnd() : 0;
}

J2ME_API void j2me_core_m3g_animcontroller_set_speed(uintptr_t ctrl_handle, float speed, int world_time) {
    if (!ctrl_handle) return;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::AnimationController>*>(ctrl_handle);
    if (*wrapper) (*wrapper)->setSpeed(speed, world_time);
}

J2ME_API float j2me_core_m3g_animcontroller_get_speed(uintptr_t ctrl_handle) {
    if (!ctrl_handle) return 0.0f;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::AnimationController>*>(ctrl_handle);
    return (*wrapper) ? (*wrapper)->getSpeed() : 0.0f;
}

J2ME_API void j2me_core_m3g_animcontroller_set_position(uintptr_t ctrl_handle, float sequence_time, int world_time) {
    if (!ctrl_handle) return;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::AnimationController>*>(ctrl_handle);
    if (*wrapper) (*wrapper)->setPosition(sequence_time, world_time);
}

J2ME_API float j2me_core_m3g_animcontroller_get_position(uintptr_t ctrl_handle, int world_time) {
    if (!ctrl_handle) return 0.0f;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::AnimationController>*>(ctrl_handle);
    return (*wrapper) ? (*wrapper)->getPosition(world_time) : 0.0f;
}

J2ME_API void j2me_core_m3g_animcontroller_set_weight(uintptr_t ctrl_handle, float weight) {
    if (!ctrl_handle) return;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::AnimationController>*>(ctrl_handle);
    if (*wrapper) (*wrapper)->setWeight(weight);
}

J2ME_API float j2me_core_m3g_animcontroller_get_weight(uintptr_t ctrl_handle) {
    if (!ctrl_handle) return 0.0f;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::AnimationController>*>(ctrl_handle);
    return (*wrapper) ? (*wrapper)->getWeight() : 0.0f;
}

J2ME_API void j2me_core_m3g_animcontroller_destroy(uintptr_t ctrl_handle) {
    if (ctrl_handle) {
        delete reinterpret_cast<std::shared_ptr<universal_loader::m3g::AnimationController>*>(ctrl_handle);
    }
}

J2ME_API uintptr_t j2me_core_m3g_animtrack_create(uintptr_t seq_handle, int property_id) {
    if (!seq_handle) return 0;
    auto* seq_wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::KeyframeSequence>*>(seq_handle);
    if (!*seq_wrapper) return 0;
    auto track = std::make_shared<universal_loader::m3g::AnimationTrack>(*seq_wrapper, property_id);
    auto* wrapper = new std::shared_ptr<universal_loader::m3g::AnimationTrack>(track);
    return reinterpret_cast<uintptr_t>(wrapper);
}

J2ME_API void j2me_core_m3g_animtrack_set_controller(uintptr_t track_handle, uintptr_t ctrl_handle) {
    if (!track_handle) return;
    auto* track_wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::AnimationTrack>*>(track_handle);
    if (!*track_wrapper) return;

    if (ctrl_handle) {
        auto* ctrl_wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::AnimationController>*>(ctrl_handle);
        (*track_wrapper)->setController(*ctrl_wrapper);
    } else {
        (*track_wrapper)->setController(nullptr);
    }
}

J2ME_API uintptr_t j2me_core_m3g_animtrack_get_controller(uintptr_t track_handle) {
    if (!track_handle) return 0;
    auto* track_wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::AnimationTrack>*>(track_handle);
    if (!*track_wrapper) return 0;
    auto ctrl = (*track_wrapper)->getController();
    if (!ctrl) return 0;
    return reinterpret_cast<uintptr_t>(new std::shared_ptr<universal_loader::m3g::AnimationController>(ctrl));
}

J2ME_API uintptr_t j2me_core_m3g_animtrack_get_sequence(uintptr_t track_handle) {
    if (!track_handle) return 0;
    auto* track_wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::AnimationTrack>*>(track_handle);
    if (!*track_wrapper) return 0;
    auto seq = (*track_wrapper)->getKeyframeSequence();
    if (!seq) return 0;
    return reinterpret_cast<uintptr_t>(new std::shared_ptr<universal_loader::m3g::KeyframeSequence>(seq));
}

J2ME_API int j2me_core_m3g_animtrack_get_target_property(uintptr_t track_handle) {
    if (!track_handle) return 0;
    auto* track_wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::AnimationTrack>*>(track_handle);
    return (*track_wrapper) ? (*track_wrapper)->getTargetProperty() : 0;
}

J2ME_API void j2me_core_m3g_animtrack_destroy(uintptr_t track_handle) {
    if (track_handle) {
        delete reinterpret_cast<std::shared_ptr<universal_loader::m3g::AnimationTrack>*>(track_handle);
    }
}

J2ME_API uintptr_t j2me_core_m3g_morphing_mesh_create(uintptr_t base_vb_handle, int target_count, const uintptr_t* target_vb_handles) {
    if (!base_vb_handle) return 0;
    auto* base_vb = reinterpret_cast<universal_loader::m3g::VertexBuffer*>(base_vb_handle);

    std::vector<universal_loader::m3g::VertexBuffer> targets;
    if (target_count > 0 && target_vb_handles != nullptr) {
        targets.reserve(target_count);
        for (int i = 0; i < target_count; ++i) {
            if (target_vb_handles[i]) {
                targets.push_back(*reinterpret_cast<universal_loader::m3g::VertexBuffer*>(target_vb_handles[i]));
            }
        }
    }

    std::vector<universal_loader::m3g::Submesh> submeshes;
    auto* mesh = new universal_loader::m3g::MorphingMesh(*base_vb, targets, submeshes);
    return reinterpret_cast<uintptr_t>(mesh);
}

J2ME_API void j2me_core_m3g_morphing_mesh_set_weights(uintptr_t mesh_handle, const float* weights, int count) {
    if (!mesh_handle || !weights || count <= 0) return;
    auto* mesh = reinterpret_cast<universal_loader::m3g::MorphingMesh*>(mesh_handle);
    mesh->setWeights(weights, count);
}

J2ME_API void j2me_core_m3g_morphing_mesh_get_weights(uintptr_t mesh_handle, float* out_weights, int max_count) {
    if (!mesh_handle || !out_weights || max_count <= 0) return;
    auto* mesh = reinterpret_cast<universal_loader::m3g::MorphingMesh*>(mesh_handle);
    mesh->getWeights(out_weights, max_count);
}

J2ME_API void j2me_core_m3g_morphing_mesh_morph(uintptr_t mesh_handle) {
    if (!mesh_handle) return;
    auto* mesh = reinterpret_cast<universal_loader::m3g::MorphingMesh*>(mesh_handle);
    mesh->morph();
}

J2ME_API void j2me_core_m3g_mesh_add_animation_track(uintptr_t mesh_handle, uintptr_t track_handle) {
    if (!mesh_handle || !track_handle) return;
    auto* mesh = reinterpret_cast<universal_loader::m3g::MorphingMesh*>(mesh_handle);
    auto* track_wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::AnimationTrack>*>(track_handle);
    if (*track_wrapper) {
        mesh->addAnimationTrack(*track_wrapper);
    }
}

J2ME_API int j2me_core_m3g_mesh_animate(uintptr_t mesh_handle, int world_time) {
    if (!mesh_handle) return 0;
    auto* mesh = reinterpret_cast<universal_loader::m3g::MorphingMesh*>(mesh_handle);
    return mesh->animate(world_time);
}

J2ME_API void j2me_core_m3g_mesh_get_position(uintptr_t mesh_handle, float* out_xyz) {
    if (!mesh_handle || !out_xyz) return;
    auto* mesh = reinterpret_cast<universal_loader::m3g::MorphingMesh*>(mesh_handle);
    const auto& pos = mesh->getTranslation();
    out_xyz[0] = pos.x;
    out_xyz[1] = pos.y;
    out_xyz[2] = pos.z;
}

J2ME_API void j2me_core_m3g_mesh_get_vertex_position(uintptr_t mesh_handle, int vertex_index, float* out_xyz) {
    if (!mesh_handle || !out_xyz || vertex_index < 0) return;
    auto* mesh = reinterpret_cast<universal_loader::m3g::MorphingMesh*>(mesh_handle);
    if (vertex_index < static_cast<int>(mesh->vertexBuffer.vertices.size())) {
        const auto& p = mesh->vertexBuffer.vertices[vertex_index].position;
        out_xyz[0] = p.x;
        out_xyz[1] = p.y;
        out_xyz[2] = p.z;
    }
}

J2ME_API void j2me_core_m3g_mesh_destroy(uintptr_t mesh_handle) {
    if (mesh_handle) {
        delete reinterpret_cast<universal_loader::m3g::MorphingMesh*>(mesh_handle);
    }
}

// --- 20. M3G SKINNED MESH & BONE SKELETON (JSR-184) ---
J2ME_API uintptr_t j2me_core_m3g_node_create() {
    auto node = std::make_shared<universal_loader::m3g::Node>();
    return reinterpret_cast<uintptr_t>(new std::shared_ptr<universal_loader::m3g::Node>(node));
}

J2ME_API void j2me_core_m3g_node_set_translation(uintptr_t node_handle, float tx, float ty, float tz) {
    if (!node_handle) return;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::Node>*>(node_handle);
    if (*wrapper) (*wrapper)->setTranslation(tx, ty, tz);
}

J2ME_API void j2me_core_m3g_node_get_translation(uintptr_t node_handle, float* out_xyz) {
    if (!node_handle || !out_xyz) return;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::Node>*>(node_handle);
    if (*wrapper) {
        const auto& t = (*wrapper)->getTranslation();
        out_xyz[0] = t.x; out_xyz[1] = t.y; out_xyz[2] = t.z;
    }
}

J2ME_API void j2me_core_m3g_node_set_orientation(uintptr_t node_handle, float qx, float qy, float qz, float qw) {
    if (!node_handle) return;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::Node>*>(node_handle);
    if (*wrapper) (*wrapper)->setOrientation(universal_loader::graphics3d::Quaternion(qx, qy, qz, qw));
}

J2ME_API void j2me_core_m3g_node_get_orientation(uintptr_t node_handle, float* out_xyzw) {
    if (!node_handle || !out_xyzw) return;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::Node>*>(node_handle);
    if (*wrapper) {
        const auto& q = (*wrapper)->getOrientation();
        out_xyzw[0] = q.x; out_xyzw[1] = q.y; out_xyzw[2] = q.z; out_xyzw[3] = q.w;
    }
}

J2ME_API void j2me_core_m3g_node_set_scale(uintptr_t node_handle, float sx, float sy, float sz) {
    if (!node_handle) return;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::Node>*>(node_handle);
    if (*wrapper) (*wrapper)->setScale(sx, sy, sz);
}

J2ME_API void j2me_core_m3g_node_get_scale(uintptr_t node_handle, float* out_xyz) {
    if (!node_handle || !out_xyz) return;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::Node>*>(node_handle);
    if (*wrapper) {
        const auto& s = (*wrapper)->getScale();
        out_xyz[0] = s.x; out_xyz[1] = s.y; out_xyz[2] = s.z;
    }
}

J2ME_API void j2me_core_m3g_node_get_global_transform(uintptr_t node_handle, float* out_matrix16) {
    if (!node_handle || !out_matrix16) return;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::Node>*>(node_handle);
    if (*wrapper) {
        auto m = (*wrapper)->getGlobalTransform();
        std::memcpy(out_matrix16, m.m, 16 * sizeof(float));
    }
}

J2ME_API void j2me_core_m3g_node_add_animation_track(uintptr_t node_handle, uintptr_t track_handle) {
    if (!node_handle || !track_handle) return;
    auto* node_wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::Node>*>(node_handle);
    auto* track_wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::AnimationTrack>*>(track_handle);
    if (*node_wrapper && *track_wrapper) {
        (*node_wrapper)->addAnimationTrack(*track_wrapper);
    }
}

J2ME_API int j2me_core_m3g_node_animate(uintptr_t node_handle, int world_time) {
    if (!node_handle) return 0;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::Node>*>(node_handle);
    return (*wrapper) ? (*wrapper)->animate(world_time) : 0;
}

J2ME_API void j2me_core_m3g_node_destroy(uintptr_t node_handle) {
    if (node_handle) {
        delete reinterpret_cast<std::shared_ptr<universal_loader::m3g::Node>*>(node_handle);
    }
}

J2ME_API uintptr_t j2me_core_m3g_group_create() {
    auto grp = std::make_shared<universal_loader::m3g::Group>();
    return reinterpret_cast<uintptr_t>(new std::shared_ptr<universal_loader::m3g::Group>(grp));
}

J2ME_API void j2me_core_m3g_group_add_child(uintptr_t group_handle, uintptr_t child_node_handle) {
    if (!group_handle || !child_node_handle) return;
    auto* grp_wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::Group>*>(group_handle);
    auto* child_wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::Node>*>(child_node_handle);
    if (*grp_wrapper && *child_wrapper) {
        (*grp_wrapper)->addChild(*child_wrapper);
    }
}

J2ME_API void j2me_core_m3g_group_remove_child(uintptr_t group_handle, uintptr_t child_node_handle) {
    if (!group_handle || !child_node_handle) return;
    auto* grp_wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::Group>*>(group_handle);
    auto* child_wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::Node>*>(child_node_handle);
    if (*grp_wrapper && *child_wrapper) {
        (*grp_wrapper)->removeChild(*child_wrapper);
    }
}

J2ME_API size_t j2me_core_m3g_group_get_child_count(uintptr_t group_handle) {
    if (!group_handle) return 0;
    auto* grp_wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::Group>*>(group_handle);
    return (*grp_wrapper) ? (*grp_wrapper)->getChildCount() : 0;
}

J2ME_API uintptr_t j2me_core_m3g_group_get_child(uintptr_t group_handle, size_t index) {
    if (!group_handle) return 0;
    auto* grp_wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::Group>*>(group_handle);
    if (!*grp_wrapper) return 0;
    auto child = (*grp_wrapper)->getChild(index);
    if (!child) return 0;
    return reinterpret_cast<uintptr_t>(new std::shared_ptr<universal_loader::m3g::Node>(child));
}

J2ME_API int j2me_core_m3g_group_animate(uintptr_t group_handle, int world_time) {
    if (!group_handle) return 0;
    auto* grp_wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::Group>*>(group_handle);
    return (*grp_wrapper) ? (*grp_wrapper)->animate(world_time) : 0;
}

J2ME_API void j2me_core_m3g_group_destroy(uintptr_t group_handle) {
    if (group_handle) {
        delete reinterpret_cast<std::shared_ptr<universal_loader::m3g::Group>*>(group_handle);
    }
}

J2ME_API uintptr_t j2me_core_m3g_skinned_mesh_create(uintptr_t base_vb_handle, uintptr_t skeleton_group_handle) {
    if (!base_vb_handle) return 0;
    auto* base_vb = reinterpret_cast<universal_loader::m3g::VertexBuffer*>(base_vb_handle);
    std::shared_ptr<universal_loader::m3g::Group> skeleton = nullptr;
    if (skeleton_group_handle) {
        auto* grp_wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::Group>*>(skeleton_group_handle);
        skeleton = *grp_wrapper;
    }
    std::vector<universal_loader::m3g::Submesh> submeshes;
    auto* mesh = new universal_loader::m3g::SkinnedMesh(*base_vb, submeshes, skeleton);
    return reinterpret_cast<uintptr_t>(mesh);
}

J2ME_API void j2me_core_m3g_skinned_mesh_add_transform(uintptr_t mesh_handle, uintptr_t bone_node_handle, int weight, int first_vertex, int num_vertices) {
    if (!mesh_handle || !bone_node_handle) return;
    auto* mesh = reinterpret_cast<universal_loader::m3g::SkinnedMesh*>(mesh_handle);
    auto* bone_wrapper = reinterpret_cast<std::shared_ptr<universal_loader::m3g::Node>*>(bone_node_handle);
    if (*bone_wrapper) {
        mesh->addTransform(*bone_wrapper, weight, first_vertex, num_vertices);
    }
}

J2ME_API size_t j2me_core_m3g_skinned_mesh_get_bone_count(uintptr_t mesh_handle) {
    if (!mesh_handle) return 0;
    auto* mesh = reinterpret_cast<universal_loader::m3g::SkinnedMesh*>(mesh_handle);
    return mesh->getBoneCount();
}

J2ME_API uintptr_t j2me_core_m3g_skinned_mesh_get_bone(uintptr_t mesh_handle, size_t index) {
    if (!mesh_handle) return 0;
    auto* mesh = reinterpret_cast<universal_loader::m3g::SkinnedMesh*>(mesh_handle);
    auto bone = mesh->getBone(index);
    if (!bone) return 0;
    return reinterpret_cast<uintptr_t>(new std::shared_ptr<universal_loader::m3g::Node>(bone));
}

J2ME_API uintptr_t j2me_core_m3g_skinned_mesh_get_skeleton(uintptr_t mesh_handle) {
    if (!mesh_handle) return 0;
    auto* mesh = reinterpret_cast<universal_loader::m3g::SkinnedMesh*>(mesh_handle);
    auto skel = mesh->getSkeleton();
    if (!skel) return 0;
    return reinterpret_cast<uintptr_t>(new std::shared_ptr<universal_loader::m3g::Group>(skel));
}

J2ME_API void j2me_core_m3g_skinned_mesh_skin(uintptr_t mesh_handle) {
    if (!mesh_handle) return;
    auto* mesh = reinterpret_cast<universal_loader::m3g::SkinnedMesh*>(mesh_handle);
    mesh->skin();
}

J2ME_API int j2me_core_m3g_skinned_mesh_animate(uintptr_t mesh_handle, int world_time) {
    if (!mesh_handle) return 0;
    auto* mesh = reinterpret_cast<universal_loader::m3g::SkinnedMesh*>(mesh_handle);
    return mesh->animate(world_time);
}

J2ME_API void j2me_core_m3g_skinned_mesh_get_vertex_position(uintptr_t mesh_handle, int vertex_index, float* out_xyz) {
    if (!mesh_handle || !out_xyz || vertex_index < 0) return;
    auto* mesh = reinterpret_cast<universal_loader::m3g::SkinnedMesh*>(mesh_handle);
    if (vertex_index < static_cast<int>(mesh->vertexBuffer.vertices.size())) {
        const auto& p = mesh->vertexBuffer.vertices[vertex_index].position;
        out_xyz[0] = p.x; out_xyz[1] = p.y; out_xyz[2] = p.z;
    }
}

J2ME_API void j2me_core_m3g_skinned_mesh_get_vertex_normal(uintptr_t mesh_handle, int vertex_index, float* out_xyz) {
    if (!mesh_handle || !out_xyz || vertex_index < 0) return;
    auto* mesh = reinterpret_cast<universal_loader::m3g::SkinnedMesh*>(mesh_handle);
    if (vertex_index < static_cast<int>(mesh->vertexBuffer.vertices.size())) {
        const auto& n = mesh->vertexBuffer.vertices[vertex_index].normal;
        out_xyz[0] = n.x; out_xyz[1] = n.y; out_xyz[2] = n.z;
    }
}

J2ME_API void j2me_core_m3g_skinned_mesh_destroy(uintptr_t mesh_handle) {
    if (mesh_handle) {
        delete reinterpret_cast<universal_loader::m3g::SkinnedMesh*>(mesh_handle);
    }
}

// --- 21. APP MANAGEMENT, REPOSITORY & INSTALLER ---
static void fillAppItemInfo(const universal_loader::app::AppItem& item, J2meAppItemInfo* out_info) {
    if (!out_info) return;
    std::memset(out_info, 0, sizeof(J2meAppItemInfo));
    out_info->id = item.id;
    strncpy(out_info->path, item.path.c_str(), sizeof(out_info->path) - 1);
    strncpy(out_info->title, item.title.c_str(), sizeof(out_info->title) - 1);
    strncpy(out_info->author, item.author.c_str(), sizeof(out_info->author) - 1);
    strncpy(out_info->version, item.version.c_str(), sizeof(out_info->version) - 1);
    strncpy(out_info->image_path, item.imagePath.c_str(), sizeof(out_info->image_path) - 1);
    out_info->installed_timestamp = item.installedTimestamp;
    out_info->last_played_timestamp = item.lastPlayedTimestamp;
    out_info->play_count = item.playCount;
}

J2ME_API size_t j2me_core_app_repo_get_count(J2meEngineInstance* engine) {
    if (!engine || !engine->appRepo) return 0;
    return engine->appRepo->getCount();
}

J2ME_API bool j2me_core_app_repo_get_item(J2meEngineInstance* engine, size_t index, J2meAppItemInfo* out_info) {
    if (!engine || !engine->appRepo || !out_info) return false;
    auto apps = engine->appRepo->getAll();
    if (index >= apps.size()) return false;
    fillAppItemInfo(apps[index], out_info);
    return true;
}

J2ME_API bool j2me_core_app_repo_find_by_id(J2meEngineInstance* engine, int id, J2meAppItemInfo* out_info) {
    if (!engine || !engine->appRepo || !out_info) return false;
    universal_loader::app::AppItem item;
    if (!engine->appRepo->getById(id, item)) return false;
    fillAppItemInfo(item, out_info);
    return true;
}

J2ME_API bool j2me_core_app_repo_find_by_path(J2meEngineInstance* engine, const char* path, J2meAppItemInfo* out_info) {
    if (!engine || !engine->appRepo || !path || !out_info) return false;
    universal_loader::app::AppItem item;
    if (!engine->appRepo->getByPath(path, item)) return false;
    fillAppItemInfo(item, out_info);
    return true;
}

J2ME_API bool j2me_core_app_repo_delete(J2meEngineInstance* engine, int id) {
    if (!engine || !engine->appRepo) return false;
    return engine->appRepo->remove(id);
}

J2ME_API int j2me_core_app_installer_check_jar(J2meEngineInstance* engine, const char* jar_path, char* out_title, size_t title_cap, char* out_vendor, size_t vendor_cap, char* out_version, size_t version_cap) {
    if (!engine || !engine->appInstaller || !jar_path) return static_cast<int>(universal_loader::app::InstallStatus::STATUS_ERROR);
    auto res = engine->appInstaller->checkJar(jar_path);
    if (out_title && title_cap > 0) {
        strncpy(out_title, res.title.c_str(), title_cap - 1);
        out_title[title_cap - 1] = '\0';
    }
    if (out_vendor && vendor_cap > 0) {
        strncpy(out_vendor, res.vendor.c_str(), vendor_cap - 1);
        out_vendor[vendor_cap - 1] = '\0';
    }
    if (out_version && version_cap > 0) {
        strncpy(out_version, res.version.c_str(), version_cap - 1);
        out_version[version_cap - 1] = '\0';
    }
    return static_cast<int>(res.status);
}

J2ME_API int j2me_core_app_installer_install(J2meEngineInstance* engine, const char* jar_path, bool force_update, char* out_error, size_t error_cap) {
    if (!engine || !engine->appInstaller || !jar_path) {
        if (out_error && error_cap > 0) snprintf(out_error, error_cap, "Invalid engine or jar path");
        return 0;
    }
    int appId = 0;
    std::string err;
    bool ok = engine->appInstaller->installFromJar(jar_path, force_update, appId, err);
    if (!ok) {
        if (out_error && error_cap > 0) {
            strncpy(out_error, err.c_str(), error_cap - 1);
            out_error[error_cap - 1] = '\0';
        }
        return 0;
    }
    return appId;
}

J2ME_API bool j2me_core_app_installer_uninstall(J2meEngineInstance* engine, int app_id, char* out_error, size_t error_cap) {
    if (!engine || !engine->appInstaller) {
        if (out_error && error_cap > 0) snprintf(out_error, error_cap, "Invalid engine or installer");
        return false;
    }
    std::string err;
    bool ok = engine->appInstaller->uninstall(app_id, err);
    if (!ok && out_error && error_cap > 0) {
        strncpy(out_error, err.c_str(), error_cap - 1);
        out_error[error_cap - 1] = '\0';
    }
    return ok;
}

J2ME_API bool j2me_core_app_launch(J2meEngineInstance* engine, int app_id) {
    if (!engine || !engine->appRepo || !engine->appInstaller) return false;
    universal_loader::app::AppItem item;
    if (!engine->appRepo->getById(app_id, item)) {
        return false;
    }

    std::string appDir = engine->appInstaller->getAppDir(item.path);
    std::string jarPath = (std::filesystem::path(appDir) / "app.jar").string();
    if (!std::filesystem::exists(jarPath)) {
        return false;
    }

    // Isolate RMS storage for this app
    std::string dataDir = engine->appInstaller->getDataDir(item.path);
    j2me::RmsManager::instance().setStorageRoot(dataDir);
    universal_loader::file::FileSystemRegistry::instance().setBaseDirectory(dataDir);

    // Load & apply configuration profile
    std::string cfgDir = engine->appInstaller->getConfigDir(item.path);
    std::string cfgFile = (std::filesystem::path(cfgDir) / "config.json").string();
    if (std::filesystem::exists(cfgFile)) {
        universal_loader::config::ProfileModel prof;
        if (universal_loader::config::ProfilesManager::loadConfig(cfgFile, prof)) {
            engine->profile = prof;
            engine->fpsLimit.store(prof.fpsLimit > 0 ? prof.fpsLimit : 60);
            engine->frameBuffer.resize(prof.screenWidth, prof.screenHeight);
            universal_loader::input::KeyMapper::setLayout(static_cast<universal_loader::input::KeyLayoutType>(prof.keyCodesLayout));
        }
    }

    // Load JAR
    if (!j2me_core_load_jar_file(engine, jarPath.c_str())) {
        return false;
    }

    // Update play stats
    item.lastPlayedTimestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    item.playCount++;
    engine->appRepo->update(item);
    engine->appRepo->save();

    return true;
}

// --- 22. JSR-82 MOBILE BLUETOOTH & RFCOMM/L2CAP MULTIPLAYER ---
J2ME_API bool j2me_core_bluetooth_is_power_on(void) {
    return j2me::bluetooth::LocalDevice::getInstance().isPowerOn();
}

J2ME_API void j2me_core_bluetooth_set_power_on(bool on) {
    j2me::bluetooth::LocalDevice::getInstance().setPowerOn(on);
}

J2ME_API void j2me_core_bluetooth_get_local_address(char* out_addr, size_t cap) {
    if (!out_addr || cap == 0) return;
    std::string addr = j2me::bluetooth::LocalDevice::getInstance().getBluetoothAddress();
    strncpy(out_addr, addr.c_str(), cap - 1);
    out_addr[cap - 1] = '\0';
}

J2ME_API void j2me_core_bluetooth_set_local_address(const char* addr) {
    if (!addr) return;
    j2me::bluetooth::LocalDevice::getInstance().setBluetoothAddress(addr);
}

J2ME_API void j2me_core_bluetooth_get_local_name(char* out_name, size_t cap) {
    if (!out_name || cap == 0) return;
    std::string name = j2me::bluetooth::LocalDevice::getInstance().getFriendlyName();
    strncpy(out_name, name.c_str(), cap - 1);
    out_name[cap - 1] = '\0';
}

J2ME_API void j2me_core_bluetooth_set_local_name(const char* name) {
    if (!name) return;
    j2me::bluetooth::LocalDevice::getInstance().setFriendlyName(name);
}

J2ME_API int j2me_core_bluetooth_get_discoverable(void) {
    return j2me::bluetooth::LocalDevice::getInstance().getDiscoverable();
}

J2ME_API bool j2me_core_bluetooth_set_discoverable(int mode) {
    return j2me::bluetooth::LocalDevice::getInstance().setDiscoverable(mode);
}

J2ME_API bool j2me_core_bluetooth_get_property(const char* property, char* out_val, size_t cap) {
    if (!property || !out_val || cap == 0) return false;
    std::string val = j2me::bluetooth::LocalDevice::getInstance().getProperty(property);
    if (val.empty()) return false;
    strncpy(out_val, val.c_str(), cap - 1);
    out_val[cap - 1] = '\0';
    return true;
}

// BTSPP Server & Client
J2ME_API uintptr_t j2me_core_bluetooth_open_btspp_server(const char* url) {
    if (!url) return 0;
    auto server = j2me::bluetooth::openBtsppServer(url);
    if (!server) return 0;
    auto* holder = new std::shared_ptr<j2me::bluetooth::SPPConnectionNotifier>(server);
    return reinterpret_cast<uintptr_t>(holder);
}

J2ME_API uintptr_t j2me_core_bluetooth_btspp_accept(uintptr_t server_handle, int timeout_ms) {
    if (!server_handle) return 0;
    auto* holder = reinterpret_cast<std::shared_ptr<j2me::bluetooth::SPPConnectionNotifier>*>(server_handle);
    if (!holder || !(*holder)) return 0;
    auto conn = (*holder)->acceptAndOpen(timeout_ms);
    if (!conn) return 0;
    auto* connHolder = new std::shared_ptr<j2me::bluetooth::SPPConnectionImpl>(conn);
    return reinterpret_cast<uintptr_t>(connHolder);
}

J2ME_API void j2me_core_bluetooth_btspp_server_close(uintptr_t server_handle) {
    if (!server_handle) return;
    auto* holder = reinterpret_cast<std::shared_ptr<j2me::bluetooth::SPPConnectionNotifier>*>(server_handle);
    if (holder) {
        if (*holder) (*holder)->close();
        delete holder;
    }
}

J2ME_API uintptr_t j2me_core_bluetooth_open_btspp_client(const char* url, int timeout_ms) {
    if (!url) return 0;
    auto client = j2me::bluetooth::openBtsppClient(url, timeout_ms);
    if (!client) return 0;
    auto* connHolder = new std::shared_ptr<j2me::bluetooth::SPPConnectionImpl>(client);
    return reinterpret_cast<uintptr_t>(connHolder);
}

J2ME_API int j2me_core_bluetooth_btspp_read(uintptr_t conn_handle, uint8_t* buffer, size_t max_len, int timeout_ms) {
    if (!conn_handle || !buffer || max_len == 0) return -1;
    auto* holder = reinterpret_cast<std::shared_ptr<j2me::bluetooth::SPPConnectionImpl>*>(conn_handle);
    if (!holder || !(*holder)) return -1;
    return (*holder)->read(buffer, max_len, timeout_ms);
}

J2ME_API int j2me_core_bluetooth_btspp_write(uintptr_t conn_handle, const uint8_t* buffer, size_t len) {
    if (!conn_handle || !buffer) return -1;
    auto* holder = reinterpret_cast<std::shared_ptr<j2me::bluetooth::SPPConnectionImpl>*>(conn_handle);
    if (!holder || !(*holder)) return -1;
    return (*holder)->write(buffer, len);
}

J2ME_API int j2me_core_bluetooth_btspp_available(uintptr_t conn_handle) {
    if (!conn_handle) return 0;
    auto* holder = reinterpret_cast<std::shared_ptr<j2me::bluetooth::SPPConnectionImpl>*>(conn_handle);
    if (!holder || !(*holder)) return 0;
    return (*holder)->available();
}

J2ME_API void j2me_core_bluetooth_btspp_close(uintptr_t conn_handle) {
    if (!conn_handle) return;
    auto* holder = reinterpret_cast<std::shared_ptr<j2me::bluetooth::SPPConnectionImpl>*>(conn_handle);
    if (holder) {
        if (*holder) (*holder)->close();
        delete holder;
    }
}

// BTL2CAP Server & Client
J2ME_API uintptr_t j2me_core_bluetooth_open_btl2cap_server(const char* url) {
    if (!url) return 0;
    auto server = j2me::bluetooth::openBtl2capServer(url);
    if (!server) return 0;
    auto* holder = new std::shared_ptr<j2me::bluetooth::L2CAPConnectionNotifier>(server);
    return reinterpret_cast<uintptr_t>(holder);
}

J2ME_API uintptr_t j2me_core_bluetooth_btl2cap_accept(uintptr_t server_handle, int timeout_ms) {
    if (!server_handle) return 0;
    auto* holder = reinterpret_cast<std::shared_ptr<j2me::bluetooth::L2CAPConnectionNotifier>*>(server_handle);
    if (!holder || !(*holder)) return 0;
    auto conn = (*holder)->acceptAndOpen(timeout_ms);
    if (!conn) return 0;
    auto* connHolder = new std::shared_ptr<j2me::bluetooth::L2CAPConnectionImpl>(conn);
    return reinterpret_cast<uintptr_t>(connHolder);
}

J2ME_API void j2me_core_bluetooth_btl2cap_server_close(uintptr_t server_handle) {
    if (!server_handle) return;
    auto* holder = reinterpret_cast<std::shared_ptr<j2me::bluetooth::L2CAPConnectionNotifier>*>(server_handle);
    if (holder) {
        if (*holder) (*holder)->close();
        delete holder;
    }
}

J2ME_API uintptr_t j2me_core_bluetooth_open_btl2cap_client(const char* url, int timeout_ms) {
    if (!url) return 0;
    auto client = j2me::bluetooth::openBtl2capClient(url, timeout_ms);
    if (!client) return 0;
    auto* connHolder = new std::shared_ptr<j2me::bluetooth::L2CAPConnectionImpl>(client);
    return reinterpret_cast<uintptr_t>(connHolder);
}

J2ME_API bool j2me_core_bluetooth_btl2cap_send(uintptr_t conn_handle, const uint8_t* data, size_t len) {
    if (!conn_handle || !data) return false;
    auto* holder = reinterpret_cast<std::shared_ptr<j2me::bluetooth::L2CAPConnectionImpl>*>(conn_handle);
    if (!holder || !(*holder)) return false;
    return (*holder)->send(data, len);
}

J2ME_API int j2me_core_bluetooth_btl2cap_receive(uintptr_t conn_handle, uint8_t* in_buf, size_t in_buf_len, int timeout_ms) {
    if (!conn_handle || !in_buf || in_buf_len == 0) return -1;
    auto* holder = reinterpret_cast<std::shared_ptr<j2me::bluetooth::L2CAPConnectionImpl>*>(conn_handle);
    if (!holder || !(*holder)) return -1;
    return (*holder)->receive(in_buf, in_buf_len, timeout_ms);
}

J2ME_API bool j2me_core_bluetooth_btl2cap_ready(uintptr_t conn_handle) {
    if (!conn_handle) return false;
    auto* holder = reinterpret_cast<std::shared_ptr<j2me::bluetooth::L2CAPConnectionImpl>*>(conn_handle);
    if (!holder || !(*holder)) return false;
    return (*holder)->ready();
}

J2ME_API void j2me_core_bluetooth_btl2cap_close(uintptr_t conn_handle) {
    if (!conn_handle) return;
    auto* holder = reinterpret_cast<std::shared_ptr<j2me::bluetooth::L2CAPConnectionImpl>*>(conn_handle);
    if (holder) {
        if (*holder) (*holder)->close();
        delete holder;
    }
}

// --- 23. VODAFONE VSCL & CARRIER OEM EXTENSIONS ---

J2ME_API uintptr_t j2me_core_vodafone_sprite_create(int num_palettes, int num_patterns) {
    auto* canvas = new universal_loader::oem::vodafone::VodafoneSpriteCanvas(num_palettes, num_patterns);
    return reinterpret_cast<uintptr_t>(canvas);
}

J2ME_API void j2me_core_vodafone_sprite_destroy(uintptr_t sprite_handle) {
    if (!sprite_handle) return;
    auto* canvas = reinterpret_cast<universal_loader::oem::vodafone::VodafoneSpriteCanvas*>(sprite_handle);
    delete canvas;
}

J2ME_API void j2me_core_vodafone_sprite_set_palette(uintptr_t sprite_handle, int index, uint32_t color) {
    if (!sprite_handle) return;
    auto* canvas = reinterpret_cast<universal_loader::oem::vodafone::VodafoneSpriteCanvas*>(sprite_handle);
    canvas->setPalette(index, color);
}

J2ME_API uint32_t j2me_core_vodafone_sprite_get_palette(uintptr_t sprite_handle, int index) {
    if (!sprite_handle) return 0;
    auto* canvas = reinterpret_cast<universal_loader::oem::vodafone::VodafoneSpriteCanvas*>(sprite_handle);
    return canvas->getPalette(index);
}

J2ME_API void j2me_core_vodafone_sprite_set_pattern(uintptr_t sprite_handle, int index, const uint8_t* data, size_t length) {
    if (!sprite_handle || !data) return;
    auto* canvas = reinterpret_cast<universal_loader::oem::vodafone::VodafoneSpriteCanvas*>(sprite_handle);
    canvas->setPattern(index, data, length);
}

J2ME_API int16_t j2me_core_vodafone_sprite_create_command(uintptr_t sprite_handle, int offset, bool transparent, int rotation, bool upside_down, bool rightside_left, int pattern_no) {
    if (!sprite_handle) return -1;
    auto* canvas = reinterpret_cast<universal_loader::oem::vodafone::VodafoneSpriteCanvas*>(sprite_handle);
    return canvas->createCharacterCommand(offset, transparent, rotation, upside_down, rightside_left, pattern_no);
}

J2ME_API void j2me_core_vodafone_sprite_create_framebuffer(uintptr_t sprite_handle, int fw, int fh) {
    if (!sprite_handle) return;
    auto* canvas = reinterpret_cast<universal_loader::oem::vodafone::VodafoneSpriteCanvas*>(sprite_handle);
    canvas->createFrameBuffer(fw, fh);
}

J2ME_API void j2me_core_vodafone_sprite_dispose_framebuffer(uintptr_t sprite_handle) {
    if (!sprite_handle) return;
    auto* canvas = reinterpret_cast<universal_loader::oem::vodafone::VodafoneSpriteCanvas*>(sprite_handle);
    canvas->disposeFrameBuffer();
}

J2ME_API void j2me_core_vodafone_sprite_draw_char(uintptr_t sprite_handle, int16_t command_id, int16_t x, int16_t y) {
    if (!sprite_handle) return;
    auto* canvas = reinterpret_cast<universal_loader::oem::vodafone::VodafoneSpriteCanvas*>(sprite_handle);
    canvas->drawSpriteChar(command_id, x, y);
}

J2ME_API void j2me_core_vodafone_sprite_copy_area(uintptr_t sprite_handle, int sx, int sy, int fw, int fh, int tx, int ty) {
    if (!sprite_handle) return;
    auto* canvas = reinterpret_cast<universal_loader::oem::vodafone::VodafoneSpriteCanvas*>(sprite_handle);
    canvas->copyArea(sx, sy, fw, fh, tx, ty);
}

J2ME_API void j2me_core_vodafone_sprite_draw_framebuffer(uintptr_t sprite_handle, uint32_t* target_pixels, int target_w, int target_h, int tx, int ty) {
    if (!sprite_handle) return;
    auto* canvas = reinterpret_cast<universal_loader::oem::vodafone::VodafoneSpriteCanvas*>(sprite_handle);
    canvas->drawFrameBuffer(target_pixels, target_w, target_h, tx, ty);
}

J2ME_API const uint32_t* j2me_core_vodafone_sprite_get_framebuffer(uintptr_t sprite_handle, int* out_w, int* out_h) {
    if (!sprite_handle) return nullptr;
    auto* canvas = reinterpret_cast<universal_loader::oem::vodafone::VodafoneSpriteCanvas*>(sprite_handle);
    if (out_w) *out_w = canvas->getFrameBufferWidth();
    if (out_h) *out_h = canvas->getFrameBufferHeight();
    return canvas->getFrameBuffer();
}

J2ME_API int j2me_core_vodafone_device_get_state(int device_no) {
    return universal_loader::oem::vodafone::VodafoneDeviceControl::instance().getDeviceState(device_no);
}

J2ME_API bool j2me_core_vodafone_device_is_active(int device_no) {
    return universal_loader::oem::vodafone::VodafoneDeviceControl::instance().isDeviceActive(device_no);
}

J2ME_API bool j2me_core_vodafone_device_set_active(int device_no, bool active) {
    return universal_loader::oem::vodafone::VodafoneDeviceControl::instance().setDeviceActive(device_no, active);
}

J2ME_API void j2me_core_vodafone_device_blink(int lighting_ms, int extinction_ms, int repeat) {
    universal_loader::oem::vodafone::VodafoneDeviceControl::instance().blink(lighting_ms, extinction_ms, repeat);
}

J2ME_API uint32_t j2me_core_vodafone_device_get_keystates(void) {
    return universal_loader::oem::vodafone::VodafoneDeviceControl::instance().getKeyStatesVodafone();
}

J2ME_API void j2me_core_vodafone_device_set_keystates_mask(uint32_t mask) {
    universal_loader::oem::vodafone::VodafoneDeviceControl::instance().setKeyStatesMask(mask);
}

J2ME_API void j2me_core_vodafone_device_key_event(int key_code, bool is_down) {
    if (is_down) {
        universal_loader::oem::vodafone::VodafoneDeviceControl::instance().setKeyDown(key_code);
    } else {
        universal_loader::oem::vodafone::VodafoneDeviceControl::instance().setKeyUp(key_code);
    }
}

J2ME_API int j2me_core_vodafone_encode_offscreen(const uint32_t* pixels, int src_w, int src_h, int x, int y, int w, int h, int format, uint8_t* out_buf, size_t out_cap) {
    if (!pixels || !out_buf || out_cap == 0) return 0;
    auto png = universal_loader::oem::vodafone::VodafoneImageEncoder::encodeOffscreen(pixels, src_w, src_h, x, y, w, h, format);
    if (png.empty() || png.size() > out_cap) return 0;
    std::memcpy(out_buf, png.data(), png.size());
    return static_cast<int>(png.size());
}

J2ME_API int j2me_core_carrier_kddi_get_keystate(bool eight_directions) {
    return universal_loader::oem::carrier::KDDISystem::getKeyState(eight_directions);
}

J2ME_API void j2me_core_carrier_motorola_funlight_set_color(int region, uint32_t rgb) {
    universal_loader::oem::carrier::MotorolaFunLight::instance().setColor(region, rgb);
}

J2ME_API uint32_t j2me_core_carrier_motorola_funlight_get_color(int region) {
    return universal_loader::oem::carrier::MotorolaFunLight::instance().getColor(region);
}

J2ME_API void j2me_core_carrier_sony_accel_set(float x, float y, float z) {
    universal_loader::oem::carrier::SonyEricssonAccelerometer::instance().setAcceleration(x, y, z);
}

J2ME_API void j2me_core_carrier_sony_accel_get(float* out_x, float* out_y, float* out_z) {
    if (!out_x || !out_y || !out_z) return;
    universal_loader::oem::carrier::SonyEricssonAccelerometer::instance().getAcceleration(*out_x, *out_y, *out_z);
}

J2ME_API void j2me_core_carrier_sprint_play_clip(const char* name, int loop_count) {
    if (!name) return;
    universal_loader::oem::carrier::SprintPCSPlayer::instance().playClip(name, loop_count);
}

J2ME_API void j2me_core_carrier_sprint_stop(void) {
    universal_loader::oem::carrier::SprintPCSPlayer::instance().stop();
}

J2ME_API bool j2me_core_carrier_sprint_is_playing(void) {
    return universal_loader::oem::carrier::SprintPCSPlayer::instance().isPlaying();
}

// --- 24. JSR-179 MOBILE LOCATION API ---

J2ME_API int j2me_core_location_provider_get_state(void) {
    return universal_loader::location::LocationProvider::getInstance()->getState();
}

J2ME_API void j2me_core_location_provider_set_state(int state) {
    universal_loader::location::LocationProvider::setHostProviderState(state);
}

J2ME_API void j2me_core_location_provider_update_host_location(double lat, double lon, float alt, float speed, float course, float horiz_acc, float vert_acc) {
    universal_loader::location::LocationProvider::updateHostLocation(lat, lon, alt, speed, course, horiz_acc, vert_acc);
}

J2ME_API bool j2me_core_location_provider_get_last_known(double* out_lat, double* out_lon, float* out_alt, float* out_speed, float* out_course, int64_t* out_timestamp) {
    auto loc = universal_loader::location::LocationProvider::getLastKnownLocation();
    if (!loc.isValid()) return false;
    const auto& coords = loc.getQualifiedCoordinates();
    if (out_lat) *out_lat = coords.getLatitude();
    if (out_lon) *out_lon = coords.getLongitude();
    if (out_alt) *out_alt = coords.getAltitude();
    if (out_speed) *out_speed = loc.getSpeed();
    if (out_course) *out_course = loc.getCourse();
    if (out_timestamp) *out_timestamp = loc.getTimestamp();
    return true;
}

J2ME_API int j2me_core_location_provider_get_nmea(char* out_buf, size_t cap) {
    if (!out_buf || cap == 0) return 0;
    auto loc = universal_loader::location::LocationProvider::getLastKnownLocation();
    std::string nmea = loc.getExtraInfo("application/X-jsr179-location-nmea");
    if (nmea.empty() || nmea.size() >= cap) return 0;
    std::memcpy(out_buf, nmea.c_str(), nmea.size() + 1);
    return static_cast<int>(nmea.size());
}

J2ME_API float j2me_core_location_coordinates_distance(double lat1, double lon1, double lat2, double lon2) {
    try {
        universal_loader::location::Coordinates c1(lat1, lon1);
        universal_loader::location::Coordinates c2(lat2, lon2);
        return c1.distance(c2);
    } catch (...) {
        return std::numeric_limits<float>::quiet_NaN();
    }
}

J2ME_API float j2me_core_location_coordinates_azimuth(double lat1, double lon1, double lat2, double lon2) {
    try {
        universal_loader::location::Coordinates c1(lat1, lon1);
        universal_loader::location::Coordinates c2(lat2, lon2);
        return c1.azimuthTo(c2);
    } catch (...) {
        return std::numeric_limits<float>::quiet_NaN();
    }
}

J2ME_API bool j2me_core_location_coordinates_convert_to_string(double coord, int output_type, char* out_buf, size_t cap) {
    if (!out_buf || cap == 0) return false;
    try {
        std::string s = universal_loader::location::Coordinates::convert(coord, output_type);
        if (s.size() >= cap) return false;
        std::memcpy(out_buf, s.c_str(), s.size() + 1);
        return true;
    } catch (...) {
        return false;
    }
}

J2ME_API double j2me_core_location_coordinates_convert_from_string(const char* str) {
    if (!str) return std::numeric_limits<double>::quiet_NaN();
    try {
        return universal_loader::location::Coordinates::convert(str);
    } catch (...) {
        return std::numeric_limits<double>::quiet_NaN();
    }
}

J2ME_API void j2me_core_location_orientation_get(float* out_azimuth, bool* out_is_magnetic, float* out_pitch, float* out_roll) {
    auto o = universal_loader::location::Orientation::getOrientation();
    if (out_azimuth) *out_azimuth = o.getCompassAzimuth();
    if (out_is_magnetic) *out_is_magnetic = o.isOrientationMagnetic();
    if (out_pitch) *out_pitch = o.getPitch();
    if (out_roll) *out_roll = o.getRoll();
}

J2ME_API void j2me_core_location_orientation_set(float azimuth, bool is_magnetic, float pitch, float roll) {
    universal_loader::location::Orientation::setGlobalOrientation(azimuth, is_magnetic, pitch, roll);
}

J2ME_API bool j2me_core_location_landmark_store_create(const char* store_name) {
    if (!store_name) return false;
    try {
        universal_loader::location::LandmarkStore::createLandmarkStore(store_name);
        return true;
    } catch (...) {
        return false;
    }
}

J2ME_API bool j2me_core_location_landmark_store_delete(const char* store_name) {
    if (!store_name) return false;
    try {
        universal_loader::location::LandmarkStore::deleteLandmarkStore(store_name);
        return true;
    } catch (...) {
        return false;
    }
}

J2ME_API void j2me_core_location_landmark_store_add_landmark(const char* store_name, const char* name, const char* desc, double lat, double lon, float alt, const char* category) {
    if (!store_name || !name) return;
    auto* store = universal_loader::location::LandmarkStore::getInstance(store_name);
    if (!store) return;
    auto coords = std::make_shared<universal_loader::location::QualifiedCoordinates>(lat, lon, alt);
    universal_loader::location::Landmark lm(name, desc ? desc : "", coords, nullptr);
    store->addLandmark(lm, category ? category : "");
}

J2ME_API int j2me_core_location_landmark_store_get_count(const char* store_name, const char* category) {
    if (!store_name) return 0;
    auto* store = universal_loader::location::LandmarkStore::getInstance(store_name);
    if (!store) return 0;
    auto lms = store->getLandmarks(category ? category : "");
    return static_cast<int>(lms.size());
}

// --- 25. JSR-256 MOBILE SENSOR API ---

J2ME_API int j2me_core_sensor_get_count(void) {
    return static_cast<int>(universal_loader::sensor::SensorManager::instance().getAllSensors().size());
}

J2ME_API bool j2me_core_sensor_get_url(int index, char* out_buf, size_t cap) {
    if (!out_buf || cap == 0) return false;
    auto sensors = universal_loader::sensor::SensorManager::instance().getAllSensors();
    if (index < 0 || static_cast<size_t>(index) >= sensors.size()) return false;
    std::string url = sensors[index].getUrl();
    if (url.size() >= cap) return false;
    std::memcpy(out_buf, url.c_str(), url.size() + 1);
    return true;
}

J2ME_API bool j2me_core_sensor_get_quantity(int index, char* out_buf, size_t cap) {
    if (!out_buf || cap == 0) return false;
    auto sensors = universal_loader::sensor::SensorManager::instance().getAllSensors();
    if (index < 0 || static_cast<size_t>(index) >= sensors.size()) return false;
    std::string qty = sensors[index].getQuantity();
    if (qty.size() >= cap) return false;
    std::memcpy(out_buf, qty.c_str(), qty.size() + 1);
    return true;
}

J2ME_API bool j2me_core_sensor_get_context_type(int index, char* out_buf, size_t cap) {
    if (!out_buf || cap == 0) return false;
    auto sensors = universal_loader::sensor::SensorManager::instance().getAllSensors();
    if (index < 0 || static_cast<size_t>(index) >= sensors.size()) return false;
    std::string ctx = sensors[index].getContextType();
    if (ctx.size() >= cap) return false;
    std::memcpy(out_buf, ctx.c_str(), ctx.size() + 1);
    return true;
}

J2ME_API int j2me_core_sensor_find(const char* quantity, const char* context_type, int* out_indices, int max_count) {
    if (!out_indices || max_count <= 0) return 0;
    auto allSensors = universal_loader::sensor::SensorManager::instance().getAllSensors();
    std::string q = quantity ? quantity : "";
    std::string c = context_type ? context_type : "";
    int written = 0;
    for (size_t i = 0; i < allSensors.size() && written < max_count; ++i) {
        bool matchQty = q.empty() || (allSensors[i].getQuantity() == q);
        bool matchCtx = c.empty() || (allSensors[i].getContextType() == c);
        if (matchQty && matchCtx) {
            out_indices[written++] = static_cast<int>(i);
        }
    }
    return written;
}

J2ME_API uintptr_t j2me_core_sensor_open(const char* url) {
    if (!url) return 0;
    try {
        auto conn = universal_loader::sensor::SensorManager::instance().openSensor(url);
        if (!conn) return 0;
        auto* holder = new std::shared_ptr<universal_loader::sensor::SensorConnection>(conn);
        return reinterpret_cast<uintptr_t>(holder);
    } catch (...) {
        return 0;
    }
}

J2ME_API void j2me_core_sensor_close(uintptr_t handle) {
    if (!handle) return;
    auto* holder = reinterpret_cast<std::shared_ptr<universal_loader::sensor::SensorConnection>*>(handle);
    if (holder && *holder) {
        (*holder)->close();
    }
    delete holder;
}

J2ME_API int j2me_core_sensor_get_state(uintptr_t handle) {
    if (!handle) return universal_loader::sensor::SENSOR_STATE_CLOSED;
    auto* holder = reinterpret_cast<std::shared_ptr<universal_loader::sensor::SensorConnection>*>(handle);
    if (!holder || !*holder) return universal_loader::sensor::SENSOR_STATE_CLOSED;
    return (*holder)->getState();
}

J2ME_API int j2me_core_sensor_get_channel_count(uintptr_t handle) {
    if (!handle) return 0;
    auto* holder = reinterpret_cast<std::shared_ptr<universal_loader::sensor::SensorConnection>*>(handle);
    if (!holder || !*holder) return 0;
    return static_cast<int>((*holder)->getChannelCount());
}

J2ME_API bool j2me_core_sensor_get_channel_name(uintptr_t handle, int channel_index, char* out_buf, size_t cap) {
    if (!handle || !out_buf || cap == 0 || channel_index < 0) return false;
    auto* holder = reinterpret_cast<std::shared_ptr<universal_loader::sensor::SensorConnection>*>(handle);
    if (!holder || !*holder) return false;
    auto* ch = (*holder)->getChannel(static_cast<size_t>(channel_index));
    if (!ch) return false;
    std::string name = ch->getChannelInfo().getName();
    if (name.size() >= cap) return false;
    std::memcpy(out_buf, name.c_str(), name.size() + 1);
    return true;
}

J2ME_API int j2me_core_sensor_get_data(uintptr_t handle, int channel_index, double* out_buf, int max_samples) {
    if (!handle || !out_buf || max_samples <= 0 || channel_index < 0) return 0;
    auto* holder = reinterpret_cast<std::shared_ptr<universal_loader::sensor::SensorConnection>*>(handle);
    if (!holder || !*holder) return 0;
    try {
        auto dataList = (*holder)->getData(max_samples);
        if (static_cast<size_t>(channel_index) >= dataList.size()) return 0;
        const auto& d = dataList[channel_index];
        const auto& chInfo = d.getChannelInfo();
        int written = 0;
        if (chInfo.getDataType() == universal_loader::sensor::CHANNEL_TYPE_DOUBLE) {
            const auto& vals = d.getDoubleValues();
            written = std::min(max_samples, static_cast<int>(vals.size()));
            for (int i = 0; i < written; ++i) {
                out_buf[i] = vals[i];
            }
        } else if (chInfo.getDataType() == universal_loader::sensor::CHANNEL_TYPE_INT) {
            const auto& vals = d.getIntValues();
            written = std::min(max_samples, static_cast<int>(vals.size()));
            double scale = chInfo.getScale() > 0 ? static_cast<double>(chInfo.getScale()) : 1.0;
            for (int i = 0; i < written; ++i) {
                out_buf[i] = static_cast<double>(vals[i]) / scale;
            }
        }
        return written;
    } catch (...) {
        return 0;
    }
}

J2ME_API void j2me_core_sensor_update_accelerometer(double x, double y, double z) {
    universal_loader::sensor::SensorManager::instance().updateAccelerometer(x, y, z);
}

J2ME_API void j2me_core_sensor_update_ambient_light(double lux) {
    universal_loader::sensor::SensorManager::instance().updateAmbientLight(lux);
}

J2ME_API void j2me_core_sensor_update_magnetic_field(double x, double y, double z) {
    universal_loader::sensor::SensorManager::instance().updateMagneticField(x, y, z);
}

J2ME_API void j2me_core_sensor_update_orientation(double azimuth, double pitch, double roll) {
    universal_loader::sensor::SensorManager::instance().updateOrientation(azimuth, pitch, roll);
}

J2ME_API void j2me_core_sensor_update_temperature(double celsius) {
    universal_loader::sensor::SensorManager::instance().updateTemperature(celsius);
}

// --- 26. JSR-75 PIM (PERSONAL INFORMATION MANAGEMENT) ---
J2ME_API void j2me_core_pim_init(const char* sandbox_dir) {
    universal_loader::pim::PIMManager::getInstance().init(sandbox_dir ? sandbox_dir : "");
}

J2ME_API int j2me_core_pim_list_count(int pim_list_type) {
    return static_cast<int>(universal_loader::pim::PIMManager::getInstance().listPIMLists(pim_list_type).size());
}

J2ME_API bool j2me_core_pim_list_get_name(int pim_list_type, int index, char* out_buf, size_t cap) {
    if (!out_buf || cap == 0 || index < 0) return false;
    auto lists = universal_loader::pim::PIMManager::getInstance().listPIMLists(pim_list_type);
    if (index >= static_cast<int>(lists.size())) return false;
    const auto& name = lists[index];
    if (name.size() >= cap) return false;
    std::memcpy(out_buf, name.c_str(), name.size() + 1);
    return true;
}

J2ME_API uintptr_t j2me_core_pim_open_list(int pim_list_type, int mode, const char* name) {
    auto list = universal_loader::pim::PIMManager::getInstance().openPIMList(pim_list_type, mode, name ? name : "");
    return reinterpret_cast<uintptr_t>(list.get());
}

J2ME_API void j2me_core_pim_close_list(uintptr_t list_handle) {
    if (!list_handle) return;
    auto* list = reinterpret_cast<universal_loader::pim::PIMList*>(list_handle);
    list->close();
}

J2ME_API int j2me_core_pim_list_get_item_count(uintptr_t list_handle) {
    if (!list_handle) return 0;
    auto* list = reinterpret_cast<universal_loader::pim::PIMList*>(list_handle);
    return static_cast<int>(list->items().size());
}

J2ME_API uintptr_t j2me_core_pim_list_get_item(uintptr_t list_handle, int index) {
    if (!list_handle || index < 0) return 0;
    auto* list = reinterpret_cast<universal_loader::pim::PIMList*>(list_handle);
    auto items = list->items();
    if (index >= static_cast<int>(items.size())) return 0;
    return reinterpret_cast<uintptr_t>(items[index].get());
}

J2ME_API bool j2me_core_pim_list_remove_item(uintptr_t list_handle, uintptr_t item_handle) {
    if (!list_handle || !item_handle) return false;
    auto* list = reinterpret_cast<universal_loader::pim::PIMList*>(list_handle);
    auto* target = reinterpret_cast<universal_loader::pim::PIMItem*>(item_handle);
    auto items = list->items();
    for (const auto& it : items) {
        if (it.get() == target) {
            list->removeItem(it);
            return true;
        }
    }
    return false;
}

J2ME_API uintptr_t j2me_core_pim_contact_create(uintptr_t list_handle) {
    if (!list_handle) return 0;
    auto* cList = dynamic_cast<universal_loader::pim::ContactList*>(reinterpret_cast<universal_loader::pim::PIMList*>(list_handle));
    if (!cList) return 0;
    auto c = cList->createContact();
    return reinterpret_cast<uintptr_t>(c.get());
}

J2ME_API bool j2me_core_pim_contact_set_name(uintptr_t item_handle, const char* family, const char* given, const char* other, const char* prefix, const char* suffix) {
    if (!item_handle) return false;
    auto* c = reinterpret_cast<universal_loader::pim::Contact*>(item_handle);
    c->setName(family ? family : "", given ? given : "", other ? other : "", prefix ? prefix : "", suffix ? suffix : "");
    return true;
}

J2ME_API bool j2me_core_pim_contact_get_formatted_name(uintptr_t item_handle, char* out_buf, size_t cap) {
    if (!item_handle || !out_buf || cap == 0) return false;
    auto* c = reinterpret_cast<universal_loader::pim::Contact*>(item_handle);
    std::string fn = c->getFormattedName();
    if (fn.size() >= cap) return false;
    std::memcpy(out_buf, fn.c_str(), fn.size() + 1);
    return true;
}

J2ME_API bool j2me_core_pim_contact_add_tel(uintptr_t item_handle, int attributes, const char* number) {
    if (!item_handle || !number) return false;
    auto* c = reinterpret_cast<universal_loader::pim::Contact*>(item_handle);
    c->addString(universal_loader::pim::CONTACT_TEL, attributes, number);
    return true;
}

J2ME_API int j2me_core_pim_contact_get_tel_count(uintptr_t item_handle) {
    if (!item_handle) return 0;
    auto* c = reinterpret_cast<universal_loader::pim::Contact*>(item_handle);
    return c->countValues(universal_loader::pim::CONTACT_TEL);
}

J2ME_API bool j2me_core_pim_contact_get_tel(uintptr_t item_handle, int index, char* out_buf, size_t cap, int* out_attributes) {
    if (!item_handle || !out_buf || cap == 0 || index < 0) return false;
    auto* c = reinterpret_cast<universal_loader::pim::Contact*>(item_handle);
    if (index >= c->countValues(universal_loader::pim::CONTACT_TEL)) return false;
    std::string tel = c->getString(universal_loader::pim::CONTACT_TEL, index);
    if (tel.size() >= cap) return false;
    std::memcpy(out_buf, tel.c_str(), tel.size() + 1);
    if (out_attributes) *out_attributes = c->getAttributes(universal_loader::pim::CONTACT_TEL, index);
    return true;
}

J2ME_API bool j2me_core_pim_contact_add_email(uintptr_t item_handle, int attributes, const char* email) {
    if (!item_handle || !email) return false;
    auto* c = reinterpret_cast<universal_loader::pim::Contact*>(item_handle);
    c->addString(universal_loader::pim::CONTACT_EMAIL, attributes, email);
    return true;
}

J2ME_API int j2me_core_pim_contact_get_email_count(uintptr_t item_handle) {
    if (!item_handle) return 0;
    auto* c = reinterpret_cast<universal_loader::pim::Contact*>(item_handle);
    return c->countValues(universal_loader::pim::CONTACT_EMAIL);
}

J2ME_API bool j2me_core_pim_contact_get_email(uintptr_t item_handle, int index, char* out_buf, size_t cap, int* out_attributes) {
    if (!item_handle || !out_buf || cap == 0 || index < 0) return false;
    auto* c = reinterpret_cast<universal_loader::pim::Contact*>(item_handle);
    if (index >= c->countValues(universal_loader::pim::CONTACT_EMAIL)) return false;
    std::string em = c->getString(universal_loader::pim::CONTACT_EMAIL, index);
    if (em.size() >= cap) return false;
    std::memcpy(out_buf, em.c_str(), em.size() + 1);
    if (out_attributes) *out_attributes = c->getAttributes(universal_loader::pim::CONTACT_EMAIL, index);
    return true;
}

J2ME_API bool j2me_core_pim_contact_set_address(uintptr_t item_handle, int attributes, const char* street, const char* locality, const char* region, const char* postal_code, const char* country) {
    if (!item_handle) return false;
    auto* c = reinterpret_cast<universal_loader::pim::Contact*>(item_handle);
    c->setAddress(0, attributes, street ? street : "", locality ? locality : "", region ? region : "", postal_code ? postal_code : "", country ? country : "");
    return true;
}

J2ME_API uintptr_t j2me_core_pim_event_create(uintptr_t list_handle) {
    if (!list_handle) return 0;
    auto* eList = dynamic_cast<universal_loader::pim::EventList*>(reinterpret_cast<universal_loader::pim::PIMList*>(list_handle));
    if (!eList) return 0;
    auto ev = eList->createEvent();
    return reinterpret_cast<uintptr_t>(ev.get());
}

J2ME_API bool j2me_core_pim_event_set_details(uintptr_t item_handle, const char* summary, const char* location, int64_t start_ms, int64_t end_ms, int alarm_sec) {
    if (!item_handle) return false;
    auto* ev = reinterpret_cast<universal_loader::pim::Event*>(item_handle);
    if (summary) {
        if (ev->countValues(universal_loader::pim::EVENT_SUMMARY) == 0)
            ev->addString(universal_loader::pim::EVENT_SUMMARY, universal_loader::pim::ATTR_NONE, summary);
        else
            ev->setString(universal_loader::pim::EVENT_SUMMARY, 0, universal_loader::pim::ATTR_NONE, summary);
    }
    if (location) {
        if (ev->countValues(universal_loader::pim::EVENT_LOCATION) == 0)
            ev->addString(universal_loader::pim::EVENT_LOCATION, universal_loader::pim::ATTR_NONE, location);
        else
            ev->setString(universal_loader::pim::EVENT_LOCATION, 0, universal_loader::pim::ATTR_NONE, location);
    }
    if (start_ms > 0) {
        if (ev->countValues(universal_loader::pim::EVENT_START) == 0)
            ev->addDate(universal_loader::pim::EVENT_START, universal_loader::pim::ATTR_NONE, start_ms);
        else
            ev->setDate(universal_loader::pim::EVENT_START, 0, universal_loader::pim::ATTR_NONE, start_ms);
    }
    if (end_ms > 0) {
        if (ev->countValues(universal_loader::pim::EVENT_END) == 0)
            ev->addDate(universal_loader::pim::EVENT_END, universal_loader::pim::ATTR_NONE, end_ms);
        else
            ev->setDate(universal_loader::pim::EVENT_END, 0, universal_loader::pim::ATTR_NONE, end_ms);
    }
    if (alarm_sec >= 0) {
        if (ev->countValues(universal_loader::pim::EVENT_ALARM) == 0)
            ev->addInt(universal_loader::pim::EVENT_ALARM, universal_loader::pim::ATTR_NONE, alarm_sec);
        else
            ev->setInt(universal_loader::pim::EVENT_ALARM, 0, universal_loader::pim::ATTR_NONE, alarm_sec);
    }
    return true;
}

J2ME_API bool j2me_core_pim_event_get_details(uintptr_t item_handle, char* out_summary, size_t summary_cap, char* out_location, size_t loc_cap, int64_t* out_start_ms, int64_t* out_end_ms) {
    if (!item_handle) return false;
    auto* ev = reinterpret_cast<universal_loader::pim::Event*>(item_handle);
    if (out_summary && summary_cap > 0) {
        std::string sum = ev->getString(universal_loader::pim::EVENT_SUMMARY, 0);
        if (sum.size() < summary_cap) std::memcpy(out_summary, sum.c_str(), sum.size() + 1);
    }
    if (out_location && loc_cap > 0) {
        std::string loc = ev->getString(universal_loader::pim::EVENT_LOCATION, 0);
        if (loc.size() < loc_cap) std::memcpy(out_location, loc.c_str(), loc.size() + 1);
    }
    if (out_start_ms) *out_start_ms = ev->getDate(universal_loader::pim::EVENT_START, 0);
    if (out_end_ms) *out_end_ms = ev->getDate(universal_loader::pim::EVENT_END, 0);
    return true;
}

J2ME_API bool j2me_core_pim_event_set_repeat_rule(uintptr_t item_handle, int frequency, int interval, int count, int64_t end_ms) {
    if (!item_handle) return false;
    auto* ev = reinterpret_cast<universal_loader::pim::Event*>(item_handle);
    universal_loader::pim::RepeatRule rule;
    rule.setInt(universal_loader::pim::REPEAT_FREQUENCY, frequency);
    rule.setInt(universal_loader::pim::REPEAT_INTERVAL, interval);
    if (count > 0) rule.setInt(universal_loader::pim::REPEAT_COUNT, count);
    if (end_ms > 0) rule.setDate(universal_loader::pim::REPEAT_END, end_ms);
    ev->setRepeatRule(rule);
    return true;
}

J2ME_API uintptr_t j2me_core_pim_todo_create(uintptr_t list_handle) {
    if (!list_handle) return 0;
    auto* tList = dynamic_cast<universal_loader::pim::ToDoList*>(reinterpret_cast<universal_loader::pim::PIMList*>(list_handle));
    if (!tList) return 0;
    auto td = tList->createToDo();
    return reinterpret_cast<uintptr_t>(td.get());
}

J2ME_API bool j2me_core_pim_todo_set_details(uintptr_t item_handle, const char* summary, int priority, bool completed, int64_t due_ms, int64_t completion_ms) {
    if (!item_handle) return false;
    auto* td = reinterpret_cast<universal_loader::pim::ToDo*>(item_handle);
    if (summary) {
        if (td->countValues(universal_loader::pim::TODO_SUMMARY) == 0)
            td->addString(universal_loader::pim::TODO_SUMMARY, universal_loader::pim::ATTR_NONE, summary);
        else
            td->setString(universal_loader::pim::TODO_SUMMARY, 0, universal_loader::pim::ATTR_NONE, summary);
    }
    if (priority >= 0) {
        if (td->countValues(universal_loader::pim::TODO_PRIORITY) == 0)
            td->addInt(universal_loader::pim::TODO_PRIORITY, universal_loader::pim::ATTR_NONE, priority);
        else
            td->setInt(universal_loader::pim::TODO_PRIORITY, 0, universal_loader::pim::ATTR_NONE, priority);
    }
    if (td->countValues(universal_loader::pim::TODO_COMPLETED) == 0)
        td->addBoolean(universal_loader::pim::TODO_COMPLETED, universal_loader::pim::ATTR_NONE, completed);
    else
        td->setBoolean(universal_loader::pim::TODO_COMPLETED, 0, universal_loader::pim::ATTR_NONE, completed);

    if (due_ms > 0) {
        if (td->countValues(universal_loader::pim::TODO_DUE) == 0)
            td->addDate(universal_loader::pim::TODO_DUE, universal_loader::pim::ATTR_NONE, due_ms);
        else
            td->setDate(universal_loader::pim::TODO_DUE, 0, universal_loader::pim::ATTR_NONE, due_ms);
    }
    if (completion_ms > 0) {
        if (td->countValues(universal_loader::pim::TODO_COMPLETION_DATE) == 0)
            td->addDate(universal_loader::pim::TODO_COMPLETION_DATE, universal_loader::pim::ATTR_NONE, completion_ms);
        else
            td->setDate(universal_loader::pim::TODO_COMPLETION_DATE, 0, universal_loader::pim::ATTR_NONE, completion_ms);
    }
    return true;
}

J2ME_API bool j2me_core_pim_todo_get_details(uintptr_t item_handle, char* out_summary, size_t summary_cap, int* out_priority, bool* out_completed, int64_t* out_due_ms) {
    if (!item_handle) return false;
    auto* td = reinterpret_cast<universal_loader::pim::ToDo*>(item_handle);
    if (out_summary && summary_cap > 0) {
        std::string sum = td->getString(universal_loader::pim::TODO_SUMMARY, 0);
        if (sum.size() < summary_cap) std::memcpy(out_summary, sum.c_str(), sum.size() + 1);
    }
    if (out_priority) *out_priority = td->getInt(universal_loader::pim::TODO_PRIORITY, 0);
    if (out_completed) *out_completed = td->getBoolean(universal_loader::pim::TODO_COMPLETED, 0);
    if (out_due_ms) *out_due_ms = td->getDate(universal_loader::pim::TODO_DUE, 0);
    return true;
}

J2ME_API void j2me_core_pim_item_commit(uintptr_t item_handle) {
    if (!item_handle) return;
    auto* it = reinterpret_cast<universal_loader::pim::PIMItem*>(item_handle);
    it->commit();
}

J2ME_API void j2me_core_pim_item_add_category(uintptr_t item_handle, const char* category) {
    if (!item_handle || !category) return;
    auto* it = reinterpret_cast<universal_loader::pim::PIMItem*>(item_handle);
    it->addToCategory(category);
}

J2ME_API int j2me_core_pim_item_get_category_count(uintptr_t item_handle) {
    if (!item_handle) return 0;
    auto* it = reinterpret_cast<universal_loader::pim::PIMItem*>(item_handle);
    return static_cast<int>(it->getCategories().size());
}

J2ME_API bool j2me_core_pim_item_get_category(uintptr_t item_handle, int index, char* out_buf, size_t cap) {
    if (!item_handle || !out_buf || cap == 0 || index < 0) return false;
    auto* it = reinterpret_cast<universal_loader::pim::PIMItem*>(item_handle);
    const auto& cats = it->getCategories();
    if (index >= static_cast<int>(cats.size())) return false;
    const auto& c = cats[index];
    if (c.size() >= cap) return false;
    std::memcpy(out_buf, c.c_str(), c.size() + 1);
    return true;
}

J2ME_API int j2me_core_pim_export_serial(uintptr_t item_handle, const char* format, char* out_buf, size_t cap) {
    if (!item_handle || !out_buf || cap == 0) return 0;
    auto* item = reinterpret_cast<universal_loader::pim::PIMItem*>(item_handle);
    std::string text;
    if (item->getType() == universal_loader::pim::PIM_CONTACT_LIST) {
        text = reinterpret_cast<universal_loader::pim::Contact*>(item)->toVCard();
    } else if (item->getType() == universal_loader::pim::PIM_EVENT_LIST) {
        text = reinterpret_cast<universal_loader::pim::Event*>(item)->toVCalendar();
    } else if (item->getType() == universal_loader::pim::PIM_TODO_LIST) {
        text = reinterpret_cast<universal_loader::pim::ToDo*>(item)->toVCalendar();
    }
    if (text.size() >= cap) return 0;
    std::memcpy(out_buf, text.c_str(), text.size() + 1);
    return static_cast<int>(text.size());
}

J2ME_API uintptr_t j2me_core_pim_import_serial(uintptr_t list_handle, const char* data) {
    if (!data) return 0;
    std::string strData(data);
    universal_loader::pim::PIMList* list = reinterpret_cast<universal_loader::pim::PIMList*>(list_handle);
    int type = list ? list->getType() : universal_loader::pim::PIM_CONTACT_LIST;
    auto items = universal_loader::pim::PIMManager::getInstance().fromSerialFormat(type, strData, "UTF-8");
    if (items.empty()) return 0;
    auto item = items.front();
    if (list) {
        list->addItem(item);
    }
    return reinterpret_cast<uintptr_t>(item.get());
}

J2ME_API void j2me_core_pim_save_all(void) {
    universal_loader::pim::PIMManager::getInstance().saveAll();
}

// --- 27. JSR-234 AMMS (ADVANCED MULTIMEDIA SUPPLEMENTS) ---
J2ME_API void j2me_core_amms_spectator_set_location(int x, int y, int z) {
    universal_loader::amms::GlobalManager::getInstance().getSpectator().getLocation().setCartesian(x, y, z);
}

J2ME_API void j2me_core_amms_spectator_get_location(int* out_x, int* out_y, int* out_z) {
    if (!out_x || !out_y || !out_z) return;
    universal_loader::amms::GlobalManager::getInstance().getSpectator().getLocation().getCartesian(*out_x, *out_y, *out_z);
}

J2ME_API void j2me_core_amms_spectator_set_orientation(int heading, int pitch, int roll) {
    universal_loader::amms::GlobalManager::getInstance().getSpectator().getOrientation().setOrientation(heading, pitch, roll);
}

J2ME_API void j2me_core_amms_spectator_get_orientation(int* out_heading, int* out_pitch, int* out_roll) {
    if (!out_heading || !out_pitch || !out_roll) return;
    universal_loader::amms::GlobalManager::getInstance().getSpectator().getOrientation().getEulerAngles(*out_heading, *out_pitch, *out_roll);
}

J2ME_API uintptr_t j2me_core_amms_sound_source_create(void) {
    auto src = universal_loader::amms::GlobalManager::getInstance().createSoundSource3D();
    return reinterpret_cast<uintptr_t>(src.get());
}

J2ME_API void j2me_core_amms_sound_source_destroy(uintptr_t /*handle*/) {
    // Managed by GlobalManager
}

J2ME_API void j2me_core_amms_sound_source_set_location(uintptr_t handle, int x, int y, int z) {
    if (!handle) return;
    auto* src = reinterpret_cast<universal_loader::amms::SoundSource3D*>(handle);
    src->getLocation().setCartesian(x, y, z);
}

J2ME_API void j2me_core_amms_sound_source_get_location(uintptr_t handle, int* out_x, int* out_y, int* out_z) {
    if (!handle || !out_x || !out_y || !out_z) return;
    auto* src = reinterpret_cast<universal_loader::amms::SoundSource3D*>(handle);
    src->getLocation().getCartesian(*out_x, *out_y, *out_z);
}

J2ME_API void j2me_core_amms_sound_source_set_velocity(uintptr_t handle, int vx, int vy, int vz) {
    if (!handle) return;
    auto* src = reinterpret_cast<universal_loader::amms::SoundSource3D*>(handle);
    src->getDoppler().setVelocityCartesian(vx, vy, vz);
}

J2ME_API void j2me_core_amms_sound_source_set_attenuation(uintptr_t handle, int min_dist, int max_dist, bool mute_after_max, int rolloff) {
    if (!handle) return;
    auto* src = reinterpret_cast<universal_loader::amms::SoundSource3D*>(handle);
    src->getAttenuation().setParameters(min_dist, max_dist, mute_after_max, rolloff);
}

J2ME_API bool j2me_core_amms_sound_source_evaluate(uintptr_t handle, float* out_gain, int* out_pan, float* out_doppler, float* out_distance_mm) {
    if (!handle) return false;
    auto* src = reinterpret_cast<universal_loader::amms::SoundSource3D*>(handle);
    const auto& spec = universal_loader::amms::GlobalManager::getInstance().getSpectator();
    auto res = src->evaluate(spec);
    if (out_gain) *out_gain = res.gain;
    if (out_pan) *out_pan = res.pan;
    if (out_doppler) *out_doppler = res.dopplerFactor;
    if (out_distance_mm) *out_distance_mm = res.distanceMm;
    return true;
}

J2ME_API uintptr_t j2me_core_amms_effect_module_create(void) {
    auto mod = universal_loader::amms::GlobalManager::getInstance().createEffectModule();
    return reinterpret_cast<uintptr_t>(mod.get());
}

J2ME_API void j2me_core_amms_effect_module_destroy(uintptr_t /*handle*/) {
    // Managed by GlobalManager
}

J2ME_API void j2me_core_amms_reverb_set_level(uintptr_t module_handle, int level_mb) {
    if (!module_handle) return;
    auto* mod = reinterpret_cast<universal_loader::amms::EffectModule*>(module_handle);
    mod->getReverb().setReverbLevel(level_mb);
}

J2ME_API int j2me_core_amms_reverb_get_level(uintptr_t module_handle) {
    if (!module_handle) return 0;
    auto* mod = reinterpret_cast<universal_loader::amms::EffectModule*>(module_handle);
    return mod->getReverb().getReverbLevel();
}

J2ME_API void j2me_core_amms_reverb_set_preset(uintptr_t module_handle, const char* preset) {
    if (!module_handle || !preset) return;
    auto* mod = reinterpret_cast<universal_loader::amms::EffectModule*>(module_handle);
    mod->getReverb().setPreset(preset);
}

J2ME_API bool j2me_core_amms_reverb_get_preset(uintptr_t module_handle, char* out_buf, size_t cap) {
    if (!module_handle || !out_buf || cap == 0) return false;
    auto* mod = reinterpret_cast<universal_loader::amms::EffectModule*>(module_handle);
    std::string p = mod->getReverb().getPreset();
    if (p.size() >= cap) return false;
    std::memcpy(out_buf, p.c_str(), p.size() + 1);
    return true;
}

J2ME_API void j2me_core_amms_equalizer_set_band_level(uintptr_t module_handle, int band, int level_mb) {
    if (!module_handle) return;
    auto* mod = reinterpret_cast<universal_loader::amms::EffectModule*>(module_handle);
    mod->getEqualizer().setBandLevel(band, level_mb);
}

J2ME_API int j2me_core_amms_equalizer_get_band_level(uintptr_t module_handle, int band) {
    if (!module_handle) return 0;
    auto* mod = reinterpret_cast<universal_loader::amms::EffectModule*>(module_handle);
    return mod->getEqualizer().getBandLevel(band);
}

J2ME_API int j2me_core_amms_equalizer_get_band_count(uintptr_t module_handle) {
    if (!module_handle) return 0;
    auto* mod = reinterpret_cast<universal_loader::amms::EffectModule*>(module_handle);
    return mod->getEqualizer().getNumberOfBands();
}

J2ME_API void j2me_core_amms_equalizer_set_preset(uintptr_t module_handle, const char* preset) {
    if (!module_handle || !preset) return;
    auto* mod = reinterpret_cast<universal_loader::amms::EffectModule*>(module_handle);
    mod->getEqualizer().setPreset(preset);
}

J2ME_API void j2me_core_amms_pan_set(uintptr_t module_handle, int pan) {
    if (!module_handle) return;
    auto* mod = reinterpret_cast<universal_loader::amms::EffectModule*>(module_handle);
    mod->getPan().setPan(pan);
}

J2ME_API int j2me_core_amms_pan_get(uintptr_t module_handle) {
    if (!module_handle) return 0;
    auto* mod = reinterpret_cast<universal_loader::amms::EffectModule*>(module_handle);
    return mod->getPan().getPan();
}

J2ME_API void j2me_core_amms_camera_set_rotation(int rotation) {
    universal_loader::amms::GlobalManager::getInstance().getCameraControl().setCameraRotation(rotation);
}

J2ME_API int j2me_core_amms_camera_get_rotation(void) {
    return universal_loader::amms::GlobalManager::getInstance().getCameraControl().getCameraRotation();
}

J2ME_API void j2me_core_amms_camera_set_exposure_mode(const char* mode) {
    if (!mode) return;
    universal_loader::amms::GlobalManager::getInstance().getCameraControl().setExposureMode(mode);
}

J2ME_API bool j2me_core_amms_camera_get_exposure_mode(char* out_buf, size_t cap) {
    if (!out_buf || cap == 0) return false;
    std::string m = universal_loader::amms::GlobalManager::getInstance().getCameraControl().getExposureMode();
    if (m.size() >= cap) return false;
    std::memcpy(out_buf, m.c_str(), m.size() + 1);
    return true;
}

J2ME_API void j2me_core_amms_flash_set_mode(int mode) {
    universal_loader::amms::GlobalManager::getInstance().getFlashControl().setMode(mode);
}

J2ME_API int j2me_core_amms_flash_get_mode(void) {
    return universal_loader::amms::GlobalManager::getInstance().getFlashControl().getMode();
}

J2ME_API void j2me_core_amms_zoom_set_digital(int level) {
    universal_loader::amms::GlobalManager::getInstance().getZoomControl().setDigitalZoom(level);
}

J2ME_API int j2me_core_amms_zoom_get_digital(void) {
    return universal_loader::amms::GlobalManager::getInstance().getZoomControl().getDigitalZoom();
}

J2ME_API void j2me_core_amms_image_transform_set_crop(int x, int y, int w, int h) {
    universal_loader::amms::GlobalManager::getInstance().getImageTransformControl().setSourceRect(x, y, w, h);
}

J2ME_API void j2me_core_amms_image_transform_set_target(int w, int h) {
    universal_loader::amms::GlobalManager::getInstance().getImageTransformControl().setTargetSize(w, h);
}

// --- 28. MIDP 2.0 PUSH REGISTRY & COMM CONNECTION ---
J2ME_API void j2me_core_push_register_connection(const char* connection, const char* midlet, const char* filter) {
    if (!connection || !midlet) return;
    universal_loader::push::PushRegistry::getInstance().registerConnection(
        connection, midlet, filter ? filter : "*"
    );
}

J2ME_API bool j2me_core_push_unregister_connection(const char* connection) {
    if (!connection) return false;
    return universal_loader::push::PushRegistry::getInstance().unregisterConnection(connection);
}

J2ME_API int j2me_core_push_list_connections(bool available_only, char* out_buf, size_t cap) {
    auto list = universal_loader::push::PushRegistry::getInstance().listConnections(available_only);
    std::string joined;
    for (size_t i = 0; i < list.size(); ++i) {
        joined += list[i];
        if (i + 1 < list.size()) joined += "\n";
    }
    if (out_buf && cap > 0) {
        size_t copyLen = std::min(cap - 1, joined.size());
        std::memcpy(out_buf, joined.c_str(), copyLen);
        out_buf[copyLen] = '\0';
    }
    return static_cast<int>(list.size());
}

J2ME_API bool j2me_core_push_get_midlet(const char* connection, char* out_buf, size_t cap) {
    if (!connection || !out_buf || cap == 0) return false;
    std::string m = universal_loader::push::PushRegistry::getInstance().getMIDlet(connection);
    if (m.empty() || m.size() >= cap) return false;
    std::memcpy(out_buf, m.c_str(), m.size() + 1);
    return true;
}

J2ME_API bool j2me_core_push_get_filter(const char* connection, char* out_buf, size_t cap) {
    if (!connection || !out_buf || cap == 0) return false;
    std::string f = universal_loader::push::PushRegistry::getInstance().getFilter(connection);
    if (f.empty() || f.size() >= cap) return false;
    std::memcpy(out_buf, f.c_str(), f.size() + 1);
    return true;
}

J2ME_API int64_t j2me_core_push_register_alarm(const char* midlet, int64_t time_ms) {
    if (!midlet) return 0;
    return universal_loader::push::PushRegistry::getInstance().registerAlarm(midlet, time_ms);
}

J2ME_API bool j2me_core_push_notify_inbound(const char* connection, const char* sender_address) {
    if (!connection) return false;
    return universal_loader::push::PushRegistry::getInstance().notifyInboundConnection(
        connection, sender_address ? sender_address : ""
    );
}

J2ME_API int j2me_core_push_check_alarms(int64_t current_time_ms, char* out_woken_midlets, size_t cap) {
    auto woken = universal_loader::push::PushRegistry::getInstance().checkAlarms(current_time_ms);
    std::string joined;
    for (size_t i = 0; i < woken.size(); ++i) {
        joined += woken[i];
        if (i + 1 < woken.size()) joined += "\n";
    }
    if (out_woken_midlets && cap > 0) {
        size_t copyLen = std::min(cap - 1, joined.size());
        std::memcpy(out_woken_midlets, joined.c_str(), copyLen);
        out_woken_midlets[copyLen] = '\0';
    }
    return static_cast<int>(woken.size());
}

J2ME_API bool j2me_core_push_save(const char* file_path) {
    if (!file_path) return false;
    return universal_loader::push::PushRegistry::getInstance().saveToFile(file_path);
}

J2ME_API bool j2me_core_push_load(const char* file_path) {
    if (!file_path) return false;
    return universal_loader::push::PushRegistry::getInstance().loadFromFile(file_path);
}

// CommConnection
J2ME_API uintptr_t j2me_core_comm_open(const char* url) {
    if (!url) return 0;
    auto conn = universal_loader::comm::CommConnection::open(url);
    if (!conn) return 0;
    return reinterpret_cast<uintptr_t>(new std::shared_ptr<universal_loader::comm::CommConnection>(conn));
}

J2ME_API void j2me_core_comm_close(uintptr_t handle) {
    if (!handle) return;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::comm::CommConnection>*>(handle);
    if (*wrapper) {
        (*wrapper)->close();
    }
    delete wrapper;
}

J2ME_API int j2me_core_comm_get_baud_rate(uintptr_t handle) {
    if (!handle) return 0;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::comm::CommConnection>*>(handle);
    return (*wrapper) ? (*wrapper)->getBaudRate() : 0;
}

J2ME_API int j2me_core_comm_set_baud_rate(uintptr_t handle, int baud_rate) {
    if (!handle) return 0;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::comm::CommConnection>*>(handle);
    return (*wrapper) ? (*wrapper)->setBaudRate(baud_rate) : 0;
}

J2ME_API size_t j2me_core_comm_write(uintptr_t handle, const uint8_t* data, size_t len) {
    if (!handle || !data || len == 0) return 0;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::comm::CommConnection>*>(handle);
    return (*wrapper) ? (*wrapper)->write(data, len) : 0;
}

J2ME_API size_t j2me_core_comm_read(uintptr_t handle, uint8_t* out_buf, size_t max_len) {
    if (!handle || !out_buf || max_len == 0) return 0;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::comm::CommConnection>*>(handle);
    return (*wrapper) ? (*wrapper)->read(out_buf, max_len) : 0;
}

J2ME_API size_t j2me_core_comm_available(uintptr_t handle) {
    if (!handle) return 0;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::comm::CommConnection>*>(handle);
    return (*wrapper) ? (*wrapper)->available() : 0;
}

// --- 29. PKI SECURITY, SSL & HTTPS (JAVAX.MICROEDITION.PKI.*) ---
J2ME_API uintptr_t j2me_core_cert_create(const char* subject, const char* issuer, const char* type, const char* version, const char* sig_alg, int64_t not_before, int64_t not_after, const char* serial) {
    auto* cert = new universal_loader::security::Certificate(
        subject ? subject : "",
        issuer ? issuer : "",
        type ? type : "X.509",
        version ? version : "3",
        sig_alg ? sig_alg : "SHA256withRSA",
        not_before,
        not_after,
        serial ? serial : ""
    );
    return reinterpret_cast<uintptr_t>(cert);
}

J2ME_API void j2me_core_cert_destroy(uintptr_t cert_handle) {
    if (cert_handle) {
        delete reinterpret_cast<universal_loader::security::Certificate*>(cert_handle);
    }
}

J2ME_API bool j2me_core_cert_get_subject(uintptr_t cert_handle, char* out_buf, size_t cap) {
    if (!cert_handle || !out_buf || cap == 0) return false;
    auto* cert = reinterpret_cast<universal_loader::security::Certificate*>(cert_handle);
    const auto& s = cert->getSubject();
    if (s.size() >= cap) return false;
    std::memcpy(out_buf, s.c_str(), s.size() + 1);
    return true;
}

J2ME_API bool j2me_core_cert_get_issuer(uintptr_t cert_handle, char* out_buf, size_t cap) {
    if (!cert_handle || !out_buf || cap == 0) return false;
    auto* cert = reinterpret_cast<universal_loader::security::Certificate*>(cert_handle);
    const auto& s = cert->getIssuer();
    if (s.size() >= cap) return false;
    std::memcpy(out_buf, s.c_str(), s.size() + 1);
    return true;
}

J2ME_API bool j2me_core_cert_get_type(uintptr_t cert_handle, char* out_buf, size_t cap) {
    if (!cert_handle || !out_buf || cap == 0) return false;
    auto* cert = reinterpret_cast<universal_loader::security::Certificate*>(cert_handle);
    const auto& s = cert->getType();
    if (s.size() >= cap) return false;
    std::memcpy(out_buf, s.c_str(), s.size() + 1);
    return true;
}

J2ME_API bool j2me_core_cert_get_version(uintptr_t cert_handle, char* out_buf, size_t cap) {
    if (!cert_handle || !out_buf || cap == 0) return false;
    auto* cert = reinterpret_cast<universal_loader::security::Certificate*>(cert_handle);
    const auto& s = cert->getVersion();
    if (s.size() >= cap) return false;
    std::memcpy(out_buf, s.c_str(), s.size() + 1);
    return true;
}

J2ME_API bool j2me_core_cert_get_sig_alg(uintptr_t cert_handle, char* out_buf, size_t cap) {
    if (!cert_handle || !out_buf || cap == 0) return false;
    auto* cert = reinterpret_cast<universal_loader::security::Certificate*>(cert_handle);
    const auto& s = cert->getSigAlgName();
    if (s.size() >= cap) return false;
    std::memcpy(out_buf, s.c_str(), s.size() + 1);
    return true;
}

J2ME_API int64_t j2me_core_cert_get_not_before(uintptr_t cert_handle) {
    if (!cert_handle) return 0;
    auto* cert = reinterpret_cast<universal_loader::security::Certificate*>(cert_handle);
    return cert->getNotBefore();
}

J2ME_API int64_t j2me_core_cert_get_not_after(uintptr_t cert_handle) {
    if (!cert_handle) return 0;
    auto* cert = reinterpret_cast<universal_loader::security::Certificate*>(cert_handle);
    return cert->getNotAfter();
}

J2ME_API bool j2me_core_cert_get_serial(uintptr_t cert_handle, char* out_buf, size_t cap) {
    if (!cert_handle || !out_buf || cap == 0) return false;
    auto* cert = reinterpret_cast<universal_loader::security::Certificate*>(cert_handle);
    const auto& s = cert->getSerialNumber();
    if (s.size() >= cap) return false;
    std::memcpy(out_buf, s.c_str(), s.size() + 1);
    return true;
}

J2ME_API int j2me_core_cert_validate(uintptr_t cert_handle, const char* expected_host, int64_t current_time_ms) {
    if (!cert_handle) return universal_loader::security::CERT_VERIFICATION_FAILED;
    auto* cert = reinterpret_cast<universal_loader::security::Certificate*>(cert_handle);
    return cert->validate(expected_host ? expected_host : "", current_time_ms);
}

// SecureConnection (ssl://)
J2ME_API uintptr_t j2me_core_ssl_open(const char* url, bool verify_host) {
    if (!url) return 0;
    auto conn = universal_loader::security::SecureConnection::open(url, verify_host);
    if (!conn) return 0;
    return reinterpret_cast<uintptr_t>(new std::shared_ptr<universal_loader::security::SecureConnection>(conn));
}

J2ME_API void j2me_core_ssl_close(uintptr_t handle) {
    if (!handle) return;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::security::SecureConnection>*>(handle);
    if (*wrapper) {
        (*wrapper)->close();
    }
    delete wrapper;
}

J2ME_API bool j2me_core_ssl_is_open(uintptr_t handle) {
    if (!handle) return false;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::security::SecureConnection>*>(handle);
    return (*wrapper) ? (*wrapper)->isOpen() : false;
}

J2ME_API int j2me_core_ssl_get_port(uintptr_t handle) {
    if (!handle) return 0;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::security::SecureConnection>*>(handle);
    return (*wrapper) ? (*wrapper)->getPort() : 0;
}

J2ME_API size_t j2me_core_ssl_write(uintptr_t handle, const uint8_t* data, size_t len) {
    if (!handle || !data || len == 0) return 0;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::security::SecureConnection>*>(handle);
    return (*wrapper) ? (*wrapper)->write(data, len) : 0;
}

J2ME_API size_t j2me_core_ssl_read(uintptr_t handle, uint8_t* out_buf, size_t max_len) {
    if (!handle || !out_buf || max_len == 0) return 0;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::security::SecureConnection>*>(handle);
    return (*wrapper) ? (*wrapper)->read(out_buf, max_len) : 0;
}

J2ME_API size_t j2me_core_ssl_available(uintptr_t handle) {
    if (!handle) return 0;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::security::SecureConnection>*>(handle);
    return (*wrapper) ? (*wrapper)->available() : 0;
}

J2ME_API void j2me_core_ssl_feed_input(uintptr_t handle, const uint8_t* data, size_t len) {
    if (!handle || !data || len == 0) return;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::security::SecureConnection>*>(handle);
    if (*wrapper) {
        (*wrapper)->feedInput(data, len);
    }
}

J2ME_API bool j2me_core_ssl_get_security_info(uintptr_t handle, char* out_proto_name, size_t proto_cap, char* out_proto_ver, size_t ver_cap, char* out_cipher, size_t cipher_cap, uintptr_t* out_cert_handle) {
    if (!handle) return false;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::security::SecureConnection>*>(handle);
    if (!*wrapper) return false;
    const auto& si = (*wrapper)->getSecurityInfo();
    if (out_proto_name && proto_cap > si.getProtocolName().size()) {
        std::memcpy(out_proto_name, si.getProtocolName().c_str(), si.getProtocolName().size() + 1);
    }
    if (out_proto_ver && ver_cap > si.getProtocolVersion().size()) {
        std::memcpy(out_proto_ver, si.getProtocolVersion().c_str(), si.getProtocolVersion().size() + 1);
    }
    if (out_cipher && cipher_cap > si.getCipherSuite().size()) {
        std::memcpy(out_cipher, si.getCipherSuite().c_str(), si.getCipherSuite().size() + 1);
    }
    if (out_cert_handle) {
        auto* certCopy = new universal_loader::security::Certificate(si.getServerCertificate());
        *out_cert_handle = reinterpret_cast<uintptr_t>(certCopy);
    }
    return true;
}

// HttpsConnection (https://)
J2ME_API uintptr_t j2me_core_https_open(const char* url) {
    if (!url) return 0;
    auto conn = universal_loader::security::HttpsConnection::open(url);
    if (!conn) return 0;
    return reinterpret_cast<uintptr_t>(new std::shared_ptr<universal_loader::security::HttpsConnection>(conn));
}

J2ME_API void j2me_core_https_close(uintptr_t handle) {
    if (!handle) return;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::security::HttpsConnection>*>(handle);
    if (*wrapper) {
        (*wrapper)->close();
    }
    delete wrapper;
}

J2ME_API bool j2me_core_https_is_open(uintptr_t handle) {
    if (!handle) return false;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::security::HttpsConnection>*>(handle);
    return (*wrapper) ? (*wrapper)->isOpen() : false;
}

J2ME_API void j2me_core_https_set_method(uintptr_t handle, const char* method) {
    if (!handle || !method) return;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::security::HttpsConnection>*>(handle);
    if (*wrapper) {
        (*wrapper)->setRequestMethod(method);
    }
}

J2ME_API void j2me_core_https_set_request_property(uintptr_t handle, const char* key, const char* val) {
    if (!handle || !key || !val) return;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::security::HttpsConnection>*>(handle);
    if (*wrapper) {
        (*wrapper)->setRequestProperty(key, val);
    }
}

J2ME_API int j2me_core_https_get_response_code(uintptr_t handle) {
    if (!handle) return 0;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::security::HttpsConnection>*>(handle);
    return (*wrapper) ? (*wrapper)->getResponseCode() : 0;
}

J2ME_API int j2me_core_https_get_port(uintptr_t handle) {
    if (!handle) return 0;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::security::HttpsConnection>*>(handle);
    return (*wrapper) ? (*wrapper)->getPort() : 0;
}

J2ME_API void j2me_core_https_set_response_header(uintptr_t handle, const char* key, const char* val) {
    if (!handle || !key || !val) return;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::security::HttpsConnection>*>(handle);
    if (*wrapper) {
        (*wrapper)->setResponseHeader(key, val);
    }
}

J2ME_API bool j2me_core_https_get_header_field(uintptr_t handle, const char* key, char* out_buf, size_t cap) {
    if (!handle || !key || !out_buf || cap == 0) return false;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::security::HttpsConnection>*>(handle);
    if (!*wrapper) return false;
    std::string val = (*wrapper)->getHeaderField(key);
    if (val.empty() || cap <= val.size()) return false;
    std::memcpy(out_buf, val.c_str(), val.size() + 1);
    return true;
}

J2ME_API void j2me_core_https_feed_response(uintptr_t handle, int code, const char* body) {
    if (!handle) return;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::security::HttpsConnection>*>(handle);
    if (*wrapper) {
        (*wrapper)->feedResponse(code, body ? body : "");
    }
}

J2ME_API size_t j2me_core_https_read(uintptr_t handle, uint8_t* out_buf, size_t max_len) {
    if (!handle || !out_buf || max_len == 0) return 0;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::security::HttpsConnection>*>(handle);
    return (*wrapper) ? (*wrapper)->read(out_buf, max_len) : 0;
}

J2ME_API size_t j2me_core_https_write(uintptr_t handle, const uint8_t* data, size_t len) {
    if (!handle || !data || len == 0) return 0;
    auto* wrapper = reinterpret_cast<std::shared_ptr<universal_loader::security::HttpsConnection>*>(handle);
    return (*wrapper) ? (*wrapper)->write(data, len) : 0;
}

// --- 30. 3D BINARY ASSET LOADERS (M3G & MICRO3D) ---
J2ME_API int j2me_core_3d_identify_format(const uint8_t* data, size_t size) {
    if (!data || size == 0) return 0;
    auto m3gType = universal_loader::m3g::M3gLoader::identify(data, size);
    if (m3gType == universal_loader::m3g::M3G_FILE_M3G) return 1; // M3G
    int m3dType = universal_loader::micro3d::Micro3dLoader::identify(data, size);
    if (m3dType == 1) return 2; // MBAC
    if (m3dType == 2) return 3; // MTRA
    return 0; // Unknown
}

J2ME_API uintptr_t j2me_core_m3g_load_memory(const uint8_t* data, size_t size, size_t* out_root_count, size_t* out_mesh_count) {
    if (!data || size == 0) return 0;
    auto scene = std::make_unique<universal_loader::m3g::M3gLoadedScene>();
    if (!universal_loader::m3g::M3gLoader::load(data, size, *scene)) {
        return 0;
    }
    if (out_root_count) *out_root_count = scene->rootNodes.size();
    if (out_mesh_count) *out_mesh_count = scene->meshes.size();
    return reinterpret_cast<uintptr_t>(scene.release());
}

J2ME_API void j2me_core_m3g_scene_destroy(uintptr_t handle) {
    if (handle) {
        delete reinterpret_cast<universal_loader::m3g::M3gLoadedScene*>(handle);
    }
}

J2ME_API uintptr_t j2me_core_micro3d_load_figure(const uint8_t* data, size_t size) {
    if (!data || size == 0) return 0;
    auto figure = std::make_unique<universal_loader::micro3d::Micro3dFigure>();
    if (!universal_loader::micro3d::Micro3dLoader::loadMbac(data, size, *figure)) {
        return 0;
    }
    return reinterpret_cast<uintptr_t>(figure.release());
}

J2ME_API void j2me_core_micro3d_figure_destroy(uintptr_t handle) {
    if (handle) {
        delete reinterpret_cast<universal_loader::micro3d::Micro3dFigure*>(handle);
    }
}

J2ME_API bool j2me_core_micro3d_figure_get_counts(uintptr_t handle, int32_t* out_verts, int32_t* out_poly_t3, int32_t* out_poly_t4, int32_t* out_bones) {
    if (!handle) return false;
    auto* fig = reinterpret_cast<universal_loader::micro3d::Micro3dFigure*>(handle);
    if (out_verts) *out_verts = fig->numVertices;
    if (out_poly_t3) *out_poly_t3 = fig->numPolyT3;
    if (out_poly_t4) *out_poly_t4 = fig->numPolyT4;
    if (out_bones) *out_bones = fig->numBones;
    return true;
}

J2ME_API uintptr_t j2me_core_micro3d_load_action_table(const uint8_t* data, size_t size) {
    if (!data || size == 0) return 0;
    auto table = std::make_unique<universal_loader::micro3d::ActionTable>();
    if (!universal_loader::micro3d::Micro3dLoader::loadMtra(data, size, *table)) {
        return 0;
    }
    return reinterpret_cast<uintptr_t>(table.release());
}

J2ME_API void j2me_core_micro3d_action_table_destroy(uintptr_t handle) {
    if (handle) {
        delete reinterpret_cast<universal_loader::micro3d::ActionTable*>(handle);
    }
}

J2ME_API int32_t j2me_core_micro3d_action_table_get_count(uintptr_t handle) {
    if (!handle) return 0;
    auto* tbl = reinterpret_cast<universal_loader::micro3d::ActionTable*>(handle);
    return static_cast<int32_t>(tbl->getActionCount());
}

// ============================================================================
// 31. LCDUI FONT ENGINE, SYSTEM PROPERTIES & PCM WAV PLAYER
// ============================================================================

// --- Font Engine ---
J2ME_API uintptr_t j2me_core_font_get_default(void) {
    auto f = j2me::LcduiFont::getDefaultFont();
    return reinterpret_cast<uintptr_t>(f.get());
}

J2ME_API uintptr_t j2me_core_font_get(int face, int style, int size) {
    auto f = j2me::LcduiFont::getFont(face, style, size);
    return reinterpret_cast<uintptr_t>(f.get());
}

J2ME_API int j2me_core_font_get_height(uintptr_t font_handle) {
    if (!font_handle) return 0;
    auto* f = reinterpret_cast<j2me::LcduiFont*>(font_handle);
    return f->getHeight();
}

J2ME_API int j2me_core_font_get_baseline(uintptr_t font_handle) {
    if (!font_handle) return 0;
    auto* f = reinterpret_cast<j2me::LcduiFont*>(font_handle);
    return f->getBaselinePosition();
}

J2ME_API int j2me_core_font_char_width(uintptr_t font_handle, char c) {
    if (!font_handle) return 0;
    auto* f = reinterpret_cast<j2me::LcduiFont*>(font_handle);
    return f->charWidth(c);
}

J2ME_API int j2me_core_font_string_width(uintptr_t font_handle, const char* str) {
    if (!font_handle || !str) return 0;
    auto* f = reinterpret_cast<j2me::LcduiFont*>(font_handle);
    return f->stringWidth(str);
}

J2ME_API int j2me_core_font_get_face(uintptr_t font_handle) {
    if (!font_handle) return 0;
    auto* f = reinterpret_cast<j2me::LcduiFont*>(font_handle);
    return f->getFace();
}

J2ME_API int j2me_core_font_get_style(uintptr_t font_handle) {
    if (!font_handle) return 0;
    auto* f = reinterpret_cast<j2me::LcduiFont*>(font_handle);
    return f->getStyle();
}

J2ME_API int j2me_core_font_get_size(uintptr_t font_handle) {
    if (!font_handle) return 0;
    auto* f = reinterpret_cast<j2me::LcduiFont*>(font_handle);
    return f->getSize();
}

J2ME_API void j2me_core_graphics_set_font(J2meEngineInstance* inst, uintptr_t font_handle) {
    if (!inst) return;
    if (font_handle) {
        auto* f = reinterpret_cast<j2me::LcduiFont*>(font_handle);
        inst->currentFont = j2me::LcduiFont::getFont(f->getFace(), f->getStyle(), f->getSize());
    } else {
        inst->currentFont = j2me::LcduiFont::getDefaultFont();
    }
}

J2ME_API uintptr_t j2me_core_graphics_get_font(J2meEngineInstance* inst) {
    if (!inst) return 0;
    if (!inst->currentFont) {
        inst->currentFont = j2me::LcduiFont::getDefaultFont();
    }
    return reinterpret_cast<uintptr_t>(inst->currentFont.get());
}

J2ME_API void j2me_core_graphics_draw_string(J2meEngineInstance* inst, const char* text, int x, int y, int anchor) {
    if (!inst || !text) return;
    if (!inst->currentFont) {
        inst->currentFont = j2me::LcduiFont::getDefaultFont();
    }
    int w = inst->frameBuffer.getWidth();
    int h = inst->frameBuffer.getHeight();
    inst->currentFont->renderString(std::string(text), inst->frameBuffer.getRawDrawBuffer(), w, h, 0, 0, w, h, x, y, anchor, 0xFFFFFFFF);
}

J2ME_API void j2me_core_graphics_draw_char(J2meEngineInstance* inst, char c, int x, int y, int anchor) {
    if (!inst) return;
    if (!inst->currentFont) {
        inst->currentFont = j2me::LcduiFont::getDefaultFont();
    }
    int w = inst->frameBuffer.getWidth();
    int h = inst->frameBuffer.getHeight();
    std::string s(1, c);
    inst->currentFont->renderString(s, inst->frameBuffer.getRawDrawBuffer(), w, h, 0, 0, w, h, x, y, anchor, 0xFFFFFFFF);
}

// --- System Properties Manager ---
J2ME_API bool j2me_core_system_get_property(const char* key, char* out_buf, size_t cap) {
    if (!key || !out_buf || cap == 0) return false;
    if (!j2me::SystemPropertiesManager::instance().hasProperty(key)) return false;
    std::string val = j2me::SystemPropertiesManager::instance().getProperty(key);
    strncpy_s(out_buf, cap, val.c_str(), _TRUNCATE);
    return true;
}

J2ME_API void j2me_core_system_set_property(const char* key, const char* val) {
    if (!key || !val) return;
    j2me::SystemPropertiesManager::instance().setProperty(key, val);
}

J2ME_API bool j2me_core_system_has_property(const char* key) {
    if (!key) return false;
    return j2me::SystemPropertiesManager::instance().hasProperty(key);
}

J2ME_API void j2me_core_system_reset_properties(void) {
    j2me::SystemPropertiesManager::instance().resetToDefaults();
}

J2ME_API int j2me_core_system_get_property_count(void) {
    return static_cast<int>(j2me::SystemPropertiesManager::instance().getPropertyCount());
}

J2ME_API bool j2me_core_system_load_properties(const char* prop_content) {
    if (!prop_content) return false;
    return j2me::SystemPropertiesManager::instance().loadProperties(prop_content);
}

// --- PCM WAV Audio Player ---
struct J2meWavPlayerWrapper {
    std::shared_ptr<j2me::WavPlayer> player;
};

J2ME_API uintptr_t j2me_core_wav_create_memory(const uint8_t* data, size_t size) {
    if (!data || size < 44) return 0;
    auto player = j2me::WavPlayer::createFromMemory(data, size);
    if (!player) return 0;
    auto* wrapper = new J2meWavPlayerWrapper{player};
    return reinterpret_cast<uintptr_t>(wrapper);
}

J2ME_API uintptr_t j2me_core_wav_create_file(const char* file_path) {
    if (!file_path) return 0;
    auto player = j2me::WavPlayer::createFromFile(file_path);
    if (!player) return 0;
    auto* wrapper = new J2meWavPlayerWrapper{player};
    return reinterpret_cast<uintptr_t>(wrapper);
}

J2ME_API void j2me_core_wav_start(uintptr_t player_handle) {
    if (!player_handle) return;
    auto* w = reinterpret_cast<J2meWavPlayerWrapper*>(player_handle);
    if (w->player) w->player->start();
}

J2ME_API void j2me_core_wav_stop(uintptr_t player_handle) {
    if (!player_handle) return;
    auto* w = reinterpret_cast<J2meWavPlayerWrapper*>(player_handle);
    if (w->player) w->player->stop();
}

J2ME_API void j2me_core_wav_set_loop(uintptr_t player_handle, int count) {
    if (!player_handle) return;
    auto* w = reinterpret_cast<J2meWavPlayerWrapper*>(player_handle);
    if (w->player) w->player->setLoopCount(count);
}

J2ME_API void j2me_core_wav_set_volume(uintptr_t player_handle, int volume) {
    if (!player_handle) return;
    auto* w = reinterpret_cast<J2meWavPlayerWrapper*>(player_handle);
    if (w->player) w->player->setLevel(volume);
}

J2ME_API int j2me_core_wav_get_volume(uintptr_t player_handle) {
    if (!player_handle) return 0;
    auto* w = reinterpret_cast<J2meWavPlayerWrapper*>(player_handle);
    return w->player ? w->player->getLevel() : 0;
}

J2ME_API int64_t j2me_core_wav_get_duration_us(uintptr_t player_handle) {
    if (!player_handle) return 0;
    auto* w = reinterpret_cast<J2meWavPlayerWrapper*>(player_handle);
    return w->player ? w->player->getDuration() : 0;
}

J2ME_API int64_t j2me_core_wav_get_media_time_us(uintptr_t player_handle) {
    if (!player_handle) return 0;
    auto* w = reinterpret_cast<J2meWavPlayerWrapper*>(player_handle);
    return w->player ? w->player->getMediaTime() : 0;
}

J2ME_API void j2me_core_wav_set_media_time_us(uintptr_t player_handle, int64_t time_us) {
    if (!player_handle) return;
    auto* w = reinterpret_cast<J2meWavPlayerWrapper*>(player_handle);
    if (w->player) w->player->setMediaTime(time_us);
}

J2ME_API size_t j2me_core_wav_render_pcm(uintptr_t player_handle, int16_t* out_stereo_pcm, size_t frames) {
    if (!player_handle || !out_stereo_pcm || frames == 0) return 0;
    auto* w = reinterpret_cast<J2meWavPlayerWrapper*>(player_handle);
    return w->player ? w->player->renderAudio44100(out_stereo_pcm, frames) : 0;
}

J2ME_API void j2me_core_wav_destroy(uintptr_t player_handle) {
    if (player_handle) {
        auto* w = reinterpret_cast<J2meWavPlayerWrapper*>(player_handle);
        delete w;
    }
}

} // extern "C"
