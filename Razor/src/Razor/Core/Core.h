#pragma once

#pragma warning (disable : 4251)

#ifdef RZ_PLATFORM_WINDOWS
#if RZ_DYNAMIC_LINK
	#ifdef RZ_BUILD_DLL
		#define RAZOR_API __declspec(dllexport)
	#else
		#define RAZOR_API __declspec(dllimport)
	#endif
#else
	#define RAZOR_API
#endif
#else
	#error Razor only supports Windows ! 
#endif

#ifdef RZ_DEBUG
	#define RZ_ENABLE_ASSERTS
#endif

#ifdef RZ_ENABLE_ASSERTS
	#define RZ_ASSERT(x, ...) { if(!(x)) { Log::error("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#else
	#define RZ_ASSERT(x, ...)
#endif

#define BIT(x) (1 << x)

#define RZ_BIND_EVENT_FN(fn) std::bind(&fn, this, std::placeholders::_1)

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

#define PI 3.141592653589793f
#define TAU 2.0f * PI
#define EPSILON 0.001f

typedef __int16 int16;
typedef unsigned __int16 uint16;

typedef __int32 int32;
typedef unsigned __int32 uint32;

typedef __int64 int64;
typedef unsigned __int64 uint64;
