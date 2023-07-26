#pragma once
#include <type_traits>
#include <string>
#include <algorithm>
#include <type_traits>

#define ZBIO_UNUSED(v) static_cast<void>(v)

namespace ZBio {

enum class Endianness { LE, BE };

inline void reverseEndianness(char* data, size_t size) {
    std::reverse(data, data + size);
}

template <size_t TypeSize>
inline void reverseEndianness(char* data) {
    reverseEndianness(data, TypeSize);
}

template <typename T>
inline void reverseEndianness(T& t) {
    static_assert(std::is_fundamental_v<T>);
    reverseEndianness(reinterpret_cast<char*>(&t), sizeof(T));
}

template <>
inline void reverseEndianness<std::string>(std::string& str) {
    reverseEndianness(str.data(), str.size());
}

} // namespace ZBio