#include "jar_reader.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <filesystem>

namespace j2me {

#pragma pack(push, 1)
struct ZipLocalFileHeader {
    uint32_t signature;           // 0x04034b50
    uint16_t versionNeeded;
    uint16_t flags;
    uint16_t compressionMethod;
    uint16_t lastModTime;
    uint16_t lastModDate;
    uint32_t crc32;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    uint16_t fileNameLength;
    uint16_t extraFieldLength;
};

struct ZipCDFileHeader {
    uint32_t signature;           // 0x02014b50
    uint16_t versionMadeBy;
    uint16_t versionNeeded;
    uint16_t flags;
    uint16_t compressionMethod;
    uint16_t lastModTime;
    uint16_t lastModDate;
    uint32_t crc32;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    uint16_t fileNameLength;
    uint16_t extraFieldLength;
    uint16_t fileCommentLength;
    uint16_t diskNumberStart;
    uint16_t internalAttributes;
    uint32_t externalAttributes;
    uint32_t localHeaderOffset;
};

struct ZipEOCD {
    uint32_t signature;           // 0x06054b50
    uint16_t diskNumber;
    uint16_t diskWithCD;
    uint16_t totalEntriesDisk;
    uint16_t totalEntries;
    uint32_t sizeOfCD;
    uint32_t offsetOfCD;
    uint16_t commentLength;
};
#pragma pack(pop)

// --- Tinf (Tiny Inflate) RFC 1951 Deflate implementation ---
struct TinfTree {
    uint16_t table[16];
    uint16_t trans[288];
};

struct TinfData {
    const uint8_t* source;
    size_t sourceLen;
    size_t sourcePos;
    uint32_t tag;
    int bitcount;

    uint8_t* dest;
    size_t destLen;
    size_t destPos;

