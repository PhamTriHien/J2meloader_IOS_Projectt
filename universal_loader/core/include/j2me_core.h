#ifndef J2ME_CORE_H
#define J2ME_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#if defined(_WIN32) || defined(_WIN64)
    #ifdef J2ME_CORE_EXPORTS
        #define J2ME_API __declspec(dllexport)
    #else
        #define J2ME_API __declspec(dllimport)
    #endif
#else
    #define J2ME_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// --- Cấu trúc thực thể máy ảo J2ME ---
typedef struct J2meEngineInstance J2meEngineInstance;

// --- Định nghĩa phím chuẩn J2ME Canvas & Phone Keypad ---
enum J2meKeyCodes {
    J2ME_KEY_NUM0       = 48,  // '0'
    J2ME_KEY_NUM1       = 49,  // '1'
    J2ME_KEY_NUM2       = 50,  // '2'
    J2ME_KEY_NUM3       = 51,  // '3'
    J2ME_KEY_NUM4       = 52,  // '4'
    J2ME_KEY_NUM5       = 53,  // '5'
    J2ME_KEY_NUM6       = 54,  // '6'
    J2ME_KEY_NUM7       = 55,  // '7'
    J2ME_KEY_NUM8       = 56,  // '8'
    J2ME_KEY_NUM9       = 57,  // '9'
    J2ME_KEY_STAR       = 42,  // '*'
    J2ME_KEY_POUND      = 35,  // '#'
    J2ME_KEY_UP         = -1,
    J2ME_KEY_DOWN       = -2,
    J2ME_KEY_LEFT       = -3,
    J2ME_KEY_RIGHT      = -4,
    J2ME_KEY_FIRE       = -5,  // OK / Select
    J2ME_KEY_SOFT_LEFT  = -6,
    J2ME_KEY_SOFT_RIGHT = -7,
    J2ME_KEY_CLEAR      = -8
};

// --- Hành động cảm ứng Pointer ---
enum J2meTouchAction {
    J2ME_TOUCH_PRESSED  = 0,
    J2ME_TOUCH_RELEASED = 1,
    J2ME_TOUCH_DRAGGED  = 2
};

// --- 1. QUẢN LÝ VÒNG ĐỜI ENGINE (LIFECYCLE) ---
J2ME_API J2meEngineInstance* j2me_core_create(const char* storage_root_dir);
J2ME_API bool                j2me_core_load_jar(J2meEngineInstance* inst, const uint8_t* jar_bytes, size_t jar_size);
J2ME_API bool                j2me_core_load_jar_file(J2meEngineInstance* inst, const char* jar_file_path);
J2ME_API void                j2me_core_start(J2meEngineInstance* inst);
J2ME_API void                j2me_core_pause(J2meEngineInstance* inst);
J2ME_API void                j2me_core_resume(J2meEngineInstance* inst);
J2ME_API void                j2me_core_stop(J2meEngineInstance* inst);
J2ME_API void                j2me_core_destroy(J2meEngineInstance* inst);

// --- 2. ĐỒ HỌA & PIPELINE KHUNG HÌNH (ZERO-COPY FRAMEBUFFER) ---
// Trả về con trỏ vùng nhớ ARGB8888 32-bit (width * height * 4 bytes)
J2ME_API const uint32_t*     j2me_core_lock_framebuffer(J2meEngineInstance* inst, int* out_width, int* out_height, bool* out_dirty);
J2ME_API void                j2me_core_unlock_framebuffer(J2meEngineInstance* inst);
J2ME_API void                j2me_core_set_screen_dimensions(J2meEngineInstance* inst, int width, int height);

// --- 3. XỬ LÝ NHẬP LIỆU PHÍM BẤM & CẢM ỨNG ---
J2ME_API void                j2me_core_send_key(J2meEngineInstance* inst, int key_code, bool is_pressed);
J2ME_API void                j2me_core_send_touch(J2meEngineInstance* inst, int action, int x, int y);

// --- 4. ÂM THANH (AUDIO STREAMING & MMAPI SYNTHESIZER) ---
// Frontend gọi định kỳ trong callback audio để nạp dữ liệu PCM 16-bit stereo 44.1kHz
J2ME_API size_t              j2me_core_render_audio(J2meEngineInstance* inst, int16_t* pcm_stereo_buffer, size_t sample_count);
J2ME_API void                j2me_core_play_tone(J2meEngineInstance* inst, int note, int duration_ms, int volume);
J2ME_API bool                j2me_core_play_midi(J2meEngineInstance* inst, const uint8_t* midi_bytes, size_t length);
J2ME_API void                j2me_core_stop_midi(J2meEngineInstance* inst);
J2ME_API void                j2me_core_set_volume(J2meEngineInstance* inst, int volume);

// --- 5. TRUY VẤN THÔNG TIN ỨNG DỤNG ĐANG NẠP ---
J2ME_API const char*         j2me_core_get_app_title(J2meEngineInstance* inst);
J2ME_API const char*         j2me_core_get_app_vendor(J2meEngineInstance* inst);
J2ME_API const char*         j2me_core_get_app_version(J2meEngineInstance* inst);
J2ME_API int                 j2me_core_get_fps_limit(J2meEngineInstance* inst);
J2ME_API void                j2me_core_set_fps_limit(J2meEngineInstance* inst, int fps);

// --- 6. ĐỒ HỌA 3D (M3G JSR-184 & MASCOT CAPSULE MICRO3D) ---
typedef struct J2meGraphics3DContext J2meGraphics3DContext;

J2ME_API J2meGraphics3DContext* j2me_core_3d_create_context(int width, int height);
J2ME_API void                   j2me_core_3d_destroy_context(J2meGraphics3DContext* ctx);
J2ME_API void                   j2me_core_3d_bind_target(J2meGraphics3DContext* ctx, uint32_t* color_buffer, int width, int height);
J2ME_API void                   j2me_core_3d_set_viewport(J2meGraphics3DContext* ctx, int x, int y, int width, int height);
J2ME_API void                   j2me_core_3d_clear(J2meGraphics3DContext* ctx, uint32_t argb_color, float depth);
J2ME_API float                  j2me_core_3d_get_depth(J2meGraphics3DContext* ctx, int x, int y);
J2ME_API uint32_t               j2me_core_3d_get_color(J2meGraphics3DContext* ctx, int x, int y);

// --- 7. OEM & VENDOR EXTENSIONS (NOKIA, SIEMENS, SAMSUNG, DEVICE CONTROL) ---
typedef void (*J2meDeviceVibrationCallback)(int duration_ms, int frequency, void* user_data);

