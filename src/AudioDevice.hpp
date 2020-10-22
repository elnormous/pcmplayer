#ifndef AUDIODEVICE_HPP
#define AUDIODEVICE_HPP

#include <cstdint>
#include <functional>
#include <vector>
#include "Driver.hpp"
#include "SampleFormat.hpp"

namespace pcmplayer
{
    class AudioDevice
    {
    public:
        AudioDevice(Driver initDriver,
                    uint32_t initBufferSize,
                    uint32_t initSampleRate,
                    SampleFormat initSampleFormat,
                    uint16_t initChannels);
        virtual ~AudioDevice() = default;

        void play(const std::vector<float> s);

    protected:
        virtual void start() = 0;
        virtual void stop() = 0;

        void getData(uint32_t frames, std::vector<float>& result);

        Driver driver;

        SampleFormat sampleFormat = SampleFormat::signedInt16;
        uint32_t bufferSize; // in frames
        uint32_t sampleRate;
        uint16_t channels;

    private:
        std::function<void(uint32_t frames, uint16_t channels, uint32_t sampleRate, std::vector<float>& samples)> dataGetter;
        std::vector<float> samples;
        std::size_t offset = 0;
    };
}

#endif // AUDIODEVICE_HPP
