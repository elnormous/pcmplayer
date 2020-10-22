#ifndef CAAUDIODEVICE_HPP
#define CAAUDIODEVICE_HPP

#if defined(__APPLE__)
#  include <TargetConditionals.h>
#endif

#if TARGET_OS_MAC && !TARGET_OS_IOS && !TARGET_OS_TV
#  include <CoreAudio/CoreAudio.h>
#endif

#include <AudioUnit/AudioUnit.h>

#include "../AudioDevice.hpp"

namespace pcmplayer::coreaudio
{
    class AudioDevice: public pcmplayer::AudioDevice
    {
    public:
        AudioDevice(std::uint32_t initBufferSize,
                    std::uint32_t initSampleRate,
                    SampleFormat initSampleFormat,
                    std::uint16_t initChannels);
        ~AudioDevice();

        void start() final;
        void stop() final;

        void outputCallback(AudioBufferList* ioData);

    private:
#if TARGET_OS_MAC && !TARGET_OS_IOS && !TARGET_OS_TV
        AudioDeviceID deviceId = 0;
#endif
        AudioComponent audioComponent = nullptr;
        AudioUnit audioUnit = nullptr;

        std::uint32_t sampleSize = 0;
        std::vector<float> data;
    };
}

#endif // CAAUDIODEVICE_HPP
