#pragma once

#include "command.h"
#include "../lcdui_graphics.h"
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace universal_loader {
namespace lcdui {

class Form;
class Item;

class J2ME_API ItemStateListener {
public:
    virtual ~ItemStateListener() = default;
    virtual void itemStateChanged(Item* item) = 0;
};

class J2ME_API ItemCommandListener {
public:
    virtual ~ItemCommandListener() = default;
    virtual void commandAction(const Command& c, Item* item) = 0;
};

class J2ME_API Item {
public:
    static constexpr int PLAIN   = 0;
    static constexpr int HYPERLINK = 1;
    static constexpr int BUTTON  = 2;

    static constexpr int LAYOUT_DEFAULT        = 0;
    static constexpr int LAYOUT_LEFT           = 1;
    static constexpr int LAYOUT_RIGHT          = 2;
    static constexpr int LAYOUT_CENTER         = 3;
    static constexpr int LAYOUT_TOP            = 16;
    static constexpr int LAYOUT_BOTTOM         = 32;
    static constexpr int LAYOUT_VCENTER        = 48;
    static constexpr int LAYOUT_NEWLINE_BEFORE = 256;
    static constexpr int LAYOUT_NEWLINE_AFTER  = 512;
    static constexpr int LAYOUT_SHRINK         = 1024;
    static constexpr int LAYOUT_EXPAND         = 2048;
    static constexpr int LAYOUT_VSHRINK        = 4096;
    static constexpr int LAYOUT_VEXPAND        = 8192;
    static constexpr int LAYOUT_2              = 16384;

    explicit Item(std::string label);
    virtual ~Item() = default;

    const std::string& getLabel() const { return m_label; }
    void setLabel(std::string label) { m_label = std::move(label); }

    int getLayout() const { return m_layout; }
    void setLayout(int layout) { m_layout = layout; }

    void setOwnerForm(Form* form) { m_ownerForm = form; }
    Form* getOwnerForm() const { return m_ownerForm; }

    void addCommand(const Command& cmd);
    void removeCommand(const Command& cmd);
    void setDefaultCommand(const Command& cmd);
    void setItemCommandListener(ItemCommandListener* l) { m_commandListener = l; }

    virtual int getPreferredHeight(int width) const = 0;
    virtual void paint(j2me::LcduiGraphics* g, int x, int y, int width, int height, bool focused) = 0;

    virtual void keyPressed(int keyCode);
    virtual void pointerPressed(int x, int y, int itemW, int itemH);

    void notifyStateChanged();

protected:
    std::string m_label;
    int m_layout;
    Form* m_ownerForm;
    std::vector<Command> m_commands;
    Command m_defaultCommand;
    ItemCommandListener* m_commandListener;
};

// --- StringItem ---
class J2ME_API StringItem : public Item {
public:
    StringItem(std::string label, std::string text, int appearanceMode = PLAIN);

    const std::string& getText() const { return m_text; }
    void setText(std::string text) { m_text = std::move(text); }

    int getAppearanceMode() const { return m_appearanceMode; }
    void setAppearanceMode(int mode) { m_appearanceMode = mode; }

    int getPreferredHeight(int width) const override;
    void paint(j2me::LcduiGraphics* g, int x, int y, int width, int height, bool focused) override;

private:
    std::string m_text;
    int m_appearanceMode;
};

// --- TextField ---
class J2ME_API TextField : public Item {
public:
    static constexpr int ANY = 0;
    static constexpr int EMAILADDR = 1;
    static constexpr int NUMERIC = 2;
    static constexpr int PHONENUMBER = 3;
    static constexpr int URL = 4;
    static constexpr int DECIMAL = 5;
    static constexpr int CONSTRAINT_MASK = 65535;

    static constexpr int PASSWORD = 65536;
    static constexpr int UNEDITABLE = 131072;
    static constexpr int SENSITIVE = 262144;
    static constexpr int NON_PREDICTIVE = 524288;
    static constexpr int INITIAL_CAPS_WORD = 1048576;
    static constexpr int INITIAL_CAPS_SENTENCE = 2097152;

    TextField(std::string label, std::string text, int maxSize, int constraints);

    const std::string& getString() const { return m_text; }
    void setString(std::string text);

    int getMaxSize() const { return m_maxSize; }
    void setMaxSize(int maxSize);

    int getConstraints() const { return m_constraints; }
    void setConstraints(int constraints) { m_constraints = constraints; }

    int getCaretPosition() const { return m_caretPos; }

    void insert(const std::string& src, int pos);
    void deleteChar(int pos);

    int getPreferredHeight(int width) const override;
    void paint(j2me::LcduiGraphics* g, int x, int y, int width, int height, bool focused) override;
    void keyPressed(int keyCode) override;

private:
    std::string m_text;
    int m_maxSize;
    int m_constraints;
    int m_caretPos;
};

// --- ChoiceGroup ---
struct J2ME_API ChoiceElement {
    std::string stringPart;
    std::shared_ptr<j2me::LcduiImage> imagePart;
    bool selected;
};

class J2ME_API ChoiceGroup : public Item {
public:
    static constexpr int EXCLUSIVE = 1;
    static constexpr int MULTIPLE  = 2;
    static constexpr int POPUP     = 4;

    ChoiceGroup(std::string label, int choiceType);

    int append(std::string stringPart, std::shared_ptr<j2me::LcduiImage> imagePart);
    void insert(int elementNum, std::string stringPart, std::shared_ptr<j2me::LcduiImage> imagePart);
    void deleteElement(int elementNum);
    void deleteAll();

    int size() const { return static_cast<int>(m_elements.size()); }
    int getChoiceType() const { return m_choiceType; }

    bool isSelected(int elementNum) const;
    void setSelectedIndex(int elementNum, bool selected);
    int getSelectedIndex() const;

    int getSelectedFlags(std::vector<bool>& result) const;
    void setSelectedFlags(const std::vector<bool>& flags);

    const std::string& getString(int elementNum) const;
    std::shared_ptr<j2me::LcduiImage> getImage(int elementNum) const;

    int getPreferredHeight(int width) const override;
    void paint(j2me::LcduiGraphics* g, int x, int y, int width, int height, bool focused) override;
    void keyPressed(int keyCode) override;
    void pointerPressed(int x, int y, int itemW, int itemH) override;

private:
    int m_choiceType;
    std::vector<ChoiceElement> m_elements;
    int m_hoverIndex;
};

// --- Gauge ---
class J2ME_API Gauge : public Item {
public:
    static constexpr int CONTINUOUS_IDLE     = 0;
    static constexpr int INCREMENTAL_IDLE    = 1;
    static constexpr int CONTINUOUS_RUNNING  = 2;
    static constexpr int INCREMENTAL_UPDATING = 3;
    static constexpr int INDEFINITE          = -1;

    Gauge(std::string label, bool interactive, int maxValue, int initialValue);

    int getValue() const { return m_value; }
    void setValue(int value);

    int getMaxValue() const { return m_maxValue; }
    void setMaxValue(int maxValue);

    bool isInteractive() const { return m_interactive; }

    int getPreferredHeight(int width) const override;
    void paint(j2me::LcduiGraphics* g, int x, int y, int width, int height, bool focused) override;
    void keyPressed(int keyCode) override;
    void pointerPressed(int x, int y, int itemW, int itemH) override;

private:
    bool m_interactive;
    int m_maxValue;
    int m_value;
};

// --- ImageItem ---
class J2ME_API ImageItem : public Item {
public:
    ImageItem(std::string label, std::shared_ptr<j2me::LcduiImage> image, int layout,
              std::string altText, int appearanceMode = PLAIN);

    std::shared_ptr<j2me::LcduiImage> getImage() const { return m_image; }
    void setImage(std::shared_ptr<j2me::LcduiImage> image) { m_image = image; }

    const std::string& getAltText() const { return m_altText; }
    void setAltText(std::string alt) { m_altText = std::move(alt); }

    int getPreferredHeight(int width) const override;
    void paint(j2me::LcduiGraphics* g, int x, int y, int width, int height, bool focused) override;

private:
    std::shared_ptr<j2me::LcduiImage> m_image;
    std::string m_altText;
    int m_appearanceMode;
};

// --- Spacer ---
class J2ME_API Spacer : public Item {
public:
    Spacer(int minWidth, int minHeight);

    void setMinimumSize(int minWidth, int minHeight);

    int getPreferredHeight(int width) const override { (void)width; return m_minHeight; }
    void paint(j2me::LcduiGraphics* g, int x, int y, int width, int height, bool focused) override {
        (void)g; (void)x; (void)y; (void)width; (void)height; (void)focused;
    }

private:
    int m_minWidth;
    int m_minHeight;
};

// --- DateField ---
class J2ME_API DateField : public Item {
public:
    static constexpr int DATE = 1;
    static constexpr int TIME = 2;
    static constexpr int DATE_TIME = 3;

    DateField(std::string label, int mode);

    int64_t getDate() const { return m_epochMs; }
    void setDate(int64_t epochMs) { m_epochMs = epochMs; notifyStateChanged(); }

    int getMode() const { return m_mode; }
    void setMode(int mode) { m_mode = mode; }

    int getPreferredHeight(int width) const override;
    void paint(j2me::LcduiGraphics* g, int x, int y, int width, int height, bool focused) override;

private:
    int m_mode;
    int64_t m_epochMs;
};

} // namespace lcdui
} // namespace universal_loader
