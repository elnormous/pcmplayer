#ifndef AUDIODEVICE_HPP
#define AUDIODEVICE_HPP

#include <cstdint>
#include <string>

namespace pcmplayer
{
    class AudioDevice final
    {
        AudioDevice(std::uint32_t i, const std::string& n):
        id{i}, name{n} {}

    private:
        std::uint32_t id;
        std::string name;
    };
}

#endif // AUDIODEVICE_HPP
