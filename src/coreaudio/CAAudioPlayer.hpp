#ifndef CAAUDIOPLAYER_HPP
#define CAAUDIOPLAYER_HPP

#include <condition_variable>
#include <mutex>
#include <vector>
#if defined(__APPLE__)
#  include <TargetConditionals.h>
#endif

#if TARGET_OS_MAC && !TARGET_OS_IOS && !TARGET_OS_TV
#  include <CoreAudio/CoreAudio.h>
#endif

#include <AudioUnit/AudioUnit.h>

#include "../AudioPlayer.hpp"
#include "../AudioDevice.hpp"
#include "CAErrorCategory.hpp"

namespace pcmplayer::coreaudio
{
    class AudioPlayer: public pcmplayer::AudioPlayer
    {
    public:
        AudioPlayer(std::uint32_t audioDeviceId,
                    std::uint32_t initBufferSize,
                    std::uint32_t initSampleRate,
                    SampleFormat initSampleFormat,
                    std::uint16_t initChannels);
        ~AudioPlayer();

        void start() final;
        void stop() final;

        void outputCallback(AudioBufferList* ioData);

        static std::vector<AudioDevice> getAudioDevices();

    private:
        void run();

#if TARGET_OS_MAC && !TARGET_OS_IOS && !TARGET_OS_TV
        AudioDeviceID deviceId = 0;
#endif
        AudioComponent audioComponent = nullptr;
        AudioUnit audioUnit = nullptr;

        std::uint32_t sampleSize = 0;
        std::vector<float> data;

        std::mutex runningMutex;
        std::condition_variable runningCondition;
        bool running = false;
    };
}

#endif // CAAUDIOPLAYER_HPP
