#include <system_error>
#include "CAAudioPlayer.hpp"

namespace pcmplayer::coreaudio
{
    namespace
    {
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
            auto audioPlayer = static_cast<pcmplayer::coreaudio::AudioPlayer*>(inRefCon);

            try
            {
                audioPlayer->outputCallback(ioData);
            }
            catch (const std::exception& e)
            {
                //std::cerr << e.what();
                return -1;
            }

            return noErr;
        }
    }

    const ErrorCategory errorCategory {};

    AudioPlayer::AudioPlayer(std::uint32_t audioDeviceId,
                             std::uint32_t initBufferSize,
                             std::uint32_t initSampleRate,
                             SampleFormat initSampleFormat,
                             std::uint16_t initChannels):
        pcmplayer::AudioPlayer(Driver::coreAudio, initBufferSize, initSampleRate, initSampleFormat, initChannels)
    {
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

        if (const auto result = AudioObjectAddPropertyListener(kAudioObjectSystemObject,
                                                               &deviceListAddress,
                                                               deviceListChanged,
                                                               this); result != noErr)
            throw std::system_error(result, errorCategory, "Failed to add CoreAudio property listener");

        if (audioDeviceId == 0)
        {
            constexpr AudioObjectPropertyAddress defaultDeviceAddress = {
                kAudioHardwarePropertyDefaultOutputDevice,
                kAudioObjectPropertyScopeGlobal,
                kAudioObjectPropertyElementMaster
            };

            UInt32 size = sizeof(AudioDeviceID);
            if (const auto result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                                               &defaultDeviceAddress,
                                                               0,
                                                               nullptr,
                                                               &size,
                                                               &deviceId); result != noErr)
                throw std::system_error(result, errorCategory, "Failed to get CoreAudio output device");
        }
        else
        {
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

            if (audioDeviceId > deviceIds.size())
                throw std::runtime_error("Invalid device");

            deviceId = deviceIds[audioDeviceId - 1];
        }

        constexpr AudioObjectPropertyAddress aliveAddress = {
            kAudioDevicePropertyDeviceIsAlive,
            kAudioDevicePropertyScopeOutput,
            kAudioObjectPropertyElementMaster
        };

        UInt32 alive = 0;
        UInt32 aliveSize = sizeof(alive);
        if (const auto result = AudioObjectGetPropertyData(deviceId,
                                                           &aliveAddress,
                                                           0,
                                                           nullptr,
                                                           &aliveSize,
                                                           &alive); result != noErr)
            throw std::system_error(result, errorCategory, "Failed to get CoreAudio device status");

        if (!alive)
            throw std::runtime_error("Requested CoreAudio device is not alive");

        constexpr AudioObjectPropertyAddress hogModeAddress = {
            kAudioDevicePropertyHogMode,
            kAudioDevicePropertyScopeOutput,
            kAudioObjectPropertyElementMaster
        };

        pid_t pid = 0;
        UInt32 pidSize = sizeof(pid);
        if (const auto result = AudioObjectGetPropertyData(deviceId,
                                                           &hogModeAddress,
                                                           0,
                                                           nullptr,
                                                           &pidSize,
                                                           &pid); result != noErr)
            throw std::system_error(result, errorCategory, "Failed to check if CoreAudio device is in hog mode");

        if (pid != -1)
            throw std::runtime_error("Requested CoreAudio device is being hogged");

        //std::cout << "Using " << getDeviceName(deviceId) << " for audio";
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
        if (const auto result = AudioObjectAddPropertyListener(deviceId,
                                                               &aliveAddress,
                                                               deviceUnplugged,
                                                               this); result != noErr)
            throw std::system_error(result, errorCategory, "Failed to add CoreAudio property listener");
#endif

        if (const auto result = AudioComponentInstanceNew(audioComponent,
                                                          &audioUnit); result != noErr)
            throw std::system_error(result, errorCategory, "Failed to create CoreAudio component instance");

