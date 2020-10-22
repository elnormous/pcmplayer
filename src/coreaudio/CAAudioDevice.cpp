#include <system_error>
#include "CAAudioDevice.hpp"

namespace pcmplayer::coreaudio
{
    namespace
    {
        class ErrorCategory final: public std::error_category
        {
        public:
            const char* name() const noexcept final
            {
                return "CoreAudio";
            }

            std::string message(int condition) const final
            {
                switch (condition)
                {
                    case kAudio_UnimplementedError: return "kAudio_UnimplementedError";
                    case kAudio_FileNotFoundError: return "kAudio_FileNotFoundError";
                    case kAudio_FilePermissionError: return "kAudio_FilePermissionError";
                    case kAudio_TooManyFilesOpenError: return "kAudio_TooManyFilesOpenError";
                    case kAudio_BadFilePathError: return "kAudio_BadFilePathError";
                    case kAudio_ParamError: return "kAudio_ParamError";
                    case kAudio_MemFullError: return "kAudio_MemFullError";
                    default: return "Unknown error (" + std::to_string(condition) + ")";
                }
            }
        };

        const ErrorCategory errorCategory {};

#if TARGET_OS_MAC && !TARGET_OS_IOS && !TARGET_OS_TV
        OSStatus deviceListChanged(AudioObjectID, UInt32, const AudioObjectPropertyAddress*, void*)
        {
            // TODO: implement
            return 0;
        }

        OSStatus deviceUnplugged(AudioObjectID, UInt32, const AudioObjectPropertyAddress*, void*)
        {
            // TODO: implement
            return noErr;
        }
#endif

        OSStatus outputCallback(void* inRefCon,
                                AudioUnitRenderActionFlags*,
                                const AudioTimeStamp*,
                                UInt32, UInt32,
                                AudioBufferList* ioData)
        {
            pcmplayer::coreaudio::AudioDevice* audioDevice = static_cast<pcmplayer::coreaudio::AudioDevice*>(inRefCon);

            try
            {
                audioDevice->outputCallback(ioData);
            }
            catch (const std::exception& e)
            {
                //std::cerr << e.what();
                return -1;
            }

            return noErr;
        }
    }

    AudioDevice::AudioDevice(std::uint32_t initBufferSize,
                             std::uint32_t initSampleRate,
                             SampleFormat initSampleFormat,
                             std::uint16_t initChannels):
        pcmplayer::AudioDevice(Driver::coreAudio, initBufferSize, initSampleRate, initSampleFormat, initChannels)
    {
        OSStatus result;

#if TARGET_OS_IOS || TARGET_OS_TV
        id audioSession = reinterpret_cast<id (*)(Class, SEL)>(&objc_msgSend)(objc_getClass("AVAudioSession"), sel_getUid("sharedInstance")); // [AVAudioSession sharedInstance]
        if (!reinterpret_cast<BOOL (*)(id, SEL, id, id)>(&objc_msgSend)(audioSession, sel_getUid("setCategory:error:"), AVAudioSessionCategoryAmbient, nil)) // [audioSession setCategory:AVAudioSessionCategoryAmbient error:nil]
            throw std::runtime_error("Failed to set audio session category");

        id currentRoute = reinterpret_cast<id (*)(id, SEL)>(&objc_msgSend)(audioSession, sel_getUid("currentRoute")); // [audioSession currentRoute]
        id outputs = reinterpret_cast<id (*)(id, SEL)>(&objc_msgSend)(currentRoute, sel_getUid("outputs")); // [currentRoute outputs]
        const NSUInteger count = reinterpret_cast<NSUInteger (*)(id, SEL)>(&objc_msgSend)(outputs, sel_getUid("count")); // [outputs count]

        NSUInteger maxChannelCount = 0;
        for (NSUInteger outputIndex = 0; outputIndex < count; ++outputIndex)
        {
            id output = reinterpret_cast<id (*)(id, SEL, NSUInteger)>(&objc_msgSend)(outputs, sel_getUid("objectAtIndex:"), outputIndex); // [outputs objectAtIndex:outputIndex]
            id channels = reinterpret_cast<id (*)(id, SEL)>(&objc_msgSend)(output, sel_getUid("channels")); // [output channels]
            const NSUInteger channelCount = reinterpret_cast<NSUInteger (*)(id, SEL)>(&objc_msgSend)(channels, sel_getUid("count")); // [channels count]
            if (channelCount > maxChannelCount)
                maxChannelCount = channelCount;
        }

        if (channels > maxChannelCount)
            channels = static_cast<std::uint16_t>(maxChannelCount);
#elif TARGET_OS_MAC
        constexpr AudioObjectPropertyAddress deviceListAddress = {
            kAudioHardwarePropertyDevices,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
        };

        if ((result = AudioObjectAddPropertyListener(kAudioObjectSystemObject,
                                                     &deviceListAddress,
                                                     deviceListChanged,
                                                     this)) != noErr)
            throw std::system_error(result, errorCategory, "Failed to add CoreAudio property listener");

        constexpr AudioObjectPropertyAddress defaultDeviceAddress = {
            kAudioHardwarePropertyDefaultOutputDevice,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
        };

        UInt32 size = sizeof(AudioDeviceID);
        if ((result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &defaultDeviceAddress,
                                                 0, nullptr, &size, &deviceId)) != noErr)
            throw std::system_error(result, errorCategory, "Failed to get CoreAudio output device");

