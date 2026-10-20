#pragma once

#ifdef MAT_JSON_DYNAMIC
    #if defined(_WIN32) && !defined(__CYGWIN__)
        #ifdef MAT_JSON_EXPORTING
            #define MAT_JSON_DLL __declspec(dllexport)
        #else
            #define MAT_JSON_DLL __declspec(dllimport)
        #endif
    #else
        #ifdef MAT_JSON_EXPORTING
            #define MAT_JSON_DLL [[gnu::visibility("default")]]
        #else
            #define MAT_JSON_DLL
        #endif
    #endif
#else
    #define MAT_JSON_DLL
#endif
