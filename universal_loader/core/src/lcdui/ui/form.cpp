#include "form.h"
#include <algorithm>

namespace universal_loader {
namespace lcdui {

Form::Form(std::string title)
    : Displayable(),
      m_stateListener(nullptr),
      m_focusedIndex(0),
      m_scrollY(0),
      m_lastPointerY(0) {
    m_title = std::move(title);
}

Form::Form(std::string title, const std::vector<std::shared_ptr<Item>>& items)
    : Displayable(),
      m_stateListener(nullptr),
      m_focusedIndex(0),
      m_scrollY(0),
      m_lastPointerY(0) {
    m_title = std::move(title);
    for (const auto& item : items) {
        append(item);
    }
}

int Form::append(std::shared_ptr<Item> item) {
    if (!item) return -1;
    item->setOwnerForm(this);
    m_items.push_back(item);
    return static_cast<int>(m_items.size()) - 1;
}

int Form::append(const std::string& str) {
    auto stringItem = std::make_shared<StringItem>("", str, Item::PLAIN);
    return append(stringItem);
}

int Form::append(std::shared_ptr<j2me::LcduiImage> img) {
    auto imgItem = std::make_shared<ImageItem>("", img, Item::LAYOUT_DEFAULT, "");
    return append(imgItem);
}

void Form::insert(int itemNum, std::shared_ptr<Item> item) {
    if (!item) return;
    if (itemNum < 0) itemNum = 0;
    if (itemNum > static_cast<int>(m_items.size())) itemNum = static_cast<int>(m_items.size());

    item->setOwnerForm(this);
    m_items.insert(m_items.begin() + itemNum, item);
}

void Form::deleteItem(int itemNum) {
    if (itemNum >= 0 && itemNum < static_cast<int>(m_items.size())) {
        m_items[itemNum]->setOwnerForm(nullptr);
        m_items.erase(m_items.begin() + itemNum);
        if (m_focusedIndex >= static_cast<int>(m_items.size())) {
            m_focusedIndex = std::max(0, static_cast<int>(m_items.size()) - 1);
        }
    }
}

void Form::deleteAll() {
    for (auto& item : m_items) {
        if (item) item->setOwnerForm(nullptr);
    }
    m_items.clear();
    m_focusedIndex = 0;
    m_scrollY = 0;
}

void Form::set(int itemNum, std::shared_ptr<Item> item) {
    if (!item) return;
    if (itemNum >= 0 && itemNum < static_cast<int>(m_items.size())) {
        m_items[itemNum]->setOwnerForm(nullptr);
        item->setOwnerForm(this);
        m_items[itemNum] = item;
    }
}

std::shared_ptr<Item> Form::get(int itemNum) const {
    if (itemNum >= 0 && itemNum < static_cast<int>(m_items.size())) {
        return m_items[itemNum];
    }
    return nullptr;
}

int Form::size() const {
    return static_cast<int>(m_items.size());
}

void Form::notifyItemStateChanged(Item* item) {
    if (m_stateListener) {
        m_stateListener->itemStateChanged(item);
    }
}

void Form::setFocusedIndex(int index) {
    if (m_items.empty()) {
        m_focusedIndex = 0;
        return;
    }
    m_focusedIndex = std::clamp(index, 0, static_cast<int>(m_items.size()) - 1);

    // Calculate focused item Y and scroll into view
    int curY = 0;
    int targetY = 0;
    int targetH = 20;
    for (int i = 0; i < static_cast<int>(m_items.size()); ++i) {
        int h = m_items[i]->getPreferredHeight(m_width);
        if (i == m_focusedIndex) {
            targetY = curY;
            targetH = h;
            break;
        }
        curY += h + 6;
    }

    int viewH = m_height - 44; // Approx height minus title and softkeys
    if (viewH > 0) {
        if (targetY < m_scrollY) {
            m_scrollY = targetY;
        } else if (targetY + targetH > m_scrollY + viewH) {
            m_scrollY = targetY + targetH - viewH;
        }
    }
    m_scrollY = std::max(0, m_scrollY);
}

void Form::keyPressed(int keyCode) {
    Displayable::keyPressed(keyCode);
    if (m_menuOpen) return;

    if (m_focusedIndex >= 0 && m_focusedIndex < static_cast<int>(m_items.size())) {
        m_items[m_focusedIndex]->keyPressed(keyCode);
    }

    if (keyCode == 1) { // UP
        if (m_focusedIndex > 0) {
            setFocusedIndex(m_focusedIndex - 1);
        }
    } else if (keyCode == 6) { // DOWN
        if (m_focusedIndex + 1 < static_cast<int>(m_items.size())) {
            setFocusedIndex(m_focusedIndex + 1);
        }
    }
}

void Form::pointerPressed(int x, int y) {
    Displayable::pointerPressed(x, y);
    if (m_menuOpen) return;

    int contentTop = 0;
    int contentBottom = m_height;
    int dummy = 0;
    if (!m_title.empty()) contentTop += 22;
    if (!m_ticker.empty()) contentTop += 16;
    contentBottom = m_height - 22;

    if (y >= contentTop && y < contentBottom) {
        m_lastPointerY = y;
        int curY = contentTop - m_scrollY;
        for (size_t i = 0; i < m_items.size(); ++i) {
            int h = m_items[i]->getPreferredHeight(m_width);
            if (y >= curY && y < curY + h) {
                setFocusedIndex(static_cast<int>(i));
                m_items[i]->pointerPressed(x - 6, y - curY, m_width - 12, h);
                break;
            }
            curY += h + 6;
        }
    }
}

void Form::pointerDragged(int x, int y) {
    (void)x;
    int dy = m_lastPointerY - y;
    m_lastPointerY = y;
    m_scrollY = std::max(0, m_scrollY + dy);
}

void Form::paint(j2me::LcduiGraphics* g) {
    if (!g) return;

    // Background
    g->setColor(0xFFEBEBEB);
    g->fillRect(0, 0, m_width, m_height);

    int contentTop = 0;
    int contentBottom = m_height;
    renderTitleAndTicker(g, contentTop);
    renderSoftKeys(g, contentBottom);

    int viewH = contentBottom - contentTop;
    if (viewH <= 0) return;

    g->setClip(0, contentTop, m_width, viewH);

    int curY = contentTop - m_scrollY;
    for (size_t i = 0; i < m_items.size(); ++i) {
        auto& item = m_items[i];
        if (!item) continue;

        int h = item->getPreferredHeight(m_width);
        if (curY + h >= contentTop && curY <= contentBottom) {
            bool focused = (static_cast<int>(i) == m_focusedIndex);
            item->paint(g, 6, curY, m_width - 12, h, focused);
        }
        curY += h + 6;
    }

    g->setClip(0, 0, m_width, m_height);
}

} // namespace lcdui
} // namespace universal_loader