J2ME_API void j2me_core_set_vibration_callback(J2meEngineInstance* inst, J2meDeviceVibrationCallback cb, void* user_data);
J2ME_API void j2me_core_device_vibrate(J2meEngineInstance* inst, int duration_ms, int frequency);
J2ME_API void j2me_core_device_stop_vibration(J2meEngineInstance* inst);

// --- 8. LCDUI HIGH-LEVEL UI & EVENT SYSTEM ---
J2ME_API int  j2me_core_display_get_system_color(int color_specifier);
J2ME_API bool j2me_core_display_vibrate(int duration_ms);
J2ME_API bool j2me_core_display_flash_backlight(int duration_ms);
J2ME_API void j2me_core_display_send_key(int key_code, bool is_pressed);
J2ME_API void j2me_core_display_send_pointer(int event_type, int x, int y);

// --- 9. JSR-75 FILECONNECTION & FILESYSTEM ---
J2ME_API void    j2me_core_fs_set_base_dir(const char* base_dir);
J2ME_API bool    j2me_core_fs_file_exists(const char* url);
J2ME_API int64_t j2me_core_fs_file_size(const char* url);

// --- 10. JSR-120 WIRELESS MESSAGING API (SMS) ---
typedef void (*j2me_sms_callback_t)(const char* address, const char* textPayload, const uint8_t* binaryPayload, size_t binaryLen, void* userData);
J2ME_API void   j2me_core_sms_set_callback(j2me_sms_callback_t callback, void* user_data);
J2ME_API void   j2me_core_sms_set_autoreply(bool enabled, const char* reply_text);
J2ME_API size_t j2me_core_sms_get_sent_count();
J2ME_API void   j2me_core_sms_clear_all();

// --- 11. MIDLET LIFECYCLE & DESCRIPTOR ---
J2ME_API uintptr_t j2me_core_descriptor_create(const char* content, bool is_jad);
J2ME_API bool      j2me_core_descriptor_get(uintptr_t handle, const char* key, char* out_buf, size_t max_len);
J2ME_API bool      j2me_core_descriptor_get_name(uintptr_t handle, char* out_buf, size_t max_len);
J2ME_API bool      j2me_core_descriptor_get_version(uintptr_t handle, char* out_buf, size_t max_len);
J2ME_API bool      j2me_core_descriptor_get_vendor(uintptr_t handle, char* out_buf, size_t max_len);
J2ME_API void      j2me_core_descriptor_destroy(uintptr_t handle);

J2ME_API void j2me_core_midlet_start();
J2ME_API void j2me_core_midlet_pause();
J2ME_API void j2me_core_midlet_resume();
J2ME_API void j2me_core_midlet_destroy(bool unconditional);
J2ME_API int  j2me_core_midlet_get_state();

// --- 12. PHONE KEYPAD, KEYMAPPER & VIRTUAL KEYBOARD ---
J2ME_API void j2me_core_keymap_set_layout(int layout_type);
J2ME_API int  j2me_core_keymap_get_layout();
J2ME_API int  j2me_core_keymap_convert(int midp_key_code);
J2ME_API int  j2me_core_keymap_get_game_action(int key_code);
J2ME_API int  j2me_core_keymap_get_key_code(int game_action);
J2ME_API bool j2me_core_keymap_get_key_name(int key_code, char* out_buf, size_t max_len);
J2ME_API int  j2me_core_vk_hit_test(float touch_x, float touch_y);

// --- 13. JAR RESOURCE LOADER ---
J2ME_API uintptr_t j2me_core_jar_open(const char* file_path);
J2ME_API bool      j2me_core_jar_has_resource(uintptr_t handle, const char* resource_path);
J2ME_API size_t    j2me_core_jar_get_resource_size(uintptr_t handle, const char* resource_path);
J2ME_API size_t    j2me_core_jar_read_resource(uintptr_t handle, const char* resource_path, uint8_t* out_buf, size_t max_len);
J2ME_API void      j2me_core_jar_close(uintptr_t handle);

// --- 14. CONFIGURATION, PROFILE & SETTINGS ---
J2ME_API uintptr_t j2me_core_profile_create_default();
J2ME_API uintptr_t j2me_core_profile_load(const char* json_str);
J2ME_API bool      j2me_core_profile_save(uintptr_t profile_handle, char* out_buf, size_t max_len);
J2ME_API int       j2me_core_profile_get_int(uintptr_t profile_handle, const char* key, int default_val);
J2ME_API void      j2me_core_profile_set_int(uintptr_t profile_handle, const char* key, int value);
J2ME_API bool      j2me_core_profile_get_string(uintptr_t profile_handle, const char* key, char* out_buf, size_t max_len);
J2ME_API void      j2me_core_profile_set_string(uintptr_t profile_handle, const char* key, const char* value);
J2ME_API void      j2me_core_profile_destroy(uintptr_t profile_handle);
J2ME_API bool      j2me_core_apply_profile(J2meEngineInstance* inst, uintptr_t profile_handle);
J2ME_API size_t    j2me_core_get_preset_resolution_count();
J2ME_API bool      j2me_core_get_preset_resolution(size_t index, int* out_w, int* out_h, char* out_name, size_t max_len);

// --- 15. JAVA CLDC BYTECODE VIRTUAL MACHINE ---
J2ME_API uintptr_t j2me_core_vm_create();
J2ME_API bool      j2me_core_vm_load_class(uintptr_t vm_handle, const uint8_t* class_bytes, size_t size);
J2ME_API int32_t   j2me_core_vm_invoke_static_int(uintptr_t vm_handle, const char* class_name, const char* method_name, const char* desc);
J2ME_API void      j2me_core_vm_destroy(uintptr_t vm_handle);

// --- 16. J2ME 2D GAME API (TILEDLAYER & LAYERMANAGER) ---
J2ME_API uintptr_t j2me_core_tiled_layer_create(int columns, int rows, int image_w, int image_h, int tile_w, int tile_h);
J2ME_API void      j2me_core_tiled_layer_set_cell(uintptr_t handle, int col, int row, int tile_index);
J2ME_API int       j2me_core_tiled_layer_get_cell(uintptr_t handle, int col, int row);
J2ME_API void      j2me_core_tiled_layer_fill_cells(uintptr_t handle, int col, int row, int num_cols, int num_rows, int tile_index);
J2ME_API int       j2me_core_tiled_layer_create_animated_tile(uintptr_t handle, int static_tile_index);
J2ME_API void      j2me_core_tiled_layer_set_animated_tile(uintptr_t handle, int anim_tile_index, int static_tile_index);
J2ME_API int       j2me_core_tiled_layer_get_animated_tile(uintptr_t handle, int anim_tile_index);
J2ME_API void      j2me_core_tiled_layer_destroy(uintptr_t handle);

