#pragma once

#include "command.h"
#include "soft_keys_bar.h"
#include "../lcdui_graphics.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>

namespace universal_loader {
namespace lcdui {

class Display;

class J2ME_API Displayable {
public:
    Displayable();
    virtual ~Displayable() = default;

    void setTitle(std::string title);
    const std::string& getTitle() const { return m_title; }

    void setTicker(std::string ticker);
    const std::string& getTicker() const { return m_ticker; }

    void addCommand(const Command& cmd);
    void removeCommand(const Command& cmd);
    const std::vector<Command>& getCommands() const { return m_commands; }

    void setCommandListener(CommandListener* listener);
    CommandListener* getCommandListener() const { return m_listener; }

    void fireCommandAction(const Command& cmd);

    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    void setSize(int width, int height);

    const SoftKeysBar& getSoftKeysBar() const { return m_softKeysBar; }
    SoftKeysBar& getSoftKeysBar() { return m_softKeysBar; }

    // Input handlers
    virtual void keyPressed(int keyCode);
    virtual void keyReleased(int keyCode);
    virtual void pointerPressed(int x, int y);
    virtual void pointerReleased(int x, int y);
    virtual void pointerDragged(int x, int y);

    // Lifecycle
    virtual void showNotify() {}
    virtual void hideNotify() {}
    virtual void sizeChanged(int w, int h) {}

    // Rendering into LCDUI Framebuffer
    virtual void paint(j2me::LcduiGraphics* g) = 0;

protected:
    void renderTitleAndTicker(j2me::LcduiGraphics* g, int& contentTop);
    void renderSoftKeys(j2me::LcduiGraphics* g, int& contentBottom);

    std::string m_title;
    std::string m_ticker;
    std::vector<Command> m_commands;
    CommandListener* m_listener;
    SoftKeysBar m_softKeysBar;

    int m_width;
    int m_height;
    mutable std::mutex m_mutex;

    // Menu popup state for softkeys
    bool m_menuOpen;
    int m_selectedMenuItem;
};

} // namespace lcdui
} // namespace universal_loader
