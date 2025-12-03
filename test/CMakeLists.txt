option(ENABLE_TEST "Enable Unit Tests" OFF)
if(ENABLE_TEST AND NOT STORMBYTE_AS_DEPENDENCY)
	add_executable(FIFOTests fifo_test.cxx)
	target_link_libraries(FIFOTests StormByte-Buffer)
	add_test(NAME FIFOTests COMMAND FIFOTests)

	add_executable(SharedFIFOTests shared_fifo_test.cxx)
	target_link_libraries(SharedFIFOTests StormByte-Buffer)
	add_test(NAME SharedFIFOTests COMMAND SharedFIFOTests)

	add_executable(ProducerConsumerTests producer_consumer_test.cxx)
	target_link_libraries(ProducerConsumerTests StormByte-Buffer)
	add_test(NAME ProducerConsumerTests COMMAND ProducerConsumerTests)

	add_executable(PipelineTests pipeline_test.cxx)
	target_link_libraries(PipelineTests StormByte-Buffer)
	add_test(NAME PipelineTests COMMAND PipelineTests)

	add_executable(ForwarderTests forwarder_test.cxx)
	target_link_libraries(ForwarderTests StormByte-Buffer)
	add_test(NAME ForwarderTests COMMAND ForwarderTests)

	add_executable(BridgeTests bridge_test.cxx)
	target_link_libraries(BridgeTests StormByte-Buffer)
	add_test(NAME BridgeTests COMMAND BridgeTests)

	# If AddressSanitizer is enabled, set global environment for all ctest runs.
	if(ENABLE_ASAN)
		set_property(GLOBAL PROPERTY ENVIRONMENT 
			"ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:strict_string_checks=1:alloc_dealloc_mismatch=1;UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1")
	endif()
endif()