#pragma once

#include "Common.h"

#include <algorithm>
#include <assert.h>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <vector>

// Check if system is little endian
static_assert(static_cast<const uint8_t&>(0x0B00B135) == 0x35);

namespace ZBio {

namespace ZBinaryReader {

class ISource {
public:
    virtual void read(char* dst, int64_t len) = 0;
    virtual void peek(char* dst, int64_t len) const = 0;
    virtual void seek(int64_t offset) = 0;
    virtual int64_t tell() const noexcept = 0;
    virtual int64_t size() const noexcept = 0;

    virtual ~ISource(){};
};

class FileSource : public ISource {
private:
    int64_t size_;
    mutable std::ifstream ifs;

public:
    explicit FileSource(const std::string& path);
    explicit FileSource(const char* path);
    explicit FileSource(const std::filesystem::path& path);

    ~FileSource();

    void read(char* dst, int64_t len) override;
    void peek(char* dst, int64_t len) const override;
    void seek(int64_t offset) override final;
    [[nodiscard]] int64_t tell() const noexcept override final;
    [[nodiscard]] int64_t size() const noexcept override final;
};

class BufferSource : public ISource {
private:
    std::unique_ptr<char[]> ownedBuffer;

    const char* buffer;
    const int64_t bufferSize;
    int64_t cur;

public:
    // Non owning constructor
    BufferSource(const char* data, int64_t data_size);
    // Owning constructor
    BufferSource(std::unique_ptr<char[]> data, int64_t data_size);

    void read(char* dst, int64_t len) override;
    void peek(char* dst, int64_t len) const override;
    void seek(int64_t offset) override final;
    [[nodiscard]] int64_t tell() const noexcept override final;
    [[nodiscard]] int64_t size() const noexcept override final;
};

class BinaryReader;

template <typename Source>
class CoverageTrackingSource : public Source {
    static_assert(std::is_base_of_v<ISource, Source>);

    std::vector<char> accessPattern; // TODO: replace with more space efficient, interval based, access pattern tracking

    bool completeCoverageInternal() const;

public:
    template <typename... Args>
    CoverageTrackingSource(Args&&... args);

    void read(char* dst, int64_t len) override;

    [[nodiscard]] static bool completeCoverage(const BinaryReader* br);
};

class BinaryReader {
    std::unique_ptr<ISource> source;

public:
    BinaryReader(BinaryReader& br) = delete;
    BinaryReader(BinaryReader&& br) noexcept;

    explicit BinaryReader(const std::filesystem::path& path);
    explicit BinaryReader(const std::string& path);
    explicit BinaryReader(const char* path);

    BinaryReader(const char* data, int64_t data_size);
    BinaryReader(std::unique_ptr<char[]> data, int64_t data_size);

    explicit BinaryReader(std::unique_ptr<ISource> source);

    BinaryReader& operator=(const BinaryReader& br) = delete;
    BinaryReader& operator=(BinaryReader&& br) noexcept;

    [[nodiscard]] int64_t tell() const noexcept;
    void seek(int64_t pos);
    [[nodiscard]] int64_t size() const noexcept;

    template <typename T, Endianness en = Endianness::LE>
    void read(T* arr, int64_t len);

    template <typename T, Endianness en = Endianness::LE>
    [[nodiscard]] T read();

    template <typename T, Endianness en = Endianness::LE>
    void peek(T* arr, int64_t len) const;

    template <typename T, Endianness en = Endianness::LE>
    [[nodiscard]] T peek() const;

    template <unsigned int len, Endianness en = Endianness::BE>
    [[nodiscard]] std::string readString();

    template <Endianness en = Endianness::BE>
    [[nodiscard]] std::string readString(size_t charCount);

    template <Endianness en = Endianness::BE>
    [[nodiscard]] std::string readCString();

    template <typename T>
    void sink(int64_t len);

    template <typename T>
    void sink();

    template <unsigned int alignment = 0x10>
    void align();

    template <unsigned int alignment = 0x10>
    void alignZeroPad();

