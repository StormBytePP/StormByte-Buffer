if(WIN32)
	set(WITH_STORMBYTE "BUNDLED" CACHE STRING "Use StormByte components (SYSTEM/BUNDLED)" FORCE)
else()
	set(WITH_STORMBYTE "BUNDLED" CACHE STRING "Use StormByte components (SYSTEM/BUNDLED)")
endif()

if(WITH_STORMBYTE STREQUAL "SYSTEM")
	# String is a Buffer-level pin only until BuildMaster waits on a shared
	# leaf _install after skip-configure (Logger+System both need String).
	find_package(StormByte REQUIRED String Logger System)
else()
	# Drop add_subdirectory(string) when that BM race is fixed.
	add_subdirectory(string)
	add_subdirectory(logger)
	add_subdirectory(system)
endif()

# Temporary edges: consumer _build must wait for this String _install.
# Remove both depends (and the String component) after the BM fix.
buildmaster_depend(StormByte-Logger StormByte-String)
buildmaster_depend(StormByte-System StormByte-String)
buildmaster_link(StormByte-Buffer StormByte-String)
buildmaster_link(StormByte-Buffer StormByte-Logger)
buildmaster_link(StormByte-Buffer StormByte-System)
