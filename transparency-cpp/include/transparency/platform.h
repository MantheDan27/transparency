#pragma once

// Platform detection
#if defined(_WIN32) || defined(_WIN64)
    #define TRANSPARENCY_PLATFORM_WINDOWS 1
    #define TRANSPARENCY_PLATFORM_LINUX   0
#elif defined(__linux__)
    #define TRANSPARENCY_PLATFORM_WINDOWS 0
    #define TRANSPARENCY_PLATFORM_LINUX   1
#else
    #error "Unsupported platform"
#endif

// Platform string helpers
#if TRANSPARENCY_PLATFORM_WINDOWS
    #include <string>
    using tstring = std::wstring;
    #define T(x) L##x
#else
    #include <string>
    using tstring = std::string;
    #define T(x) x
#endif
