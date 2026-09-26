#include "soft_keys_bar.h"
#include <algorithm>

namespace universal_loader {
namespace lcdui {

SoftKeysBar::SoftKeysBar(bool middleSoft)
    : m_middleSoft(middleSoft),
      m_hasLeft(false),
      m_isLeftMenu(false),
      m_hasRight(false),
      m_hasMiddle(false) {}

void SoftKeysBar::update(const std::vector<Command>& rawCommands) {
    m_hasLeft = false;
    m_isLeftMenu = false;
    m_hasRight = false;
    m_hasMiddle = false;
    m_menuCommands.clear();

    if (rawCommands.empty()) {
        return;
    }

    // 1. Sort all commands according to Command::compareTo
    std::vector<Command> sorted = rawCommands;
    std::sort(sorted.begin(), sorted.end());

    std::optional<Command> middleOpt;
    std::optional<Command> rightOpt;

    // 2. Extract first OK -> middle, first BACK or EXIT -> right
    std::vector<Command> remaining;
    for (const auto& cmd : sorted) {
        int type = cmd.getCommandType();
        if (type == CMD_OK && !middleOpt.has_value()) {
            middleOpt = cmd;
        } else if ((type == CMD_BACK || type == CMD_EXIT) && !rightOpt.has_value()) {
            rightOpt = cmd;
        } else {
            remaining.push_back(cmd);
        }
    }

    std::vector<Command> commands;
    int menuStartIndex = 0;

    if (middleOpt.has_value()) {
        commands.insert(commands.begin(), middleOpt.value());
        if (rawCommands.size() == 1 || !m_middleSoft) {
            middleOpt.reset();
        } else {
            menuStartIndex++;
        }
    }

    if (rightOpt.has_value()) {
        commands.insert(commands.begin(), rightOpt.value());
        menuStartIndex++;
    }

    commands.insert(commands.end(), remaining.begin(), remaining.end());

    // 3. Setup Left softkey
    int size = static_cast<int>(commands.size());
    if (size - menuStartIndex > 1) {
        m_hasLeft = true;
        m_isLeftMenu = true;
        m_leftCommand = Command("Menu", CMD_SCREEN, 0);
        for (int i = menuStartIndex; i < size; ++i) {
            m_menuCommands.push_back(commands[i]);
        }
    } else if (menuStartIndex < size) {
        m_hasLeft = true;
        m_isLeftMenu = false;
        m_leftCommand = commands[menuStartIndex];
    }

    // 4. Setup Right and Middle softkeys
    if (rightOpt.has_value()) {
        m_hasRight = true;
        m_rightCommand = rightOpt.value();
        if (middleOpt.has_value()) {
            m_hasMiddle = true;
            m_middleCommand = middleOpt.value();
        }
    } else {
        if (middleOpt.has_value()) {
            m_hasRight = true;
            m_rightCommand = middleOpt.value();
        }
    }
}

} // namespace lcdui
} // namespace universal_loader
