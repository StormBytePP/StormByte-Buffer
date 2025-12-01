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

	add_executable(ExternalProducerTests external_producer_test.cxx)
	target_link_libraries(ExternalProducerTests StormByte-Buffer)
	add_test(NAME ExternalProducerTests COMMAND ExternalProducerTests)
	
endif()