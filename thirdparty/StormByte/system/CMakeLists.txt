if(MSVC)
	set(STORMBYTE_SYSTEM_OPTSTR "TOOLCHAIN=clang-cl;ALIAS=StormByte::System")
else()
	set(STORMBYTE_SYSTEM_OPTSTR "ALIAS=StormByte::System")
endif()

buildmaster_component(
	StormByte-System
	"StormByte Suite - System"
	"${CMAKE_CURRENT_LIST_DIR}/src"
	"WITH_STORMBYTE=BUNDLED"
	shared
	"StormByte-System"
	"${STORMBYTE_SYSTEM_OPTSTR}"
)
