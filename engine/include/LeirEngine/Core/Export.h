#pragma once

#if defined(_WIN32) || defined(_WIN64)
    #ifdef LEIR_ENGINE_BUILD
        #define LEIR_API __declspec(dllexport)
    #else
        #define LEIR_API __declspec(dllimport)
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    #define LEIR_API __attribute__((visibility("default")))
#else
    #define LEIR_API
#endif
