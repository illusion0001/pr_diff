cmake_minimum_required(VERSION 3.15)

set(MY_CMAKE_CONFIG_TYPES Debug RelWithDebInfo)
set(CMAKE_CONFIGURATION_TYPES ${MY_CMAKE_CONFIG_TYPES} CACHE STRING "" FORCE)

set(AE_SDK ${CMAKE_SOURCE_DIR}/aesdk/Examples)

set(CMAKE_CXX_STANDARD 20)

# So do it here.
project(ae_diff)
if (WIN32 AND MSVC)
    set_directory_properties(PROPERTIES VS_STARTUP_PROJECT ${PROJECT_NAME})
endif()

include_directories(
    ${AE_SDK}/Headers
    ${AE_SDK}/Headers/SP
    ${AE_SDK}/Headers/Win
    ${AE_SDK}/Resources
    ${AE_SDK}/Util
)

set(HEADER_FILES
    ${CMAKE_SOURCE_DIR}/ae_overlay/assert.hpp
    ${CMAKE_SOURCE_DIR}/ae_overlay/macro.hpp
    ${CMAKE_SOURCE_DIR}/ae_overlay/is_bad_mem_ptr.hpp
    ${CMAKE_SOURCE_DIR}/ae_overlay/stringid.hpp
)

set(SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/ae_overlay/assert.cpp
    ${CMAKE_SOURCE_DIR}/ae_overlay/ae_overlay.cpp
)

add_library(${PROJECT_NAME} SHARED
    ${HEADER_FILES}
    ${SOURCE_FILES}
)

target_link_libraries(${PROJECT_NAME})

# Private!!
target_compile_definitions(${PROJECT_NAME} PRIVATE __AE__)
target_compile_definitions(${PROJECT_NAME} PRIVATE _UNICODE)
target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)
set_target_properties(${PROJECT_NAME} PROPERTIES
    SUFFIX ".aex"
    PREFIX ""
)

if (WIN32 AND MSVC AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(OPT_WARNS
        "-W4" # Wall is too much!
        "-Wno-pragma-pack"
        "-Wno-microsoft-string-literal-from-predefined"
        "-Wno-unused-function"
        # "-Wno-missing-field-initializers" # Some stuff in AE SDK, ignored under `__AE__` now
        "-Wno-unused-parameter" # Mostly from PF, but mine has some too in `ae_dialog`
    )
    set(IGNORE_STUFF "")
    foreach (link_line ${OPT_WARNS})
        string(APPEND IGNORE_STUFF " \"${link_line}\" ")
    endforeach(link_line)
    set(EXTRA_STUFF "-MP")
    set(BOTH_FLAGS "${IGNORE_STUFF} ${EXTRA_STUFF}")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${BOTH_FLAGS}")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${BOTH_FLAGS} -O1 -Os")
endif()

if(WIN32)
    set(PIPL_SOURCE ${CMAKE_SOURCE_DIR}/ae_overlay/ae_overlayPiPL.r)
    set(PIPL_INTERMEDIATE_DIR ${CMAKE_CURRENT_BINARY_DIR}/pipl_temp)
    set(PIPL_RC ${CMAKE_CURRENT_BINARY_DIR}/ae_overlay/ae_overlayPiPL.rc)
    file(MAKE_DIRECTORY ${PIPL_INTERMEDIATE_DIR})
    add_custom_command(
        OUTPUT ${PIPL_RC}
        COMMAND cl /I "${AE_SDK}/Headers" /EP "${PIPL_SOURCE}" > "${PIPL_INTERMEDIATE_DIR}/SDK_NoisePiPL.rr"
        COMMAND "${AE_SDK}/Resources/PiPLTool" "${PIPL_INTERMEDIATE_DIR}/SDK_NoisePiPL.rr" "${PIPL_INTERMEDIATE_DIR}/SDK_NoisePiPL.rrc"
        COMMAND cl /D "MSWindows" /EP "${PIPL_INTERMEDIATE_DIR}/SDK_NoisePiPL.rrc" > "${PIPL_RC}"
        DEPENDS ${PIPL_SOURCE}
        COMMENT "Compiling the PiPL"
        VERBATIM
    )
    target_sources(${PROJECT_NAME} PRIVATE ${PIPL_RC})
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS YES
    # So single symlink is possible
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_SOURCE_DIR}/bin
)
