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

namespace ZBinaryWriter {

class ISink {
public:
    // Write 'len' bytes from 'buf' to the sink destination.
    virtual void write(const char* buf, int len) = 0;
    // Move the current write head to 'offset'.
    // Seeking past the end of a sink extends the size of the sink with a null byte padding.
    virtual void seek(int64_t offset) = 0;
    // Return the current write head position.
    virtual int64_t tell() const = 0;
    // Release/Close the sink destination and optionally return the written data.
    virtual std::optional<std::vector<char>> release() = 0;

    virtual ~ISink();
};

class FileSink : public ISink {
private:
    mutable std::ofstream ofs;

public:
    explicit FileSink(const char* path);
    explicit FileSink(const std::string& path);
    explicit FileSink(const std::filesystem::path& path);

    ~FileSink();

    void write(const char* read_buffer, int len) override;
    void seek(int64_t offset) override final;
    int64_t tell() const override final;
    std::optional<std::vector<char>> release() override;
};

class BufferSink : public ISink {
private:
    std::vector<char> data;
    int64_t cur;

    void resizeIfNecessary(int len);

public:
    BufferSink();

    void write(const char* read_buffer, int len) override;
    void seek(int64_t offset) override final;
    int64_t tell() const override final;
    std::optional<std::vector<char>> release() override;
};

class BinaryWriter {
private:
    std::unique_ptr<ISink> sink;

public:
    explicit BinaryWriter(const BinaryWriter& bw) = delete;
    explicit BinaryWriter(BinaryWriter&& bw) noexcept;

    // File Writer Constructor
    explicit BinaryWriter(const char* path);
    explicit BinaryWriter(const std::string& path);
    explicit BinaryWriter(const std::filesystem::path& path);

    // Buffer Writer Constructor
    BinaryWriter();

    // Custom Sink Constructor
    explicit BinaryWriter(std::unique_ptr<ISink> sink);

    BinaryWriter& operator=(const BinaryWriter& bw) = delete;
    BinaryWriter& operator=(BinaryWriter&& bw) noexcept;

    template <typename T, Endianness en = Endianness::LE>
    void write(const T& value);

    template <typename T, Endianness en = Endianness::LE>
    void write(const T* arr, int64_t arr_len);

    template <unsigned int al = 0x10>
    void align();

    [[nodiscard]] int64_t tell();

    void seek(int64_t offset);

    template <Endianness en = Endianness::BE>
    void writeString(const std::string& str);

    template <Endianness en = Endianness::BE>
    void writeCString(const std::string& str);

    std::optional<std::vector<char>> release();