J2ME_API uintptr_t j2me_core_layer_manager_create();
J2ME_API void      j2me_core_layer_manager_append(uintptr_t mgr_handle, uintptr_t layer_handle);
J2ME_API void      j2me_core_layer_manager_insert(uintptr_t mgr_handle, uintptr_t layer_handle, int index);
J2ME_API int       j2me_core_layer_manager_get_size(uintptr_t mgr_handle);
J2ME_API void      j2me_core_layer_manager_remove(uintptr_t mgr_handle, uintptr_t layer_handle);
J2ME_API void      j2me_core_layer_manager_set_view_window(uintptr_t mgr_handle, int x, int y, int width, int height);
J2ME_API void      j2me_core_layer_manager_paint(uintptr_t mgr_handle, J2meEngineInstance* inst, int x, int y);
J2ME_API void      j2me_core_layer_manager_destroy(uintptr_t mgr_handle);

// --- 17. EMULATOR UX ENHANCEMENTS (SPEED MULTIPLIER & SCREENSHOT) ---
J2ME_API void j2me_core_set_speed_multiplier(J2meEngineInstance* inst, int multiplier);
J2ME_API int  j2me_core_get_speed_multiplier(J2meEngineInstance* inst);
J2ME_API bool j2me_core_capture_screenshot_png(J2meEngineInstance* inst, const char* out_png_path);

// --- 18. GCF DATAGRAM / UDP NETWORKING (JSR-118) ---
J2ME_API uintptr_t j2me_core_datagram_create(int size, const char* address);
J2ME_API bool      j2me_core_datagram_get_address(uintptr_t dgram_handle, char* out_buf, size_t max_len);
J2ME_API void      j2me_core_datagram_set_address(uintptr_t dgram_handle, const char* address);
J2ME_API int       j2me_core_datagram_get_length(uintptr_t dgram_handle);
J2ME_API void      j2me_core_datagram_set_length(uintptr_t dgram_handle, int length);
J2ME_API int       j2me_core_datagram_get_offset(uintptr_t dgram_handle);
J2ME_API size_t    j2me_core_datagram_get_data(uintptr_t dgram_handle, uint8_t* out_buf, size_t max_len);
J2ME_API void      j2me_core_datagram_set_data(uintptr_t dgram_handle, const uint8_t* buf, int offset, int length);
J2ME_API void      j2me_core_datagram_reset(uintptr_t dgram_handle);
J2ME_API void      j2me_core_datagram_write(uintptr_t dgram_handle, const uint8_t* data, size_t length);
J2ME_API size_t    j2me_core_datagram_read(uintptr_t dgram_handle, uint8_t* out_buf, size_t length);
J2ME_API void      j2me_core_datagram_destroy(uintptr_t dgram_handle);

J2ME_API uintptr_t j2me_core_datagram_conn_open(const char* url);
J2ME_API bool      j2me_core_datagram_conn_send(uintptr_t conn_handle, uintptr_t dgram_handle);
J2ME_API bool      j2me_core_datagram_conn_receive(uintptr_t conn_handle, uintptr_t dgram_handle, int timeout_ms);
J2ME_API int       j2me_core_datagram_conn_get_local_port(uintptr_t conn_handle);
J2ME_API void      j2me_core_datagram_conn_close(uintptr_t conn_handle);

// --- 19. M3G KEYFRAME ANIMATION & MORPHING MESH (JSR-184) ---
J2ME_API uintptr_t j2me_core_m3g_vertexbuffer_create();
J2ME_API void      j2me_core_m3g_vertexbuffer_set_positions(uintptr_t vb_handle, const float* coords, int vertex_count);
J2ME_API void      j2me_core_m3g_vertexbuffer_set_normals(uintptr_t vb_handle, const float* normals, int vertex_count);
J2ME_API void      j2me_core_m3g_vertexbuffer_destroy(uintptr_t vb_handle);

J2ME_API uintptr_t j2me_core_m3g_keyframesequence_create(int num_keyframes, int num_components, int interpolation);
J2ME_API void      j2me_core_m3g_keyframesequence_set_duration(uintptr_t seq_handle, int duration);
J2ME_API int       j2me_core_m3g_keyframesequence_get_duration(uintptr_t seq_handle);
J2ME_API void      j2me_core_m3g_keyframesequence_set_repeat_mode(uintptr_t seq_handle, int mode);
J2ME_API int       j2me_core_m3g_keyframesequence_get_repeat_mode(uintptr_t seq_handle);
J2ME_API void      j2me_core_m3g_keyframesequence_set_keyframe(uintptr_t seq_handle, int index, int time, const float* value);
J2ME_API bool      j2me_core_m3g_keyframesequence_sample(uintptr_t seq_handle, int sequence_time, float* out_val, int max_components);
J2ME_API void      j2me_core_m3g_keyframesequence_destroy(uintptr_t seq_handle);

J2ME_API uintptr_t j2me_core_m3g_animcontroller_create();
J2ME_API void      j2me_core_m3g_animcontroller_set_active_interval(uintptr_t ctrl_handle, int start, int end);
J2ME_API int       j2me_core_m3g_animcontroller_get_active_interval_start(uintptr_t ctrl_handle);
J2ME_API int       j2me_core_m3g_animcontroller_get_active_interval_end(uintptr_t ctrl_handle);
J2ME_API void      j2me_core_m3g_animcontroller_set_speed(uintptr_t ctrl_handle, float speed, int world_time);
J2ME_API float     j2me_core_m3g_animcontroller_get_speed(uintptr_t ctrl_handle);
J2ME_API void      j2me_core_m3g_animcontroller_set_position(uintptr_t ctrl_handle, float sequence_time, int world_time);
J2ME_API float     j2me_core_m3g_animcontroller_get_position(uintptr_t ctrl_handle, int world_time);
J2ME_API void      j2me_core_m3g_animcontroller_set_weight(uintptr_t ctrl_handle, float weight);
J2ME_API float     j2me_core_m3g_animcontroller_get_weight(uintptr_t ctrl_handle);
J2ME_API void      j2me_core_m3g_animcontroller_destroy(uintptr_t ctrl_handle);

J2ME_API uintptr_t j2me_core_m3g_animtrack_create(uintptr_t seq_handle, int property_id);
J2ME_API void      j2me_core_m3g_animtrack_set_controller(uintptr_t track_handle, uintptr_t ctrl_handle);
J2ME_API uintptr_t j2me_core_m3g_animtrack_get_controller(uintptr_t track_handle);
J2ME_API uintptr_t j2me_core_m3g_animtrack_get_sequence(uintptr_t track_handle);
J2ME_API int       j2me_core_m3g_animtrack_get_target_property(uintptr_t track_handle);
J2ME_API void      j2me_core_m3g_animtrack_destroy(uintptr_t track_handle);