        constexpr AudioObjectPropertyAddress aliveAddress = {
            kAudioDevicePropertyDeviceIsAlive,
            kAudioDevicePropertyScopeOutput,
            kAudioObjectPropertyElementMaster
        };

        UInt32 alive = 0;
        size = sizeof(alive);
        if ((result = AudioObjectGetPropertyData(deviceId, &aliveAddress, 0, nullptr, &size, &alive)) != noErr)
            throw std::system_error(result, errorCategory, "Failed to get CoreAudio device status");

        if (!alive)
            throw std::system_error(result, errorCategory, "Requested CoreAudio device is not alive");

        constexpr AudioObjectPropertyAddress hogModeAddress = {
            kAudioDevicePropertyHogMode,
            kAudioDevicePropertyScopeOutput,
            kAudioObjectPropertyElementMaster
        };

        pid_t pid = 0;
        size = sizeof(pid);
        if ((result = AudioObjectGetPropertyData(deviceId, &hogModeAddress, 0, nullptr, &size, &pid)) != noErr)
            throw std::system_error(result, errorCategory, "Failed to check if CoreAudio device is in hog mode");

        if (pid != -1)
            throw std::runtime_error("Requested CoreAudio device is being hogged");

        constexpr AudioObjectPropertyAddress nameAddress = {
            kAudioObjectPropertyName,
            kAudioDevicePropertyScopeOutput,
            kAudioObjectPropertyElementMaster
        };

        CFStringRef tempStringRef = nullptr;
        size = sizeof(CFStringRef);

        if ((result = AudioObjectGetPropertyData(deviceId, &nameAddress,
                                                 0, nullptr, &size, &tempStringRef)) != noErr)
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

            // std::cout << "Using " << name << " for audio";
        }
#endif

        AudioComponentDescription desc;
        desc.componentType = kAudioUnitType_Output;

#if TARGET_OS_IOS || TARGET_OS_TV
        desc.componentSubType = kAudioUnitSubType_RemoteIO;
#elif TARGET_OS_MAC
        desc.componentSubType = kAudioUnitSubType_DefaultOutput;
#endif

        desc.componentManufacturer = kAudioUnitManufacturer_Apple;
        desc.componentFlags = 0;
        desc.componentFlagsMask = 0;

        audioComponent = AudioComponentFindNext(nullptr, &desc);

        if (!audioComponent)
            throw std::runtime_error("Failed to find requested CoreAudio component");

#if TARGET_OS_MAC && !TARGET_OS_IOS && !TARGET_OS_TV
        if ((result = AudioObjectAddPropertyListener(deviceId, &aliveAddress, deviceUnplugged, this)) != noErr)
            throw std::system_error(result, errorCategory, "Failed to add CoreAudio property listener");
#endif

        if ((result = AudioComponentInstanceNew(audioComponent, &audioUnit)) != noErr)
            throw std::system_error(result, errorCategory, "Failed to create CoreAudio component instance");

#if TARGET_OS_MAC && !TARGET_OS_IOS && !TARGET_OS_TV
        if ((result = AudioUnitSetProperty(audioUnit,
                                           kAudioOutputUnitProperty_CurrentDevice,
                                           kAudioUnitScope_Global, 0,
                                           &deviceId,
                                           sizeof(AudioDeviceID))) != noErr)
            throw std::system_error(result, errorCategory, "Failed to set CoreAudio unit property");
