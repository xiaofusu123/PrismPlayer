#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
#ifdef _PRISM_EXPORTS
#define PRISM_API __declspec(dllexport)
#else
#define PRISM_API __declspec(dllimport)
#endif
#else
#define PRISM_API __attribute__((visibility("default")))
#endif
