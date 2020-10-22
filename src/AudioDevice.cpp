#include "AudioDevice.hpp"

namespace pcmplayer
{
    AudioDevice::AudioDevice(Driver initDriver,
                             uint32_t initBufferSize,
                             uint32_t initSampleRate,
                             SampleFormat initSampleFormat,
                             uint16_t initChannels):
        driver(initDriver),
        bufferSize(initBufferSize),
        sampleRate(initSampleRate),
        sampleFormat(initSampleFormat),
        channels(initChannels)
    {
    }

    void AudioDevice::play(const std::vector<float> s)
    {
        samples = s;
        start();

        for (;;) {}
    }

    void AudioDevice::getData(uint32_t frames, std::vector<float>& result)
    {
        result.clear();
        result.reserve(frames * channels);

        result.insert(result.end(),
                      samples.begin() + offset * channels,
                      samples.begin() + (offset + frames) * channels);

        offset += frames;
    }
}
