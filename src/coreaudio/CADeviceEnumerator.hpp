#ifndef CADEVICEENUMERATOR_HPP
#define CADEVICEENUMERATOR_HPP

#include <vector>
#include "../AudioDevice.hpp"

namespace pcmplayer::wasapi
{
    std::vector<AudioDevice> getAudioDevices();
}

#endif // CADEVICEENUMERATOR_HPP
