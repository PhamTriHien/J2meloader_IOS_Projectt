#include "bluetooth_data_element.h"
#include <stdexcept>

namespace j2me {
namespace bluetooth {

BluetoothDataElement::BluetoothDataElement()
    : type_(TYPE_NULL) {}

BluetoothDataElement::BluetoothDataElement(DataElementType type)
    : type_(type) {
    if (type != TYPE_NULL && type != TYPE_DATSEQ && type != TYPE_DATALT) {
        throw std::invalid_argument("Type requires value initialization");
    }
}

BluetoothDataElement::BluetoothDataElement(bool boolVal)
    : type_(TYPE_BOOL), boolVal_(boolVal) {}

BluetoothDataElement::BluetoothDataElement(DataElementType type, int64_t intVal)
    : type_(type), intVal_(intVal) {
    switch (type) {
        case TYPE_U_INT_1:
            if (intVal < 0 || intVal > 0xFF) throw std::invalid_argument("Value out of range for U_INT_1");
            break;
        case TYPE_U_INT_2:
            if (intVal < 0 || intVal > 0xFFFF) throw std::invalid_argument("Value out of range for U_INT_2");
            break;
        case TYPE_U_INT_4:
            if (intVal < 0 || intVal > 0xFFFFFFFFLL) throw std::invalid_argument("Value out of range for U_INT_4");
            break;
        case TYPE_INT_1:
            if (intVal < -128 || intVal > 127) throw std::invalid_argument("Value out of range for INT_1");
            break;
        case TYPE_INT_2:
            if (intVal < -32768 || intVal > 32767) throw std::invalid_argument("Value out of range for INT_2");
            break;
        case TYPE_INT_4:
            if (intVal < -2147483648LL || intVal > 2147483647LL) throw std::invalid_argument("Value out of range for INT_4");
            break;
        case TYPE_INT_8:
            break;
        default:
            throw std::invalid_argument("Invalid integer type for DataElement");
    }
}

BluetoothDataElement::BluetoothDataElement(const std::string& str, bool isUrl)
    : type_(isUrl ? TYPE_URL : TYPE_STRING), strVal_(str) {}

BluetoothDataElement::BluetoothDataElement(const char* str, bool isUrl)
    : type_(isUrl ? TYPE_URL : TYPE_STRING), strVal_(str ? str : "") {}

BluetoothDataElement::BluetoothDataElement(const BluetoothUUID& uuid)
    : type_(TYPE_UUID), uuidVal_(uuid) {}

BluetoothDataElement::BluetoothDataElement(DataElementType type, const std::vector<uint8_t>& rawBytes)
    : type_(type), rawBytes_(rawBytes) {
    if (type == TYPE_U_INT_8 && rawBytes.size() != 8) {
        throw std::invalid_argument("U_INT_8 requires 8 bytes");
    }
    if ((type == TYPE_U_INT_16 || type == TYPE_INT_16) && rawBytes.size() != 16) {
        throw std::invalid_argument("INT_16 / U_INT_16 requires 16 bytes");
    }
}

int64_t BluetoothDataElement::getLong() const {
    switch (type_) {
        case TYPE_U_INT_1:
        case TYPE_U_INT_2:
        case TYPE_U_INT_4:
        case TYPE_INT_1:
        case TYPE_INT_2:
        case TYPE_INT_4:
        case TYPE_INT_8:
            return intVal_;
        default:
            throw std::runtime_error("DataElement is not an integer type");
    }
}

bool BluetoothDataElement::getBoolean() const {
    if (type_ != TYPE_BOOL) {
        throw std::runtime_error("DataElement is not a boolean type");
    }
    return boolVal_;
}

std::string BluetoothDataElement::getString() const {
    if (type_ != TYPE_STRING && type_ != TYPE_URL) {
        throw std::runtime_error("DataElement is not a string or URL type");
    }
    return strVal_;
}

BluetoothUUID BluetoothDataElement::getUUID() const {
    if (type_ != TYPE_UUID) {
        throw std::runtime_error("DataElement is not a UUID type");
    }
    return uuidVal_;
}

const std::vector<uint8_t>& BluetoothDataElement::getRawBytes() const {
    return rawBytes_;
}

void BluetoothDataElement::addElement(const BluetoothDataElement& elem) {
    if (type_ != TYPE_DATSEQ && type_ != TYPE_DATALT) {
        throw std::runtime_error("Cannot add element to non-sequence DataElement");
    }
    elements_.push_back(elem);
}

void BluetoothDataElement::insertElementAt(const BluetoothDataElement& elem, size_t index) {
    if (type_ != TYPE_DATSEQ && type_ != TYPE_DATALT) {
        throw std::runtime_error("Cannot insert element to non-sequence DataElement");
    }
    if (index > elements_.size()) {
        throw std::out_of_range("Index out of bounds");
    }
    elements_.insert(elements_.begin() + index, elem);
}

bool BluetoothDataElement::removeElement(size_t index) {
    if (type_ != TYPE_DATSEQ && type_ != TYPE_DATALT) {
        return false;
    }
    if (index >= elements_.size()) {
        return false;
    }
    elements_.erase(elements_.begin() + index);
    return true;
}

size_t BluetoothDataElement::getSize() const {
    if (type_ != TYPE_DATSEQ && type_ != TYPE_DATALT) {
        return 0;
    }
    return elements_.size();
}

const BluetoothDataElement& BluetoothDataElement::getElement(size_t index) const {
    if (type_ != TYPE_DATSEQ && type_ != TYPE_DATALT) {
        throw std::runtime_error("Not a sequence DataElement");
    }
    if (index >= elements_.size()) {
        throw std::out_of_range("Index out of range");
    }
    return elements_[index];
}

const std::vector<BluetoothDataElement>& BluetoothDataElement::getElements() const {
    return elements_;
}

} // namespace bluetooth
} // namespace j2me
