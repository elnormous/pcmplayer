#ifndef AUDIOPLAYER_HPP
#define AUDIOPLAYER_HPP

#include <cstdint>
#include <functional>
#include <vector>
#include "Driver.hpp"
#include "SampleFormat.hpp"

namespace pcmplayer
{
    class AudioPlayer
    {
    public:
        AudioPlayer(Driver initDriver,
                    std::uint32_t initBufferSize,
                    std::uint32_t initSampleRate,
                    SampleFormat initSampleFormat,
                    std::uint16_t initChannels) :
            driver(initDriver),
            bufferSize(initBufferSize),
            sampleRate(initSampleRate),
            sampleFormat(initSampleFormat),
            channels(initChannels)
        {
        }

        virtual ~AudioPlayer() = default;

        void play(const std::vector<float> s)
        {
            samples = s;
            start();
        }

    protected:
        virtual void start() = 0;
        virtual void stop() = 0;

        bool getData(std::uint32_t frames, std::vector<float>& result)
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

        Driver driver;

        SampleFormat sampleFormat = SampleFormat::signedInt16;
        std::uint32_t bufferSize; // in frames
        std::uint32_t sampleRate;
        std::uint16_t channels;

    private:
        std::vector<float> samples;
        std::size_t offset = 0;
    };
}

#endif // AUDIOPLAYER_HPP
