#include "tgmr/sha256.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace tgmr
{
namespace
{

constexpr std::uint32_t K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

inline std::uint32_t rotateRight(std::uint32_t value, unsigned count)
{
    return (value >> count) | (value << (32 - count));
}

int hexDigit(char value)
{
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

} // namespace

Sha256::Sha256() :
    state_{{0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
            0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u}}
{
}

void Sha256::transform(const std::uint8_t block[64])
{
    std::uint32_t words[64];
    for (unsigned i = 0; i < 16; ++i) {
        words[i] = (static_cast<std::uint32_t>(block[4 * i]) << 24)
            | (static_cast<std::uint32_t>(block[4 * i + 1]) << 16)
            | (static_cast<std::uint32_t>(block[4 * i + 2]) << 8)
            | static_cast<std::uint32_t>(block[4 * i + 3]);
    }
    for (unsigned i = 16; i < 64; ++i) {
        const std::uint32_t s0 = rotateRight(words[i - 15], 7)
            ^ rotateRight(words[i - 15], 18) ^ (words[i - 15] >> 3);
        const std::uint32_t s1 = rotateRight(words[i - 2], 17)
            ^ rotateRight(words[i - 2], 19) ^ (words[i - 2] >> 10);
        words[i] = words[i - 16] + s0 + words[i - 7] + s1;
    }
    std::uint32_t a = state_[0];
    std::uint32_t b = state_[1];
    std::uint32_t c = state_[2];
    std::uint32_t d = state_[3];
    std::uint32_t e = state_[4];
    std::uint32_t f = state_[5];
    std::uint32_t g = state_[6];
    std::uint32_t h = state_[7];
    for (unsigned i = 0; i < 64; ++i) {
        const std::uint32_t s1 = rotateRight(e, 6) ^ rotateRight(e, 11)
            ^ rotateRight(e, 25);
        const std::uint32_t choice = (e & f) ^ (~e & g);
        const std::uint32_t temporary1 = h + s1 + choice + K[i] + words[i];
        const std::uint32_t s0 = rotateRight(a, 2) ^ rotateRight(a, 13)
            ^ rotateRight(a, 22);
        const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t temporary2 = s0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temporary1;
        d = c;
        c = b;
        b = a;
        a = temporary1 + temporary2;
    }
    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

void Sha256::update(const void *data, std::size_t size)
{
    if (finished_) {
        throw std::logic_error("SHA-256 update after finish");
    }
    if (size != 0 && data == nullptr) {
        throw std::invalid_argument("SHA-256 null input");
    }
    const auto *input = static_cast<const std::uint8_t *>(data);
    totalBytes_ += size;
    while (size != 0) {
        const std::size_t amount = std::min(size, buffer_.size() - buffered_);
        std::memcpy(buffer_.data() + buffered_, input, amount);
        buffered_ += amount;
        input += amount;
        size -= amount;
        if (buffered_ == buffer_.size()) {
            transform(buffer_.data());
            buffered_ = 0;
        }
    }
}

std::array<std::uint8_t, 32> Sha256::finish()
{
    if (finished_) {
        throw std::logic_error("SHA-256 finish called twice");
    }
    const std::uint64_t bitCount = totalBytes_ * 8;
    buffer_[buffered_++] = 0x80;
    if (buffered_ > 56) {
        std::fill(buffer_.begin() + buffered_, buffer_.end(), 0);
        transform(buffer_.data());
        buffered_ = 0;
    }
    std::fill(buffer_.begin() + buffered_, buffer_.begin() + 56, 0);
    for (unsigned i = 0; i < 8; ++i) {
        buffer_[63 - i] = static_cast<std::uint8_t>(bitCount >> (8 * i));
    }
    transform(buffer_.data());
    std::array<std::uint8_t, 32> result{};
    for (unsigned i = 0; i < 8; ++i) {
        result[4 * i] = static_cast<std::uint8_t>(state_[i] >> 24);
        result[4 * i + 1] = static_cast<std::uint8_t>(state_[i] >> 16);
        result[4 * i + 2] = static_cast<std::uint8_t>(state_[i] >> 8);
        result[4 * i + 3] = static_cast<std::uint8_t>(state_[i]);
    }
    finished_ = true;
    return result;
}

std::array<std::uint8_t, 32> sha256(const void *data, std::size_t size)
{
    Sha256 digest;
    digest.update(data, size);
    return digest.finish();
}

std::array<std::uint8_t, 32> sha256File(const std::string &path, std::uint64_t *size)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("cannot open file for SHA-256: " + path);
    }
    Sha256 digest;
    std::array<char, 1 << 16> buffer{};
    std::uint64_t count = 0;
    while (stream) {
        stream.read(buffer.data(), buffer.size());
        const std::streamsize amount = stream.gcount();
        if (amount > 0) {
            digest.update(buffer.data(), static_cast<std::size_t>(amount));
            count += static_cast<std::uint64_t>(amount);
        }
    }
    if (!stream.eof()) {
        throw std::runtime_error("file read failed during SHA-256: " + path);
    }
    if (size) {
        *size = count;
    }
    return digest.finish();
}

std::string hex(const std::array<std::uint8_t, 32> &digest)
{
    static const char digits[] = "0123456789abcdef";
    std::string result(64, '0');
    for (std::size_t i = 0; i < digest.size(); ++i) {
        result[2 * i] = digits[digest[i] >> 4];
        result[2 * i + 1] = digits[digest[i] & 15];
    }
    return result;
}

std::array<std::uint8_t, 32> parseSha256(const std::string &value)
{
    if (value.size() != 64) {
        throw std::invalid_argument("SHA-256 must contain 64 hexadecimal digits");
    }
    std::array<std::uint8_t, 32> result{};
    for (std::size_t i = 0; i < result.size(); ++i) {
        const int high = hexDigit(value[2 * i]);
        const int low = hexDigit(value[2 * i + 1]);
        if (high < 0 || low < 0) {
            throw std::invalid_argument("SHA-256 contains a non-hexadecimal digit");
        }
        result[i] = static_cast<std::uint8_t>((high << 4) | low);
    }
    return result;
}

} // namespace tgmr
