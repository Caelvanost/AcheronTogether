cmake_minimum_required(VERSION 3.24)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" ACHERON_TOGETHER_VERSION)
string(STRIP "${ACHERON_TOGETHER_VERSION}" ACHERON_TOGETHER_VERSION)

project(
    AcheronTogether
    VERSION ${ACHERON_TOGETHER_VERSION}
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CommonLibSSE CONFIG REQUIRED)

add_commonlibsse_plugin(${PROJECT_NAME}
    SOURCES
        src/PCH.h
        src/main.cpp
        src/AcheronBridge.cpp
        src/AcheronBridge.h
        src/Settings.cpp
        src/Settings.h
        src/Papyrus.cpp
        src/Papyrus.h
        src/StateSync.cpp
        src/StateSync.h
        src/STRPMApi.h
        src/STRPMClient.cpp
        src/STRPMClient.h
)

target_precompile_headers(${PROJECT_NAME}
    PRIVATE
        src/PCH.h
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        ACHERON_TOGETHER_VERSION="${ACHERON_TOGETHER_VERSION}"
        _UNICODE
        UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /permissive- /W4 /utf-8)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "AcheronTogether"
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/package/SKSE/Plugins"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:${PROJECT_NAME}>"
        "${CMAKE_BINARY_DIR}/package/SKSE/Plugins/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
)