J2ME_API uintptr_t j2me_core_m3g_morphing_mesh_create(uintptr_t base_vb_handle, int target_count, const uintptr_t* target_vb_handles);
J2ME_API void      j2me_core_m3g_morphing_mesh_set_weights(uintptr_t mesh_handle, const float* weights, int count);
J2ME_API void      j2me_core_m3g_morphing_mesh_get_weights(uintptr_t mesh_handle, float* out_weights, int max_count);
J2ME_API void      j2me_core_m3g_morphing_mesh_morph(uintptr_t mesh_handle);
J2ME_API void      j2me_core_m3g_mesh_add_animation_track(uintptr_t mesh_handle, uintptr_t track_handle);
J2ME_API int       j2me_core_m3g_mesh_animate(uintptr_t mesh_handle, int world_time);
J2ME_API void      j2me_core_m3g_mesh_get_position(uintptr_t mesh_handle, float* out_xyz);
J2ME_API void      j2me_core_m3g_mesh_get_vertex_position(uintptr_t mesh_handle, int vertex_index, float* out_xyz);
J2ME_API void      j2me_core_m3g_mesh_destroy(uintptr_t mesh_handle);

// --- 20. M3G SKINNED MESH & BONE SKELETON (JSR-184) ---
J2ME_API uintptr_t j2me_core_m3g_node_create();
J2ME_API void      j2me_core_m3g_node_set_translation(uintptr_t node_handle, float tx, float ty, float tz);
J2ME_API void      j2me_core_m3g_node_get_translation(uintptr_t node_handle, float* out_xyz);
J2ME_API void      j2me_core_m3g_node_set_orientation(uintptr_t node_handle, float qx, float qy, float qz, float qw);
J2ME_API void      j2me_core_m3g_node_get_orientation(uintptr_t node_handle, float* out_xyzw);
J2ME_API void      j2me_core_m3g_node_set_scale(uintptr_t node_handle, float sx, float sy, float sz);
J2ME_API void      j2me_core_m3g_node_get_scale(uintptr_t node_handle, float* out_xyz);
J2ME_API void      j2me_core_m3g_node_get_global_transform(uintptr_t node_handle, float* out_matrix16);
J2ME_API void      j2me_core_m3g_node_add_animation_track(uintptr_t node_handle, uintptr_t track_handle);
J2ME_API int       j2me_core_m3g_node_animate(uintptr_t node_handle, int world_time);
J2ME_API void      j2me_core_m3g_node_destroy(uintptr_t node_handle);

J2ME_API uintptr_t j2me_core_m3g_group_create();
J2ME_API void      j2me_core_m3g_group_add_child(uintptr_t group_handle, uintptr_t child_node_handle);
J2ME_API void      j2me_core_m3g_group_remove_child(uintptr_t group_handle, uintptr_t child_node_handle);
J2ME_API size_t    j2me_core_m3g_group_get_child_count(uintptr_t group_handle);
J2ME_API uintptr_t j2me_core_m3g_group_get_child(uintptr_t group_handle, size_t index);
J2ME_API int       j2me_core_m3g_group_animate(uintptr_t group_handle, int world_time);
J2ME_API void      j2me_core_m3g_group_destroy(uintptr_t group_handle);

J2ME_API uintptr_t j2me_core_m3g_skinned_mesh_create(uintptr_t base_vb_handle, uintptr_t skeleton_group_handle);
J2ME_API void      j2me_core_m3g_skinned_mesh_add_transform(uintptr_t mesh_handle, uintptr_t bone_node_handle, int weight, int first_vertex, int num_vertices);
J2ME_API size_t    j2me_core_m3g_skinned_mesh_get_bone_count(uintptr_t mesh_handle);
J2ME_API uintptr_t j2me_core_m3g_skinned_mesh_get_bone(uintptr_t mesh_handle, size_t index);
J2ME_API uintptr_t j2me_core_m3g_skinned_mesh_get_skeleton(uintptr_t mesh_handle);
J2ME_API void      j2me_core_m3g_skinned_mesh_skin(uintptr_t mesh_handle);
J2ME_API int       j2me_core_m3g_skinned_mesh_animate(uintptr_t mesh_handle, int world_time);
J2ME_API void      j2me_core_m3g_skinned_mesh_get_vertex_position(uintptr_t mesh_handle, int vertex_index, float* out_xyz);
J2ME_API void      j2me_core_m3g_skinned_mesh_get_vertex_normal(uintptr_t mesh_handle, int vertex_index, float* out_xyz);
J2ME_API void      j2me_core_m3g_skinned_mesh_destroy(uintptr_t mesh_handle);

// --- 21. APP MANAGEMENT, REPOSITORY & INSTALLER ---
typedef struct {
    int id;
    char path[128];
    char title[128];
    char author[128];
    char version[32];
    char image_path[256];
    int64_t installed_timestamp;
    int64_t last_played_timestamp;
    int play_count;
} J2meAppItemInfo;

J2ME_API size_t j2me_core_app_repo_get_count(J2meEngineInstance* engine);
J2ME_API bool   j2me_core_app_repo_get_item(J2meEngineInstance* engine, size_t index, J2meAppItemInfo* out_info);
J2ME_API bool   j2me_core_app_repo_find_by_id(J2meEngineInstance* engine, int id, J2meAppItemInfo* out_info);
J2ME_API bool   j2me_core_app_repo_find_by_path(J2meEngineInstance* engine, const char* path, J2meAppItemInfo* out_info);
J2ME_API bool   j2me_core_app_repo_delete(J2meEngineInstance* engine, int id);

J2ME_API int    j2me_core_app_installer_check_jar(J2meEngineInstance* engine, const char* jar_path, char* out_title, size_t title_cap, char* out_vendor, size_t vendor_cap, char* out_version, size_t version_cap);
J2ME_API int    j2me_core_app_installer_install(J2meEngineInstance* engine, const char* jar_path, bool force_update, char* out_error, size_t error_cap);
J2ME_API bool   j2me_core_app_installer_uninstall(J2meEngineInstance* engine, int app_id, char* out_error, size_t error_cap);
J2ME_API bool   j2me_core_app_launch(J2meEngineInstance* engine, int app_id);

