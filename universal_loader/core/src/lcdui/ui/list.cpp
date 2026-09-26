#include "list.h"
#include <algorithm>

namespace universal_loader {
namespace lcdui {

const Command List::SELECT_COMMAND = Command("", CMD_SCREEN, 0);

List::List(std::string title, int listType)
    : Displayable(),
      m_listType(listType),
      m_fitPolicy(TEXT_WRAP_DEFAULT),
      m_selectCommand(SELECT_COMMAND),
      m_focusedIndex(0),
      m_scrollY(0) {
    m_title = std::move(title);
}

List::List(std::string title, int listType, const std::vector<std::string>& stringElements,
           const std::vector<std::shared_ptr<j2me::LcduiImage>>& imageElements)
    : List(std::move(title), listType) {
    for (size_t i = 0; i < stringElements.size(); ++i) {
        std::shared_ptr<j2me::LcduiImage> img = (i < imageElements.size()) ? imageElements[i] : nullptr;
        append(stringElements[i], img);
    }
}

int List::append(std::string stringPart, std::shared_ptr<j2me::LcduiImage> imagePart) {
    bool initialSelect = (m_elements.empty() && m_listType != MULTIPLE);
    m_elements.push_back({std::move(stringPart), std::move(imagePart), initialSelect});
    return static_cast<int>(m_elements.size()) - 1;
}

void List::insert(int elementNum, std::string stringPart, std::shared_ptr<j2me::LcduiImage> imagePart) {
    if (elementNum < 0) elementNum = 0;
    if (elementNum > static_cast<int>(m_elements.size())) elementNum = static_cast<int>(m_elements.size());
    m_elements.insert(m_elements.begin() + elementNum, {std::move(stringPart), std::move(imagePart), false});
}

void List::deleteElement(int elementNum) {
    if (elementNum >= 0 && elementNum < static_cast<int>(m_elements.size())) {
        m_elements.erase(m_elements.begin() + elementNum);
        if (m_focusedIndex >= static_cast<int>(m_elements.size())) {
            m_focusedIndex = std::max(0, static_cast<int>(m_elements.size()) - 1);
        }
    }
}

void List::deleteAll() {
    m_elements.clear();
    m_focusedIndex = 0;
    m_scrollY = 0;
}

void List::set(int elementNum, std::string stringPart, std::shared_ptr<j2me::LcduiImage> imagePart) {
    if (elementNum >= 0 && elementNum < static_cast<int>(m_elements.size())) {
        m_elements[elementNum].stringPart = std::move(stringPart);
        m_elements[elementNum].imagePart = std::move(imagePart);
    }
}

const std::string& List::getString(int elementNum) const {
    static const std::string EMPTY;
    if (elementNum >= 0 && elementNum < static_cast<int>(m_elements.size())) {
        return m_elements[elementNum].stringPart;
    }
    return EMPTY;
}

std::shared_ptr<j2me::LcduiImage> List::getImage(int elementNum) const {
    if (elementNum >= 0 && elementNum < static_cast<int>(m_elements.size())) {
        return m_elements[elementNum].imagePart;
    }
    return nullptr;
}

int List::getSelectedIndex() const {
    for (size_t i = 0; i < m_elements.size(); ++i) {
        if (m_elements[i].selected) return static_cast<int>(i);
    }
    return -1;
}

void List::setSelectedIndex(int elementNum, bool selected) {
    if (elementNum < 0 || elementNum >= static_cast<int>(m_elements.size())) return;

    if (m_listType == EXCLUSIVE || m_listType == IMPLICIT) {
        for (auto& elem : m_elements) elem.selected = false;
        m_elements[elementNum].selected = selected;
    } else {
        m_elements[elementNum].selected = selected;
    }
}

bool List::isSelected(int elementNum) const {
    if (elementNum >= 0 && elementNum < static_cast<int>(m_elements.size())) {
        return m_elements[elementNum].selected;
    }
    return false;
}

int List::getSelectedFlags(std::vector<bool>& result) const {
    result.resize(m_elements.size());
    int count = 0;
    for (size_t i = 0; i < m_elements.size(); ++i) {
        result[i] = m_elements[i].selected;
        if (result[i]) count++;
    }
    return count;
}

void List::setSelectedFlags(const std::vector<bool>& flags) {
    size_t n = std::min(m_elements.size(), flags.size());
    for (size_t i = 0; i < n; ++i) {
        m_elements[i].selected = flags[i];
    }
}

void List::keyPressed(int keyCode) {
    Displayable::keyPressed(keyCode);
    if (m_menuOpen || m_elements.empty()) return;

    if (keyCode == 1) { // UP
        if (m_focusedIndex > 0) m_focusedIndex--;
    } else if (keyCode == 6) { // DOWN
        if (m_focusedIndex + 1 < static_cast<int>(m_elements.size())) m_focusedIndex++;
    } else if (keyCode == -5) { // FIRE / SELECT
        if (m_listType == IMPLICIT) {
            setSelectedIndex(m_focusedIndex, true);
            fireCommandAction(m_selectCommand);
        } else if (m_listType == EXCLUSIVE) {
            setSelectedIndex(m_focusedIndex, true);
        } else if (m_listType == MULTIPLE) {
            setSelectedIndex(m_focusedIndex, !m_elements[m_focusedIndex].selected);
        }
    }
}

void List::pointerPressed(int x, int y) {
    Displayable::pointerPressed(x, y);
    if (m_menuOpen) return;

    int contentTop = 0;
    if (!m_title.empty()) contentTop += 22;
    if (!m_ticker.empty()) contentTop += 16;
    int contentBottom = m_height - 22;

    if (y >= contentTop && y < contentBottom) {
        int itemH = 26;
        int idx = (y - contentTop + m_scrollY) / itemH;
        if (idx >= 0 && idx < static_cast<int>(m_elements.size())) {
            m_focusedIndex = idx;
            if (m_listType == IMPLICIT) {
                setSelectedIndex(idx, true);
                fireCommandAction(m_selectCommand);
            } else if (m_listType == EXCLUSIVE) {
                setSelectedIndex(idx, true);
            } else if (m_listType == MULTIPLE) {
                setSelectedIndex(idx, !m_elements[idx].selected);
            }
        }
    }
}

void List::paint(j2me::LcduiGraphics* g) {
    if (!g) return;

    // Background
    g->setColor(0xFFF5F5F5);
    g->fillRect(0, 0, m_width, m_height);

    int contentTop = 0;
    int contentBottom = m_height;
    renderTitleAndTicker(g, contentTop);
    renderSoftKeys(g, contentBottom);

    int viewH = contentBottom - contentTop;
    if (viewH <= 0) return;

    g->setClip(0, contentTop, m_width, viewH);

    int itemH = 26;
    int curY = contentTop - m_scrollY;

    for (size_t i = 0; i < m_elements.size(); ++i) {
        const auto& elem = m_elements[i];
        bool focused = (static_cast<int>(i) == m_focusedIndex);

        if (curY + itemH >= contentTop && curY <= contentBottom) {
            if (focused) {
                g->setColor(0xFF0055BB); // Deep Blue selection
                g->fillRect(2, curY, m_width - 4, itemH - 2);
            } else {
                g->setColor((i % 2 == 0) ? 0xFFFFFFFF : 0xFFEEEEEE);
                g->fillRect(2, curY, m_width - 4, itemH - 2);
            }

            int contentX = 8;
            // Indicator
            if (m_listType == EXCLUSIVE) {
                g->setColor(focused ? 0xFFFFFFFF : 0xFF333333);
                g->drawArc(contentX, curY + 6, 12, 12, 0, 360);
                if (elem.selected) {
                    g->setColor(focused ? 0xFFFFFFFF : 0xFF0000AA);
                    g->fillArc(contentX + 3, curY + 9, 6, 6, 0, 360);
                }
                contentX += 18;
            } else if (m_listType == MULTIPLE) {
                g->setColor(focused ? 0xFFFFFFFF : 0xFF333333);
                g->drawRect(contentX, curY + 6, 12, 12);
                if (elem.selected) {
                    g->setColor(focused ? 0xFFFFFFFF : 0xFF0000AA);
                    g->fillRect(contentX + 3, curY + 9, 6, 6);
                }
                contentX += 18;
            }

            // Image
            if (elem.imagePart) {
                g->drawImage(elem.imagePart.get(), contentX, curY + 3, 0);
                contentX += elem.imagePart->getWidth() + 6;
            }

            // Text bar indicator
            g->setColor(focused ? 0xFFFFFFFF : 0xFF222222);
            int textW = std::min(m_width - contentX - 10, static_cast<int>(elem.stringPart.size()) * 7 + 10);
            g->fillRect(contentX, curY + itemH / 2 - 2, textW, 4);
        }
        curY += itemH;
    }

    g->setClip(0, 0, m_width, m_height);
}

} // namespace lcdui
} // namespace universal_loader
