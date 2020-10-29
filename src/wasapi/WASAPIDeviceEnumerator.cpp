#include <mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>
#include "WASAPIAudioPlayer.hpp"

namespace pcmplayer::wasapi
{
    std::string AudioPlayer::getDeviceName(IMMDevice* device)
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
                std::vector<char> name(bufferSize);
                if (WideCharToMultiByte(CP_UTF8, 0, nameVariant.pwszVal, -1, name.data(), bufferSize, nullptr, nullptr) != 0)
                {
                    PropVariantClear(&nameVariant);
                    return name.data();
                }
            }
        }

        PropVariantClear(&nameVariant);
        return "";
    }

    std::vector<AudioDevice> AudioPlayer::getAudioDevices()
    {
        std::vector<AudioDevice> result;

        LPVOID enumeratorPointer;
        if (const auto hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, IID_IMMDeviceEnumerator, &enumeratorPointer); FAILED(hr))
            throw std::system_error(hr, errorCategory, "Failed to create device enumerator");

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