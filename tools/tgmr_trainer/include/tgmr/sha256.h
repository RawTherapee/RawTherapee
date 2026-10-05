#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace tgmr
{

class Sha256 final
{
public:
    Sha256();
    void update(const void *data, std::size_t size);
    std::array<std::uint8_t, 32> finish();

private:
    void transform(const std::uint8_t block[64]);

    std::array<std::uint32_t, 8> state_;
    std::array<std::uint8_t, 64> buffer_{};
    std::uint64_t totalBytes_ = 0;
    std::size_t buffered_ = 0;
    bool finished_ = false;
};

std::array<std::uint8_t, 32> sha256(const void *data, std::size_t size);
std::array<std::uint8_t, 32> sha256File(const std::string &path, std::uint64_t *size = nullptr);
std::string hex(const std::array<std::uint8_t, 32> &digest);
std::array<std::uint8_t, 32> parseSha256(const std::string &value);

} // namespace tgmr