    TinfTree ltree;
    TinfTree dtree;
};

static uint32_t tinf_get_bits(TinfData* d, int num) {
    while (d->bitcount < num) {
        if (d->sourcePos < d->sourceLen) {
            d->tag |= (uint32_t)d->source[d->sourcePos++] << d->bitcount;
            d->bitcount += 8;
        } else {
            break;
        }
    }
    uint32_t bits = d->tag & ((1 << num) - 1);
    d->tag >>= num;
    d->bitcount -= num;
    return bits;
}

static void tinf_build_tree(TinfTree* t, const uint8_t* lengths, int num) {
    uint16_t offs[16];
    int i, sum = 0;

    for (i = 0; i < 16; ++i) t->table[i] = 0;
    for (i = 0; i < num; ++i) t->table[lengths[i]]++;
    t->table[0] = 0;

    for (i = 0; i < 16; ++i) {
        offs[i] = sum;
        sum += t->table[i];
    }
    for (i = 0; i < num; ++i) {
        if (lengths[i]) t->trans[offs[lengths[i]]++] = i;
    }
}

static int tinf_decode_symbol(TinfData* d, const TinfTree* t) {
    while (d->bitcount < 16) {
        if (d->sourcePos < d->sourceLen) {
            d->tag |= (uint32_t)d->source[d->sourcePos++] << d->bitcount;
            d->bitcount += 8;
        } else {
            break;
        }
    }

    int sum = 0, cur = 0, len = 1;
    uint32_t tag = d->tag;

    while (len <= 15) {
        cur |= tag & 1;
        tag >>= 1;
        sum += t->table[len];
        cur -= t->table[len];
        if (cur < 0) {
            d->tag >>= len;
            d->bitcount -= len;
            return t->trans[sum + cur];
        }
        cur <<= 1;
        len++;
    }
    return -1;
}

static const uint8_t g_clcidx[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
static const uint16_t g_length_base[29] = {
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};
static const uint8_t g_length_extra[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
};
static const uint16_t g_dist_base[30] = {
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
    257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};
static const uint8_t g_dist_extra[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
    7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
};

static void tinf_build_fixed_trees(TinfTree* lt, TinfTree* dt) {
    uint8_t lengths[288];
    int i;
    for (i = 0; i < 144; ++i) lengths[i] = 8;
    for (; i < 256; ++i) lengths[i] = 9;
    for (; i < 280; ++i) lengths[i] = 7;
    for (; i < 288; ++i) lengths[i] = 8;
    tinf_build_tree(lt, lengths, 288);

    for (i = 0; i < 30; ++i) lengths[i] = 5;
    tinf_build_tree(dt, lengths, 30);
}

static bool tinf_inflate_block(TinfData* d) {
    while (true) {
        int sym = tinf_decode_symbol(d, &d->ltree);
        if (sym < 0) return false;
        if (sym < 256) {
            if (d->destPos >= d->destLen) return false;
            d->dest[d->destPos++] = (uint8_t)sym;
        } else if (sym == 256) {
            return true;
        } else {
            sym -= 257;
            if (sym >= 29) return false;
            int len = g_length_base[sym] + tinf_get_bits(d, g_length_extra[sym]);
            int dsym = tinf_decode_symbol(d, &d->dtree);
            if (dsym < 0 || dsym >= 30) return false;
            int dist = g_dist_base[dsym] + tinf_get_bits(d, g_dist_extra[dsym]);

            if (d->destPos < (size_t)dist || d->destPos + len > d->destLen) return false;
            for (int i = 0; i < len; ++i) {
                d->dest[d->destPos] = d->dest[d->destPos - dist];
                d->destPos++;
            }
        }
    }
}

bool JarReader::inflateRaw(const uint8_t* compressed, size_t compLen, uint8_t* decompressed, size_t decompLen) {
    TinfData d;
    std::memset(&d, 0, sizeof(d));
    d.source = compressed;
    d.sourceLen = compLen;
    d.dest = decompressed;
    d.destLen = decompLen;

    int bfinal = 0;
    while (!bfinal) {
        bfinal = (int)tinf_get_bits(&d, 1);
        int btype = (int)tinf_get_bits(&d, 2);

        if (btype == 0) { // Uncompressed block
            d.bitcount = 0;
            d.tag = 0;
            if (d.sourcePos + 4 > d.sourceLen) return false;
            uint16_t len = d.source[d.sourcePos] | (d.source[d.sourcePos + 1] << 8);
            d.sourcePos += 4;
            if (d.destPos + len > d.destLen || d.sourcePos + len > d.sourceLen) return false;
            std::memcpy(&d.dest[d.destPos], &d.source[d.sourcePos], len);
            d.destPos += len;
            d.sourcePos += len;
        } else if (btype == 1) { // Fixed Huffman
            tinf_build_fixed_trees(&d.ltree, &d.dtree);
            if (!tinf_inflate_block(&d)) return false;
        } else if (btype == 2) { // Dynamic Huffman
            int hlit = tinf_get_bits(&d, 5) + 257;
            int hdist = tinf_get_bits(&d, 5) + 1;
            int hclen = tinf_get_bits(&d, 4) + 4;

            uint8_t code_lengths[19] = {0};
            for (int i = 0; i < hclen; ++i) {
                code_lengths[g_clcidx[i]] = (uint8_t)tinf_get_bits(&d, 3);
            }
            TinfTree ctree;
            tinf_build_tree(&ctree, code_lengths, 19);

            uint8_t lengths[288 + 32];
            int num = 0;
            while (num < hlit + hdist) {
                int sym = tinf_decode_symbol(&d, &ctree);
                if (sym < 16) {
                    lengths[num++] = (uint8_t)sym;
                } else if (sym == 16) {
                    uint8_t prev = (num > 0) ? lengths[num - 1] : 0;
                    int rep = tinf_get_bits(&d, 2) + 3;
                    while (rep--) lengths[num++] = prev;
                } else if (sym == 17) {
                    int rep = tinf_get_bits(&d, 3) + 3;
                    while (rep--) lengths[num++] = 0;
                } else if (sym == 18) {
                    int rep = tinf_get_bits(&d, 7) + 11;
                    while (rep--) lengths[num++] = 0;
                } else {
                    return false;
                }
            }
            tinf_build_tree(&d.ltree, lengths, hlit);
            tinf_build_tree(&d.dtree, lengths + hlit, hdist);
            if (!tinf_inflate_block(&d)) return false;
        } else {
            return false;
        }
    }
    return d.destPos == decompLen;
}

JarReader::JarReader() {}
JarReader::~JarReader() { close(); }

void JarReader::close() {
    m_rawJarData.clear();
    m_entries.clear();
    m_manifest.clear();
}

bool JarReader::openFromMemory(const uint8_t* data, size_t size) {
    close();
    if (!data || size < sizeof(ZipEOCD)) return false;
    m_rawJarData.assign(data, data + size);
    return parseCentralDirectory();
}

bool JarReader::openFromFile(const std::string& filePath) {
    close();
    std::string cleanPath = filePath;
    if (cleanPath.size() >= 2 && cleanPath.front() == '"' && cleanPath.back() == '"') {
        cleanPath = cleanPath.substr(1, cleanPath.size() - 2);
    }
#if defined(_WIN32) || defined(_WIN64)
    std::ifstream file(std::filesystem::u8path(cleanPath), std::ios::binary);
#else
    std::ifstream file(cleanPath, std::ios::binary);
#endif
    if (!file.is_open()) return false;

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    if (size < sizeof(ZipEOCD)) return false;

    m_rawJarData.resize(size);
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(m_rawJarData.data()), size);

    return parseCentralDirectory();
}

bool JarReader::parseCentralDirectory() {
    size_t fileSize = m_rawJarData.size();
    if (fileSize < sizeof(ZipEOCD)) return false;

    size_t searchLen = std::min<size_t>(fileSize, 65536 + sizeof(ZipEOCD));
    size_t startPos = fileSize - searchLen;

    size_t eocdPos = 0;
    bool found = false;
    for (size_t i = searchLen - sizeof(ZipEOCD); i > 0; --i) {
        size_t idx = startPos + i;
        uint32_t sig;
        std::memcpy(&sig, &m_rawJarData[idx], sizeof(sig));
        if (sig == 0x06054b50) {
            eocdPos = idx;
            found = true;
            break;
        }
    }
    if (!found) return false;

    ZipEOCD eocd;
    std::memcpy(&eocd, &m_rawJarData[eocdPos], sizeof(eocd));

    size_t cdOffset = eocd.offsetOfCD;
    for (uint16_t i = 0; i < eocd.totalEntries; ++i) {
        if (cdOffset + sizeof(ZipCDFileHeader) > fileSize) break;

        ZipCDFileHeader cd;
        std::memcpy(&cd, &m_rawJarData[cdOffset], sizeof(cd));
        if (cd.signature != 0x02014b50) break;

        size_t namePos = cdOffset + sizeof(ZipCDFileHeader);
        if (namePos + cd.fileNameLength > fileSize) break;

        std::string fileName(reinterpret_cast<const char*>(&m_rawJarData[namePos]), cd.fileNameLength);

        ZipEntryInfo entry;
        entry.name = fileName;
        entry.compressedSize = cd.compressedSize;
        entry.uncompressedSize = cd.uncompressedSize;
        entry.localHeaderOffset = cd.localHeaderOffset;
        entry.compressionMethod = cd.compressionMethod;

        m_entries[fileName] = entry;

        cdOffset += sizeof(ZipCDFileHeader) + cd.fileNameLength + cd.extraFieldLength + cd.fileCommentLength;
    }

    m_manifest = parseManifest();
    return !m_entries.empty();
}

bool JarReader::hasEntry(const std::string& name) const {
    std::string norm = name;
    if (!norm.empty() && norm[0] == '/') norm.erase(0, 1);
    return m_entries.find(norm) != m_entries.end();
}

bool JarReader::extractEntry(const std::string& name, std::vector<uint8_t>& outData) const {
    std::string norm = name;
    if (!norm.empty() && norm[0] == '/') norm.erase(0, 1);

    auto it = m_entries.find(norm);
    if (it == m_entries.end()) {
        it = m_entries.find(name);
    }
    if (it == m_entries.end()) return false;

    const ZipEntryInfo& entry = it->second;
    if (entry.localHeaderOffset + sizeof(ZipLocalFileHeader) > m_rawJarData.size()) return false;

    ZipLocalFileHeader localHdr;
    std::memcpy(&localHdr, &m_rawJarData[entry.localHeaderOffset], sizeof(localHdr));
    if (localHdr.signature != 0x04034b50) return false;

    size_t dataOffset = entry.localHeaderOffset + sizeof(ZipLocalFileHeader) + localHdr.fileNameLength + localHdr.extraFieldLength;
    if (dataOffset + entry.compressedSize > m_rawJarData.size()) return false;

    const uint8_t* compData = &m_rawJarData[dataOffset];

    if (entry.compressionMethod == 0) { // Stored
        outData.assign(compData, compData + entry.compressedSize);
        return true;
    } else if (entry.compressionMethod == 8) { // Deflated
        outData.resize(entry.uncompressedSize);
        return inflateRaw(compData, entry.compressedSize, outData.data(), entry.uncompressedSize);
    }

    return false;
}

std::vector<std::string> JarReader::listEntries() const {
    std::vector<std::string> list;
    list.reserve(m_entries.size());
    for (const auto& pair : m_entries) {
        list.push_back(pair.first);
    }
    return list;
}

std::map<std::string, std::string> JarReader::parseManifest() const {
    std::map<std::string, std::string> manifest;
    std::vector<uint8_t> data;
    if (!extractEntry("META-INF/MANIFEST.MF", data)) return manifest;

    std::string text(data.begin(), data.end());
    std::istringstream stream(text);
    std::string line;
    std::string curKey, curVal;

    while (std::getline(stream, line)) {
        if (line.empty() || line == "\r") continue;
        if (line.back() == '\r') line.pop_back();

        if (line[0] == ' ' && !curKey.empty()) {
            curVal += line.substr(1);
        } else {
            if (!curKey.empty()) {
                manifest[curKey] = curVal;
            }
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                curKey = line.substr(0, colon);
                curVal = line.substr(colon + 1);
                if (!curVal.empty() && curVal[0] == ' ') curVal.erase(0, 1);
            }
        }
    }
    if (!curKey.empty()) {
        manifest[curKey] = curVal;
    }
    return manifest;
}

std::string JarReader::getManifestProperty(const std::string& key) const {
    auto it = m_manifest.find(key);
    if (it != m_manifest.end()) return it->second;
    return "";
}

std::string JarReader::getMainMidletClass() const {
    // MIDlet-1: Name, Icon, ClassName
    std::string midlet1 = getManifestProperty("MIDlet-1");
    if (!midlet1.empty()) {
        size_t lastComma = midlet1.rfind(',');
        if (lastComma != std::string::npos) {
            std::string className = midlet1.substr(lastComma + 1);
            size_t start = className.find_first_not_of(" \t");
            size_t end = className.find_last_not_of(" \t");
            if (start != std::string::npos && end != std::string::npos) {
                return className.substr(start, end - start + 1);
            }
        }
    }
    return "";
}

} // namespace j2me
