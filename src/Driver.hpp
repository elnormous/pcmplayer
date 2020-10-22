#ifndef DRIVER_HPP
#define DRIVER_HPP

namespace pcmplayer
{
    enum class Driver
    {
        none,
        openSL,
        coreAudio,
        alsa,
        wasapi
    };
}

#endif // DRIVER_HPP
