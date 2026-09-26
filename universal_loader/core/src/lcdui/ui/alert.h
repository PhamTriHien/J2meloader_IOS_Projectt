#pragma once

#include "displayable.h"
#include "item.h"
#include <string>
#include <memory>

namespace universal_loader {
namespace lcdui {

class J2ME_API AlertType {
public:
    static constexpr int ALARM        = 1;
    static constexpr int CONFIRMATION = 2;
    static constexpr int ERROR_TYPE   = 3;
    static constexpr int INFO         = 4;
    static constexpr int WARNING      = 5;

    explicit AlertType(int type) : m_type(type) {}
    int getType() const { return m_type; }

    uint32_t getHeaderColor() const {
        switch (m_type) {
            case ERROR_TYPE:   return 0xFFCC2222; // Crimson Red
            case WARNING:      return 0xFFDD8800; // Amber
            case CONFIRMATION: return 0xFF009944; // Green
            case ALARM:        return 0xFFAA0066; // Magenta/Crimson
            case INFO:
            default:           return 0xFF0066CC; // Ocean Blue
        }
    }

private:
    int m_type;
};

class J2ME_API Alert : public Displayable {
public:
    static constexpr int FOREVER = -2;
    static const Command DISMISS_COMMAND;

    explicit Alert(std::string title);
    Alert(std::string title, std::string alertText, std::shared_ptr<j2me::LcduiImage> alertImage, int alertType);
    ~Alert() override = default;

    const std::string& getString() const { return m_text; }
    void setString(std::string str) { m_text = std::move(str); }

    std::shared_ptr<j2me::LcduiImage> getImage() const { return m_image; }
    void setImage(std::shared_ptr<j2me::LcduiImage> img) { m_image = std::move(img); }

    std::shared_ptr<Gauge> getIndicator() const { return m_indicator; }
    void setIndicator(std::shared_ptr<Gauge> indicator) { m_indicator = std::move(indicator); }

    int getType() const { return m_type.getType(); }
    void setType(int type) { m_type = AlertType(type); }

    int getTimeout() const { return m_timeout; }
    void setTimeout(int time) { m_timeout = time; }
    int getDefaultTimeout() const { return 2000; }

    void paint(j2me::LcduiGraphics* g) override;
    void keyPressed(int keyCode) override;

private:
    std::string m_text;
    std::shared_ptr<j2me::LcduiImage> m_image;
    std::shared_ptr<Gauge> m_indicator;
    AlertType m_type;
    int m_timeout;
};

} // namespace lcdui
} // namespace universal_loader
