if(MSVC)
	set(STORMBYTE_STRING_OPTSTR "TOOLCHAIN=clang-cl;ALIAS=StormByte::String")
else()
	set(STORMBYTE_STRING_OPTSTR "ALIAS=StormByte::String")
endif()

buildmaster_component(
	StormByte-String
	"StormByte Suite - String"
	"${CMAKE_CURRENT_LIST_DIR}/src"
	"WITH_STORMBYTE=BUNDLED"
	shared
	"StormByte-String"
	"${STORMBYTE_STRING_OPTSTR}"
)
