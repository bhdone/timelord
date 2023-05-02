#ifndef TL_UTILS_H
#define TL_UTILS_H

#include <cassert>

#include <string>

#include <json/value.h>

#include "common_types.h"

std::string AddressToString(void const* p);

VdfForm MakeZeroForm();

Bytes MakeBytes(uint256 const& source);

uint256 MakeUint256(Bytes const& bytes);

std::string Uint256ToHex(uint256 const& source);

uint256 Uint256FromHex(std::string const& hex);

template <typename T> bool IsZero(T const& data)
{
    if (data.empty()) {
        return true;
    }
    for (auto const& d : data) {
        if (d != 0) {
            return false;
        }
    }
    return true;
}

template <typename T, typename V> void MakeZero(T& data, V const& v)
{
    for (int i = 0; i < data.size(); ++i) {
        data[i] = v;
    }
}

template <size_t N> std::array<uint8_t, N> MakeArray(Bytes const& val)
{
    assert(val.size() == N);
    std::array<uint8_t, N> res;
    memcpy(res.data(), val.data(), N);
    return res;
}

uint256 MakeUint256(Bytes const& vchBytes);

Bytes BytesFromHex(std::string hex);

std::string BytesToHex(Bytes const& bytes);

class BytesConnector
{
    static void ConnectBytesList(BytesConnector& connector) { }

    template <typename T, typename... Ts> static void ConnectBytesList(BytesConnector& connector, T const& data, Ts&&... rest)
    {
        connector.Connect(data);
        ConnectBytesList(connector, std::forward<Ts>(rest)...);
    }

public:
    BytesConnector& Connect(Bytes const& data);

    template <size_t N> BytesConnector& Connect(std::array<uint8_t, N> const& data)
    {
        size_t nOffset = m_vchData.size();
        m_vchData.resize(nOffset + data.size());
        memcpy(m_vchData.data() + nOffset, data.data(), data.size());
        return *this;
    }

    template <typename T> BytesConnector& Connect(T const& val)
    {
        static_assert(std::is_integral<T>::value, "Connect with only buffer/number types");
        std::array<uint8_t, sizeof(T)> data;
        memcpy(data.data(), &val, sizeof(T));
        return Connect(data);
    }

    Bytes const& GetData() const;

    template <typename... T> static Bytes Connect(T&&... dataList)
    {
        BytesConnector connector;
        ConnectBytesList(connector, std::forward<T>(dataList)...);
        return connector.GetData();
    }

private:
    Bytes m_vchData;
};

/**
 * Get part of a bytes
 *
 * @param bytes The source of those bytes
 * @param start From where the part is
 * @param count How many bytes you want to crypto_utils
 *
 * @return The bytes you want
 */
Bytes SubBytes(Bytes const& bytes, int start, int count = 0);

/**
 * Add comma to number string
 *
 * @param num_str The string contains number
 *
 * @return The new string with comma
 */
std::string FormatNumberStr(std::string const& num_str);

std::string MakeNumberTBStr(uint64_t n);

Json::Value ParseStringToJson(std::string_view str);

std::string FormatTime(int seconds);

#endif
