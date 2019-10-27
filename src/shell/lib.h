#pragma once

#if AUDIENCE_STATIC_LIBRARY

#ifdef WIN32
#define AUDIENCE_API
#else
#define AUDIENCE_API __attribute__((visibility("default")))
#endif

#else

#ifdef WIN32
#define AUDIENCE_API __declspec(dllexport)
#else
#define AUDIENCE_API __attribute__((visibility("default")))
#endif

#endif

#include <audience_api.h>
