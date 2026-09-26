#include "camera_controls.h"
#include <algorithm>

namespace universal_loader {
namespace amms {

// -------------------------------------------------------------
// CameraControl
// -------------------------------------------------------------
CameraControl::CameraControl()
    : m_rotation(ROTATE_NONE)
    , m_exposureMode("auto")
    , m_stillResIndex(0)
    , m_videoResIndex(0)
    , m_shutterFeedback(true)
{
    m_stillResolutions.push_back({640, 480});
    m_stillResolutions.push_back({1280, 960});
    m_stillResolutions.push_back({1920, 1080});

    m_videoResolutions.push_back({176, 144});
    m_videoResolutions.push_back({320, 240});
    m_videoResolutions.push_back({640, 480});
}

void CameraControl::setCameraRotation(int rotation) {
    if (rotation == ROTATE_NONE || rotation == ROTATE_LEFT || rotation == ROTATE_RIGHT) {
        m_rotation = rotation;
    }
}

void CameraControl::setExposureMode(const std::string& mode) {
    auto supported = getSupportedExposureModes();
    if (std::find(supported.begin(), supported.end(), mode) != supported.end()) {
        m_exposureMode = mode;
    }
}

std::vector<std::string> CameraControl::getSupportedExposureModes() const {
    return {"auto", "night", "sports", "portrait"};
}

void CameraControl::setStillResolution(int index) {
    if (index >= 0 && index < static_cast<int>(m_stillResolutions.size())) {
        m_stillResIndex = index;
    }
}

std::vector<Dimension2D> CameraControl::getSupportedStillResolutions() const {
    return m_stillResolutions;
}

void CameraControl::setVideoResolution(int index) {
    if (index >= 0 && index < static_cast<int>(m_videoResolutions.size())) {
        m_videoResIndex = index;
    }
}

std::vector<Dimension2D> CameraControl::getSupportedVideoResolutions() const {
    return m_videoResolutions;
}

// -------------------------------------------------------------
// FlashControl
// -------------------------------------------------------------
FlashControl::FlashControl()
    : m_mode(FLASH_AUTO)
{
}

void FlashControl::setMode(int mode) {
    auto supported = getSupportedModes();
    if (std::find(supported.begin(), supported.end(), mode) != supported.end()) {
        m_mode = mode;
    }
}

std::vector<int> FlashControl::getSupportedModes() const {
    return {FLASH_OFF, FLASH_AUTO, FLASH_AUTO_WITH_REDEYEREDUCE, FLASH_FORCE, FLASH_FORCE_WITH_REDEYEREDUCE, FLASH_FILLIN};
}

// -------------------------------------------------------------
// ZoomControl
// -------------------------------------------------------------
ZoomControl::ZoomControl()
    : m_digitalZoom(100)
    , m_opticalZoom(100)
{
}

int ZoomControl::setDigitalZoom(int level) {
    m_digitalZoom = std::clamp(level, 100, getMaxDigitalZoom());
    return m_digitalZoom;
}

int ZoomControl::setOpticalZoom(int level) {
    m_opticalZoom = std::clamp(level, 100, getMaxOpticalZoom());
    return m_opticalZoom;
}

// -------------------------------------------------------------
// ImageTransformControl
// -------------------------------------------------------------
ImageTransformControl::ImageTransformControl()
    : m_srcX(0)
    , m_srcY(0)
    , m_srcW(640)
    , m_srcH(480)
    , m_targetW(640)
    , m_targetH(480)
{
}

void ImageTransformControl::setSourceRect(int x, int y, int width, int height) {
    m_srcX = (x >= 0) ? x : 0;
    m_srcY = (y >= 0) ? y : 0;
    m_srcW = (width > 0) ? width : 1;
    m_srcH = (height > 0) ? height : 1;
}

void ImageTransformControl::setTargetSize(int width, int height) {
    m_targetW = (width > 0) ? width : 1;
    m_targetH = (height > 0) ? height : 1;
}

} // namespace amms
} // namespace universal_loader