// --- 22. JSR-82 MOBILE BLUETOOTH & RFCOMM/L2CAP MULTIPLAYER ---
J2ME_API bool      j2me_core_bluetooth_is_power_on(void);
J2ME_API void      j2me_core_bluetooth_set_power_on(bool on);
J2ME_API void      j2me_core_bluetooth_get_local_address(char* out_addr, size_t cap);
J2ME_API void      j2me_core_bluetooth_set_local_address(const char* addr);
J2ME_API void      j2me_core_bluetooth_get_local_name(char* out_name, size_t cap);
J2ME_API void      j2me_core_bluetooth_set_local_name(const char* name);
J2ME_API int       j2me_core_bluetooth_get_discoverable(void);
J2ME_API bool      j2me_core_bluetooth_set_discoverable(int mode);
J2ME_API bool      j2me_core_bluetooth_get_property(const char* property, char* out_val, size_t cap);

// BTSPP Server & Client
J2ME_API uintptr_t j2me_core_bluetooth_open_btspp_server(const char* url);
J2ME_API uintptr_t j2me_core_bluetooth_btspp_accept(uintptr_t server_handle, int timeout_ms);
J2ME_API void      j2me_core_bluetooth_btspp_server_close(uintptr_t server_handle);

J2ME_API uintptr_t j2me_core_bluetooth_open_btspp_client(const char* url, int timeout_ms);
J2ME_API int       j2me_core_bluetooth_btspp_read(uintptr_t conn_handle, uint8_t* buffer, size_t max_len, int timeout_ms);
J2ME_API int       j2me_core_bluetooth_btspp_write(uintptr_t conn_handle, const uint8_t* buffer, size_t len);
J2ME_API int       j2me_core_bluetooth_btspp_available(uintptr_t conn_handle);
J2ME_API void      j2me_core_bluetooth_btspp_close(uintptr_t conn_handle);

// BTL2CAP Server & Client
J2ME_API uintptr_t j2me_core_bluetooth_open_btl2cap_server(const char* url);
J2ME_API uintptr_t j2me_core_bluetooth_btl2cap_accept(uintptr_t server_handle, int timeout_ms);
J2ME_API void      j2me_core_bluetooth_btl2cap_server_close(uintptr_t server_handle);

J2ME_API uintptr_t j2me_core_bluetooth_open_btl2cap_client(const char* url, int timeout_ms);
J2ME_API bool      j2me_core_bluetooth_btl2cap_send(uintptr_t conn_handle, const uint8_t* data, size_t len);
J2ME_API int       j2me_core_bluetooth_btl2cap_receive(uintptr_t conn_handle, uint8_t* in_buf, size_t in_buf_len, int timeout_ms);
J2ME_API bool      j2me_core_bluetooth_btl2cap_ready(uintptr_t conn_handle);
J2ME_API void      j2me_core_bluetooth_btl2cap_close(uintptr_t conn_handle);

// --- 23. VODAFONE VSCL & CARRIER OEM EXTENSIONS ---
// Vodafone Sprite Engine
J2ME_API uintptr_t j2me_core_vodafone_sprite_create(int num_palettes, int num_patterns);
J2ME_API void      j2me_core_vodafone_sprite_destroy(uintptr_t sprite_handle);
J2ME_API void      j2me_core_vodafone_sprite_set_palette(uintptr_t sprite_handle, int index, uint32_t color);
J2ME_API uint32_t  j2me_core_vodafone_sprite_get_palette(uintptr_t sprite_handle, int index);
J2ME_API void      j2me_core_vodafone_sprite_set_pattern(uintptr_t sprite_handle, int index, const uint8_t* data, size_t length);
J2ME_API int16_t   j2me_core_vodafone_sprite_create_command(uintptr_t sprite_handle, int offset, bool transparent, int rotation, bool upside_down, bool rightside_left, int pattern_no);
J2ME_API void      j2me_core_vodafone_sprite_create_framebuffer(uintptr_t sprite_handle, int fw, int fh);
J2ME_API void      j2me_core_vodafone_sprite_dispose_framebuffer(uintptr_t sprite_handle);
J2ME_API void      j2me_core_vodafone_sprite_draw_char(uintptr_t sprite_handle, int16_t command_id, int16_t x, int16_t y);
J2ME_API void      j2me_core_vodafone_sprite_copy_area(uintptr_t sprite_handle, int sx, int sy, int fw, int fh, int tx, int ty);
J2ME_API void      j2me_core_vodafone_sprite_draw_framebuffer(uintptr_t sprite_handle, uint32_t* target_pixels, int target_w, int target_h, int tx, int ty);
J2ME_API const uint32_t* j2me_core_vodafone_sprite_get_framebuffer(uintptr_t sprite_handle, int* out_w, int* out_h);

// Vodafone Device Control & Key States
J2ME_API int       j2me_core_vodafone_device_get_state(int device_no);
J2ME_API bool      j2me_core_vodafone_device_is_active(int device_no);
J2ME_API bool      j2me_core_vodafone_device_set_active(int device_no, bool active);
J2ME_API void      j2me_core_vodafone_device_blink(int lighting_ms, int extinction_ms, int repeat);
J2ME_API uint32_t  j2me_core_vodafone_device_get_keystates(void);
J2ME_API void      j2me_core_vodafone_device_set_keystates_mask(uint32_t mask);
J2ME_API void      j2me_core_vodafone_device_key_event(int key_code, bool is_down);

// Vodafone Offscreen Image Encoder
J2ME_API int       j2me_core_vodafone_encode_offscreen(const uint32_t* pixels, int src_w, int src_h, int x, int y, int w, int h, int format, uint8_t* out_buf, size_t out_cap);

// Carrier OEM Extensions (KDDI, Motorola, Sony Ericsson, Sprint PCS)
J2ME_API int       j2me_core_carrier_kddi_get_keystate(bool eight_directions);
J2ME_API void      j2me_core_carrier_motorola_funlight_set_color(int region, uint32_t rgb);
J2ME_API uint32_t  j2me_core_carrier_motorola_funlight_get_color(int region);
J2ME_API void      j2me_core_carrier_sony_accel_set(float x, float y, float z);
J2ME_API void      j2me_core_carrier_sony_accel_get(float* out_x, float* out_y, float* out_z);
J2ME_API void      j2me_core_carrier_sprint_play_clip(const char* name, int loop_count);
J2ME_API void      j2me_core_carrier_sprint_stop(void);
J2ME_API bool      j2me_core_carrier_sprint_is_playing(void);

// --- 24. JSR-179 MOBILE LOCATION API ---
J2ME_API int       j2me_core_location_provider_get_state(void);
J2ME_API void      j2me_core_location_provider_set_state(int state);
J2ME_API void      j2me_core_location_provider_update_host_location(double lat, double lon, float alt, float speed, float course, float horiz_acc, float vert_acc);
J2ME_API bool      j2me_core_location_provider_get_last_known(double* out_lat, double* out_lon, float* out_alt, float* out_speed, float* out_course, int64_t* out_timestamp);
J2ME_API int       j2me_core_location_provider_get_nmea(char* out_buf, size_t cap);

