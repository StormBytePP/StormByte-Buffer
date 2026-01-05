if(WIN32)
	set(WITH_STORMBYTE "BUNDLED" CACHE STRING "Use StormByte components (SYSTEM/BUNDLED)" FORCE)
else()
	set(WITH_STORMBYTE "BUNDLED" CACHE STRING "Use StormByte components (SYSTEM/BUNDLED)")
endif()
if(WITH_STORMBYTE STREQUAL "SYSTEM")
	find_package(StormByte REQUIRED Logger)
else()
	set(STORMBYTE_LOGGER_SRCDIR "${CMAKE_CURRENT_LIST_DIR}/src")
	ensure_build_dir(STORMBYTE_LOGGER_BUILDDIR)
	set(STORMBYTE_LOGGER_COMPONENT "StormByte Suite - Logger")
	set(STORMBYTE_LOGGER_OPTIONS
		"-DWITH_STORMBYTE=BUNDLED"
	)
	create_cmake_component(
		STORMBYTE_LOGGER_LIBRARY_CREATE_FILE
		"StormByte-Logger"
		"${STORMBYTE_LOGGER_COMPONENT}"
		"${STORMBYTE_LOGGER_SRCDIR}"
		"${STORMBYTE_LOGGER_BUILDDIR}"
		"${STORMBYTE_LOGGER_OPTIONS}"
		"shared"
		"StormByte-Logger;StormByte"
		1
	)
	include("${STORMBYTE_LOGGER_LIBRARY_CREATE_FILE}")
	add_dependencies(StormByte-Buffer StormByte-Logger_install)
	add_library(StormByte::Logger ALIAS StormByte-Logger)
endif()

cmake_policy(SET CMP0079 NEW)
target_link_libraries(StormByte-Buffer StormByte::Logger)