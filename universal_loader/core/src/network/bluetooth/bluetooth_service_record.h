#pragma once

#include "j2me_core.h"
#include "bluetooth_types.h"
#include "bluetooth_uuid.h"
#include "bluetooth_data_element.h"
#include <map>
#include <string>
#include <vector>

namespace j2me {
namespace bluetooth {

class J2ME_API BluetoothServiceRecord {
public:
    BluetoothServiceRecord(const std::string& hostAddress, const BluetoothUUID& uuid, bool btl2cap = false, bool skipAfterWrite = false);

    const std::string& getHostAddress() const { return hostAddress_; }
    const BluetoothUUID& getUUID() const { return uuid_; }
    bool isBtl2cap() const { return btl2cap_; }
    bool isSkipAfterWrite() const { return skipAfterWrite_; }

    void setServiceName(const std::string& name);
    std::string getServiceName() const;

    bool setAttributeValue(int attrId, const BluetoothDataElement& elem);
    bool getAttributeValue(int attrId, BluetoothDataElement& outElem) const;
    bool hasAttribute(int attrId) const;
    std::vector<int> getAttributeIDs() const;

    // Upstream standard connection URL generator
    std::string getConnectionURL(int requiredSecurity, bool mustBeMaster) const;

private:
    std::string hostAddress_; // empty or "localhost" if hosted locally
    BluetoothUUID uuid_;
    bool btl2cap_;
    bool skipAfterWrite_;
    std::map<int, BluetoothDataElement> attributes_;

    void populateSppRecords();
    void populateL2capRecords();
};

} // namespace bluetooth
} // namespace j2me
