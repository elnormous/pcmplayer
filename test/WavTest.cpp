#include "catch2/catch.hpp"
#include "Wav.hpp"

TEST_CASE("Empty", "[empty]")
{
    Wav wav;
    REQUIRE(wav.getChannels() == 0);
    REQUIRE(wav.getSampleRate() == 0);
    REQUIRE(wav.getFrames() == 0);
    REQUIRE(wav.getSamples().empty());
}

TEST_CASE("SingleFrame", "[single_frame]")
{
    Wav wav(2, 44100, 1, {0.0F, 0.0F});
    REQUIRE(wav.getChannels() == 2);
    REQUIRE(wav.getSampleRate() == 44100);
    REQUIRE(wav.getFrames() == 1);
    REQUIRE(wav.getSamples().size() == 2);
}

class MemoryBuffer final: public std::basic_streambuf<char>
{
public:
    MemoryBuffer(std::uint8_t* begin, std::uint8_t* end)
    {
        setg(reinterpret_cast<char_type*>(begin),
             reinterpret_cast<char_type*>(begin),
             reinterpret_cast<char_type*>(end));
    }

private:
    pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which = std::ios_base::in) override
    {
        if (dir == std::ios_base::cur)
            setg(eback(), gptr() + off, egptr());
        else if (dir == std::ios_base::end)
            setg(eback(), egptr() + off, egptr());
        else if (dir == std::ios_base::beg)
            setg(eback(), eback() + off, egptr());
        return gptr() - eback();
    }

    pos_type seekpos(pos_type sp, std::ios_base::openmode which) override
    {
        setg(eback(), eback() + sp, egptr());
        return gptr() - eback();
    }
};

TEST_CASE("Decoding", "[decoding]")
{
    SECTION("16-bit-fixed")
    {
        std::uint8_t data[] = {0x52, 0x49, 0x46, 0x46, 0x30, 0x00, 0x00, 0x00, 0x57, 0x41, 0x56, 0x45, 0x66, 0x6D, 0x74, 0x20, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x80, 0xBB, 0x00, 0x00, 0x00, 0xEE, 0x02, 0x00, 0x04, 0x00, 0x10, 0x00, 0x64, 0x61, 0x74, 0x61, 0x0C, 0x00, 0x00, 0x00, 0xFF, 0x7F, 0xFF, 0x7F, 0x01, 0x80, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00};

        MemoryBuffer buffer(std::begin(data), std::end(data));
        std::istream stream(&buffer);

        Wav wav(stream);
        REQUIRE(wav.getChannels() == 2);
        REQUIRE(wav.getSampleRate() == 48000);
        REQUIRE(wav.getFrames() == 3);

        const auto& samples = wav.getSamples();
        REQUIRE(samples[0] == Approx(1.0F));
        REQUIRE(samples[1] == Approx(1.0F));
        REQUIRE(samples[2] == Approx(-1.0F));
        REQUIRE(samples[3] == Approx(-1.0F));
        REQUIRE(samples[4] == Approx(0.0F));
        REQUIRE(samples[5] == Approx(0.0F));
    }

    SECTION("24-bit-fixed")
    {
        std::uint8_t data[] = {0x52, 0x49, 0x46, 0x46, 0x36, 0x00, 0x00, 0x00, 0x57, 0x41, 0x56, 0x45, 0x66, 0x6D, 0x74, 0x20, 0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x80, 0xBB, 0x00, 0x00, 0x00, 0x65, 0x04, 0x00, 0x06, 0x00, 0x18, 0x00, 0x64, 0x61, 0x74, 0x61, 0x12, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0x7F, 0x01, 0x00, 0x80, 0x01, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

        MemoryBuffer buffer(std::begin(data), std::end(data));
        std::istream stream(&buffer);

        Wav wav(stream);
        REQUIRE(wav.getChannels() == 2);
        REQUIRE(wav.getSampleRate() == 48000);
        REQUIRE(wav.getFrames() == 3);

        const auto& samples = wav.getSamples();
        REQUIRE(samples[0] == Approx(1.0F));
        REQUIRE(samples[1] == Approx(1.0F));
        REQUIRE(samples[2] == Approx(-1.0F));
        REQUIRE(samples[3] == Approx(-1.0F));
        REQUIRE(samples[4] == Approx(0.0F));
        REQUIRE(samples[5] == Approx(0.0F));
    }

    SECTION("32-bit-float")
    {
        std::uint8_t data[] = {0x52, 0x49, 0x46, 0x46, 0x68, 0x00, 0x00, 0x00, 0x57, 0x41, 0x56, 0x45, 0x66, 0x6D, 0x74, 0x20, 0x10, 0x00, 0x00, 0x00, 0x03, 0x00, 0x02, 0x00, 0x80, 0xBB, 0x00, 0x00, 0x00, 0xDC, 0x05, 0x00, 0x08, 0x00, 0x20, 0x00, 0x66, 0x61, 0x63, 0x74, 0x04, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x50, 0x45, 0x41, 0x4B, 0x18, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xC2, 0x5C, 0x8E, 0x5F, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x64, 0x61, 0x74, 0x61, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x80, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

        MemoryBuffer buffer(std::begin(data), std::end(data));
        std::istream stream(&buffer);

        Wav wav(stream);
        REQUIRE(wav.getChannels() == 2);
        REQUIRE(wav.getSampleRate() == 48000);
        REQUIRE(wav.getFrames() == 3);

        const auto& samples = wav.getSamples();
        REQUIRE(samples[0] == Approx(1.0F));
        REQUIRE(samples[1] == Approx(1.0F));
        REQUIRE(samples[2] == Approx(-1.0F));
        REQUIRE(samples[3] == Approx(-1.0F));
        REQUIRE(samples[4] == Approx(0.0F));
        REQUIRE(samples[5] == Approx(0.0F));
    }
}

TEST_CASE("Encodin", "[encoding]")
{
    SECTION("32-bit-float")
    {
        Wav wav(2, 48000, 3, {1.0F, 1.0F, -1.0F, -1.0F, 0.0F, 0.0F});
    }
}
