#ifndef WASAPIAUDIOPLAYER_HPP
#define WASAPIAUDIOPLAYER_HPP

#include <vector>
#include <Audioclient.h>
#include <mmdeviceapi.h>
#include "../AudioPlayer.hpp"
#include "../AudioDevice.hpp"
#include "WASAPIPointer.hpp"
#include "WASAPIErrorCategory.hpp"

namespace pcmplayer::wasapi
{
    extern const ErrorCategory errorCategory;

    class AudioPlayer final: public pcmplayer::AudioPlayer
    {
    public:
        AudioPlayer(std::uint32_t audioDeviceId,
                    std::uint32_t initBufferSize,
                    std::uint32_t initSampleRate,
                    SampleFormat initSampleFormat,
                    std::uint16_t initChannels);
        ~AudioPlayer() override;

        void start() final;
        void stop() final;

        static std::vector<AudioDevice> getAudioDevices();

    private:
        static std::string getDeviceName(IMMDevice* device);
        void run();

        Pointer<IMMDeviceEnumerator> enumerator;
        Pointer<IMMDevice> device;
        Pointer<IMMNotificationClient> notificationClient;
        Pointer<IAudioClient> audioClient;
        Pointer<IAudioRenderClient> renderClient;
        HANDLE notifyEvent = nullptr;

        UINT32 bufferFrameCount;
        std::uint32_t sampleSize = 0;
        bool started = false;
        std::vector<float> data;
    };
}

#endif // WASAPIAUDIOPLAYER_HPP
