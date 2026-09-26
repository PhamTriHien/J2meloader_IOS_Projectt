#include "bluetooth_service_record.h"
#include <sstream>

namespace j2me {
namespace bluetooth {

BluetoothServiceRecord::BluetoothServiceRecord(const std::string& hostAddress, const BluetoothUUID& uuid, bool btl2cap, bool skipAfterWrite)
    : hostAddress_(hostAddress), uuid_(uuid), btl2cap_(btl2cap), skipAfterWrite_(skipAfterWrite) {
    if (btl2cap_) {
        populateL2capRecords();
    } else {
        populateSppRecords();
    }
}

void BluetoothServiceRecord::setServiceName(const std::string& name) {
    setAttributeValue(ATTR_SERVICE_NAME, BluetoothDataElement(name));
}

std::string BluetoothServiceRecord::getServiceName() const {
    BluetoothDataElement elem;
    if (getAttributeValue(ATTR_SERVICE_NAME, elem)) {
        return elem.getString();
    }
    return "";
}

bool BluetoothServiceRecord::setAttributeValue(int attrId, const BluetoothDataElement& elem) {
    attributes_[attrId] = elem;
    return true;
}

bool BluetoothServiceRecord::getAttributeValue(int attrId, BluetoothDataElement& outElem) const {
    auto it = attributes_.find(attrId);
    if (it != attributes_.end()) {
        outElem = it->second;
        return true;
    }
    return false;
}

bool BluetoothServiceRecord::hasAttribute(int attrId) const {
    return attributes_.find(attrId) != attributes_.end();
}

std::vector<int> BluetoothServiceRecord::getAttributeIDs() const {
    std::vector<int> ids;
    ids.reserve(attributes_.size());
    for (const auto& kv : attributes_) {
        ids.push_back(kv.first);
    }
    return ids;
}

void BluetoothServiceRecord::populateSppRecords() {
    // ServiceClassIDList
    BluetoothDataElement serviceClassIDList(TYPE_DATSEQ);
    serviceClassIDList.addElement(BluetoothDataElement(uuid_));
    serviceClassIDList.addElement(BluetoothDataElement(BluetoothUUID(UUID_SERIAL_PORT)));
    setAttributeValue(ATTR_SERVICE_CLASS_ID_LIST, serviceClassIDList);

    // ProtocolDescriptorList (L2CAP + RFCOMM)
    BluetoothDataElement protocolDescriptorList(TYPE_DATSEQ);

    BluetoothDataElement l2capDescriptor(TYPE_DATSEQ);
    l2capDescriptor.addElement(BluetoothDataElement(BluetoothUUID(UUID_L2CAP)));
    protocolDescriptorList.addElement(l2capDescriptor);

    BluetoothDataElement rfcommDescriptor(TYPE_DATSEQ);
    rfcommDescriptor.addElement(BluetoothDataElement(BluetoothUUID(UUID_RFCOMM)));
    rfcommDescriptor.addElement(BluetoothDataElement(TYPE_U_INT_1, 0)); // Channel 0
    protocolDescriptorList.addElement(rfcommDescriptor);

    setAttributeValue(ATTR_PROTOCOL_DESCRIPTOR_LIST, protocolDescriptorList);
}

void BluetoothServiceRecord::populateL2capRecords() {
    // ServiceClassIDList
    BluetoothDataElement serviceClassIDList(TYPE_DATSEQ);
    serviceClassIDList.addElement(BluetoothDataElement(uuid_));
    setAttributeValue(ATTR_SERVICE_CLASS_ID_LIST, serviceClassIDList);

    // ProtocolDescriptorList (L2CAP with PSM 0)
    BluetoothDataElement protocolDescriptorList(TYPE_DATSEQ);
    BluetoothDataElement l2capDescriptor(TYPE_DATSEQ);
    l2capDescriptor.addElement(BluetoothDataElement(BluetoothUUID(UUID_L2CAP)));
    l2capDescriptor.addElement(BluetoothDataElement(TYPE_U_INT_2, 0));
    protocolDescriptorList.addElement(l2capDescriptor);

    setAttributeValue(ATTR_PROTOCOL_DESCRIPTOR_LIST, protocolDescriptorList);
}

std::string BluetoothServiceRecord::getConnectionURL(int requiredSecurity, bool mustBeMaster) const {
    std::ostringstream ss;
    if (btl2cap_) {
        ss << "btl2cap://";
    } else {
        ss << "btspp://";
    }

    if (!hostAddress_.empty()) {
        ss << hostAddress_;
    } else {
        ss << "localhost";
    }

    ss << ":" << uuid_.toString();

    switch (requiredSecurity) {
        case NOAUTHENTICATE_NOENCRYPT:
            ss << ";authenticate=false;encrypt=false";
            break;
        case AUTHENTICATE_NOENCRYPT:
            ss << ";authenticate=true;encrypt=false";
            break;
        case AUTHENTICATE_ENCRYPT:
            ss << ";authenticate=true;encrypt=true";
            break;
        default:
            ss << ";authenticate=false;encrypt=false";
            break;
    }

    if (mustBeMaster) {
        ss << ";master=true";
    } else {
        ss << ";master=false";
    }

    if (skipAfterWrite_) {
        ss << ";skipAfterWrite=true";
    }

    return ss.str();
}

} // namespace bluetooth
} // namespace j2me
