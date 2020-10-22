#ifndef WASAPIPOINTER_HPP
#define WASAPIPOINTER_HPP

namespace pcmplayer::wasapi
{
    template <class T>
    class Pointer final
    {
    public:
        Pointer() noexcept = default;

        Pointer(T* a) noexcept : p(a) {}
        Pointer& operator=(T* a) noexcept
        {
            if (p) p->Release();
            p = a;
            return *this;
        }

        Pointer(const Pointer& other) noexcept: p(other.p)
        {
            if (p) p->AddRef();
        }

        Pointer& operator=(const Pointer& other) noexcept
        {
            if (this == &other) return *this;
            if (p) p->Release();
            p = other.p;
            if (p) p->AddRef();
        }

        Pointer(Pointer&& other) noexcept : p(other.p)
        {
            other.p = nullptr;
        }

        Pointer& operator=(Pointer&& other) noexcept
        {
            if (this == &other) return *this;
            if (p) p->Release();
            p = other.p;
            other.p = nullptr;
            return *this;
        }

        ~Pointer()
        {
            if (p) p->Release();
        }

        T* operator->() const noexcept
        {
            return p;
        }

        T* get() const noexcept
        {
            return p;
        }

        explicit operator bool() const noexcept
        {
            return p != nullptr;
        }

    private:
        T* p = nullptr;
    };
}

#endif // WASAPIPOINTER_HPP
