if(MSVC)
	set(STORMBYTE_LOGGER_OPTSTR "TOOLCHAIN=clang-cl;ALIAS=StormByte::Logger;LINK=StormByte")
else()
	set(STORMBYTE_LOGGER_OPTSTR "ALIAS=StormByte::Logger;LINK=StormByte")
endif()

buildmaster_component(
	StormByte-Logger
	"StormByte Suite - Logger"
	"${CMAKE_CURRENT_LIST_DIR}/src"
	"WITH_STORMBYTE=BUNDLED"
	shared
	"StormByte-Logger"
	"${STORMBYTE_LOGGER_OPTSTR}"
)