J2ME_API float     j2me_core_location_coordinates_distance(double lat1, double lon1, double lat2, double lon2);
J2ME_API float     j2me_core_location_coordinates_azimuth(double lat1, double lon1, double lat2, double lon2);
J2ME_API bool      j2me_core_location_coordinates_convert_to_string(double coord, int output_type, char* out_buf, size_t cap);
J2ME_API double    j2me_core_location_coordinates_convert_from_string(const char* str);

J2ME_API void      j2me_core_location_orientation_get(float* out_azimuth, bool* out_is_magnetic, float* out_pitch, float* out_roll);
J2ME_API void      j2me_core_location_orientation_set(float azimuth, bool is_magnetic, float pitch, float roll);

J2ME_API bool      j2me_core_location_landmark_store_create(const char* store_name);
J2ME_API bool      j2me_core_location_landmark_store_delete(const char* store_name);
J2ME_API void      j2me_core_location_landmark_store_add_landmark(const char* store_name, const char* name, const char* desc, double lat, double lon, float alt, const char* category);
J2ME_API int       j2me_core_location_landmark_store_get_count(const char* store_name, const char* category);

// --- 25. JSR-256 MOBILE SENSOR API ---
J2ME_API int       j2me_core_sensor_get_count(void);
J2ME_API bool      j2me_core_sensor_get_url(int index, char* out_buf, size_t cap);
J2ME_API bool      j2me_core_sensor_get_quantity(int index, char* out_buf, size_t cap);
J2ME_API bool      j2me_core_sensor_get_context_type(int index, char* out_buf, size_t cap);
J2ME_API int       j2me_core_sensor_find(const char* quantity, const char* context_type, int* out_indices, int max_count);

J2ME_API uintptr_t j2me_core_sensor_open(const char* url);
J2ME_API void      j2me_core_sensor_close(uintptr_t handle);
J2ME_API int       j2me_core_sensor_get_state(uintptr_t handle);
J2ME_API int       j2me_core_sensor_get_channel_count(uintptr_t handle);
J2ME_API bool      j2me_core_sensor_get_channel_name(uintptr_t handle, int channel_index, char* out_buf, size_t cap);
J2ME_API int       j2me_core_sensor_get_data(uintptr_t handle, int channel_index, double* out_buf, int max_samples);

J2ME_API void      j2me_core_sensor_update_accelerometer(double x, double y, double z);
J2ME_API void      j2me_core_sensor_update_ambient_light(double lux);
J2ME_API void      j2me_core_sensor_update_magnetic_field(double x, double y, double z);
J2ME_API void      j2me_core_sensor_update_orientation(double azimuth, double pitch, double roll);
J2ME_API void      j2me_core_sensor_update_temperature(double celsius);

// --- 26. JSR-75 PIM (PERSONAL INFORMATION MANAGEMENT) ---
J2ME_API void      j2me_core_pim_init(const char* sandbox_dir);
J2ME_API int       j2me_core_pim_list_count(int pim_list_type);
J2ME_API bool      j2me_core_pim_list_get_name(int pim_list_type, int index, char* out_buf, size_t cap);
J2ME_API uintptr_t j2me_core_pim_open_list(int pim_list_type, int mode, const char* name);
J2ME_API void      j2me_core_pim_close_list(uintptr_t list_handle);
J2ME_API int       j2me_core_pim_list_get_item_count(uintptr_t list_handle);
J2ME_API uintptr_t j2me_core_pim_list_get_item(uintptr_t list_handle, int index);
J2ME_API bool      j2me_core_pim_list_remove_item(uintptr_t list_handle, uintptr_t item_handle);

J2ME_API uintptr_t j2me_core_pim_contact_create(uintptr_t list_handle);
J2ME_API bool      j2me_core_pim_contact_set_name(uintptr_t item_handle, const char* family, const char* given, const char* other, const char* prefix, const char* suffix);
J2ME_API bool      j2me_core_pim_contact_get_formatted_name(uintptr_t item_handle, char* out_buf, size_t cap);
J2ME_API bool      j2me_core_pim_contact_add_tel(uintptr_t item_handle, int attributes, const char* number);
J2ME_API int       j2me_core_pim_contact_get_tel_count(uintptr_t item_handle);
J2ME_API bool      j2me_core_pim_contact_get_tel(uintptr_t item_handle, int index, char* out_buf, size_t cap, int* out_attributes);
J2ME_API bool      j2me_core_pim_contact_add_email(uintptr_t item_handle, int attributes, const char* email);
J2ME_API int       j2me_core_pim_contact_get_email_count(uintptr_t item_handle);
J2ME_API bool      j2me_core_pim_contact_get_email(uintptr_t item_handle, int index, char* out_buf, size_t cap, int* out_attributes);
J2ME_API bool      j2me_core_pim_contact_set_address(uintptr_t item_handle, int attributes, const char* street, const char* locality, const char* region, const char* postal_code, const char* country);

J2ME_API uintptr_t j2me_core_pim_event_create(uintptr_t list_handle);
J2ME_API bool      j2me_core_pim_event_set_details(uintptr_t item_handle, const char* summary, const char* location, int64_t start_ms, int64_t end_ms, int alarm_sec);
J2ME_API bool      j2me_core_pim_event_get_details(uintptr_t item_handle, char* out_summary, size_t summary_cap, char* out_location, size_t loc_cap, int64_t* out_start_ms, int64_t* out_end_ms);
J2ME_API bool      j2me_core_pim_event_set_repeat_rule(uintptr_t item_handle, int frequency, int interval, int count, int64_t end_ms);

J2ME_API uintptr_t j2me_core_pim_todo_create(uintptr_t list_handle);
J2ME_API bool      j2me_core_pim_todo_set_details(uintptr_t item_handle, const char* summary, int priority, bool completed, int64_t due_ms, int64_t completion_ms);
J2ME_API bool      j2me_core_pim_todo_get_details(uintptr_t item_handle, char* out_summary, size_t summary_cap, int* out_priority, bool* out_completed, int64_t* out_due_ms);

J2ME_API void      j2me_core_pim_item_commit(uintptr_t item_handle);
J2ME_API void      j2me_core_pim_item_add_category(uintptr_t item_handle, const char* category);
J2ME_API int       j2me_core_pim_item_get_category_count(uintptr_t item_handle);
J2ME_API bool      j2me_core_pim_item_get_category(uintptr_t item_handle, int index, char* out_buf, size_t cap);

