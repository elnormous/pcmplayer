#ifndef WASAPIDEVICEENUMERATOR_HPP
#define WASAPIDEVICEENUMERATOR_HPP

#include <vector>
#include "../AudioDevice.hpp"

namespace pcmplayer::wasapi
{
    std::vector<AudioDevice> getAudioDevices();
}

#endif // WASAPIDEVICEENUMERATOR_HPP