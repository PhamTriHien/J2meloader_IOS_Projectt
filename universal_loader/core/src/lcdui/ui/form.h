#pragma once

#include "displayable.h"
#include "item.h"
#include <vector>
#include <memory>
#include <string>

namespace universal_loader {
namespace lcdui {

class J2ME_API Form : public Displayable {
public:
    explicit Form(std::string title);
    Form(std::string title, const std::vector<std::shared_ptr<Item>>& items);
    ~Form() override = default;

    int append(std::shared_ptr<Item> item);
    int append(const std::string& str);
    int append(std::shared_ptr<j2me::LcduiImage> img);

    void insert(int itemNum, std::shared_ptr<Item> item);
    void deleteItem(int itemNum);
    void deleteAll();

    void set(int itemNum, std::shared_ptr<Item> item);
    std::shared_ptr<Item> get(int itemNum) const;
    int size() const;

    void setItemStateListener(ItemStateListener* listener) { m_stateListener = listener; }
    ItemStateListener* getItemStateListener() const { return m_stateListener; }

    void notifyItemStateChanged(Item* item);

    int getFocusedIndex() const { return m_focusedIndex; }
    void setFocusedIndex(int index);

    int getScrollY() const { return m_scrollY; }

    void paint(j2me::LcduiGraphics* g) override;
    void keyPressed(int keyCode) override;
    void pointerPressed(int x, int y) override;
    void pointerDragged(int x, int y) override;

private:
    std::vector<std::shared_ptr<Item>> m_items;
    ItemStateListener* m_stateListener;
    int m_focusedIndex;
    int m_scrollY;
    int m_lastPointerY;
};

} // namespace lcdui
} // namespace universal_loader