#if TARGET_OS_MAC && !TARGET_OS_IOS && !TARGET_OS_TV
        if (const auto result = AudioUnitSetProperty(audioUnit,
                                                     kAudioOutputUnitProperty_CurrentDevice,
                                                     kAudioUnitScope_Global,
                                                     0,
                                                     &deviceId,
                                                     sizeof(AudioDeviceID)); result != noErr)
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

        if (const auto result = AudioUnitSetProperty(audioUnit,
                                                     kAudioUnitProperty_StreamFormat,
                                                     kAudioUnitScope_Input,
                                                     bus,
                                                     &streamDescription,
                                                     sizeof(streamDescription)); result != noErr)
        {
            // std::cerr << "Failed to set CoreAudio unit stream format to float, error: " << result;

            streamDescription.mFormatFlags = kLinearPCMFormatFlagIsPacked | kAudioFormatFlagIsSignedInteger;
            streamDescription.mBitsPerChannel = sizeof(std::int16_t) * 8;
            streamDescription.mBytesPerFrame = streamDescription.mBitsPerChannel * streamDescription.mChannelsPerFrame / 8;
            streamDescription.mBytesPerPacket = streamDescription.mBytesPerFrame * streamDescription.mFramesPerPacket;

            if (const auto setPropertyResult = AudioUnitSetProperty(audioUnit,
                                                                    kAudioUnitProperty_StreamFormat,
                                                                    kAudioUnitScope_Input,
                                                                    bus,
                                                                    &streamDescription,
                                                                    sizeof(streamDescription)); setPropertyResult != noErr)
                throw std::system_error(setPropertyResult, errorCategory, "Failed to set CoreAudio unit stream format");

            sampleFormat = SampleFormat::signedInt16;
            sampleSize = sizeof(std::int16_t);
        }

        AURenderCallbackStruct callback;
        callback.inputProc = coreaudio::outputCallback;
        callback.inputProcRefCon = this;
        if (const auto result = AudioUnitSetProperty(audioUnit,
                                                     kAudioUnitProperty_SetRenderCallback,
                                                     kAudioUnitScope_Input,
                                                     bus,
                                                     &callback,
                                                     sizeof(callback)); result != noErr)
            throw std::system_error(result, errorCategory, "Failed to set CoreAudio unit output callback");

#if TARGET_OS_MAC && !TARGET_OS_IOS && !TARGET_OS_TV
        const UInt32 inIOBufferFrameSize = static_cast<UInt32>(bufferSize);
        if (const auto result = AudioUnitSetProperty(audioUnit,
                                                     kAudioDevicePropertyBufferFrameSize,
                                                     kAudioUnitScope_Global,
                                                     0,
                                                     &inIOBufferFrameSize,
                                                     sizeof(UInt32)); result != noErr)
            throw std::system_error(result, errorCategory, "Failed to set CoreAudio buffer size");
#endif

        if (const auto result = AudioUnitInitialize(audioUnit); result != noErr)
            throw std::system_error(result, errorCategory, "Failed to initialize CoreAudio unit");
    }

    AudioPlayer::~AudioPlayer()
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
                                 kAudioUnitScope_Input,
                                 bus,
                                 &callback,
                                 sizeof(callback));

            AudioComponentInstanceDispose(audioUnit);
        }

#if TARGET_OS_MAC && !TARGET_OS_IOS && !TARGET_OS_TV
        constexpr AudioObjectPropertyAddress deviceListAddress = {
            kAudioHardwarePropertyDevices,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMaster
        };

        AudioObjectRemovePropertyListener(kAudioObjectSystemObject,
                                          &deviceListAddress,
                                          deviceListChanged,
                                          this);

        if (deviceId)
        {
            constexpr AudioObjectPropertyAddress aliveAddress = {
                kAudioDevicePropertyDeviceIsAlive,
                kAudioDevicePropertyScopeOutput,
                kAudioObjectPropertyElementMaster
            };

            AudioObjectRemovePropertyListener(deviceId,
                                              &aliveAddress,
                                              deviceUnplugged,
                                              this);
        }
#endif
    }

    void AudioPlayer::start()
    {
        running = true;

        if (const auto result = AudioOutputUnitStart(audioUnit); result != noErr)
            throw std::system_error(result, errorCategory, "Failed to start CoreAudio output unit");

        run();
    }

    void AudioPlayer::stop()
    {
        if (const auto result = AudioOutputUnitStop(audioUnit); result != noErr)
            throw std::system_error(result, errorCategory, "Failed to stop CoreAudio output unit");

        running = false;
    }

    void AudioPlayer::outputCallback(AudioBufferList* ioData)
    {
        for (UInt32 i = 0; i < ioData->mNumberBuffers; ++i)
        {
            AudioBuffer& buffer = ioData->mBuffers[i];
            bool hasMoreData = getData(buffer.mDataByteSize / (sampleSize * channels), data);
            std::copy(data.begin(), data.end(), static_cast<float*>(buffer.mData));

            if (!hasMoreData)
            {
                std::unique_lock<std::mutex> lock(runningMutex);
                running = false;
                lock.unlock();
                runningCondition.notify_all();
            }
        }
    }

    void AudioPlayer::run()
    {
        std::unique_lock<std::mutex> lock(runningMutex);
        while (running) runningCondition.wait(lock);
    }
}
