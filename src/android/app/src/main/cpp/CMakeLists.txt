# Mutable Dreams for Android: SDL 2.30.9 (shared, as SDLActivity loads it) and libmain.so = the 2.1 engine, the
# Mutable Instruments modules and the Brick port's core + SDL front end, with build.py's flags (sources.cmake).
cmake_minimum_required(VERSION 3.22)
project(mutabledreams C CXX)
include(${CMAKE_CURRENT_SOURCE_DIR}/sources.cmake)

set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
add_subdirectory(${MD_SDL_DIR} sdl2)

set(MD_COMMON -O2 -ffp-contract=off -ffunction-sections -fdata-sections -DPLAT_HOST -DPLAT_WEB -DTEST
              -DFM1B_MIRROR=1 -DFM1B_UAC_CH=8)

# the 2.1 engine, as the firmware builds it (fm1.toml o2_cflags)
add_library(md_engine OBJECT ${MD_ENGINE_SOURCES})
target_include_directories(md_engine PRIVATE ${MD_INCLUDES} ${MD_ENGINE_COMPAT})
separate_arguments(MD_ENGINE_FLAGS_LIST UNIX_COMMAND "${MD_ENGINE_FLAGS}")
target_compile_options(md_engine PRIVATE ${MD_COMMON} ${MD_ENGINE_FLAGS_LIST})

# the original Mutable Instruments DSP, its newer stmlib in a namespace of its own (as build.py)
add_library(md_mi OBJECT ${MD_MI_SOURCES})
target_include_directories(md_mi PRIVATE ${MD_MI_INCLUDES})
target_compile_options(md_mi PRIVATE -O2 -ffp-contract=off -ffunction-sections -fdata-sections -DTEST
    -Dstmlib=mi_stmlib -Dplaits=mi_plaits -Drings=mi_rings -Delements=mi_elements -Dtides=mi_tides
    -include stdio.h -std=c++11 -fno-exceptions -fno-rtti -Wno-unused-parameter -Wno-vla-cxx-extension -Wno-vla)

add_library(main SHARED ${MD_CORE_SOURCES} $<TARGET_OBJECTS:md_engine> $<TARGET_OBJECTS:md_mi>)
target_include_directories(main PRIVATE ${MD_INCLUDES})
set_source_files_properties(${MD_CORE_SOURCES} PROPERTIES COMPILE_OPTIONS "-std=gnu99;-Wno-unknown-attributes")
target_compile_options(main PRIVATE ${MD_COMMON})
target_link_libraries(main PRIVATE SDL2 log android m)
target_link_options(main PRIVATE -Wl,--gc-sections)
