#ifndef CAERRORCATEGORY_HPP
#define CAERRORCATEGORY_HPP

#include <system_error>
#include <AudioUnit/AudioUnit.h>

namespace pcmplayer::coreaudio
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
}
#endif

#endif // CAERRORCATEGORY_HPP
