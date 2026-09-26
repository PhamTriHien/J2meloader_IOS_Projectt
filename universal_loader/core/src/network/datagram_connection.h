#ifndef J2ME_DATAGRAM_CONNECTION_H
#define J2ME_DATAGRAM_CONNECTION_H

#include "../../include/j2me_core.h"
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <mutex>
#include <atomic>
#include <stdexcept>

namespace j2me {

class J2ME_API Datagram {
public:
    Datagram(int size);
    Datagram(int size, const std::string& address);
    Datagram(const uint8_t* buf, int size);
    Datagram(const uint8_t* buf, int size, const std::string& address);
    ~Datagram() = default;

    std::string getAddress() const { return m_address; }
    void setAddress(const std::string& address) { m_address = address; }
    void setAddress(const Datagram& other) { m_address = other.m_address; }

    const uint8_t* getData() const { return m_data.data(); }
    uint8_t* getDataMutable() { return m_data.data(); }

    int getLength() const { return m_length; }
    void setLength(int len);

    int getOffset() const { return m_offset; }
    void setData(const uint8_t* buf, int offset, int length);

    void reset();

    // Stream-style Read/Write operations (DataInput/DataOutput)
    void write(const uint8_t* buf, size_t length);
    void writeByte(int v);
    void writeShort(int v);
    void writeInt(int v);
    void writeLong(int64_t v);
    void writeUTF(const std::string& str);

    size_t read(uint8_t* out, size_t length);
    uint8_t readByte();
    int16_t readShort();
    int32_t readInt();
    int64_t readLong();
    std::string readUTF();

private:
    std::vector<uint8_t> m_data;
    int m_offset{0};
    int m_length{0};
    int m_readPos{0};
    int m_writePos{0};
    std::string m_address;
};

class J2ME_API DatagramConnection {
public:
    DatagramConnection();
    ~DatagramConnection();

    bool open(const std::string& url, int timeoutMs = 5000);
    void close();

    int getMaximumLength() const { return 65507; }
    int getNominalLength() const { return 1500; }

    std::shared_ptr<Datagram> newDatagram(int size);
    std::shared_ptr<Datagram> newDatagram(int size, const std::string& addr);
    std::shared_ptr<Datagram> newDatagram(const uint8_t* buf, int size);
    std::shared_ptr<Datagram> newDatagram(const uint8_t* buf, int size, const std::string& addr);

    bool send(Datagram* dgram);
    bool receive(Datagram* dgram, int timeoutMs = 5000);

    std::string getLocalAddress() const { return m_localAddress; }
    int getLocalPort() const { return m_localPort; }

private:
#if defined(_WIN32) || defined(_WIN64)
    uintptr_t m_sock{static_cast<uintptr_t>(~0)}; // INVALID_SOCKET
#else
    int m_sock{-1};
#endif
    std::atomic<bool> m_open{false};
    std::string m_defaultAddress;
    std::string m_localAddress{"127.0.0.1"};
    int m_localPort{0};
    mutable std::recursive_mutex m_mutex;

    static void ensurePlatformNetInit();
};

} // namespace j2me

#endif // J2ME_DATAGRAM_CONNECTION_H
