#include <iostream>
#include <fstream>
#include <string>
#include "Wav.hpp"
#if defined(_WIN32)
#  include "wasapi/WASAPIAudioPlayer.hpp"
#  include "windows/Com.hpp"
#else
#  include "coreaudio/CAAudioPlayer.hpp"
#endif

int main(int argc, char* argv[])
{
#if defined(_WIN32)
    pcmplayer::Com com;
#endif

    try
    {
        enum class Output
        {
            file,
            device
        };
        Output output = Output::device;
        std::string inputFilename;
        std::string outputFilename;
        std::uint32_t outputDeviceId = 0;
        std::size_t delay = 0;

        for (int arg = 1; arg < argc; ++arg)
            if (std::string(argv[arg]) == "--help")
            {
                std::cout << "pcmplayer\n";
                return EXIT_SUCCESS;
            }
            else if (std::string(argv[arg]) == "--input")
            {
                if (++arg >= argc) throw std::runtime_error("Expected a parameter");
                inputFilename = argv[arg];
            }
            else if (std::string(argv[arg]) == "--output-file")
            {
                if (++arg >= argc) throw std::runtime_error("Expected a parameter");
                outputFilename = argv[arg];
                output = Output::file;
            }
            else if (std::string(argv[arg]) == "--devices")
            {
#if defined(_WIN32)
                const auto audioDevices = pcmplayer::wasapi::AudioPlayer::getAudioDevices();
#else
                const auto audioDevices = pcmplayer::coreaudio::AudioPlayer::getAudioDevices();
#endif
                for (const auto& audioDevice : audioDevices)
                    std::cout << audioDevice.getId() << ":\t" << audioDevice.getName() << '\n';

                return EXIT_SUCCESS;
            }
            else if (std::string(argv[arg]) == "--output-device")
            {
                if (++arg >= argc) throw std::runtime_error("Expected a parameter");
                outputDeviceId = static_cast<std::uint32_t>(std::stoi(argv[arg]));
                output = Output::device;
            }
            else if (std::string(argv[arg]) == "--delay")
            {
                if (++arg >= argc) throw std::runtime_error("Expected a parameter");
                delay = static_cast<size_t>(std::stoi(argv[arg], nullptr, 10));
            }

        if (inputFilename.empty())
            throw std::runtime_error("Missing input");

        std::ifstream inputFile(inputFilename, std::ios::binary);
        if (!inputFile)
            throw std::runtime_error("Failed to open " + inputFilename);

        Wav input(inputFile);
        std::vector<float> buffer(delay * input.getChannels());
        buffer.insert(buffer.end(), input.getSamples().begin(), input.getSamples().end());

        if (output == Output::file)
        {
            std::ofstream outputFile(outputFilename, std::ios::binary | std::ios::trunc);
            if (!outputFile)
                throw std::runtime_error("Failed to open " + outputFilename);

            Wav output(input.getChannels(),
                       input.getSampleRate(),
                       static_cast<std::uint32_t>(buffer.size() / input.getChannels()),
                       buffer);
            output.save(outputFile);

        }
        else if (output == Output::device)
        {
#if defined(_WIN32)
            pcmplayer::wasapi::AudioPlayer audioPlayer(outputDeviceId,
                                                       512,
                                                       input.getSampleRate(),
                                                       pcmplayer::SampleFormat::float32,
                                                       input.getChannels());
#else
            pcmplayer::coreaudio::AudioPlayer audioPlayer(outputDeviceId,
                                                          512,
                                                          input.getSampleRate(),
                                                          pcmplayer::SampleFormat::float32,
                                                          input.getChannels());
#endif

            audioPlayer.play(buffer);
        }
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
