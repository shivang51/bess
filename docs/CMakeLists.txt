find_package(Doxygen REQUIRED)
set(DOXYGEN_INPUT_DIRS
    ${CMAKE_SOURCE_DIR}/src/Bess/
    ${CMAKE_SOURCE_DIR}/src/Bess/
		${CMAKE_SOURCE_DIR}/src/BessVulkan/include
		${CMAKE_SOURCE_DIR}/src/BessVulkan/src
		${CMAKE_SOURCE_DIR}/src/BessSimEngine/include
		${CMAKE_SOURCE_DIR}/src/BessSimEngine/src
		${CMAKE_SOURCE_DIR}/src/BessPluginManager/include
		${CMAKE_SOURCE_DIR}/src/BessPluginManager/src
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

