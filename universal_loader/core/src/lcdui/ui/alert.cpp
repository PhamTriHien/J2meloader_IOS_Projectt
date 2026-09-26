#include "alert.h"
#include <algorithm>

namespace universal_loader {
namespace lcdui {

const Command Alert::DISMISS_COMMAND = Command("OK", CMD_OK, 0);

Alert::Alert(std::string title)
    : Displayable(),
      m_type(AlertType::INFO),
      m_timeout(getDefaultTimeout()) {
    m_title = std::move(title);
    addCommand(DISMISS_COMMAND);
}

Alert::Alert(std::string title, std::string alertText, std::shared_ptr<j2me::LcduiImage> alertImage, int alertType)
    : Displayable(),
      m_text(std::move(alertText)),
      m_image(std::move(alertImage)),
      m_type(alertType),
      m_timeout(getDefaultTimeout()) {
    m_title = std::move(title);
    addCommand(DISMISS_COMMAND);
}

void Alert::keyPressed(int keyCode) {
    Displayable::keyPressed(keyCode);
    if (keyCode == -5 || keyCode == -6 || keyCode == -7) { // Any softkey or select dismisses alert
        fireCommandAction(DISMISS_COMMAND);
    }
}

void Alert::paint(j2me::LcduiGraphics* g) {
    if (!g) return;

    // Darkened semi-transparent overlay
    g->setColor(0x80000000);
    g->fillRect(0, 0, m_width, m_height);

    // Modal dialog dimensions
    int padX = 16;
    int dlgW = m_width - padX * 2;
    int dlgH = std::min(m_height * 3 / 4, 180);
    int dlgX = padX;
    int dlgY = (m_height - dlgH) / 2;

    // Dialog drop shadow
    g->setColor(0x60000000);
    g->fillRoundRect(dlgX + 4, dlgY + 4, dlgW, dlgH, 8, 8);

    // Dialog background
    g->setColor(0xFFFAFAFA);
    g->fillRoundRect(dlgX, dlgY, dlgW, dlgH, 8, 8);

    // Dialog header
    uint32_t headerColor = m_type.getHeaderColor();
    g->setColor(headerColor);
    g->fillRoundRect(dlgX, dlgY, dlgW, 26, 8, 8);
    g->fillRect(dlgX, dlgY + 12, dlgW, 14); // Fill bottom curve of header

    // Header white title representation
    g->setColor(0xFFFFFFFF);
    g->fillRect(dlgX + 10, dlgY + 10, std::min(dlgW - 20, static_cast<int>(m_title.size()) * 7 + 10), 6);

    // Image if available
    int contentY = dlgY + 34;
    int contentX = dlgX + 12;
    if (m_image) {
        g->drawImage(m_image.get(), contentX, contentY, 0);
        contentX += m_image->getWidth() + 8;
    }

    // Message text representation
    g->setColor(0xFF222222);
    int textW = std::min(dlgW - (contentX - dlgX) - 12, static_cast<int>(m_text.size()) * 6 + 10);
    g->fillRect(contentX, contentY + 6, std::max(textW, 30), 4);
    g->fillRect(contentX, contentY + 14, std::max(textW * 2 / 3, 20), 4);

    // Indicator if available
    if (m_indicator) {
        int indY = dlgY + dlgH - 40;
        m_indicator->paint(g, dlgX + 10, indY, dlgW - 20, 20, false);
    }

    // Dialog Border
    g->setColor(0xFF555555);
    g->drawRoundRect(dlgX, dlgY, dlgW, dlgH, 8, 8);

    // Render softkey at bottom
    int contentBottom = m_height;
    renderSoftKeys(g, contentBottom);
}

} // namespace lcdui
} // namespace universal_loader
