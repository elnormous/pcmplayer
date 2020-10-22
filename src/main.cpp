#include <iostream>
#include <fstream>
#include <string>
#include "Wav.hpp"
#if defined(_WIN32)
#  include "wasapi/WASAPIAudioDevice.hpp"
#else
#  include "coreaudio/CAAudioDevice.hpp"
#endif

int main(int argc, char* argv[])
{
    try
    {
        std::string inputFilename;
        std::string outputFilename;
        uint64_t delay = 0;

        for (int arg = 1; arg < argc; ++arg)
            if (std::string(argv[arg]) == "--input")
            {
                if (++arg >= argc) throw std::runtime_error("Expected a parameter");
                inputFilename = argv[arg];
            }
            else if (std::string(argv[arg]) == "--output")
            {
                if (++arg >= argc) throw std::runtime_error("Expected a parameter");
                outputFilename = argv[arg];
            }
            else if (std::string(argv[arg]) == "--delay")
            {
                if (++arg >= argc) throw std::runtime_error("Expected a parameter");
                delay = std::strtoull(argv[arg], nullptr, 10);
            }

        if (inputFilename.empty())
            throw std::runtime_error("Missing input");

        std::ifstream inputFile(inputFilename, std::ios::binary);
        if (!inputFile)
            throw std::runtime_error("Failed to open " + inputFilename);

        Wav input(inputFile);

#if defined(_WIN32)
        pcmplayer::wasapi::AudioDevice audioDevice(512,
                                                   input.getSampleRate(),
                                                   pcmplayer::SampleFormat::float32,
                                                   input.getChannels());
#else
        pcmplayer::coreaudio::AudioDevice audioDevice(512,
                                                      input.getSampleRate(),
                                                      pcmplayer::SampleFormat::float32,
                                                      input.getChannels());
#endif
        audioDevice.play(input.getSamples());
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << '\n';
        return EXIT_FAILURE;
    }

    // TODO: pass PCM data to callback
    // TODO: add delay

    // TODO: save to WAV file

    return EXIT_SUCCESS;
}
