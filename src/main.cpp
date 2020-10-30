#include <iostream>
#include <fstream>
#include <string>
#include "Wav.hpp"
#if defined(_WIN32)
#  include "wasapi/WASAPIAudioPlayer.hpp"
#else
#  include "coreaudio/CAAudioPlayer.hpp"
#endif

#if defined(_WIN32)
class Com final
{
public:
    Com()
    {
        if (const auto hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED); FAILED(hr))
            throw std::system_error(hr, std::system_category(), "Failed to initialize COM");
    }

    ~Com()
    {
        CoUninitialize();
    }

    Com(const Com&) = delete;
    Com& operator=(const Com&) = delete;
};
#endif

int main(int argc, char* argv[])
{
#if defined(_WIN32)
    Com com;
#endif

    try
    {
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
            else if (std::string(argv[arg]) == "--output")
            {
                if (++arg >= argc) throw std::runtime_error("Expected a parameter");
                outputFilename = argv[arg];
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
            else if (std::string(argv[arg]) == "--device")
            {
                if (++arg >= argc) throw std::runtime_error("Expected a parameter");
                outputDeviceId = static_cast<std::uint32_t>(std::stoi(argv[arg]));
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
        std::vector<float> buffer(delay * input.getChannels());
        buffer.insert(buffer.end(), input.getSamples().begin(), input.getSamples().end());

        audioPlayer.play(buffer);
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
