#ifndef UNIVERSAL_LOADER_AMMS_CAMERA_CONTROLS_H
#define UNIVERSAL_LOADER_AMMS_CAMERA_CONTROLS_H

#include "amms_types.h"
#include <string>
#include <vector>

namespace universal_loader {
namespace amms {

class J2ME_API CameraControl {
public:
    CameraControl();

    int getCameraRotation() const { return m_rotation; }
    void setCameraRotation(int rotation);

    std::string getExposureMode() const { return m_exposureMode; }
    void setExposureMode(const std::string& mode);
    std::vector<std::string> getSupportedExposureModes() const;

    int getStillResolution() const { return m_stillResIndex; }
    void setStillResolution(int index);
    std::vector<Dimension2D> getSupportedStillResolutions() const;

    int getVideoResolution() const { return m_videoResIndex; }
    void setVideoResolution(int index);
    std::vector<Dimension2D> getSupportedVideoResolutions() const;

    bool isShutterFeedbackEnabled() const { return m_shutterFeedback; }
    void enableShutterFeedback(bool enable) { m_shutterFeedback = enable; }

private:
    int m_rotation{ROTATE_NONE};
    std::string m_exposureMode{"auto"};
    int m_stillResIndex{0};
    int m_videoResIndex{0};
    bool m_shutterFeedback{true};
    std::vector<Dimension2D> m_stillResolutions;
    std::vector<Dimension2D> m_videoResolutions;
};

class J2ME_API FlashControl {
public:
    FlashControl();

    int getMode() const { return m_mode; }
    void setMode(int mode);
    std::vector<int> getSupportedModes() const;
    bool isFlashReady() const { return true; }

private:
    int m_mode{FLASH_AUTO};
};

class J2ME_API ZoomControl {
public:
    ZoomControl();

    int getDigitalZoom() const { return m_digitalZoom; }
    int setDigitalZoom(int level);
    int getMaxDigitalZoom() const { return 400; } // 4.0x
    int getDigitalZoomLevels() const { return 30; }

    int getOpticalZoom() const { return m_opticalZoom; }
    int setOpticalZoom(int level);
    int getMaxOpticalZoom() const { return 200; } // 2.0x
    int getOpticalZoomLevels() const { return 10; }

private:
    int m_digitalZoom{100}; // 1.0x (in 100ths)
    int m_opticalZoom{100}; // 1.0x
};

class J2ME_API ImageTransformControl {
public:
    ImageTransformControl();

    int getSourceX() const { return m_srcX; }
    int getSourceY() const { return m_srcY; }
    int getSourceWidth() const { return m_srcW; }
    int getSourceHeight() const { return m_srcH; }
    void setSourceRect(int x, int y, int width, int height);

    int getTargetWidth() const { return m_targetW; }
    int getTargetHeight() const { return m_targetH; }
    void setTargetSize(int width, int height);

private:
    int m_srcX{0};
    int m_srcY{0};
    int m_srcW{640};
    int m_srcH{480};
    int m_targetW{640};
    int m_targetH{480};
};

} // namespace amms
} // namespace universal_loader

#endif // UNIVERSAL_LOADER_AMMS_CAMERA_CONTROLS_H
