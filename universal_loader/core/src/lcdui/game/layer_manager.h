#ifndef J2ME_GAME_LAYER_MANAGER_H
#define J2ME_GAME_LAYER_MANAGER_H

#include "layer.h"
#include <vector>
#include <memory>
#include <climits>

namespace universal_loader::lcdui::game {

class J2ME_API LayerManager {
public:
    LayerManager();
    ~LayerManager() = default;

    void append(std::shared_ptr<Layer> l);
    void insert(std::shared_ptr<Layer> l, int index);
    std::shared_ptr<Layer> getLayerAt(int index) const;
    int getSize() const;
    void remove(std::shared_ptr<Layer> l);

    void setViewWindow(int x, int y, int width, int height);
    void paint(j2me::LcduiGraphics* g, int x, int y);

private:
    std::vector<std::shared_ptr<Layer>> m_layers;
    int m_viewX{0};
    int m_viewY{0};
    int m_viewWidth{INT_MAX};
    int m_viewHeight{INT_MAX};
};

} // namespace universal_loader::lcdui::game

#endif // J2ME_GAME_LAYER_MANAGER_H
