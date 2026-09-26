#include "display.h"
#include "alert.h"
#include "../../oem/device_control.h"

namespace universal_loader {
namespace lcdui {

static const uint32_t SYSTEM_COLORS[] = {
    0xFFD0D0D0u, // COLOR_BACKGROUND
    0xFF000080u, // COLOR_FOREGROUND
    0xFF000080u, // COLOR_HIGHLIGHTED_BACKGROUND
    0xFFFFFFFFu, // COLOR_HIGHLIGHTED_FOREGROUND
    0xFFFFFFFFu, // COLOR_BORDER
    0xFF000080u  // COLOR_HIGHLIGHTED_BORDER
};

Display& Display::instance() {
    static Display s_instance;
    return s_instance;
}

Display::Display()
    : m_screenWidth(240),
      m_screenHeight(320) {}

void Display::setCurrent(std::shared_ptr<Displayable> next) {
    std::shared_ptr<Displayable> old;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        old = m_current;
        m_current = next;
        if (m_current) {
            m_current->setSize(m_screenWidth, m_screenHeight);
        }
    }

    if (old) {
        old->hideNotify();
    }
    if (next) {
        next->showNotify();
    }
}

void Display::setCurrent(std::shared_ptr<Alert> alert, std::shared_ptr<Displayable> next) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_afterAlert = next;
    }
    setCurrent(std::static_pointer_cast<Displayable>(alert));
}

std::shared_ptr<Displayable> Display::getCurrent() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_current;
}

int Display::getColor(int colorSpecifier) const {
    if (colorSpecifier >= 0 && colorSpecifier <= 5) {
        return static_cast<int>(SYSTEM_COLORS[colorSpecifier]);
    }
    return static_cast<int>(0xFF000000u);
}

int Display::getBorderStyle(bool highlighted) const {
    // 0 = SOLID, 1 = DOTTED
    return highlighted ? 0 : 1;
}

bool Display::vibrate(int durationMs) {
    oem::DeviceControlManager::instance().vibrate(durationMs);
    return true;
}

bool Display::flashBacklight(int durationMs) {
    oem::DeviceControlManager::instance().flashLights(durationMs);
    return true;
}

void Display::setScreenSize(int width, int height) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_screenWidth = width;
    m_screenHeight = height;
    if (m_current) {
        m_current->setSize(width, height);
    }
}

} // namespace lcdui
} // namespace universal_loader
