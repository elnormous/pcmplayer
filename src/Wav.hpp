#ifndef Wav_h
#define Wav_h

#include <istream>
#include <ostream>
#include <stdexcept>
#include <vector>

namespace
{
    constexpr std::uint16_t WAVE_FORMAT_PCM = 1;
    constexpr std::uint16_t WAVE_FORMAT_IEEE_FLOAT = 3;
}

class Wav final
{
public:
    Wav() = default;

    Wav(std::uint16_t c, std::uint32_t sr, std::uint32_t f, const std::vector<float>& s):
        channels{c}, sampleRate{sr}, frames{f}, samples{s}
    {
        if (channels < 1)
            throw std::runtime_error("Invalid channel count");
        if (sampleRate == 0)
            throw std::runtime_error("Invalid sample rate");
        if (s.size() != channels * f)
            throw std::runtime_error("Invalid sample count");
    }

    Wav(std::istream& input)
    {
        char riffHeader[4];
        input.read(riffHeader, sizeof(riffHeader));

        if (static_cast<char>(riffHeader[0]) != 'R' ||
            static_cast<char>(riffHeader[1]) != 'I' ||
            static_cast<char>(riffHeader[2]) != 'F' ||
            static_cast<char>(riffHeader[3]) != 'F')
            throw std::runtime_error("Failed to load sound file, not a RIFF format");

        char lengthBuffer[4];
        input.read(lengthBuffer, sizeof(lengthBuffer));

        const auto length = static_cast<std::uint32_t>(lengthBuffer[0] & 0xFFU) |
            static_cast<std::uint32_t>(static_cast<uint8_t>(lengthBuffer[1] & 0xFFU) << 8) |
            static_cast<std::uint32_t>(static_cast<uint8_t>(lengthBuffer[2] & 0xFFU) << 16) |
            static_cast<std::uint32_t>(static_cast<uint8_t>(lengthBuffer[3] & 0xFFU) << 24);

        char waveHeader[4];
        input.read(waveHeader, sizeof(waveHeader));

        size_t offset = 4;

        if (static_cast<char>(waveHeader[0]) != 'W' ||
            static_cast<char>(waveHeader[1]) != 'A' ||
            static_cast<char>(waveHeader[2]) != 'V' ||
            static_cast<char>(waveHeader[3]) != 'E')
            throw std::runtime_error("Failed to load sound file, not a WAVE file");

        std::uint16_t bitsPerSample = 0;
        std::uint16_t formatTag = 0;

        for (; offset < length;)
        {
            char chunkHeader[4];
            input.read(chunkHeader, sizeof(chunkHeader));

            offset += 4;

            char chunkSizeBuffer[4];
            input.read(chunkSizeBuffer, sizeof(chunkSizeBuffer));

            offset += 4;

            const auto chunkSize = (chunkSizeBuffer[0] & 0xFFU) |
                ((chunkSizeBuffer[1] & 0xFFU) << 8) |
                ((chunkSizeBuffer[2] & 0xFFU) << 16) |
                ((chunkSizeBuffer[3] & 0xFFU) << 24);

            if (static_cast<char>(chunkHeader[0]) == 'f' &&
                static_cast<char>(chunkHeader[1]) == 'm' &&
                static_cast<char>(chunkHeader[2]) == 't' &&
                static_cast<char>(chunkHeader[3]) == ' ')
            {
                char formatTagBuffer[2];
                input.read(formatTagBuffer, sizeof(formatTagBuffer));
                offset += sizeof(formatTagBuffer);

                formatTag = static_cast<std::uint16_t>((formatTagBuffer[0] & 0xFFU) |
                                                       ((formatTagBuffer[1] & 0xFFU) << 8));

                if (formatTag != WAVE_FORMAT_PCM && formatTag != WAVE_FORMAT_IEEE_FLOAT)
                    throw std::runtime_error("Failed to load sound file, unsupported format");

                char channelsBuffer[2];
                input.read(channelsBuffer, sizeof(channelsBuffer));
                offset += sizeof(channelsBuffer);

                channels = static_cast<std::uint16_t>(channelsBuffer[0]) |
                    (static_cast<std::uint16_t>(channelsBuffer[1]) << 8);

                char sampleRateBuffer[4];
                input.read(sampleRateBuffer, sizeof(sampleRateBuffer));
                offset += sizeof(sampleRateBuffer);

                sampleRate = (sampleRateBuffer[0] & 0xFFU) |
                    ((sampleRateBuffer[1] & 0xFFU) << 8) |
                    ((sampleRateBuffer[2] & 0xFFU) << 16) |
                    ((sampleRateBuffer[3] & 0xFFU) << 24);

                input.seekg(4, std::ios::cur); // skip byte rate
                offset += 4;
                input.seekg(2, std::ios::cur); // skip byte align
                offset += 2;

                char bitsPerSampleBuffer[2];
                input.read(bitsPerSampleBuffer, sizeof(bitsPerSampleBuffer));
                offset += sizeof(bitsPerSampleBuffer);

                bitsPerSample = (bitsPerSampleBuffer[0] & 0xFFU) |
                    ((bitsPerSampleBuffer[1] & 0xFFU) << 8);

                if (formatTag != WAVE_FORMAT_PCM && formatTag != WAVE_FORMAT_IEEE_FLOAT)
                {
                    char sizeBuffer[4];
                    input.read(sizeBuffer, sizeof(sizeBuffer));
                    offset += sizeof(sizeBuffer);

                    const auto size = (sizeBuffer[0] & 0xFFU) |
                        ((sizeBuffer[1] & 0xFFU) << 8) |
                        ((sizeBuffer[2] & 0xFFU) << 16) |
                        ((sizeBuffer[3] & 0xFFU) << 24);

                    input.seekg(size, std::ios::cur); // skip the additional data
                    offset += sizeof(size);
                }
            }
            else if (chunkHeader[0] == 'd' &&
                     chunkHeader[1] == 'a' &&
                     chunkHeader[2] == 't' &&
                     chunkHeader[3] == 'a')
            {
                std::vector<char> chunkBuffer(chunkSize);
                input.read(chunkBuffer.data(), chunkSize);

                const auto sampleCount = static_cast<std::uint32_t>(chunkBuffer.size() / (bitsPerSample / 8));
                frames = sampleCount / channels;
                samples.resize(sampleCount);

                if (formatTag == WAVE_FORMAT_PCM)
                {
                    switch (bitsPerSample)
                    {
                        case 8:
                        {
                            for (std::uint32_t frame = 0; frame < frames; ++frame)
                            {
                                float* outputFrame = &samples[frame * channels];

                                for (std::uint32_t channel = 0; channel < channels; ++channel)
                                {
                                    const auto* sourceData = &chunkBuffer[frame * channels + channel];
                                    const auto value = static_cast<std::uint8_t>(sourceData[0]);
                                    outputFrame[channel] = 2.0F * value / 255.0F - 1.0F;
                                }
                            }
                            break;
                        }
                        case 16:
                        {
                            for (std::uint32_t frame = 0; frame < frames; ++frame)
                            {
                                float* outputFrame = &samples[frame * channels];

                                for (std::uint32_t channel = 0; channel < channels; ++channel)
                                {
                                    const auto* sourceData = &chunkBuffer[(frame * channels + channel) * 2];
                                    const auto value = static_cast<int16_t>(static_cast<std::uint8_t>(sourceData[0]) |
                                                                            (static_cast<std::uint8_t>(sourceData[1]) << 8));
                                    outputFrame[channel] = static_cast<float>(value) / 32767.0F;
                                }
                            }
                            break;
                        }
                        case 24:
                        {
                            for (std::uint32_t frame = 0; frame < frames; ++frame)
                            {
                                float* outputFrame = &samples[frame * channels];

                                for (std::uint32_t channel = 0; channel < channels; ++channel)
                                {
                                    const auto* sourceData = &chunkBuffer[(frame * channels + channel) * 3];
                                    const auto value = (static_cast<std::uint8_t>(sourceData[2]) & 0x80) ?
                                        static_cast<int32_t>(static_cast<std::uint8_t>(sourceData[0]) |
                                                             (static_cast<std::uint8_t>(sourceData[1]) << 8) |
                                                             (static_cast<std::uint8_t>(sourceData[2]) << 16) |
                                                             (static_cast<std::uint8_t>(0xFF) << 24)) :
                                        static_cast<int32_t>(static_cast<std::uint8_t>(sourceData[0]) |
                                                             (static_cast<std::uint8_t>(sourceData[1]) << 8) |
                                                             (static_cast<std::uint8_t>(sourceData[2]) << 16));

                                    outputFrame[channel] = static_cast<float>(value / 8388607.0);
                                }
                            }
                            break;
                        }
                        case 32:
                        {
                            for (std::uint32_t frame = 0; frame < frames; ++frame)
                            {
                                float* outputFrame = &samples[frame * channels];

                                for (std::uint32_t channel = 0; channel < channels; ++channel)
                                {
                                    const auto* sourceData = &chunkBuffer[(frame * channels + channel) * 4];
                                    const auto value = static_cast<int32_t>(static_cast<std::uint8_t>(sourceData[0]) |
                                                                            (static_cast<std::uint8_t>(sourceData[1]) << 8) |
                                                                            (static_cast<std::uint8_t>(sourceData[2]) << 16) |
                                                                            (static_cast<std::uint8_t>(sourceData[3]) << 24));
                                    outputFrame[channel] = static_cast<float>(value / 2147483647.0);
                                }
                            }
                            break;
                        }
                        default:
                            throw std::runtime_error("Failed to load sound file, unsupported bit depth");
                    }
                }
                else if (formatTag == WAVE_FORMAT_IEEE_FLOAT)
                {
                    if (bitsPerSample == 32)
                    {
                        for (std::uint32_t frame = 0; frame < frames; ++frame)
                        {
                            float* outputFrame = &samples[frame * channels];

                            for (std::uint32_t channel = 0; channel < channels; ++channel)
                            {
                                const auto* sourceData = &chunkBuffer[(frame * channels + channel) * 4];
                                float value;
                                std::memcpy(&value, sourceData, sizeof(float));
                                outputFrame[channel] = value;
                            }
                        }
                    }
                    else
                        throw std::runtime_error("Failed to load sound file, unsupported bit depth");
                }

                input.seekg(chunkSize, std::ios::cur); // skip the data
                offset += chunkSize;
            }
            else
            {
                input.seekg(chunkSize, std::ios::cur); // skip the chunk
                offset += chunkSize;
            }

            // padding
            offset = ((offset + 1) & 0xFFFFFFFE);
            input.seekg(offset + sizeof(riffHeader) + sizeof(lengthBuffer), std::ios::beg);
        }
    }

