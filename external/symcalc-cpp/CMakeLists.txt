project(symcalc-cpp)

set(INCLUDE_DIRS
		${CMAKE_CURRENT_SOURCE_DIR}/include
)


add_library(symcalc-cpp STATIC
	src/equation.cpp
	src/functions.cpp
	src/helpers.cpp
	src/symcalc.cpp
)

target_include_directories(symcalc-cpp PUBLIC ${INCLUDE_DIRS})
