#pragma once

#include "j2me_core.h"
#include "bluetooth_types.h"
#include "bluetooth_uuid.h"
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>

namespace j2me {
namespace bluetooth {

class J2ME_API BluetoothDataElement {
public:
    BluetoothDataElement();
    explicit BluetoothDataElement(DataElementType type);
    explicit BluetoothDataElement(bool boolVal);
    BluetoothDataElement(DataElementType type, int64_t intVal);
    BluetoothDataElement(const std::string& str, bool isUrl = false);
    BluetoothDataElement(const char* str, bool isUrl = false);
    explicit BluetoothDataElement(const BluetoothUUID& uuid);
    BluetoothDataElement(DataElementType type, const std::vector<uint8_t>& rawBytes);

    DataElementType getDataType() const { return type_; }

    int64_t getLong() const;
    bool getBoolean() const;
    std::string getString() const;
    BluetoothUUID getUUID() const;
    const std::vector<uint8_t>& getRawBytes() const;

    // Sequence / Alternative methods
    void addElement(const BluetoothDataElement& elem);
    void insertElementAt(const BluetoothDataElement& elem, size_t index);
    bool removeElement(size_t index);
    size_t getSize() const;
    const BluetoothDataElement& getElement(size_t index) const;
    const std::vector<BluetoothDataElement>& getElements() const;

private:
    DataElementType type_;
    int64_t intVal_ = 0;
    bool boolVal_ = false;
    std::string strVal_;
    BluetoothUUID uuidVal_;
    std::vector<uint8_t> rawBytes_;
    std::vector<BluetoothDataElement> elements_;
};

} // namespace bluetooth
} // namespace j2me
