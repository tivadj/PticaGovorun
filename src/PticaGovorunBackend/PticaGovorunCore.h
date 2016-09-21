#pragma once

#if defined _WIN32
    #if defined PGAPI_EXPORTS
        #define PG_EXPORTS __declspec(dllexport)
    #else
        //#define PG_EXPORTS __declspec(dllimport)
    #define PG_EXPORTS
    #endif
#else
    #define PG_EXPORTS __attribute__ ((visibility ("default")))
#endif

