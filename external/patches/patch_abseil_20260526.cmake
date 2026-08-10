if(NOT DEFINED ABSEIL_SOURCE_DIR)
    message(FATAL_ERROR "ABSEIL_SOURCE_DIR was not provided")
endif()

set(ABSEIL_STRINGS_CMAKE
    "${ABSEIL_SOURCE_DIR}/absl/strings/CMakeLists.txt")

if(NOT EXISTS "${ABSEIL_STRINGS_CMAKE}")
    message(FATAL_ERROR
        "Abseil strings CMake file was not found: ${ABSEIL_STRINGS_CMAKE}")
endif()

file(READ "${ABSEIL_STRINGS_CMAKE}" ABSEIL_STRINGS_CONTENT)

set(BAD_DEPENDENCIES_LF
    "    absl::source_location\n    absl::strings\n    absl::throw_delegate")
set(FIXED_DEPENDENCIES_LF
    "    absl::source_location\n    absl::throw_delegate")
set(BAD_DEPENDENCIES_CRLF
    "    absl::source_location\r\n    absl::strings\r\n    absl::throw_delegate")
set(FIXED_DEPENDENCIES_CRLF
    "    absl::source_location\r\n    absl::throw_delegate")

string(FIND "${ABSEIL_STRINGS_CONTENT}" "${BAD_DEPENDENCIES_LF}"
    BAD_DEPENDENCIES_OFFSET)
if(NOT BAD_DEPENDENCIES_OFFSET EQUAL -1)
    string(REPLACE "${BAD_DEPENDENCIES_LF}" "${FIXED_DEPENDENCIES_LF}"
        ABSEIL_STRINGS_CONTENT "${ABSEIL_STRINGS_CONTENT}")
    file(WRITE "${ABSEIL_STRINGS_CMAKE}" "${ABSEIL_STRINGS_CONTENT}")
    message(STATUS "Patched Abseil strings target self-dependency")
    return()
endif()

string(FIND "${ABSEIL_STRINGS_CONTENT}" "${BAD_DEPENDENCIES_CRLF}"
    BAD_DEPENDENCIES_OFFSET)
if(NOT BAD_DEPENDENCIES_OFFSET EQUAL -1)
    string(REPLACE "${BAD_DEPENDENCIES_CRLF}" "${FIXED_DEPENDENCIES_CRLF}"
        ABSEIL_STRINGS_CONTENT "${ABSEIL_STRINGS_CONTENT}")
    file(WRITE "${ABSEIL_STRINGS_CMAKE}" "${ABSEIL_STRINGS_CONTENT}")
    message(STATUS "Patched Abseil strings target self-dependency")
    return()
endif()

string(FIND "${ABSEIL_STRINGS_CONTENT}" "${FIXED_DEPENDENCIES_LF}"
    FIXED_DEPENDENCIES_OFFSET)
if(FIXED_DEPENDENCIES_OFFSET EQUAL -1)
    string(FIND "${ABSEIL_STRINGS_CONTENT}" "${FIXED_DEPENDENCIES_CRLF}"
        FIXED_DEPENDENCIES_OFFSET)
endif()

if(FIXED_DEPENDENCIES_OFFSET EQUAL -1)
    message(FATAL_ERROR
        "Abseil 20260526.0 strings dependencies do not match the expected source")
endif()

message(STATUS "Abseil strings target self-dependency is already patched")
