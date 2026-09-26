#pragma once

#include "command.h"
#include <vector>
#include <optional>

namespace universal_loader {
namespace lcdui {

class J2ME_API SoftKeysBar {
public:
    explicit SoftKeysBar(bool middleSoft = true);

    void update(const std::vector<Command>& rawCommands);

    bool hasLeft() const { return m_hasLeft; }
    bool isLeftMenu() const { return m_isLeftMenu; }
    const Command& getLeftCommand() const { return m_leftCommand; }

    bool hasRight() const { return m_hasRight; }
    const Command& getRightCommand() const { return m_rightCommand; }

    bool hasMiddle() const { return m_hasMiddle; }
    const Command& getMiddleCommand() const { return m_middleCommand; }

    const std::vector<Command>& getMenuCommands() const { return m_menuCommands; }

    void setMiddleSoft(bool enabled) { m_middleSoft = enabled; }
    bool isMiddleSoft() const { return m_middleSoft; }

private:
    bool m_middleSoft;
    bool m_hasLeft;
    bool m_isLeftMenu;
    Command m_leftCommand;

    bool m_hasRight;
    Command m_rightCommand;

    bool m_hasMiddle;
    Command m_middleCommand;

    std::vector<Command> m_menuCommands;
};

} // namespace lcdui
} // namespace universal_loader
