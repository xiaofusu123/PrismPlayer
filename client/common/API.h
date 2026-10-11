#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
    #ifdef _PRISM_EXPORTS
        #define _API __declspec(dllexport)
    #else
        #define _API __declspec(dllimport)
    #endif
#else
    #define _API __attribute__((visibility("default")))
#endif
