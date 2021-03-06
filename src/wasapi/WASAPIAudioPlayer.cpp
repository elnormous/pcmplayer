#include <mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>
#include "WASAPIAudioPlayer.hpp"

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

namespace pcmplayer::wasapi
{
    namespace
    {
        class NotificationClient final: public IMMNotificationClient
        {
        public:
            NotificationClient() noexcept = default;
            NotificationClient(const NotificationClient&) = delete;
            NotificationClient(NotificationClient&&) = delete;
            NotificationClient& operator=(const NotificationClient&) = delete;
            NotificationClient& operator=(NotificationClient&&) = delete;

            ULONG STDMETHODCALLTYPE AddRef()
            {
                return InterlockedIncrement(&refCount);
            }

            ULONG STDMETHODCALLTYPE Release()
            {
                const ULONG newRefCount = InterlockedDecrement(&refCount);
                if (!newRefCount)
                    delete this;

                return newRefCount;
            }

            HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID** ppvInterface)
            {
                if (riid == IID_IUnknown ||
                    riid == __uuidof(IMMNotificationClient))
                {
                    AddRef();
                    *ppvInterface = this;
                }
                else
                {
                    *ppvInterface = nullptr;
                    return E_NOINTERFACE;
                }
                return S_OK;
            }

            HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId)
            {
                return S_OK;
            };

            HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId)
            {
                return S_OK;
            }

            HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState)
            {
                switch (dwNewState)
                {
                    case DEVICE_STATE_ACTIVE:
                        break;

                    case DEVICE_STATE_DISABLED:
                        break;

                    case DEVICE_STATE_NOTPRESENT:
                        break;

                    case DEVICE_STATE_UNPLUGGED:
                        break;
                }

                return S_OK;
            }

            HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId)
            {
                if (role == eConsole)
                {
                    if (flow == eRender)
                    {
                        // TODO: implement
                    }
                    else if (flow == eCapture)
                    {
                        // TODO: implement
                    }
                }

                return S_OK;
            }

            HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key)
            {
                return S_OK;
            }

        private:
            LONG refCount = 1;
        };
    }

    const ErrorCategory errorCategory{};

    AudioPlayer::AudioPlayer(std::uint32_t audioDeviceId,
                             std::uint32_t initBufferSize,
                             std::uint32_t initSampleRate,
                             SampleFormat initSampleFormat,
                             std::uint16_t initChannels):
        pcmplayer::AudioPlayer(Driver::wasapi, initBufferSize, initSampleRate, initSampleFormat, initChannels)
    {
        LPVOID enumeratorPointer;
        if (const auto hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, IID_IMMDeviceEnumerator, &enumeratorPointer); FAILED(hr))
            throw std::system_error(hr, std::system_category(), "Failed to create device enumerator");

        enumerator = static_cast<IMMDeviceEnumerator*>(enumeratorPointer);

        if (audioDeviceId == 0)
        {
            IMMDevice* devicePointer;
            if (const auto hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &devicePointer); FAILED(hr))
                throw std::system_error(hr, errorCategory, "Failed to get audio endpoint");

            device = devicePointer;
        }
        else
        {
            IMMDeviceCollection* deviceCollectionPointer;
            if (const auto hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &deviceCollectionPointer); FAILED(hr))
                throw std::system_error(hr, errorCategory, "Failed to enumerate endpoints");

           Pointer<IMMDeviceCollection> deviceCollection = deviceCollectionPointer;

            UINT count;
            if (const auto hr = deviceCollection->GetCount(&count); FAILED(hr))
                throw std::system_error(hr, errorCategory, "Failed to get endpoint count");

            if (audioDeviceId > count)
                throw std::runtime_error("Invalid device");

            IMMDevice* devicePointer;
            if (const auto hr = deviceCollection->Item(audioDeviceId - 1, &devicePointer); FAILED(hr))
                throw std::system_error(hr, errorCategory, "Failed to get device");

            device = devicePointer;
        }

        notificationClient = new NotificationClient();

        if (const auto hr = enumerator->RegisterEndpointNotificationCallback(notificationClient.get()); FAILED(hr))
            throw std::system_error(hr, errorCategory, "Failed to get audio endpoint");

        void* audioClientPointer;
        if (const auto hr = device->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, &audioClientPointer); FAILED(hr))
            throw std::system_error(hr, errorCategory, "Failed to activate audio device");

        audioClient = static_cast<IAudioClient*>(audioClientPointer);

        WAVEFORMATEX* audioClientWaveFormat;

        if (const auto hr = audioClient->GetMixFormat(&audioClientWaveFormat); FAILED(hr))
            throw std::system_error(hr, errorCategory, "Failed to get audio mix format");

        WAVEFORMATEX waveFormat;
        waveFormat.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        waveFormat.nChannels = static_cast<WORD>(channels);
        waveFormat.nSamplesPerSec = sampleRate;
        waveFormat.wBitsPerSample = sizeof(float) * 8;
        waveFormat.nBlockAlign = waveFormat.nChannels * (waveFormat.wBitsPerSample / 8);
        waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
        waveFormat.cbSize = 0;

        sampleFormat = SampleFormat::float32;
        sampleSize = sizeof(float);

        const DWORD streamFlags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
            (waveFormat.nSamplesPerSec != audioClientWaveFormat->nSamplesPerSec ? AUDCLNT_STREAMFLAGS_RATEADJUST : 0);

        CoTaskMemFree(audioClientWaveFormat);

        constexpr std::uint64_t timesPerSecond = 10'000'000U;
        auto bufferPeriod = static_cast<REFERENCE_TIME>(512U * timesPerSecond / waveFormat.nSamplesPerSec);

        WAVEFORMATEX* closesMatch;
        if (!FAILED(audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED,
                                                   &waveFormat,
                                                   &closesMatch)))
        {
            // TODO: implement
        }

        CoTaskMemFree(closesMatch);

        if (const auto floatResult = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                                             streamFlags,
                                                             bufferPeriod,
                                                             0,
                                                             &waveFormat,
                                                             nullptr); FAILED(floatResult))
        {
            waveFormat.wFormatTag = WAVE_FORMAT_PCM;
            waveFormat.wBitsPerSample = sizeof(std::int16_t) * 8;
            waveFormat.nBlockAlign = waveFormat.nChannels * (waveFormat.wBitsPerSample / 8);
            waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;

            if (const auto pcmResult = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                                               streamFlags,
                                                               bufferPeriod,
                                                               0,
                                                               &waveFormat,
                                                               nullptr); FAILED(pcmResult))
                throw std::system_error(pcmResult, errorCategory, "Failed to initialize audio client");

            sampleFormat = SampleFormat::signedInt16;
            sampleSize = sizeof(std::int16_t);
        }

        // init output device
        if (const auto hr = audioClient->GetBufferSize(&bufferFrameCount); FAILED(hr))
            throw std::system_error(hr, errorCategory, "Failed to get audio buffer size");
        bufferSize = bufferFrameCount * channels;

        void* renderClientPointer;
        if (const auto hr = audioClient->GetService(IID_IAudioRenderClient, &renderClientPointer); FAILED(hr))
            throw std::system_error(GetLastError(), std::system_category(), "Failed to get render client service");

        renderClient = static_cast<IAudioRenderClient*>(renderClientPointer);

        notifyEvent = CreateEvent(nullptr, false, false, nullptr);
        if (!notifyEvent)
            throw std::system_error(GetLastError(), std::system_category(), "Failed to create event");

        if (const auto hr = audioClient->SetEventHandle(notifyEvent); FAILED(hr))
            throw std::system_error(hr, errorCategory, "Failed to set event handle");
    }

    AudioPlayer::~AudioPlayer()
    {
        if (notifyEvent) CloseHandle(notifyEvent);

        if (audioClient && started) audioClient->Stop();
    }

    void AudioPlayer::start()
    {
        if (const auto hr = audioClient->Start(); FAILED(hr))
            throw std::system_error(hr, errorCategory, "Failed to start audio");

        started = true;
        run();
    }

    void AudioPlayer::stop()
    {
        if (started)
        {
            if (const auto hr = audioClient->Stop(); FAILED(hr))
                throw std::system_error(hr, errorCategory, "Failed to stop audio");
            
            started = false;
        }
    }

    void AudioPlayer::run()
    {
        for (;;)
        {
            try
            {
                DWORD result;
                if ((result = WaitForSingleObject(notifyEvent, INFINITE)) == WAIT_FAILED)
                    throw std::system_error(GetLastError(), std::system_category(), "Failed to wait for event");

                if (result == WAIT_OBJECT_0)
                {
                    UINT32 bufferPadding;
                    if (const auto hr = audioClient->GetCurrentPadding(&bufferPadding); FAILED(hr))
                        throw std::system_error(hr, errorCategory, "Failed to get buffer padding");

                    const UINT32 frameCount = bufferFrameCount - bufferPadding;
                    if (frameCount != 0)
                    {
                        BYTE* renderBuffer;
                        if (const auto hr = renderClient->GetBuffer(frameCount, &renderBuffer); FAILED(hr))
                            throw std::system_error(hr, errorCategory, "Failed to get buffer");

                        bool hasMoreData = getData(frameCount, data);

                        std::copy(data.begin(), data.end(), reinterpret_cast<float*>(renderBuffer));

                        if (const auto hr = renderClient->ReleaseBuffer(frameCount, 0); FAILED(hr))
                            throw std::system_error(hr, errorCategory, "Failed to release buffer");

                        if (!hasMoreData)
                            return;
                    }
                }
            }
            catch (const std::exception& e)
            {
                // e.what();
            }
        }
    }

    namespace
    {
        std::string getDeviceName(IMMDevice* device)
        {
            LPWSTR deviceId;
            device->GetId(&deviceId);

            CoTaskMemFree(deviceId);

            IPropertyStore* propertyStorePointer;
            if (const auto hr = device->OpenPropertyStore(STGM_READ, &propertyStorePointer); FAILED(hr))
                throw std::system_error(hr, errorCategory, "Failed to open property store");

            Pointer<IPropertyStore> propertyStore = propertyStorePointer;

            PROPVARIANT nameVariant;
            PropVariantInit(&nameVariant);

            if (const auto hr = propertyStore->GetValue(PKEY_Device_FriendlyName, &nameVariant); SUCCEEDED(hr))
            {
                int bufferSize = WideCharToMultiByte(CP_UTF8, 0, nameVariant.pwszVal, -1, nullptr, 0, nullptr, nullptr);
                if (bufferSize != 0)
                {
                    auto name = std::make_unique<char[]>(bufferSize);
                    if (WideCharToMultiByte(CP_UTF8, 0, nameVariant.pwszVal, -1, name.get(), bufferSize, nullptr, nullptr) != 0)
                    {
                        PropVariantClear(&nameVariant);
                        return name.get();
                    }
                }
            }

            PropVariantClear(&nameVariant);
            return "";
        }
    }

    std::vector<AudioDevice> AudioPlayer::getAudioDevices()
    {
        std::vector<AudioDevice> result;

        LPVOID enumeratorPointer;
        if (const auto hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, IID_IMMDeviceEnumerator, &enumeratorPointer); FAILED(hr))
            throw std::system_error(hr, std::system_category(), "Failed to create device enumerator");

        Pointer<IMMDeviceEnumerator> enumerator = static_cast<IMMDeviceEnumerator*>(enumeratorPointer);

        // get the default audio device
        IMMDevice* devicePointer;
        if (const auto hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &devicePointer); FAILED(hr))
            throw std::system_error(hr, errorCategory, "Failed to get audio endpoint");

        Pointer<IMMDevice> device = devicePointer;

        std::uint32_t audioDeviceId = 0;
        result.push_back(AudioDevice{ audioDeviceId++, "Default device (" + getDeviceName(devicePointer) + ")" });

        // get all audio devices
        IMMDeviceCollection* deviceCollectionPointer;
        if (const auto hr = enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &deviceCollectionPointer); FAILED(hr))
            throw std::system_error(hr, errorCategory, "Failed to enumerate endpoints");

        Pointer<IMMDeviceCollection> deviceCollection = deviceCollectionPointer;

        UINT count;
        if (const auto hr = deviceCollection->GetCount(&count); FAILED(hr))
            throw std::system_error(hr, errorCategory, "Failed to get endpoint count");

        for (UINT i = 0; i < count; i++)
        {
            IMMDevice* devicePointer;
            if (const auto hr = deviceCollection->Item(i, &devicePointer); FAILED(hr))
                throw std::system_error(hr, errorCategory, "Failed to get device");

            Pointer<IMMDevice> device = devicePointer;

            result.push_back(AudioDevice{ audioDeviceId++, getDeviceName(devicePointer) });
        }

        return result;
    }
}
