#ifndef J2ME_GAME_LAYER_H
#define J2ME_GAME_LAYER_H

#include "../lcdui_graphics.h"
#include <memory>
#include <stdexcept>

namespace universal_loader::lcdui::game {

class J2ME_API Layer {
public:
    Layer(int width, int height) {
        setWidthImpl(width);
        setHeightImpl(height);
    }
    virtual ~Layer() = default;

    virtual void setPosition(int x, int y) {
        m_x = x;
        m_y = y;
    }

    virtual void move(int dx, int dy) {
        m_x += dx;
        m_y += dy;
    }

    int getX() const { return m_x; }
    int getY() const { return m_y; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }

    void setVisible(bool visible) { m_visible = visible; }
    bool isVisible() const { return m_visible; }

    virtual void paint(j2me::LcduiGraphics* g) = 0;

protected:
    void setWidthImpl(int width) {
        if (width < 0) throw std::invalid_argument("Width must not be negative");
        m_width = width;
    }

    void setHeightImpl(int height) {
        if (height < 0) throw std::invalid_argument("Height must not be negative");
        m_height = height;
    }

    int m_x{0};
    int m_y{0};
    int m_width{0};
    int m_height{0};
    bool m_visible{true};
};

} // namespace universal_loader::lcdui::game

#endif // J2ME_GAME_LAYER_H
