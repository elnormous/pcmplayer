#include <system_error>
#include <TargetConditionals.h>
#if TARGET_OS_MAC && !TARGET_OS_IOS && !TARGET_OS_TV
#  include <CoreAudio/CoreAudio.h>
#endif
#include "CAAudioPlayer.hpp"

namespace pcmplayer::coreaudio
{
    std::string AudioPlayer::getDeviceName(AudioDeviceID deviceId)
    {
        constexpr AudioObjectPropertyAddress nameAddress = {
            kAudioObjectPropertyName,
            kAudioDevicePropertyScopeOutput,
            kAudioObjectPropertyElementMaster
        };

        CFStringRef tempStringRef = nullptr;
        UInt32 stringSize = sizeof(CFStringRef);
        if (const auto result = AudioObjectGetPropertyData(deviceId,
                                                           &nameAddress,
                                                           0,
                                                           nullptr,
                                                           &stringSize,
                                                           &tempStringRef); result != noErr)
            throw std::system_error(result, errorCategory, "Failed to get CoreAudio device name");

        if (tempStringRef)
        {
            std::string name;
            if (const char* deviceName = CFStringGetCStringPtr(tempStringRef, kCFStringEncodingUTF8))
                name = deviceName;
            else
            {
                const CFIndex stringLength = CFStringGetLength(tempStringRef);
                std::vector<char> temp(static_cast<std::size_t>(CFStringGetMaximumSizeForEncoding(stringLength, kCFStringEncodingUTF8)) + 1);
                if (CFStringGetCString(tempStringRef, temp.data(), static_cast<CFIndex>(temp.size()), kCFStringEncodingUTF8))
                    name = temp.data();
            }
            CFRelease(tempStringRef);

            return name;
        }

        return "";
    }

    std::vector<AudioDevice> AudioPlayer::getAudioDevices()
    {
        std::vector<AudioDevice> result;

#if TARGET_OS_IOS || TARGET_OS_TV
#else
        // get the default audio device
        constexpr AudioObjectPropertyAddress defaultDeviceAddress = {
            kAudioHardwarePropertyDefaultOutputDevice,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
        };

        AudioDeviceID deviceId;

        UInt32 size = sizeof(AudioDeviceID);
        if (const auto result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                                           &defaultDeviceAddress,
                                                           0,
                                                           nullptr,
                                                           &size,
                                                           &deviceId); result != noErr)
            throw std::system_error(result, errorCategory, "Failed to get CoreAudio output device");

        std::uint32_t audioDeviceId = 0;
        result.push_back(AudioDevice{audioDeviceId++, "Default device (" + getDeviceName(deviceId) + ")"});

        // get all audio devices
        constexpr AudioObjectPropertyAddress deviceListAddress = {
            kAudioHardwarePropertyDevices,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
        };

        UInt32 dataSize;
        if (const auto result = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject,
                                                               &deviceListAddress,
                                                               sizeof(deviceListAddress),
                                                               nullptr,
                                                               &dataSize); result != noErr)
            throw std::system_error(result, errorCategory, "Failed to get CoreAudio property data size");

        const auto deviceCount = dataSize / sizeof(AudioDeviceID);
        std::vector<AudioDeviceID> deviceIds(deviceCount);
        if (const auto result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                                           &deviceListAddress,
                                                           0,
                                                           nullptr,
                                                           &dataSize,
                                                           deviceIds.data()); result != noErr)
        throw std::system_error(result, errorCategory, "Failed to get CoreAudio output devices");

        for (const auto deviceId : deviceIds)
            result.push_back(AudioDevice{audioDeviceId++, getDeviceName(deviceId)});
#endif

        return result;
    }
}
