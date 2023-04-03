#include "timelord_utils.h"

#include <cstring>

#include <sstream>

#include <json/reader.h>

#include "utils.h"

std::string AddressToString(void const* p)
{
    auto addr = reinterpret_cast<uint64_t>(p);
    std::stringstream ss;
    ss << "0x" << std::hex << addr;
    return ss.str();
}

VdfForm MakeZeroForm()
{
    VdfForm form;
    memset(form.data(), 0, form.size());
    form[0] = 0x08;
    return form;
}

template <typename Source, typename Dest> void SwitchByteOrder(Source& src, Dest& dst)
{
    int index { 0 };
    for (auto i = std::crbegin(src); i != std::crend(src); ++i) {
        dst[index++] = *i;
    }
}

Bytes MakeBytes(uint256 const& source)
{
    Bytes res(256 / 8);
    SwitchByteOrder(source, res);
    return res;
}

uint256 MakeUint256(Bytes const& bytes)
{
    uint256 res;
    SwitchByteOrder(bytes, res);
    return res;
}

std::string Uint256ToHex(uint256 const& source)
{
    return BytesToHex(MakeBytes(source));
}

uint256 Uint256FromHex(std::string const& hex)
{
    assert(hex.size() == 256 / 8 * 2);
    return MakeUint256(BytesFromHex(hex));
}

Bytes StrToBytes(std::string str)
{
    Bytes b;
    b.resize(str.size());
    memcpy(b.data(), str.data(), str.size());
    SwitchByteOrder(b, b);
    return b;
}

uint8_t HexCharToByte4b(char ch)
{
    if (ch >= 'A' && ch <= 'F') {
        ch = std::tolower(ch);
    }
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    // Not found, the character is invalid
    std::stringstream err_ss;
    err_ss << "invalid hex character (" << static_cast<int>(ch) << ") in order to convert into number";
    throw std::runtime_error(err_ss.str());
}

uint8_t ByteFromHex(std::string const& hex, int* consumed)
{
    int actual_len = strlen(hex.c_str());
    if (actual_len == 0) {
        if (consumed) {
            *consumed = 0;
        }
        return 0;
    }
    if (actual_len == 1) {
        if (consumed) {
            *consumed = 1;
        }
        return HexCharToByte4b(hex[0]);
    }
    uint8_t byte = (static_cast<int>(HexCharToByte4b(hex[0])) << 4) + HexCharToByte4b(hex[1]);
    if (consumed) {
        *consumed = 2;
    }
    return byte;
}

Bytes BytesFromHex(std::string hex)
{
    Bytes res;
    int consumed;
    uint8_t byte = ByteFromHex(hex, &consumed);
    while (consumed > 0) {
        res.push_back(byte);
        hex = hex.substr(consumed);
        // Next byte
        byte = ByteFromHex(hex, &consumed);
    }
    return res;
}

BytesConnector& BytesConnector::Connect(Bytes const& vchData)
{
    size_t nOffset = m_vchData.size();
    m_vchData.resize(nOffset + vchData.size());
    memcpy(m_vchData.data() + nOffset, vchData.data(), vchData.size());
    return *this;
}

Bytes const& BytesConnector::GetData() const
{
    return m_vchData;
}

Bytes SubBytes(Bytes const& bytes, int start, int count)
{
    int n;
    if (count == 0) {
        n = bytes.size() - start;
    } else {
        n = count;
    }
    Bytes res(n);
    memcpy(res.data(), bytes.data() + start, n);
    return res;
}

std::string FormatNumberStr(std::string const& num_str)
{
    std::string res;
    int c { 0 };
    for (auto i = num_str.rbegin(); i != num_str.rend(); ++i) {
        if (c == 3) {
            c = 1;
            res.insert(res.begin(), 1, ',');
        } else {
            ++c;
        }
        res.insert(res.begin(), 1, *i);
    }
    return res;
}

std::string MakeNumberTBStr(uint64_t n)
{
    uint64_t n_tb = n / 1000 / 1000 / 1000 / 1000;
    return FormatNumberStr(std::to_string(n_tb));
}

Json::Value ParseStringToJson(std::string_view str)
{
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::Value root;
    std::string errs;
    bool succ = reader->parse(std::cbegin(str), std::cend(str), &root, &errs);
    if (!succ) {
        throw std::runtime_error(errs);
    }
    return root;
}

std::string FormatTime(int seconds)
{
    int min = seconds / 60;
    int sec = seconds % 60;
    int hour = min / 60;
    min %= 60;
    return std::to_string(hour) + ":" + std::to_string(min) + ":" + std::to_string(sec);
}