J2ME_API int       j2me_core_pim_export_serial(uintptr_t item_handle, const char* format, char* out_buf, size_t cap);
J2ME_API uintptr_t j2me_core_pim_import_serial(uintptr_t list_handle, const char* data);
J2ME_API void      j2me_core_pim_save_all(void);

// --- 27. JSR-234 AMMS (ADVANCED MULTIMEDIA SUPPLEMENTS) ---
J2ME_API void      j2me_core_amms_spectator_set_location(int x, int y, int z);
J2ME_API void      j2me_core_amms_spectator_get_location(int* out_x, int* out_y, int* out_z);
J2ME_API void      j2me_core_amms_spectator_set_orientation(int heading, int pitch, int roll);
J2ME_API void      j2me_core_amms_spectator_get_orientation(int* out_heading, int* out_pitch, int* out_roll);

J2ME_API uintptr_t j2me_core_amms_sound_source_create(void);
J2ME_API void      j2me_core_amms_sound_source_destroy(uintptr_t handle);
J2ME_API void      j2me_core_amms_sound_source_set_location(uintptr_t handle, int x, int y, int z);
J2ME_API void      j2me_core_amms_sound_source_get_location(uintptr_t handle, int* out_x, int* out_y, int* out_z);
J2ME_API void      j2me_core_amms_sound_source_set_velocity(uintptr_t handle, int vx, int vy, int vz);
J2ME_API void      j2me_core_amms_sound_source_set_attenuation(uintptr_t handle, int min_dist, int max_dist, bool mute_after_max, int rolloff);
J2ME_API bool      j2me_core_amms_sound_source_evaluate(uintptr_t handle, float* out_gain, int* out_pan, float* out_doppler, float* out_distance_mm);

J2ME_API uintptr_t j2me_core_amms_effect_module_create(void);
J2ME_API void      j2me_core_amms_effect_module_destroy(uintptr_t handle);
J2ME_API void      j2me_core_amms_reverb_set_level(uintptr_t module_handle, int level_mb);
J2ME_API int       j2me_core_amms_reverb_get_level(uintptr_t module_handle);
J2ME_API void      j2me_core_amms_reverb_set_preset(uintptr_t module_handle, const char* preset);
J2ME_API bool      j2me_core_amms_reverb_get_preset(uintptr_t module_handle, char* out_buf, size_t cap);
J2ME_API void      j2me_core_amms_equalizer_set_band_level(uintptr_t module_handle, int band, int level_mb);
J2ME_API int       j2me_core_amms_equalizer_get_band_level(uintptr_t module_handle, int band);
J2ME_API int       j2me_core_amms_equalizer_get_band_count(uintptr_t module_handle);
J2ME_API void      j2me_core_amms_equalizer_set_preset(uintptr_t module_handle, const char* preset);
J2ME_API void      j2me_core_amms_pan_set(uintptr_t module_handle, int pan);
J2ME_API int       j2me_core_amms_pan_get(uintptr_t module_handle);

J2ME_API void      j2me_core_amms_camera_set_rotation(int rotation);
J2ME_API int       j2me_core_amms_camera_get_rotation(void);
J2ME_API void      j2me_core_amms_camera_set_exposure_mode(const char* mode);
J2ME_API bool      j2me_core_amms_camera_get_exposure_mode(char* out_buf, size_t cap);
J2ME_API void      j2me_core_amms_flash_set_mode(int mode);
J2ME_API int       j2me_core_amms_flash_get_mode(void);
J2ME_API void      j2me_core_amms_zoom_set_digital(int level);
J2ME_API int       j2me_core_amms_zoom_get_digital(void);
J2ME_API void      j2me_core_amms_image_transform_set_crop(int x, int y, int w, int h);
J2ME_API void      j2me_core_amms_image_transform_set_target(int w, int h);

// --- 28. MIDP 2.0 PUSH REGISTRY & COMM CONNECTION ---
J2ME_API void      j2me_core_push_register_connection(const char* connection, const char* midlet, const char* filter);
J2ME_API bool      j2me_core_push_unregister_connection(const char* connection);
J2ME_API int       j2me_core_push_list_connections(bool available_only, char* out_buf, size_t cap);
J2ME_API bool      j2me_core_push_get_midlet(const char* connection, char* out_buf, size_t cap);
J2ME_API bool      j2me_core_push_get_filter(const char* connection, char* out_buf, size_t cap);
J2ME_API int64_t   j2me_core_push_register_alarm(const char* midlet, int64_t time_ms);
J2ME_API bool      j2me_core_push_notify_inbound(const char* connection, const char* sender_address);
J2ME_API int       j2me_core_push_check_alarms(int64_t current_time_ms, char* out_woken_midlets, size_t cap);
J2ME_API bool      j2me_core_push_save(const char* file_path);
J2ME_API bool      j2me_core_push_load(const char* file_path);

J2ME_API uintptr_t j2me_core_comm_open(const char* url);
J2ME_API void      j2me_core_comm_close(uintptr_t handle);
J2ME_API int       j2me_core_comm_get_baud_rate(uintptr_t handle);
J2ME_API int       j2me_core_comm_set_baud_rate(uintptr_t handle, int baud_rate);
J2ME_API size_t    j2me_core_comm_write(uintptr_t handle, const uint8_t* data, size_t len);
J2ME_API size_t    j2me_core_comm_read(uintptr_t handle, uint8_t* out_buf, size_t max_len);
J2ME_API size_t    j2me_core_comm_available(uintptr_t handle);

// --- 29. PKI SECURITY, SSL & HTTPS (JAVAX.MICROEDITION.PKI.*) ---
J2ME_API uintptr_t j2me_core_cert_create(const char* subject, const char* issuer, const char* type, const char* version, const char* sig_alg, int64_t not_before, int64_t not_after, const char* serial);
J2ME_API void      j2me_core_cert_destroy(uintptr_t cert_handle);
J2ME_API bool      j2me_core_cert_get_subject(uintptr_t cert_handle, char* out_buf, size_t cap);
J2ME_API bool      j2me_core_cert_get_issuer(uintptr_t cert_handle, char* out_buf, size_t cap);
J2ME_API bool      j2me_core_cert_get_type(uintptr_t cert_handle, char* out_buf, size_t cap);
J2ME_API bool      j2me_core_cert_get_version(uintptr_t cert_handle, char* out_buf, size_t cap);
J2ME_API bool      j2me_core_cert_get_sig_alg(uintptr_t cert_handle, char* out_buf, size_t cap);
J2ME_API int64_t   j2me_core_cert_get_not_before(uintptr_t cert_handle);
J2ME_API int64_t   j2me_core_cert_get_not_after(uintptr_t cert_handle);
J2ME_API bool      j2me_core_cert_get_serial(uintptr_t cert_handle, char* out_buf, size_t cap);
J2ME_API int       j2me_core_cert_validate(uintptr_t cert_handle, const char* expected_host, int64_t current_time_ms);

