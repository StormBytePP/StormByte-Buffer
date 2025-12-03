option(ENABLE_ASAN "Enable AddressSanitizer (and UBSan) for debug builds" OFF)

if(ENABLE_ASAN)
	# Recommend Debug builds for ASan
	set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)

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