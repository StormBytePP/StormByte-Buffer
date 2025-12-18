include(GNUInstallDirs)

# Sources
file(GLOB_RECURSE STORMBYTE_BUFFER_SOURCES CONFIGURE_DEPEND "${CMAKE_CURRENT_LIST_DIR}/*.cxx")

# Library
add_library(StormByte-Buffer SHARED ${STORMBYTE_BUFFER_SOURCES})
add_library(StormByte::Buffer ALIAS StormByte-Buffer)
set_target_properties(StormByte-Buffer PROPERTIES
	LINKER_LANGUAGE CXX
	SOVERSION		${CMAKE_PROJECT_VERSION}
	VERSION 		${CMAKE_PROJECT_VERSION}
)
set_target_properties(StormByte-Buffer PROPERTIES
	POSITION_INDEPENDENT_CODE        ON
	CXX_VISIBILITY_PRESET            hidden
	VISIBILITY_INLINES_HIDDEN        ON
)

# Compile options
if(MSVC)
	target_compile_options(StormByte-Buffer PRIVATE /EHsc)
	target_compile_options(StormByte-Buffer PRIVATE $<$<CONFIG:Release>:/O2>)
	target_compile_options(StormByte-Buffer PRIVATE $<$<CONFIG:Debug>:/Od>)
	target_link_options(StormByte-Buffer PRIVATE $<$<CONFIG:Release>:/LTCG /GL>)
else()
	set(CMAKE_CXX_FLAGS_DEBUG "-pipe -g -ggdb -Wall -Wextra -Wnon-virtual-dtor -pedantic -pedantic-errors -O0")
endif()

# Include directories
target_include_directories(StormByte-Buffer
	SYSTEM BEFORE PUBLIC "${CMAKE_CURRENT_LIST_DIR}/public" "${CMAKE_CURRENT_LIST_DIR}/private"
)