#endif

        constexpr AudioUnitElement bus = 0;

        AudioStreamBasicDescription streamDescription;
        streamDescription.mSampleRate = sampleRate;
        streamDescription.mFormatID = kAudioFormatLinearPCM;
        streamDescription.mFormatFlags = kLinearPCMFormatFlagIsFloat;
        streamDescription.mChannelsPerFrame = channels;
        streamDescription.mFramesPerPacket = 1;
        streamDescription.mBitsPerChannel = sizeof(float) * 8;
        streamDescription.mBytesPerFrame = streamDescription.mBitsPerChannel * streamDescription.mChannelsPerFrame / 8;
        streamDescription.mBytesPerPacket = streamDescription.mBytesPerFrame * streamDescription.mFramesPerPacket;
        streamDescription.mReserved = 0;

        sampleFormat = SampleFormat::float32;
        sampleSize = sizeof(float);

        if ((result = AudioUnitSetProperty(audioUnit,
                                           kAudioUnitProperty_StreamFormat,
                                           kAudioUnitScope_Input, bus, &streamDescription, sizeof(streamDescription))) != noErr)
        {
            // std::cerr << "Failed to set CoreAudio unit stream format to float, error: " << result;

            streamDescription.mFormatFlags = kLinearPCMFormatFlagIsPacked | kAudioFormatFlagIsSignedInteger;
            streamDescription.mBitsPerChannel = sizeof(std::int16_t) * 8;
            streamDescription.mBytesPerFrame = streamDescription.mBitsPerChannel * streamDescription.mChannelsPerFrame / 8;
            streamDescription.mBytesPerPacket = streamDescription.mBytesPerFrame * streamDescription.mFramesPerPacket;

            if ((result = AudioUnitSetProperty(audioUnit,
                                               kAudioUnitProperty_StreamFormat,
                                               kAudioUnitScope_Input, bus, &streamDescription, sizeof(streamDescription))) != noErr)
                throw std::system_error(result, errorCategory, "Failed to set CoreAudio unit stream format");

            sampleFormat = SampleFormat::signedInt16;
            sampleSize = sizeof(std::int16_t);
        }

        AURenderCallbackStruct callback;
        callback.inputProc = coreaudio::outputCallback;
        callback.inputProcRefCon = this;
        if ((result = AudioUnitSetProperty(audioUnit,
                                           kAudioUnitProperty_SetRenderCallback,
                                           kAudioUnitScope_Input, bus, &callback, sizeof(callback))) != noErr)
            throw std::system_error(result, errorCategory, "Failed to set CoreAudio unit output callback");

#if TARGET_OS_MAC && !TARGET_OS_IOS && !TARGET_OS_TV
        const UInt32 inIOBufferFrameSize = static_cast<UInt32>(bufferSize);
        if ((result = AudioUnitSetProperty(audioUnit,
                                           kAudioDevicePropertyBufferFrameSize,
                                           kAudioUnitScope_Global,
                                           0,
                                           &inIOBufferFrameSize, sizeof(UInt32))) != noErr)
            throw std::system_error(result, errorCategory, "Failed to set CoreAudio buffer size");
#endif

        if ((result = AudioUnitInitialize(audioUnit)) != noErr)
            throw std::system_error(result, errorCategory, "Failed to initialize CoreAudio unit");
    }

    AudioDevice::~AudioDevice()
    {
        if (audioUnit)
        {
            AudioOutputUnitStop(audioUnit);

            AURenderCallbackStruct callback;
            callback.inputProc = nullptr;
            callback.inputProcRefCon = nullptr;

            constexpr AudioUnitElement bus = 0;

            AudioUnitSetProperty(audioUnit,
                                 kAudioUnitProperty_SetRenderCallback,
                                 kAudioUnitScope_Input, bus, &callback, sizeof(callback));

            AudioComponentInstanceDispose(audioUnit);
        }

#if TARGET_OS_MAC && !TARGET_OS_IOS && !TARGET_OS_TV
        constexpr AudioObjectPropertyAddress deviceListAddress = {
            kAudioHardwarePropertyDevices,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
        };

        AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &deviceListAddress, deviceListChanged, this);

        if (deviceId)
        {
            constexpr AudioObjectPropertyAddress aliveAddress = {
                kAudioDevicePropertyDeviceIsAlive,
                kAudioDevicePropertyScopeOutput,
                kAudioObjectPropertyElementMaster
            };

            AudioObjectRemovePropertyListener(deviceId, &aliveAddress, deviceUnplugged, this);
        }
#endif
    }

    void AudioDevice::start()
    {
        OSStatus result;
        if ((result = AudioOutputUnitStart(audioUnit)) != noErr)
            throw std::system_error(result, errorCategory, "Failed to start CoreAudio output unit");
    }

    void AudioDevice::stop()
    {
        OSStatus result;
        if ((result = AudioOutputUnitStop(audioUnit)) != noErr)
            throw std::system_error(result, errorCategory, "Failed to stop CoreAudio output unit");
    }

    void AudioDevice::outputCallback(AudioBufferList* ioData)
    {
        for (UInt32 i = 0; i < ioData->mNumberBuffers; ++i)
        {
            AudioBuffer& buffer = ioData->mBuffers[i];
            getData(buffer.mDataByteSize / (sampleSize * channels), data);
            std::copy(data.begin(), data.end(), static_cast<float*>(buffer.mData));
        }
    }
}