    void save(std::ostream& output)
    {
        const std::size_t sampleSize = sizeof(float);

        char riffHeader[] = {'R', 'I', 'F', 'F'};

        const std::uint32_t waveHeaderSize = 4;
        const std::uint32_t chunkHeaderSize = 4;
        const std::uint32_t fmtChunkSize = 16;
        const std::uint16_t formatTag = WAVE_FORMAT_IEEE_FLOAT;
        const auto byteRate = static_cast<std::uint32_t>(sampleSize * channels * sampleRate);
        const std::uint16_t byteAlign = sampleSize * channels;
        const std::uint16_t bitsPerSample = sampleSize * 8;
        const auto dataChunkSize = static_cast<std::uint32_t>(samples.size() * sampleSize);

        const std::uint32_t dataLength = waveHeaderSize +
            chunkHeaderSize +
            fmtChunkSize +
            chunkHeaderSize +
            dataChunkSize;

        const char dataLengthBuffer[] = {static_cast<char>(dataLength),
            static_cast<char>(dataLength >> 8),
            static_cast<char>(dataLength >> 16),
            static_cast<char>(dataLength >> 24)
        };
        const char waveHeader[] = {'W', 'A', 'V', 'E'};

        const char fmtChunkHeader[] = {'f', 'm', 't', ' '};
        const char fmtChunkSizeBuffer[] = {static_cast<char>(fmtChunkSize),
            static_cast<char>(fmtChunkSize >> 8),
            static_cast<char>(fmtChunkSize >> 16),
            static_cast<char>(fmtChunkSize >> 24)
        };
        const char formatTagBuffer[] = {static_cast<char>(formatTag),
            static_cast<char>(formatTag >> 8)
        };
        const char channelsBuffer[] = {static_cast<char>(channels),
            static_cast<char>(channels >> 8)
        };
        const char sampleRateBuffer[] = {static_cast<char>(sampleRate),
            static_cast<char>(sampleRate >> 8),
            static_cast<char>(sampleRate >> 16),
            static_cast<char>(sampleRate >> 24)
        };
        const char byteRateBuffer[] = {static_cast<char>(byteRate),
            static_cast<char>(byteRate >> 8),
            static_cast<char>(byteRate >> 16),
            static_cast<char>(byteRate >> 24)
        };
        const char byteAlignBuffer[] = {static_cast<char>(byteAlign),
            static_cast<char>(byteAlign >> 8)
        };
        const char bitsPerSampleBuffer[] = {static_cast<char>(bitsPerSample),
            static_cast<char>(bitsPerSample >> 8)
        };

        const char dataChunkHeader[] = {'d', 'a', 't', 'a'};
        const char dataChunkSizeBuffer[] = {static_cast<char>(dataChunkSize),
            static_cast<char>(dataChunkSize >> 8),
            static_cast<char>(dataChunkSize >> 16),
            static_cast<char>(dataChunkSize >> 24)
        };
        std::vector<char> dataChunkBuffer(dataChunkSize);

        if (formatTag == WAVE_FORMAT_PCM)
        {
            switch (bitsPerSample)
            {
                case 8:
                {
                    for (std::uint32_t frame = 0; frame < frames; ++frame)
                    {
                        float* inputFrame = &samples[frame * channels];

                        for (std::uint32_t channel = 0; channel < channels; ++channel)
                        {
                            const auto value = static_cast<std::uint8_t>((inputFrame[channel] + 1.0) * 255.0 / 2.0);

                            auto* destData = &dataChunkBuffer[frame * channels + channel];
                            destData[0] = static_cast<char>(value);
                        }
                    }
                    break;
                }
                case 16:
                {
                    for (std::uint32_t frame = 0; frame < frames; ++frame)
                    {
                        float* inputFrame = &samples[frame * channels];

                        for (std::uint32_t channel = 0; channel < channels; ++channel)
                        {
                            const auto value = static_cast<std::int32_t>(inputFrame[channel] * 32767.0F);

                            auto* destData = &dataChunkBuffer[(frame * channels + channel) * 2];
                            destData[0] = static_cast<char>(value);
                            destData[1] = static_cast<char>(value >> 8);
                        }
                    }
                    break;
                }
                case 24:
                {
                    for (std::uint32_t frame = 0; frame < frames; ++frame)
                    {
                        float* inputFrame = &samples[frame * channels];

                        for (std::uint32_t channel = 0; channel < channels; ++channel)
                        {
                            const auto value = static_cast<std::int32_t>(inputFrame[channel] * 2147483647.0);

                            auto* destData = &dataChunkBuffer[(frame * channels + channel) * 3];
                            destData[0] = static_cast<char>(value >> 8);
                            destData[1] = static_cast<char>(value >> 16);
                            destData[1] = static_cast<char>(value >> 24);
                        }
                    }
                    break;
                }
                case 32:
                {
                    for (std::uint32_t frame = 0; frame < frames; ++frame)
                    {
                        float* inputFrame = &samples[frame * channels];

                        for (std::uint32_t channel = 0; channel < channels; ++channel)
                        {
                            const auto value = static_cast<std::int32_t>(inputFrame[channel] * 2147483647.0);

                            auto* destData = &dataChunkBuffer[(frame * channels + channel) * 4];
                            destData[0] = static_cast<char>(value);
                            destData[0] = static_cast<char>(value >> 8);
                            destData[1] = static_cast<char>(value >> 16);
                            destData[1] = static_cast<char>(value >> 24);
                        }
                    }
                    break;
                }
                default:
                    throw std::runtime_error("Failed to load sound file, unsupported bit depth");
            }
        }
        else if (formatTag == WAVE_FORMAT_IEEE_FLOAT)
        {
            if (bitsPerSample == 32)
            {
                for (std::uint32_t frame = 0; frame < frames; ++frame)
                {
                    float* inputFrame = &samples[frame * channels];

                    for (std::uint32_t channel = 0; channel < channels; ++channel)
                    {
                        const auto value = static_cast<float>(inputFrame[channel]);

                        auto* destData = &dataChunkBuffer[(frame * channels + channel) * 4];
                        std::memcpy(destData, &value, sizeof(float));
                    }
                }
            }
            else
                throw std::runtime_error("Failed to load sound file, unsupported bit depth");
        }

        output.write(riffHeader, sizeof(riffHeader));
        output.write(dataLengthBuffer, sizeof(dataLengthBuffer));
        output.write(waveHeader, sizeof(waveHeader));

        // fmt chunk
        output.write(fmtChunkHeader, sizeof(fmtChunkHeader));
        output.write(fmtChunkSizeBuffer, sizeof(fmtChunkSizeBuffer));
        output.write(formatTagBuffer, sizeof(formatTagBuffer));
        output.write(channelsBuffer, sizeof(channelsBuffer));
        output.write(sampleRateBuffer, sizeof(sampleRateBuffer));
        output.write(byteRateBuffer, sizeof(byteRateBuffer));
        output.write(byteAlignBuffer, sizeof(byteAlignBuffer));
        output.write(bitsPerSampleBuffer, sizeof(bitsPerSampleBuffer));

        // data chunk
        output.write(dataChunkHeader, sizeof(dataChunkHeader));
        output.write(dataChunkSizeBuffer, sizeof(dataChunkSizeBuffer));
        output.write(dataChunkBuffer.data(), dataChunkBuffer.size());
    }

    auto& getChannels() const noexcept { return channels; }
    auto& getSampleRate() const noexcept { return sampleRate; }
    auto& getFrames() const noexcept { return frames; }
    auto& getSamples() const noexcept { return samples; }

private:
    std::uint16_t channels = 0;
    std::uint32_t sampleRate = 0;
    std::uint32_t frames = 0;
    std::vector<float> samples;
};

#endif /* Wav_h */
