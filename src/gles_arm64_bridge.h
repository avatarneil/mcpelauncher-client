#pragma once
#include <cstdint>
#include <type_traits>

// AAPCS64 gives each spilled scalar argument an eight-byte stack slot; Darwin
// packs adjacent 32-bit integers. These GLES calls contain only integer/pointer
// arguments, so widening the incoming integers preserves Android's slot layout.
// The typed native call then repacks them for Darwin. Never use this for floats,
// aggregates, variadic calls, or callbacks in the opposite direction.
template<class T>
using AndroidGLESArgument = std::conditional_t<std::is_integral_v<T> && (sizeof(T) < 8), uint64_t, T>;

template<auto& Target, class Signature = std::remove_reference_t<decltype(Target)>>
struct AndroidGLESBridge;

template<auto& Target, class R, class... Args>
struct AndroidGLESBridge<Target, R (*)(Args...)> {
    static_assert(((std::is_integral_v<Args> || std::is_pointer_v<Args>) && ...));
    static R invoke(AndroidGLESArgument<Args>... args) {
        return Target(static_cast<Args>(args)...);
    }
};

using GLESCopyImage = void (*)(unsigned, unsigned, int, int, int, int,
                              unsigned, unsigned, int, int, int, int, int, int, int);
using GLESBlitFramebuffer = void (*)(int, int, int, int, int, int, int, int, unsigned, unsigned);
using GLESTexSubImage3D = void (*)(unsigned, int, int, int, int, int, int, int, unsigned, unsigned, const void*);
using GLESCompressedTexSubImage3D = void (*)(unsigned, int, int, int, int, int, int, int, unsigned, int, const void*);
