include(GNUInstallDirs)

# Sources
file(GLOB_RECURSE STORMBYTE_BUFFER_SOURCES CONFIGURE_DEPEND "${CMAKE_CURRENT_LIST_DIR}/*.cxx")

# Library
add_library(StormByte-Buffer SHARED ${STORMBYTE_BUFFER_SOURCES})
add_library(StormByte::Buffer ALIAS StormByte-Buffer)
target_link_libraries(StormByte-Buffer PUBLIC StormByte::Logger StormByte)
set_target_properties(StormByte-Buffer PROPERTIES
	LINKER_LANGUAGE CXX
	SOVERSION		${CMAKE_PROJECT_VERSION}
	VERSION 		${CMAKE_PROJECT_VERSION}
)

# Compile options
if(MSVC)
	target_compile_options(StormByte-Buffer PRIVATE /EHsc)
	target_compile_options(StormByte-Buffer PRIVATE $<$<CONFIG:Release>:/O2>)
	target_compile_options(StormByte-Buffer PRIVATE $<$<CONFIG:Debug>:/Od>)
	target_link_options(StormByte-Buffer PRIVATE $<$<CONFIG:Release>:/LTCG /GL>)
else()
	set(CMAKE_CXX_FLAGS_DEBUG "-pipe -g -ggdb -Wall -Wextra -Wnon-virtual-dtor -pedantic -pedantic-errors -O0")
	target_compile_options(StormByte-Buffer PRIVATE -fvisibility=hidden $<$<COMPILE_LANGUAGE:CXX>:-fvisibility-inlines-hidden>)
endif()

# Compiler/runtime needs
# Detect support for C++23 containers ranges (e.g., std::vector::append_range)
# and define a build macro to gate usage in the codebase.
include(CheckCXXSourceCompiles)
set(_APPEND_RANGE_TEST "#include <version>\nint main(){\n#ifndef __cpp_lib_containers_ranges\n#error no containers_ranges\n#endif\nstatic_assert(__cpp_lib_containers_ranges >= 202202L, \"containers_ranges too old\");\nreturn 0;\n}")
check_cxx_source_compiles("${_APPEND_RANGE_TEST}" STORMBYTE_HAS_CONTAINERS_RANGES)

# Scope the macro definition to this target only (PRIVATE)
if(STORMBYTE_HAS_CONTAINERS_RANGES)
	target_compile_definitions(StormByte-Buffer PRIVATE STORMBYTE_HAS_CONTAINERS_RANGES=1)
else()
	target_compile_definitions(StormByte-Buffer PRIVATE STORMBYTE_HAS_CONTAINERS_RANGES=0)
endif()

# Include directories
target_include_directories(StormByte-Buffer
	SYSTEM BEFORE PUBLIC "${CMAKE_CURRENT_LIST_DIR}/public" "${CMAKE_CURRENT_LIST_DIR}/private"
)

# Install
if (NOT STORMBYTE_AS_DEPENDENCY)
	install(TARGETS StormByte-Buffer
		ARCHIVE 		DESTINATION "${CMAKE_INSTALL_LIBDIR}"
		LIBRARY 		DESTINATION "${CMAKE_INSTALL_LIBDIR}"
		RUNTIME 		DESTINATION ${CMAKE_INSTALL_BINDIR}
	)
	install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/public/StormByte/"
		DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/StormByte"
		FILES_MATCHING
		PATTERN "*.h"
		PATTERN "*.hxx"
	)
endif()