J2ME_API uintptr_t j2me_core_ssl_open(const char* url, bool verify_host);
J2ME_API void      j2me_core_ssl_close(uintptr_t handle);
J2ME_API bool      j2me_core_ssl_is_open(uintptr_t handle);
J2ME_API int       j2me_core_ssl_get_port(uintptr_t handle);
J2ME_API size_t    j2me_core_ssl_write(uintptr_t handle, const uint8_t* data, size_t len);
J2ME_API size_t    j2me_core_ssl_read(uintptr_t handle, uint8_t* out_buf, size_t max_len);
J2ME_API size_t    j2me_core_ssl_available(uintptr_t handle);
J2ME_API void      j2me_core_ssl_feed_input(uintptr_t handle, const uint8_t* data, size_t len);
J2ME_API bool      j2me_core_ssl_get_security_info(uintptr_t handle, char* out_proto_name, size_t proto_cap, char* out_proto_ver, size_t ver_cap, char* out_cipher, size_t cipher_cap, uintptr_t* out_cert_handle);

J2ME_API uintptr_t j2me_core_https_open(const char* url);
J2ME_API void      j2me_core_https_close(uintptr_t handle);
J2ME_API bool      j2me_core_https_is_open(uintptr_t handle);
J2ME_API void      j2me_core_https_set_method(uintptr_t handle, const char* method);
J2ME_API void      j2me_core_https_set_request_property(uintptr_t handle, const char* key, const char* val);
J2ME_API int       j2me_core_https_get_response_code(uintptr_t handle);
J2ME_API int       j2me_core_https_get_port(uintptr_t handle);
J2ME_API void      j2me_core_https_set_response_header(uintptr_t handle, const char* key, const char* val);
J2ME_API bool      j2me_core_https_get_header_field(uintptr_t handle, const char* key, char* out_buf, size_t cap);
J2ME_API void      j2me_core_https_feed_response(uintptr_t handle, int code, const char* body);
J2ME_API size_t    j2me_core_https_read(uintptr_t handle, uint8_t* out_buf, size_t max_len);
J2ME_API size_t    j2me_core_https_write(uintptr_t handle, const uint8_t* data, size_t len);

// --- 30. 3D BINARY ASSET LOADERS (M3G & MICRO3D) ---
J2ME_API int       j2me_core_3d_identify_format(const uint8_t* data, size_t size);
J2ME_API uintptr_t j2me_core_m3g_load_memory(const uint8_t* data, size_t size, size_t* out_root_count, size_t* out_mesh_count);
J2ME_API void      j2me_core_m3g_scene_destroy(uintptr_t handle);
J2ME_API uintptr_t j2me_core_micro3d_load_figure(const uint8_t* data, size_t size);
J2ME_API void      j2me_core_micro3d_figure_destroy(uintptr_t handle);
J2ME_API bool      j2me_core_micro3d_figure_get_counts(uintptr_t handle, int32_t* out_verts, int32_t* out_poly_t3, int32_t* out_poly_t4, int32_t* out_bones);
J2ME_API uintptr_t j2me_core_micro3d_load_action_table(const uint8_t* data, size_t size);
J2ME_API void      j2me_core_micro3d_action_table_destroy(uintptr_t handle);
J2ME_API int32_t   j2me_core_micro3d_action_table_get_count(uintptr_t handle);

// --- 31. LCDUI FONT ENGINE, SYSTEM PROPERTIES & PCM WAV PLAYER ---
// LCDUI Font Engine
J2ME_API uintptr_t j2me_core_font_get_default(void);
J2ME_API uintptr_t j2me_core_font_get(int face, int style, int size);
J2ME_API int       j2me_core_font_get_height(uintptr_t font_handle);
J2ME_API int       j2me_core_font_get_baseline(uintptr_t font_handle);
J2ME_API int       j2me_core_font_char_width(uintptr_t font_handle, char c);
J2ME_API int       j2me_core_font_string_width(uintptr_t font_handle, const char* str);
J2ME_API int       j2me_core_font_get_face(uintptr_t font_handle);
J2ME_API int       j2me_core_font_get_style(uintptr_t font_handle);
J2ME_API int       j2me_core_font_get_size(uintptr_t font_handle);
J2ME_API void      j2me_core_graphics_set_font(J2meEngineInstance* inst, uintptr_t font_handle);
J2ME_API uintptr_t j2me_core_graphics_get_font(J2meEngineInstance* inst);
J2ME_API void      j2me_core_graphics_draw_string(J2meEngineInstance* inst, const char* text, int x, int y, int anchor);
J2ME_API void      j2me_core_graphics_draw_char(J2meEngineInstance* inst, char c, int x, int y, int anchor);

// System Properties Manager
J2ME_API bool      j2me_core_system_get_property(const char* key, char* out_buf, size_t cap);
J2ME_API void      j2me_core_system_set_property(const char* key, const char* val);
J2ME_API bool      j2me_core_system_has_property(const char* key);
J2ME_API void      j2me_core_system_reset_properties(void);
J2ME_API int       j2me_core_system_get_property_count(void);
J2ME_API bool      j2me_core_system_load_properties(const char* prop_content);

// PCM WAV Audio Player
J2ME_API uintptr_t j2me_core_wav_create_memory(const uint8_t* data, size_t size);
J2ME_API uintptr_t j2me_core_wav_create_file(const char* file_path);
J2ME_API void      j2me_core_wav_start(uintptr_t player_handle);
J2ME_API void      j2me_core_wav_stop(uintptr_t player_handle);
J2ME_API void      j2me_core_wav_set_loop(uintptr_t player_handle, int count);
J2ME_API void      j2me_core_wav_set_volume(uintptr_t player_handle, int volume);
J2ME_API int       j2me_core_wav_get_volume(uintptr_t player_handle);
J2ME_API int64_t   j2me_core_wav_get_duration_us(uintptr_t player_handle);
J2ME_API int64_t   j2me_core_wav_get_media_time_us(uintptr_t player_handle);
J2ME_API void      j2me_core_wav_set_media_time_us(uintptr_t player_handle, int64_t time_us);
J2ME_API size_t    j2me_core_wav_render_pcm(uintptr_t player_handle, int16_t* out_stereo_pcm, size_t frames);
J2ME_API void      j2me_core_wav_destroy(uintptr_t player_handle);

// --- 32. NATIVE PLATFORM DIALOGS ---
J2ME_API bool      j2me_core_platform_pick_file(char* out_path, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif // J2ME_CORE_H
