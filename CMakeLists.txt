cmake_minimum_required(VERSION 3.10)

project(SynergyGDI VERSION 0.1)
add_executable(Synergy WIN32 Sources/Win32_Main.cpp )

add_compile_definitions(UNICODE)

target_include_directories(Synergy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Includes/)
target_include_directories(Synergy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/ClientIncludes)