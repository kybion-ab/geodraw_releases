#pragma once

#ifdef _WIN32
  #ifdef GEODRAW_EXPORT     // Defined only when building the library
    #define GEODRAW_API __declspec(dllexport)
  #else
    #define GEODRAW_API __declspec(dllimport)
  #endif
#else
  // On Linux/macOS, default visibility is usually enough
  #ifdef GEODRAW_EXPORT
    #define GEODRAW_API __attribute__((visibility("default")))
  #else
    #define GEODRAW_API
  #endif
#endif
