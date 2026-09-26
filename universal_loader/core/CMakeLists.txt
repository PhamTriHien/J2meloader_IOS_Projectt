cmake_minimum_required(VERSION 3.20)
project(j2me_core VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Tối ưu hóa hiệu năng cao
if(MSVC)
    add_compile_options(/O2 /W3 /utf-8 /wd4251 /wd4275 /wd4996 /wd4244 /wd4267 /wd4146 /wd4018 /wd4311 /wd4312 /wd4101)
else()
    add_compile_options(-O3 -Wall -Wextra -Wno-unused-parameter)
endif()

# Sonivox EAS definitions
add_compile_definitions(
    UNIFIED_DEBUG_MESSAGES
    EAS_WT_SYNTH
    NUM_OUTPUT_CHANNELS=2
    _SAMPLE_RATE_22050
    MAX_SYNTH_VOICES=64
    _16_BIT_SAMPLES
    _FILTER_ENABLED
    DLS_SYNTHESIZER
    _REVERB_ENABLED
    _IMELODY_PARSER
    _RTTTL_PARSER
    _OTA_PARSER
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/audio/sonivox
)

# Sonivox Synthesizer Sources
set(SONIVOX_SOURCES
    src/audio/sonivox/eas_data.c
    src/audio/sonivox/eas_dlssynth.c
    src/audio/sonivox/eas_flog.c
    src/audio/sonivox/eas_math.c
    src/audio/sonivox/eas_mdls.c
    src/audio/sonivox/eas_midi.c
    src/audio/sonivox/eas_midi_stream.c
    src/audio/sonivox/eas_mididata.c
    src/audio/sonivox/eas_mixbuf.c
    src/audio/sonivox/eas_mixer.c
    src/audio/sonivox/eas_pan.c
    src/audio/sonivox/eas_pcm.c
    src/audio/sonivox/eas_pcmdata.c
    src/audio/sonivox/eas_public.c
    src/audio/sonivox/eas_reverb.c
    src/audio/sonivox/eas_reverbdata.c
    src/audio/sonivox/eas_smf.c
    src/audio/sonivox/eas_smfdata.c
    src/audio/sonivox/eas_voicemgt.c
    src/audio/sonivox/eas_wtengine.c
    src/audio/sonivox/eas_wtsynth.c
    src/audio/sonivox/wt_22khz.c
    src/audio/sonivox/eas_config.c
    src/audio/sonivox/eas_hostmm.c
    src/audio/sonivox/eas_report.c
    src/audio/sonivox/eas_imelody.c
    src/audio/sonivox/eas_imelodydata.c
    src/audio/sonivox/eas_rtttl.c
    src/audio/sonivox/eas_rtttldata.c
    src/audio/sonivox/eas_ota.c
    src/audio/sonivox/eas_otadata.c
)

# Source files
set(CORE_SOURCES
    src/j2me_core_api.cpp
    src/lcdui/frame_buffer.cpp
    src/lcdui/lcdui_graphics.cpp
    src/lcdui/font.cpp
    src/system/system_properties.cpp
    src/audio/wav_player.cpp
    src/jvm/jar_reader.cpp
    src/storage/rms_storage.cpp
    src/network/gcf_network.cpp
    src/network/datagram_connection.cpp
    src/audio/mmapi_audio.cpp
    src/graphics3d/math3d.cpp
    src/graphics3d/micro3d_engine.cpp
    src/graphics3d/m3g_engine.cpp
    src/graphics3d/rasterizer3d.cpp
    src/graphics3d/keyframe_sequence.cpp
    src/graphics3d/animation_controller.cpp
    src/graphics3d/animation_track.cpp
    src/graphics3d/morphing_mesh.cpp
    src/graphics3d/m3g_node.cpp
    src/graphics3d/skinned_mesh.cpp
    src/graphics3d/m3g_loader.cpp
    src/graphics3d/micro3d_loader.cpp
    src/oem/nokia_direct_graphics.cpp
    src/oem/device_control.cpp
    src/oem/vodafone/sprite_engine.cpp
    src/oem/vodafone/sound_player.cpp
    src/oem/vodafone/vodafone_device_control.cpp
    src/oem/vodafone/vodafone_image_encoder.cpp
    src/oem/carrier_extensions.cpp
    src/location/coordinates.cpp
    src/location/address_info.cpp
    src/location/criteria.cpp
    src/location/location.cpp
    src/location/orientation.cpp
    src/location/landmark_store.cpp
    src/location/location_provider.cpp
    src/sensor/condition.cpp
    src/sensor/channel_info.cpp
    src/sensor/channel.cpp
    src/sensor/data.cpp
    src/sensor/sensor_info.cpp
    src/sensor/sensor_connection.cpp
    src/sensor/sensor_manager.cpp
    src/lcdui/ui/soft_keys_bar.cpp
    src/lcdui/ui/displayable.cpp
    src/lcdui/ui/display.cpp
    src/lcdui/ui/item.cpp
    src/lcdui/ui/form.cpp
    src/lcdui/ui/list.cpp
    src/lcdui/ui/alert.cpp
    src/file/file_system_registry.cpp
    src/file/file_connection.cpp
    src/messaging/wma_message.cpp
    src/messaging/sms_connection.cpp
    src/midlet/app_descriptor.cpp
    src/midlet/midlet.cpp
    src/input/phone_keypad.cpp
    src/jvm/jar_resource_loader.cpp
    src/config/app_profile_config.cpp
    src/jvm/class_file.cpp
    src/jvm/cldc_vm.cpp
    src/lcdui/game/tiled_layer.cpp
    src/lcdui/game/layer_manager.cpp
    src/app/app_item.cpp
    src/app/app_repository.cpp
    src/app/app_installer.cpp
    src/network/bluetooth/bluetooth_uuid.cpp
    src/network/bluetooth/bluetooth_data_element.cpp
    src/network/bluetooth/bluetooth_service_record.cpp
    src/network/bluetooth/bluetooth_device.cpp
    src/network/bluetooth/btspp_connection.cpp
    src/network/bluetooth/btl2cap_connection.cpp
    src/network/bluetooth/obex_headers.cpp
    src/pim/repeat_rule.cpp
    src/pim/pim_item.cpp
    src/pim/contact.cpp
    src/pim/event.cpp
    src/pim/todo.cpp
    src/pim/pim_list.cpp
    src/pim/pim_manager.cpp
    src/amms/audio3d_controls.cpp
    src/amms/sound_source_3d.cpp
    src/amms/audio_effects.cpp
    src/amms/camera_controls.cpp
    src/amms/global_manager.cpp
    src/push/push_registry.cpp
    src/comm/comm_connection.cpp
    src/security/secure_connection.cpp
    ${SONIVOX_SOURCES}
)

# Build Shared Library
add_library(j2me_core SHARED ${CORE_SOURCES})

target_compile_definitions(j2me_core PRIVATE J2ME_CORE_EXPORTS)

if(WIN32)
    target_link_libraries(j2me_core PRIVATE ws2_32)
endif()

set_target_properties(j2me_core PROPERTIES
    OUTPUT_NAME "j2me_core"
    PREFIX ""
)

# Test executable
add_executable(test_core test_main.cpp)
target_link_libraries(test_core PRIVATE j2me_core)
if(WIN32)
    target_link_libraries(test_core PRIVATE ws2_32)
endif()

# Export headers
install(TARGETS j2me_core
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES include/j2me_core.h DESTINATION include)
