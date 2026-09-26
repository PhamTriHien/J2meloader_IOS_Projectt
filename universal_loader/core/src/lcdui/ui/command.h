#pragma once

#include "j2me_core.h"
#include <string>
#include <memory>
#include <utility>

namespace universal_loader {
namespace lcdui {

class Displayable;

enum CommandType {
    CMD_SCREEN = 1,
    CMD_BACK   = 2,
    CMD_CANCEL = 3,
    CMD_OK     = 4,
    CMD_HELP   = 5,
    CMD_STOP   = 6,
    CMD_EXIT   = 7,
    CMD_ITEM   = 8
};

class J2ME_API Command {
public:
    Command() : m_commandType(CMD_SCREEN), m_priority(0) {}

    Command(std::string label, int commandType, int priority)
        : m_shortLabel(std::move(label)), m_commandType(commandType), m_priority(priority) {}

    Command(std::string shortLabel, std::string longLabel, int commandType, int priority)
        : m_shortLabel(std::move(shortLabel)), m_longLabel(std::move(longLabel)),
          m_commandType(commandType), m_priority(priority) {}

    const std::string& getLabel() const { return m_shortLabel; }
    const std::string& getLongLabel() const { return m_longLabel; }
    int getCommandType() const { return m_commandType; }
    int getPriority() const { return m_priority; }

    std::string getDisplayLabel() const {
        if (!m_shortLabel.empty()) {
            return m_shortLabel;
        }
        switch (m_commandType) {
            case CMD_SCREEN: return "Screen";
            case CMD_BACK:   return "Back";
            case CMD_CANCEL: return "Cancel";
            case CMD_OK:     return "OK";
            case CMD_HELP:   return "Help";
            case CMD_STOP:   return "Stop";
            case CMD_EXIT:   return "Exit";
            case CMD_ITEM:   return "Item";
            default:         return "";
        }
    }

    int compareTo(const Command& other) const {
        int p = m_commandType - other.m_commandType;
        if (p != 0) return p;
        return m_priority - other.m_priority;
    }

    bool operator<(const Command& other) const {
        return compareTo(other) < 0;
    }

    bool operator==(const Command& other) const {
        return m_shortLabel == other.m_shortLabel &&
               m_longLabel == other.m_longLabel &&
               m_commandType == other.m_commandType &&
               m_priority == other.m_priority;
    }

    bool operator!=(const Command& other) const {
        return !(*this == other);
    }

private:
    std::string m_shortLabel;
    std::string m_longLabel;
    int m_commandType;
    int m_priority;
};

class J2ME_API CommandListener {
public:
    virtual ~CommandListener() = default;
    virtual void commandAction(const Command& c, Displayable* d) = 0;
};

} // namespace lcdui
} // namespace universal_loader
