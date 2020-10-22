#ifndef WASAPIAUDIODEVICE_HPP
#define WASAPIAUDIODEVICE_HPP

#include <atomic>
#include <thread>
#include <Audioclient.h>
#include <mmdeviceapi.h>
#include "../AudioDevice.hpp"
#include "WASAPIPointer.hpp"

namespace pcmplayer::wasapi
{
    class AudioDevice final: public pcmplayer::AudioDevice
    {
    public:
        AudioDevice(std::uint32_t initBufferSize,
                    std::uint32_t initSampleRate,
                    SampleFormat initSampleFormat,
                    std::uint16_t initChannels);
        ~AudioDevice() override;

        void start() final;
        void stop() final;

    private:
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

        std::atomic_bool running{false};
    };
}

#endif // WASAPIAUDIODEVICE_HPP