    [[nodiscard]] const ISource* getSource() const noexcept;
};

inline FileSource::FileSource(const std::string& path) : FileSource(std::filesystem::path(path)) {
}

inline FileSource::FileSource(const char* path) : FileSource(std::filesystem::path(path)) {
}

inline FileSource::FileSource(const std::filesystem::path& path) : size_(0) {
    if(!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path))
        throw std::runtime_error("Invalid path: " + path.generic_string());

    size_ = static_cast<uint64_t>(std::filesystem::file_size(path));

    ifs.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    ifs.open(path.generic_string());
}

inline FileSource::~FileSource() {
    if(ifs.is_open())
        ifs.close();
}

inline void FileSource::read(char* dst, int64_t len) {
    ifs.read(dst, len);
}

inline void FileSource::peek(char* dst, int64_t len) const {
    auto o = tell();
    ifs.read(dst, len);
    ifs.seekg(o, std::ios::beg);
}

inline void FileSource::seek(int64_t offset) {
    ifs.seekg(offset, std::ios::beg);
}

inline int64_t FileSource::tell() const noexcept {
    return ifs.tellg();
}

inline int64_t FileSource::size() const noexcept {
    return size_;
}

// Non owning constructor
inline BufferSource::BufferSource(const char* data, int64_t data_size)
: ownedBuffer(nullptr), buffer(data), bufferSize(data_size), cur(0) {
}

// Owning constructor
inline BufferSource::BufferSource(std::unique_ptr<char[]> data, int64_t data_size)
: ownedBuffer(std::move(data)), buffer(nullptr), bufferSize(data_size), cur(0) {
    buffer = ownedBuffer.get();
}

inline void BufferSource::read(char* dst, int64_t len) {
    peek(dst, len);
    cur += len;
}

inline void BufferSource::peek(char* dst, int64_t len) const {
    if(cur + len > size())
        throw std::runtime_error("OOR read/peek");
    memcpy(dst, &(buffer[cur]), len);
}

inline void BufferSource::seek(int64_t offset) {
    if(offset < 0)
        throw std::runtime_error("negative OOR seek");
    cur = offset; // Seeking to positive OOB is not an error
}

inline int64_t BufferSource::tell() const noexcept {
    return cur;
}

inline int64_t BufferSource::size() const noexcept {
    return bufferSize;
}

inline BinaryReader::BinaryReader(BinaryReader&& other) noexcept : source(std::move(other.source)) {
}

inline BinaryReader::BinaryReader(const std::filesystem::path& path)
: source(std::make_unique<FileSource>(path)) {
}

inline BinaryReader::BinaryReader(const std::string& path)
: source(std::make_unique<FileSource>(path)) {
}

inline BinaryReader::BinaryReader(const char* path) : source(std::make_unique<FileSource>(path)) {
}

inline BinaryReader::BinaryReader(const char* data, int64_t data_size)
: source(std::make_unique<BufferSource>(data, data_size)) {
}

inline BinaryReader::BinaryReader(std::unique_ptr<char[]> data, int64_t data_size)
: source(std::make_unique<BufferSource>(std::move(data), data_size)) {
}

inline BinaryReader::BinaryReader(std::unique_ptr<ISource> source) : source(std::move(source)) {
}

inline BinaryReader& BinaryReader::operator=(BinaryReader&& other) noexcept {
    source = std::move(other.source);
    return *this;
}

inline int64_t BinaryReader::tell() const noexcept {
    return source->tell();
}

inline void BinaryReader::seek(int64_t pos) {
    source->seek(pos);
}

inline int64_t BinaryReader::size() const noexcept {
    return source->size();
}

template <typename T, Endianness en>
inline void BinaryReader::read(T* arr, int64_t len) {
    source->read(reinterpret_cast<char*>(arr), sizeof(T) * len);
    if constexpr((en == Endianness::BE) && (sizeof(T) > sizeof(char))) {
        for(int i = 0; i < len; ++i)
            reverseEndianness(arr[i]);
    }
}

template <typename T, Endianness en>
inline void BinaryReader::peek(T* arr, int64_t len) const {
    source->peek(reinterpret_cast<char*>(arr), sizeof(T) * len);
    if constexpr((en == Endianness::BE) && (sizeof(T) > sizeof(char))) {
        for(int i = 0; i < len; ++i)
            reverseEndianness(arr[i]);
    }
}

template <typename T, Endianness en>
inline T BinaryReader::read() {
    static_assert(std::is_trivially_copyable_v<T>);

    if constexpr(en == Endianness::BE)
        static_assert(std::is_fundamental_v<T>);

    T value;
    read<T, en>(&value, 1);
    return value;
}

template <typename T>
inline void BinaryReader::sink(int64_t len) {
    for(int i = 0; i < len; ++i)
        static_cast<void>(read<T>());
}

template <typename T>
inline void BinaryReader::sink() {
    static_cast<void>(read<T>());
}

template <typename T, Endianness en>
inline T BinaryReader::peek() const {
    static_assert(std::is_trivially_copyable_v<T>);

    T value;
    peek<T, en>(&value, 1);
    return value;
}

template <unsigned int len, Endianness en>
inline std::string BinaryReader::readString() {
    std::string str(len, '\0');
    read(str.data(), len);
    if constexpr(en == Endianness::LE)
        reverseEndianness(str);

    if(std::find(str.begin(), str.end(), '\0') != str.end())
        throw std::runtime_error("Read fixed size string containing null characters");

    return str;
}

template <Endianness en>
inline std::string BinaryReader::readString(size_t charCount) {
    std::string str(charCount, '\0');
    read(str.data(), charCount);
    if constexpr(en == Endianness::LE)
        reverseEndianness(str);
    return str;
}

template <Endianness en>
inline std::string BinaryReader::readCString() {
    std::vector<char> readBuffer;

    char c = '\0';
    do {
        c = read<char>();
        readBuffer.push_back(c);
    } while(c != '\0');

    std::string str(readBuffer.begin(), readBuffer.end() - 1);

    if constexpr(en == Endianness::LE)
        reverseEndianness(str);

    return str;
}

template <unsigned int alignment>
inline void BinaryReader::align() {
    char zero[alignment];
    uint64_t pos = tell();
    uint64_t padding_len = (alignment - pos) % alignment;
    read(zero, padding_len);
}

template <unsigned int alignment>
inline void BinaryReader::alignZeroPad() {
    char zero[alignment];
    uint64_t pos = tell();
    uint64_t padding_len = (alignment - pos) % alignment;
    read(zero, padding_len);

    auto it = std::find_if(std::begin(zero), &zero[padding_len], [](const char& c) { return c != 0; });

    if(it != &zero[padding_len])
        throw std::runtime_error("Non zero padding encountered");
}

inline const ISource* BinaryReader::getSource() const noexcept {
    return source.get();
}

template <typename Source>
template <typename... Args>
inline CoverageTrackingSource<Source>::CoverageTrackingSource(Args&&... args)
: Source(std::forward<Args>(args)...) {
    accessPattern.resize(Source::size(), 0);
}

template <typename Source>
inline void CoverageTrackingSource<Source>::read(char* dst, int64_t len) {
    auto cur = Source::tell();
    Source::read(dst, len);
    for(auto i = cur; i < cur + len; ++i) {
        if(accessPattern[i]++)
            throw std::runtime_error("Double read");
    }
}

template <typename Source>
inline bool CoverageTrackingSource<Source>::completeCoverage(const BinaryReader* br) {
    auto coverageReader = dynamic_cast<const CoverageTrackingSource<Source>*>(br->getSource());
    return coverageReader->completeCoverageInternal();
}

template <typename Source>
inline bool CoverageTrackingSource<Source>::completeCoverageInternal() const {
    auto it = std::find(accessPattern.begin(), accessPattern.end(), 0);
    if(it == accessPattern.end())
        return true;
    return false;
}

} // namespace ZBinaryReader

}; // namespace ZBio