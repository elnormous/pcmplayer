#ifndef COM_HPP
#define COM_HPP

#include <Combaseapi.h>

namespace pcmplayer
{
    class Com final
    {
    public:
        Com()
        {
            if (const auto hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED); FAILED(hr))
                throw std::system_error(hr, std::system_category(), "Failed to initialize COM");

            initialized = true;
        }

        ~Com()
        {
            if (initialized) CoUninitialize();
        }

        Com(Com&& other) noexcept :
            initialized(other.initialized)
        {
            other.initialized = false;
        }

        Com& operator=(Com&& other) noexcept
        {
            if (&other == this) return *this;
            if (initialized) CoUninitialize();
            initialized = other.initialized;
            other.initialized = false;
            return *this;
        }

    private:
        bool initialized = false;
    };
}

#endif // COM_HPP
