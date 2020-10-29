#include "AudioPlayer.hpp"

namespace pcmplayer
{
    AudioPlayer::AudioPlayer(Driver initDriver,
                             std::uint32_t initBufferSize,
                             std::uint32_t initSampleRate,
                             SampleFormat initSampleFormat,
                             std::uint16_t initChannels):
        driver(initDriver),
        bufferSize(initBufferSize),
        sampleRate(initSampleRate),
        sampleFormat(initSampleFormat),
        channels(initChannels)
    {
    }

    void AudioPlayer::play(const std::vector<float> s)
    {
        samples = s;
        start();
    }

    bool AudioPlayer::getData(std::uint32_t frames, std::vector<float>& result)
    {
        result.clear();
        result.reserve(frames * channels);

        const std::size_t bufferFrames = samples.size() / channels;
        const std::size_t remainingFrames = bufferFrames - offset;

        if (remainingFrames > frames)
        {
            result.insert(result.end(),
                          samples.begin() + offset * channels,
                          samples.begin() + (offset + frames) * channels);

            offset += frames;

            return true;
        }
        else
        {
            result.insert(result.end(),
                samples.begin() + offset * channels,
                samples.end());

            offset = bufferFrames;

            return false;
        }
    }
}
