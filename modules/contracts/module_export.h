#pragma once

#if defined(_WIN32)
    #define KVDB_MODULE_EXPORT __declspec(dllexport)
#else
    #define KVDB_MODULE_EXPORT __attribute__((visibility("default")))
#endif
