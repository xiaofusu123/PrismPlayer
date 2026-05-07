#pragma once

#if _WIN32
    #ifdef LAYERING_EXPORTS
        #define _API __declspec(dllexport)
    #else
        #define _API __declspec(dllimport)
    #endif
#else
    #define _API __attribute__((visibility("default")))
#endif
