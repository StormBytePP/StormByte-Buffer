#pragma once

#include <StormByte/platform.h>

#ifdef WINDOWS
	#ifdef StormByte_Memory_EXPORTS
		#define STORMBYTE_MEMORY_PUBLIC	__declspec(dllexport)
	#else
		#define STORMBYTE_MEMORY_PUBLIC	__declspec(dllimport)
	#endif
	#define STORMBYTE_MEMORY_PRIVATE
#else
	#define STORMBYTE_MEMORY_PUBLIC		__attribute__ ((visibility ("default")))
	#define STORMBYTE_MEMORY_PRIVATE	__attribute__ ((visibility ("hidden")))
#endif