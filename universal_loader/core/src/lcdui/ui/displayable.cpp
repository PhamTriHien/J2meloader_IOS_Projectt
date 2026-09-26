#include "displayable.h"
#include <algorithm>

namespace universal_loader {
namespace lcdui {

Displayable::Displayable()
    : m_listener(nullptr),
      m_width(240),
      m_height(320),
      m_menuOpen(false),
      m_selectedMenuItem(0) {
    m_softKeysBar.update(m_commands);
}

void Displayable::setTitle(std::string title) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_title = std::move(title);
}

void Displayable::setTicker(std::string ticker) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_ticker = std::move(ticker);
}

void Displayable::addCommand(const Command& cmd) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& c : m_commands) {
        if (c == cmd) return;
    }
    m_commands.push_back(cmd);
    m_softKeysBar.update(m_commands);
}

void Displayable::removeCommand(const Command& cmd) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_commands.erase(
        std::remove_if(m_commands.begin(), m_commands.end(),
                       [&](const Command& c) { return c == cmd; }),
        m_commands.end());
    m_softKeysBar.update(m_commands);
}

void Displayable::setCommandListener(CommandListener* listener) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_listener = listener;
}

void Displayable::fireCommandAction(const Command& cmd) {
    CommandListener* listener = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        listener = m_listener;
    }
    if (listener) {
        listener->commandAction(cmd, this);
    }
}

void Displayable::setSize(int width, int height) {
    if (width > 0 && height > 0) {
        m_width = width;
        m_height = height;
        sizeChanged(width, height);
    }
}

void Displayable::keyPressed(int keyCode) {
    // Key codes standard in J2ME:
    // -6: Left Softkey, -7: Right Softkey, -5: Select/Fire, 1: UP, 6: DOWN
    if (m_menuOpen) {
        const auto& menuItems = m_softKeysBar.getMenuCommands();
        if (keyCode == 1) { // UP
            if (m_selectedMenuItem > 0) m_selectedMenuItem--;
            return;
        } else if (keyCode == 6) { // DOWN
            if (m_selectedMenuItem + 1 < static_cast<int>(menuItems.size())) m_selectedMenuItem++;
            return;
        } else if (keyCode == -5 || keyCode == -6) { // Select or Soft1 in menu
            if (m_selectedMenuItem >= 0 && m_selectedMenuItem < static_cast<int>(menuItems.size())) {
                Command chosen = menuItems[m_selectedMenuItem];
                m_menuOpen = false;
                fireCommandAction(chosen);
            }
            return;
        } else if (keyCode == -7) { // Soft2 closes menu
            m_menuOpen = false;
            return;
        }
    }

    if (keyCode == -6) { // Left Softkey
        if (m_softKeysBar.isLeftMenu()) {
            m_menuOpen = !m_menuOpen;
            m_selectedMenuItem = 0;
        } else if (m_softKeysBar.hasLeft()) {
            fireCommandAction(m_softKeysBar.getLeftCommand());
        }
    } else if (keyCode == -7) { // Right Softkey
        if (m_softKeysBar.hasRight()) {
            fireCommandAction(m_softKeysBar.getRightCommand());
        }
    } else if (keyCode == -5) { // Middle / Fire
        if (m_softKeysBar.hasMiddle()) {
            fireCommandAction(m_softKeysBar.getMiddleCommand());
        }
    }
}

void Displayable::keyReleased(int keyCode) {
    (void)keyCode;
}

void Displayable::pointerPressed(int x, int y) {
    int barHeight = 22;
    int barY = m_height - barHeight;

    // Check softkeys bar touch
    if (y >= barY) {
        int thirdW = m_width / 3;
        if (x < thirdW) {
            keyPressed(-6); // Left
        } else if (x > m_width - thirdW) {
            keyPressed(-7); // Right
        } else {
            keyPressed(-5); // Middle
        }
        return;
    }

    // Check popup menu touch
    if (m_menuOpen) {
        const auto& menuItems = m_softKeysBar.getMenuCommands();
        int itemH = 18;
        int menuH = static_cast<int>(menuItems.size()) * itemH + 4;
        int menuY = barY - menuH;
        int menuW = std::min(m_width * 3 / 4, 160);

        if (x >= 4 && x <= 4 + menuW && y >= menuY && y < barY) {
            int clickedIdx = (y - menuY - 2) / itemH;
            if (clickedIdx >= 0 && clickedIdx < static_cast<int>(menuItems.size())) {
                Command chosen = menuItems[clickedIdx];
                m_menuOpen = false;
                fireCommandAction(chosen);
            }
            return;
        } else {
            // Click outside closes menu
            m_menuOpen = false;
        }
    }
}

