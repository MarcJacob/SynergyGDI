cmake_minimum_required(VERSION 3.10)

project(SynergyGDI VERSION 0.1)
add_executable(Synergy WIN32 Sources/Win32_Main.cpp )

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/Synergy/)

# Make sure Client library gets built alongside the GDI executable.
target_link_libraries(Synergy SynergyClientLib)

if (WIN32)
	# Specify that we want to run in UNICODE mode when building for Windows.
	add_compile_definitions(UNICODE)
ENDIF()

target_include_directories(Synergy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Includes/)
target_include_directories(Synergy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/Synergy/SynergyCoreLib/Includes/Public/)
target_include_directories(Synergy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Dependencies/Synergy/SynergyClientLib/Includes/Public/)