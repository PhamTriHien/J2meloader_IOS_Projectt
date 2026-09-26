#include "item.h"
#include "form.h"
#include <algorithm>
#include <ctime>

namespace universal_loader {
namespace lcdui {

// --- Item Base ---
Item::Item(std::string label)
    : m_label(std::move(label)),
      m_layout(LAYOUT_DEFAULT),
      m_ownerForm(nullptr),
      m_commandListener(nullptr) {}

void Item::addCommand(const Command& cmd) {
    for (const auto& c : m_commands) {
        if (c == cmd) return;
    }
    m_commands.push_back(cmd);
}

void Item::removeCommand(const Command& cmd) {
    m_commands.erase(
        std::remove_if(m_commands.begin(), m_commands.end(),
                       [&](const Command& c) { return c == cmd; }),
        m_commands.end());
}

void Item::setDefaultCommand(const Command& cmd) {
    m_defaultCommand = cmd;
}

void Item::notifyStateChanged() {
    if (m_ownerForm) {
        m_ownerForm->notifyItemStateChanged(this);
    }
}

void Item::keyPressed(int keyCode) {
    (void)keyCode;
}

void Item::pointerPressed(int x, int y, int itemW, int itemH) {
    (void)x; (void)y; (void)itemW; (void)itemH;
}

// --- StringItem ---
StringItem::StringItem(std::string label, std::string text, int appearanceMode)
    : Item(std::move(label)),
      m_text(std::move(text)),
      m_appearanceMode(appearanceMode) {}

int StringItem::getPreferredHeight(int width) const {
    (void)width;
    int h = 18;
    if (!m_label.empty()) h += 14;
    return h;
}

void StringItem::paint(j2me::LcduiGraphics* g, int x, int y, int width, int height, bool focused) {
    if (!g) return;

    int curY = y;
    if (!m_label.empty()) {
        g->setColor(0xFF000080); // Navy label
        g->fillRect(x, curY, std::min(width, 80), 3);
        curY += 14;
    }

    if (m_appearanceMode == BUTTON) {
        g->setColor(focused ? 0xFF0055AA : 0xFFCCCCCC);
        g->fillRoundRect(x, curY, width - 4, height - (curY - y) - 2, 6, 6);
        g->setColor(focused ? 0xFFFFFFFF : 0xFF222222);
        g->fillRect(x + 8, curY + 4, width - 20, 6);
    } else if (m_appearanceMode == HYPERLINK) {
        g->setColor(focused ? 0xFFFF6600 : 0xFF0066CC);
        g->fillRect(x, curY, width - 10, 6);
        g->drawLine(x, curY + 8, x + width - 10, curY + 8);
    } else {
        g->setColor(focused ? 0xFF000080 : 0xFF111111);
        g->fillRect(x, curY, width - 10, 6);
    }
}

// --- TextField ---
TextField::TextField(std::string label, std::string text, int maxSize, int constraints)
    : Item(std::move(label)),
      m_maxSize(maxSize > 0 ? maxSize : 256),
      m_constraints(constraints),
      m_caretPos(0) {
    setString(text);
}

void TextField::setString(std::string text) {
    if (static_cast<int>(text.size()) > m_maxSize) {
        text.resize(static_cast<size_t>(m_maxSize));
    }
    m_text = std::move(text);
    m_caretPos = static_cast<int>(m_text.size());
    notifyStateChanged();
}

void TextField::setMaxSize(int maxSize) {
    if (maxSize > 0) {
        m_maxSize = maxSize;
        if (static_cast<int>(m_text.size()) > m_maxSize) {
            m_text.resize(static_cast<size_t>(m_maxSize));
            m_caretPos = std::min(m_caretPos, m_maxSize);
            notifyStateChanged();
        }
    }
}

void TextField::insert(const std::string& src, int pos) {
    if ((m_constraints & UNEDITABLE) != 0) return;
    if (pos < 0) pos = 0;
    if (pos > static_cast<int>(m_text.size())) pos = static_cast<int>(m_text.size());

    int allowed = m_maxSize - static_cast<int>(m_text.size());
    if (allowed <= 0) return;

    std::string toInsert = src.substr(0, static_cast<size_t>(allowed));
    m_text.insert(static_cast<size_t>(pos), toInsert);
    m_caretPos = pos + static_cast<int>(toInsert.size());
    notifyStateChanged();
}

void TextField::deleteChar(int pos) {
    if ((m_constraints & UNEDITABLE) != 0) return;
    if (pos >= 0 && pos < static_cast<int>(m_text.size())) {
        m_text.erase(static_cast<size_t>(pos), 1);
        if (m_caretPos > pos) m_caretPos--;
        notifyStateChanged();
    }
}

int TextField::getPreferredHeight(int width) const {
    (void)width;
    int h = 26;
    if (!m_label.empty()) h += 14;
    return h;
}

void TextField::paint(j2me::LcduiGraphics* g, int x, int y, int width, int height, bool focused) {
    if (!g) return;

    int curY = y;
    if (!m_label.empty()) {
        g->setColor(0xFF000080);
        g->fillRect(x, curY, std::min(width, 70), 3);
        curY += 14;
    }

    int boxH = height - (curY - y) - 2;
    int boxW = width - 4;

    // Background
    g->setColor(0xFFFFFFFF);
    g->fillRect(x, curY, boxW, boxH);

    // Border
    g->setColor(focused ? 0xFF0066CC : 0xFF888888);
    g->drawRect(x, curY, boxW, boxH);

    // Text / asterisks representation
    int textLen = static_cast<int>(m_text.size());
    if (textLen > 0) {
        g->setColor(0xFF222222);
        int barW = std::min(boxW - 12, textLen * 6);
        g->fillRect(x + 4, curY + boxH / 2 - 2, barW, 4);
    }

    // Caret
    if (focused) {
        int caretX = x + 4 + std::min(boxW - 14, m_caretPos * 6);
        g->setColor(0xFF0000FF);
        g->drawLine(caretX, curY + 3, caretX, curY + boxH - 3);
    }
}

void TextField::keyPressed(int keyCode) {
    if ((m_constraints & UNEDITABLE) != 0) return;

    // Printable ASCII
    if (keyCode >= 32 && keyCode <= 126) {
        char ch = static_cast<char>(keyCode);
        if ((m_constraints & NUMERIC) && (ch < '0' || ch > '9')) return;
        insert(std::string(1, ch), m_caretPos);
    } else if (keyCode == 8) { // Backspace
        if (m_caretPos > 0) {
            deleteChar(m_caretPos - 1);
        }
    } else if (keyCode == 2) { // LEFT
        if (m_caretPos > 0) m_caretPos--;
    } else if (keyCode == 5) { // RIGHT
        if (m_caretPos < static_cast<int>(m_text.size())) m_caretPos++;
    }
}

// --- ChoiceGroup ---
ChoiceGroup::ChoiceGroup(std::string label, int choiceType)
    : Item(std::move(label)),
      m_choiceType(choiceType),
      m_hoverIndex(0) {}

int ChoiceGroup::append(std::string stringPart, std::shared_ptr<j2me::LcduiImage> imagePart) {
    m_elements.push_back({std::move(stringPart), std::move(imagePart), false});
    return static_cast<int>(m_elements.size()) - 1;
}

void ChoiceGroup::insert(int elementNum, std::string stringPart, std::shared_ptr<j2me::LcduiImage> imagePart) {
    if (elementNum < 0) elementNum = 0;
    if (elementNum > static_cast<int>(m_elements.size())) elementNum = static_cast<int>(m_elements.size());
    m_elements.insert(m_elements.begin() + elementNum, {std::move(stringPart), std::move(imagePart), false});
}

void ChoiceGroup::deleteElement(int elementNum) {
    if (elementNum >= 0 && elementNum < static_cast<int>(m_elements.size())) {
        m_elements.erase(m_elements.begin() + elementNum);
        notifyStateChanged();
    }
}

void ChoiceGroup::deleteAll() {
    m_elements.clear();
    notifyStateChanged();
}

bool ChoiceGroup::isSelected(int elementNum) const {
    if (elementNum >= 0 && elementNum < static_cast<int>(m_elements.size())) {
        return m_elements[elementNum].selected;
    }
    return false;
}

void ChoiceGroup::setSelectedIndex(int elementNum, bool selected) {
    if (elementNum < 0 || elementNum >= static_cast<int>(m_elements.size())) return;

    if (m_choiceType == EXCLUSIVE || m_choiceType == POPUP) {
        for (auto& elem : m_elements) elem.selected = false;
        m_elements[elementNum].selected = selected;
    } else {
        m_elements[elementNum].selected = selected;
    }
    notifyStateChanged();
}

int ChoiceGroup::getSelectedIndex() const {
    for (size_t i = 0; i < m_elements.size(); ++i) {
        if (m_elements[i].selected) return static_cast<int>(i);
    }
    return -1;
}

int ChoiceGroup::getSelectedFlags(std::vector<bool>& result) const {
    result.resize(m_elements.size());
    int count = 0;
    for (size_t i = 0; i < m_elements.size(); ++i) {
        result[i] = m_elements[i].selected;
        if (result[i]) count++;
    }
    return count;
}

void ChoiceGroup::setSelectedFlags(const std::vector<bool>& flags) {
    size_t n = std::min(m_elements.size(), flags.size());
    for (size_t i = 0; i < n; ++i) {
        m_elements[i].selected = flags[i];
    }
    notifyStateChanged();
}

const std::string& ChoiceGroup::getString(int elementNum) const {
    static const std::string EMPTY;
    if (elementNum >= 0 && elementNum < static_cast<int>(m_elements.size())) {
        return m_elements[elementNum].stringPart;
    }
    return EMPTY;
}

std::shared_ptr<j2me::LcduiImage> ChoiceGroup::getImage(int elementNum) const {
    if (elementNum >= 0 && elementNum < static_cast<int>(m_elements.size())) {
        return m_elements[elementNum].imagePart;
    }
    return nullptr;
}

int ChoiceGroup::getPreferredHeight(int width) const {
    (void)width;
    int h = 6;
    if (!m_label.empty()) h += 14;
    h += static_cast<int>(m_elements.size()) * 20;
    return std::max(h, 24);
}

void ChoiceGroup::paint(j2me::LcduiGraphics* g, int x, int y, int width, int height, bool focused) {
    (void)height;
    if (!g) return;

    int curY = y;
    if (!m_label.empty()) {
        g->setColor(0xFF000080);
        g->fillRect(x, curY, std::min(width, 80), 3);
        curY += 14;
    }

    for (size_t i = 0; i < m_elements.size(); ++i) {
        const auto& elem = m_elements[i];
        bool isElemFocused = focused && (static_cast<int>(i) == m_hoverIndex);

        if (isElemFocused) {
            g->setColor(0xFFE0E8FF);
            g->fillRect(x, curY, width - 4, 18);
        }

        // Indicator
        int boxX = x + 4;
        int boxY = curY + 3;
        if (m_choiceType == EXCLUSIVE || m_choiceType == POPUP) {
            // Radio circle
            g->setColor(0xFF444444);
            g->drawArc(boxX, boxY, 11, 11, 0, 360);
            if (elem.selected) {
                g->setColor(0xFF0000AA);
                g->fillArc(boxX + 2, boxY + 2, 7, 7, 0, 360);
            }
        } else {
            // Checkbox rect
            g->setColor(0xFF444444);
            g->drawRect(boxX, boxY, 11, 11);
            if (elem.selected) {
                g->setColor(0xFF0000AA);
                g->fillRect(boxX + 2, boxY + 2, 7, 7);
            }
        }

        // Text representation
        g->setColor(0xFF111111);
        g->fillRect(boxX + 16, boxY + 3, std::min(width - 30, static_cast<int>(elem.stringPart.size()) * 7 + 10), 4);

        curY += 20;
    }
}

void ChoiceGroup::keyPressed(int keyCode) {
    if (m_elements.empty()) return;

    if (keyCode == 1) { // UP
        if (m_hoverIndex > 0) m_hoverIndex--;
    } else if (keyCode == 6) { // DOWN
        if (m_hoverIndex + 1 < static_cast<int>(m_elements.size())) m_hoverIndex++;
    } else if (keyCode == -5) { // FIRE / Select
        if (m_choiceType == EXCLUSIVE || m_choiceType == POPUP) {
            setSelectedIndex(m_hoverIndex, true);
        } else {
            setSelectedIndex(m_hoverIndex, !m_elements[m_hoverIndex].selected);
        }
    }
}

void ChoiceGroup::pointerPressed(int x, int y, int itemW, int itemH) {
    (void)x; (void)itemW; (void)itemH;
    int offset = m_label.empty() ? 0 : 14;
    int idx = (y - offset) / 20;
    if (idx >= 0 && idx < static_cast<int>(m_elements.size())) {
        m_hoverIndex = idx;
        if (m_choiceType == EXCLUSIVE || m_choiceType == POPUP) {
            setSelectedIndex(idx, true);
        } else {
            setSelectedIndex(idx, !m_elements[idx].selected);
        }
    }
}

// --- Gauge ---
Gauge::Gauge(std::string label, bool interactive, int maxValue, int initialValue)
    : Item(std::move(label)),
      m_interactive(interactive),
      m_maxValue(maxValue > 0 ? maxValue : 100),
      m_value(std::clamp(initialValue, 0, maxValue > 0 ? maxValue : 100)) {}

void Gauge::setValue(int value) {
    m_value = std::clamp(value, 0, m_maxValue);
    notifyStateChanged();
}

void Gauge::setMaxValue(int maxValue) {
    if (maxValue > 0) {
        m_maxValue = maxValue;
        m_value = std::clamp(m_value, 0, m_maxValue);
        notifyStateChanged();
    }
}

int Gauge::getPreferredHeight(int width) const {
    (void)width;
    int h = 22;
    if (!m_label.empty()) h += 14;
    return h;
}

void Gauge::paint(j2me::LcduiGraphics* g, int x, int y, int width, int height, bool focused) {
    (void)height;
    if (!g) return;

    int curY = y;
    if (!m_label.empty()) {
        g->setColor(0xFF000080);
        g->fillRect(x, curY, std::min(width, 70), 3);
        curY += 14;
    }

    int trackW = width - 8;
    int trackH = 10;
    int trackX = x + 4;
    int trackY = curY + 3;

    // Track background
    g->setColor(0xFFDDDDDD);
    g->fillRoundRect(trackX, trackY, trackW, trackH, 4, 4);

    // Track border
    g->setColor(focused ? 0xFF0066CC : 0xFF888888);
    g->drawRoundRect(trackX, trackY, trackW, trackH, 4, 4);

    // Progress bar fill
    if (m_maxValue > 0 && m_value > 0) {
        int fillW = (m_value * trackW) / m_maxValue;
        fillW = std::clamp(fillW, 2, trackW);
        g->setColor(m_interactive ? 0xFF0088EE : 0xFF33CC66);
        g->fillRoundRect(trackX, trackY, fillW, trackH, 4, 4);
    }
}

void Gauge::keyPressed(int keyCode) {
    if (!m_interactive) return;

    if (keyCode == 2) { // LEFT
        setValue(m_value - 1);
    } else if (keyCode == 5) { // RIGHT
        setValue(m_value + 1);
    }
}

void Gauge::pointerPressed(int x, int y, int itemW, int itemH) {
    (void)y; (void)itemH;
    if (!m_interactive || itemW <= 8) return;

    int barW = itemW - 8;
    int clickRelX = std::clamp(x - 4, 0, barW);
    int newVal = (clickRelX * m_maxValue) / barW;
    setValue(newVal);
}

// --- ImageItem ---
ImageItem::ImageItem(std::string label, std::shared_ptr<j2me::LcduiImage> image, int layout,
                     std::string altText, int appearanceMode)
    : Item(std::move(label)),
      m_image(std::move(image)),
      m_altText(std::move(altText)),
      m_appearanceMode(appearanceMode) {
    m_layout = layout;
}

int ImageItem::getPreferredHeight(int width) const {
    (void)width;
    int h = 0;
    if (!m_label.empty()) h += 14;
    if (m_image) h += m_image->getHeight() + 4;
    else h += 20;
    return std::max(h, 20);
}

void ImageItem::paint(j2me::LcduiGraphics* g, int x, int y, int width, int height, bool focused) {
    (void)height; (void)focused;
    if (!g) return;

    int curY = y;
    if (!m_label.empty()) {
        g->setColor(0xFF000080);
        g->fillRect(x, curY, std::min(width, 70), 3);
        curY += 14;
    }

    if (m_image) {
        int drawX = x;
        if (m_layout & LAYOUT_CENTER) {
            drawX = x + (width - m_image->getWidth()) / 2;
        } else if (m_layout & LAYOUT_RIGHT) {
            drawX = x + width - m_image->getWidth();
        }
        g->drawImage(m_image.get(), drawX, curY, 0);
    } else {
        g->setColor(0xFFAAAAAA);
        g->drawRect(x + 4, curY, width - 8, 16);
    }
}

// --- Spacer ---
Spacer::Spacer(int minWidth, int minHeight)
    : Item(""),
      m_minWidth(minWidth > 0 ? minWidth : 0),
      m_minHeight(minHeight > 0 ? minHeight : 0) {}

void Spacer::setMinimumSize(int minWidth, int minHeight) {
    m_minWidth = std::max(0, minWidth);
    m_minHeight = std::max(0, minHeight);
    notifyStateChanged();
}

// --- DateField ---
DateField::DateField(std::string label, int mode)
    : Item(std::move(label)),
      m_mode(mode),
      m_epochMs(0) {}

int DateField::getPreferredHeight(int width) const {
    (void)width;
    int h = 24;
    if (!m_label.empty()) h += 14;
    return h;
}

void DateField::paint(j2me::LcduiGraphics* g, int x, int y, int width, int height, bool focused) {
    if (!g) return;

    int curY = y;
    if (!m_label.empty()) {
        g->setColor(0xFF000080);
        g->fillRect(x, curY, std::min(width, 70), 3);
        curY += 14;
    }

    int boxH = height - (curY - y) - 2;
    g->setColor(0xFFFFFFFF);
    g->fillRect(x, curY, width - 4, boxH);

    g->setColor(focused ? 0xFF0066CC : 0xFF888888);
    g->drawRect(x, curY, width - 4, boxH);

    // Indicator
    g->setColor(0xFF222222);
    g->fillRect(x + 6, curY + boxH / 2 - 2, 40, 4);
}

} // namespace lcdui
} // namespace universal_loader
