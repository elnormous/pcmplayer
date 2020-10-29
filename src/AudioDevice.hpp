#ifndef AUDIODEVICE_HPP
#define AUDIODEVICE_HPP

#include <cstdint>
#include <string>

namespace pcmplayer
{
    class AudioDevice final
    {
    public:
        AudioDevice(std::uint32_t i, const std::string& n):
        id{i}, name{n} {}

        auto getId() const noexcept { return id; }
        auto getName() const noexcept { return name; }

    private:
        std::uint32_t id;
        std::string name;
    };
}

#endif // AUDIODEVICE_HPP
