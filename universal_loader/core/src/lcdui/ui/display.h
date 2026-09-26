#pragma once

#include "displayable.h"
#include <memory>
#include <mutex>

namespace universal_loader {
namespace lcdui {

class Alert;

class J2ME_API Display {
public:
    static constexpr int COLOR_BACKGROUND             = 0;
    static constexpr int COLOR_FOREGROUND             = 1;
    static constexpr int COLOR_HIGHLIGHTED_BACKGROUND = 2;
    static constexpr int COLOR_HIGHLIGHTED_FOREGROUND = 3;
    static constexpr int COLOR_BORDER                 = 4;
    static constexpr int COLOR_HIGHLIGHTED_BORDER     = 5;

    static Display& instance();

    void setCurrent(std::shared_ptr<Displayable> next);
    void setCurrent(std::shared_ptr<Alert> alert, std::shared_ptr<Displayable> next);
    std::shared_ptr<Displayable> getCurrent() const;

    int getColor(int colorSpecifier) const;
    int getBorderStyle(bool highlighted) const;

    int numColors() const { return 16777216; }
    int numAlphaLevels() const { return 256; }
    bool isColor() const { return true; }

    bool vibrate(int durationMs);
    bool flashBacklight(int durationMs);

    void setScreenSize(int width, int height);
    int getWidth() const { return m_screenWidth; }
    int getHeight() const { return m_screenHeight; }

private:
    Display();
    ~Display() = default;

    int m_screenWidth;
    int m_screenHeight;
    std::shared_ptr<Displayable> m_current;
    std::shared_ptr<Displayable> m_afterAlert;
    mutable std::mutex m_mutex;
};

} // namespace lcdui
} // namespace universal_loader
