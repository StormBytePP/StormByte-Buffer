cmake_minimum_required(VERSION 3.12)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project("StormByte C++ Library: Buffer module"
	VERSION 0.0.9999
	DESCRIPTION "StormByte-Buffer is a StormByte library module for handling buffers"
	HOMEPAGE_URL "https://dev.stormbyte.org/StormByte-Buffer"
)

option(ENABLE_ASAN "Enable AddressSanitizer (and UBSan) for debug builds" OFF)

if(ENABLE_ASAN)
	# Recommend Debug builds for ASan
	if (NOT CMAKE_BUILD_TYPE)
		set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
	endif()

	# GCC/Clang sanitizers
	set(SANITIZER_FLAGS "-fsanitize=address,undefined -fno-omit-frame-pointer")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS}")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SANITIZER_FLAGS}")

	# Improve ASan/UBSan diagnostics
	add_compile_options(-g)
	# Linker flags for sanitizers
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SANITIZER_FLAGS}")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${SANITIZER_FLAGS}")

	message(STATUS "AddressSanitizer enabled")
endif()

enable_testing()
add_subdirectory(thirdparty)
add_subdirectory(doc)
add_subdirectory(lib)
add_subdirectory(test)
