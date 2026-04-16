find_package(Doxygen REQUIRED)
set(DOXYGEN_INPUT_DIRS
    ${CMAKE_SOURCE_DIR}/src/bess/
		${CMAKE_SOURCE_DIR}/src/bess_json/
		${CMAKE_SOURCE_DIR}/src/bess_vulkan/
		${CMAKE_SOURCE_DIR}/src/bessplug/
		${CMAKE_SOURCE_DIR}/src/bverilog/
		${CMAKE_SOURCE_DIR}/src/command_system/
		${CMAKE_SOURCE_DIR}/src/common/
		${CMAKE_SOURCE_DIR}/src/event_system/
		${CMAKE_SOURCE_DIR}/src/plugin_manager/
		${CMAKE_SOURCE_DIR}/src/sim_engine/
)

list(JOIN DOXYGEN_INPUT_DIRS " " DOXYGEN_INPUT_LIST)

set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

add_custom_target(docs
    COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating API documentation for all modules"
)