    [[nodiscard]] const ISink* getSink() const noexcept;
};

// ISink Impl
inline ISink::~ISink() {
}
// ISink Impl End

// FileSink Impl
inline FileSink::FileSink(const char* path) : FileSink(std::filesystem::path(path)) {
}

inline FileSink::FileSink(const std::string& path) : FileSink(std::filesystem::path(path)) {
}

inline FileSink::FileSink(const std::filesystem::path& path) {
    ofs.exceptions(std::ios::failbit | std::ios::badbit);
    ofs.open(path, std::ios::binary);
}

inline FileSink::~FileSink() {
}

inline void FileSink::write(const char* read_buffer, int len) {
    ofs.write(read_buffer, len);
}

inline void FileSink::seek(int64_t offset) {
    // seekp doesn't pad the file to the seek destination, we have to do it manually
    ofs.seekp(0, std::ios::end);
    const auto end = ofs.tellp();
    if(offset > end) {
        const char pad = 0;
        int64_t delta = offset - end;
        while(delta--)
            ofs.write(&pad, 1);
    } else {
        ofs.seekp(offset);
    }
}

inline int64_t FileSink::tell() const {
    return static_cast<int64_t>(ofs.tellp());
}

inline std::optional<std::vector<char>> FileSink::release() {
    if(ofs.is_open())
        ofs.close();
    return std::optional<std::vector<char>>();
}

// FileSink Impl End

// BufferSink Impl
inline void BufferSink::resizeIfNecessary(int len) {
    if(cur + len > data.size())
        data.resize(cur + len);
}

inline BufferSink::BufferSink() : cur(0) {
}


inline void BufferSink::write(const char* src, int len) {
    if(!len)
        return;
    resizeIfNecessary(len);
    memcpy(&data[cur], src, len);
    cur += len;
}

inline void BufferSink::seek(int64_t offset) {
    if(offset > data.size())
        data.resize(offset);
    cur = offset;
}

inline int64_t BufferSink::tell() const {
    return cur;
}

inline std::optional<std::vector<char>> BufferSink::release() {
    return std::optional<std::vector<char>>(std::move(data));
}

// BufferSink Impl End

// ZBinaryReader Impl
inline BinaryWriter::BinaryWriter(BinaryWriter&& other) noexcept : sink(std::move(other.sink)){
}

inline BinaryWriter::BinaryWriter(const char* path) : BinaryWriter(std::filesystem::path(path)) {
}

inline BinaryWriter::BinaryWriter(const std::string& path)
: BinaryWriter(std::filesystem::path(path)) {
}

inline BinaryWriter::BinaryWriter(const std::filesystem::path& path)
: sink(std::make_unique<FileSink>(path)) {
}

inline BinaryWriter::BinaryWriter() : sink(std::make_unique<BufferSink>()) {
}

inline BinaryWriter::BinaryWriter(std::unique_ptr<ISink> sink) : sink(std::move(sink)) {
}

inline BinaryWriter& BinaryWriter::operator=(BinaryWriter&& other) noexcept {
    sink = std::move(other.sink);
    return *this;
}

template <typename T, Endianness en>
inline void BinaryWriter::write(const T& value) {
    static_assert(std::is_trivially_copyable_v<T>);

    if constexpr(en == Endianness::BE) {
        T valueBe = value;
        reverseEndianness(valueBe);
        sink->write(reinterpret_cast<const char*>(&valueBe), sizeof(T));
    } else if constexpr(en == Endianness::LE) {
        sink->write(reinterpret_cast<const char*>(&value), sizeof(T));
    }
}

template <typename T, Endianness en>
inline void BinaryWriter::write(const T* arr, int64_t len) {
    static_assert(std::is_trivially_copyable_v<T>);

    if constexpr(en == Endianness::BE) {
        for(int64_t i = 0; i < len; ++i) {
            T valueBe = arr[i];
            reverseEndianness(valueBe);
            sink->write(reinterpret_cast<const char*>(&valueBe), sizeof(T));
        }
    } else if constexpr(en == Endianness::LE) {
        sink->write(reinterpret_cast<const char*>(arr), len * sizeof(T));
    }
}


template <unsigned int al>
inline void BinaryWriter::align() {
    char zero[al]{ 0 };
    uint64_t cur = sink->tell();
    uint64_t paddingLen = (al - cur) % al;
    write(zero, paddingLen);
}

[[nodiscard]] inline int64_t BinaryWriter::tell() {
    return sink->tell();
}

inline void BinaryWriter::seek(int64_t offset) {
    sink->seek(offset);
}

template <Endianness en>
inline void BinaryWriter::writeString(const std::string& str) {
    if constexpr(en == Endianness::LE) {
        std::string tmpStr = str;
        reverseEndianness(tmpStr);
        writeString(tmpStr);
    } else if constexpr(en == Endianness::BE) {
        write(str.data(), str.size());
    }
}

template <Endianness en>
inline void BinaryWriter::writeCString(const std::string& str) {
    writeString<en>(str);
    write('\0');
}

inline std::optional<std::vector<char>> BinaryWriter::release() {
    return sink->release();
}

[[nodiscard]] inline const ISink* BinaryWriter::getSink() const noexcept {
    return sink.get();
}
// ZBinaryReader Impl End

}; // namespace ZBinaryWriter
}; // namespace ZBio