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

enable_testing()
add_subdirectory(thirdparty)
add_subdirectory(doc)
add_subdirectory(debug)
add_subdirectory(lib)
add_subdirectory(test)
