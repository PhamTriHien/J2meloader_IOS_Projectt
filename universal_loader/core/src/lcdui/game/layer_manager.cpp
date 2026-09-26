#include "layer_manager.h"
#include <algorithm>
#include <stdexcept>

namespace universal_loader::lcdui::game {

LayerManager::LayerManager() {
    setViewWindow(0, 0, INT_MAX, INT_MAX);
}

void LayerManager::append(std::shared_ptr<Layer> l) {
    if (!l) {
        throw std::invalid_argument("Layer cannot be null in LayerManager::append");
    }
    remove(l);
    m_layers.push_back(l);
}

void LayerManager::insert(std::shared_ptr<Layer> l, int index) {
    if (!l) {
        throw std::invalid_argument("Layer cannot be null in LayerManager::insert");
    }
    int size = static_cast<int>(m_layers.size());
    if (index < 0 || index > size) {
        throw std::out_of_range("Index out of range in LayerManager::insert");
    }

    auto it = std::find(m_layers.begin(), m_layers.end(), l);
    if (it != m_layers.end()) {
        int existingIndex = static_cast<int>(std::distance(m_layers.begin(), it));
        m_layers.erase(it);
        if (existingIndex < index) {
            index--;
        }
    }

    m_layers.insert(m_layers.begin() + index, l);
}

std::shared_ptr<Layer> LayerManager::getLayerAt(int index) const {
    if (index < 0 || index >= static_cast<int>(m_layers.size())) {
        throw std::out_of_range("Index out of range in LayerManager::getLayerAt");
    }
    return m_layers[index];
}

int LayerManager::getSize() const {
    return static_cast<int>(m_layers.size());
}

void LayerManager::remove(std::shared_ptr<Layer> l) {
    if (!l) return;
    auto it = std::find(m_layers.begin(), m_layers.end(), l);
    if (it != m_layers.end()) {
        m_layers.erase(it);
    }
}

void LayerManager::setViewWindow(int x, int y, int width, int height) {
    if (width < 0 || height < 0) {
        throw std::invalid_argument("View window width and height cannot be negative");
    }
    m_viewX = x;
    m_viewY = y;
    m_viewWidth = width;
    m_viewHeight = height;
}

void LayerManager::paint(j2me::LcduiGraphics* g, int x, int y) {
    if (!g) return;

    int clipX = g->getClipX();
    int clipY = g->getClipY();
    int clipW = g->getClipWidth();
    int clipH = g->getClipHeight();

    // translate the LayerManager co-ordinates to Screen co-ordinates
    g->translate(x - m_viewX, y - m_viewY);
    // set the clip to view window
    g->clipRect(m_viewX, m_viewY, m_viewWidth, m_viewHeight);

    // draw last to first
    for (int i = static_cast<int>(m_layers.size()) - 1; i >= 0; --i) {
        auto& layer = m_layers[i];
        if (layer && layer->isVisible()) {
            layer->paint(g);
        }
    }

    // restore graphics state
    g->translate(-x + m_viewX, -y + m_viewY);
    g->setClip(clipX, clipY, clipW, clipH);
}

} // namespace universal_loader::lcdui::game