void Displayable::pointerReleased(int x, int y) {
    (void)x; (void)y;
}

void Displayable::pointerDragged(int x, int y) {
    (void)x; (void)y;
}

void Displayable::renderTitleAndTicker(j2me::LcduiGraphics* g, int& contentTop) {
    if (!g) return;

    contentTop = 0;
    if (!m_title.empty()) {
        int titleH = 22;
        g->setColor(0xFF204080); // Deep Blue Title Bar
        g->fillRect(0, 0, m_width, titleH);

        g->setColor(0xFF4070B0); // Highlight top line
        g->drawLine(0, 0, m_width, 0);

        g->setColor(0xFF102040); // Bottom shadow
        g->drawLine(0, titleH - 1, m_width, titleH - 1);

        g->setColor(0xFFFFFFFF); // White text box placeholder
        g->fillRect(4, 5, m_width - 8, titleH - 10);

        contentTop += titleH;
    }

    if (!m_ticker.empty()) {
        int tickerH = 16;
        g->setColor(0xFF333333);
        g->fillRect(0, contentTop, m_width, tickerH);

        g->setColor(0xFFEEEEEE);
        g->fillRect(6, contentTop + 4, m_width - 12, tickerH - 8);

        contentTop += tickerH;
    }
}

void Displayable::renderSoftKeys(j2me::LcduiGraphics* g, int& contentBottom) {
    if (!g) return;

    int barH = 22;
    int barY = m_height - barH;
    contentBottom = barY;

    // Background bar
    g->setColor(0xFF2B2B2B);
    g->fillRect(0, barY, m_width, barH);

    g->setColor(0xFF444444);
    g->drawLine(0, barY, m_width, barY);

    // Left softkey indicator
    if (m_softKeysBar.hasLeft()) {
        g->setColor(0xFFDDDDDD);
        // Visual button rectangle
        g->fillRect(4, barY + 3, m_width / 4, barH - 6);
    }

    // Right softkey indicator
    if (m_softKeysBar.hasRight()) {
        g->setColor(0xFFDDDDDD);
        g->fillRect(m_width - m_width / 4 - 4, barY + 3, m_width / 4, barH - 6);
    }

    // Middle softkey indicator
    if (m_softKeysBar.hasMiddle()) {
        g->setColor(0xFF00AAEE);
        g->fillRect(m_width / 2 - 16, barY + 3, 32, barH - 6);
    }

    // Popup menu render if open
    if (m_menuOpen) {
        const auto& menuItems = m_softKeysBar.getMenuCommands();
        int itemH = 18;
        int menuH = static_cast<int>(menuItems.size()) * itemH + 4;
        int menuY = barY - menuH;
        int menuW = std::min(m_width * 3 / 4, 160);

        // Menu drop shadow
        g->setColor(0x80000000);
        g->fillRect(6, menuY + 2, menuW, menuH);

        // Menu background
        g->setColor(0xFFEEEEEE);
        g->fillRect(4, menuY, menuW, menuH);

        // Border
        g->setColor(0xFF444444);
        g->drawRect(4, menuY, menuW, menuH);

        // Items
        for (size_t i = 0; i < menuItems.size(); ++i) {
            int iy = menuY + 2 + static_cast<int>(i) * itemH;
            if (static_cast<int>(i) == m_selectedMenuItem) {
                g->setColor(0xFF000080); // Blue highlight
                g->fillRect(6, iy, menuW - 4, itemH - 2);
                g->setColor(0xFFFFFFFF);
            } else {
                g->setColor(0xFF333333);
            }
            // Item glyph bar
            g->fillRect(10, iy + 4, menuW - 20, itemH - 10);
        }
    }
}

} // namespace lcdui
} // namespace universal_loader
