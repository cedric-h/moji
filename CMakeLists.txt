cmake_minimum_required(VERSION 3.0)
project(game)
set(CMAKE_C_STANDARD 11)
if (CMAKE_SYSTEM_NAME STREQUAL Emscripten)
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif()

# Linux -pthread shenanigans
if (CMAKE_SYSTEM_NAME STREQUAL Linux)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
endif()

#=== LIBRARY: bq_websocket
add_library(bq_websocket STATIC bq_websocket/bq_websocket_platform.c
                                bq_websocket/bq_websocket_platform.h
                                bq_websocket/bq_websocket.c
                                bq_websocket/bq_websocket.h)

#=== LIBRARY: microui
add_library(microui STATIC client/mui/microui.c)

#=== LIBRARY: sokol
# add headers to the the file list because they are useful to have in IDEs
set(SOKOL_HEADERS
    sokol/sokol_gfx.h
    sokol/sokol_app.h
    sokol/sokol_audio.h
    sokol/sokol_time.h
    sokol/sokol_glue.h)
add_library(sokol STATIC sokol/sokol.c ${SOKOL_HEADERS})
if(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    # compile sokol.c as Objective-C
    target_compile_options(sokol PRIVATE -x objective-c)
    target_link_libraries(sokol
        "-framework QuartzCore"
        "-framework Cocoa"
        "-framework MetalKit"
        "-framework Metal"
        "-framework AudioToolbox")
else()
    if (CMAKE_SYSTEM_NAME STREQUAL Linux)
        target_link_libraries(sokol INTERFACE X11 Xi Xcursor GL asound dl m)
        target_link_libraries(sokol PUBLIC Threads::Threads)
    endif()
endif()
target_include_directories(sokol INTERFACE sokol)

if (CMAKE_SYSTEM_NAME STREQUAL Windows)
    set(RUN_FORMPACK_GEN_CMD "../formpack/build/gen.exe")
else()
    set(RUN_FORMPACK_GEN_CMD "../formpack/build/gen")
endif()
add_custom_command(OUTPUT ./formpack/build/formpack.h
    COMMAND cmake --build ../formpack/build/ && ${RUN_FORMPACK_GEN_CMD}
    DEPENDS ./formpack/formpack.dd
    DEPENDS ./formpack/gen.c
)
set_source_files_properties(./formpack/build/formpack.h PROPERTIES GENERATED TRUE)

#=== EXECUTABLE: client

if(CMAKE_SYSTEM_NAME STREQUAL Windows)
    set(COMPILE_SHADER_CMD ../sokol-tools-bin/bin/win32/sokol-shdc.exe --input ../client/client.glsl --output ./client.glsl.h --slang hlsl5)
else()
    if (CMAKE_SYSTEM_NAME STREQUAL Linux)
        set(COMPILE_SHADER_CMD ../sokol-tools-bin/bin/linux/sokol-shdc --input ../client/client.glsl --output ./client.glsl.h --slang glsl330)
else()
    if (CMAKE_SYSTEM_NAME STREQUAL Darwin)
        set(COMPILE_SHADER_CMD ../sokol-tools-bin/bin/osx/sokol-shdc --input ../client/client.glsl --output ./client.glsl.h --slang metal_macos)
    endif()
endif()
endif()
add_custom_command(OUTPUT client.glsl.h
    COMMAND ${COMPILE_SHADER_CMD}
    DEPENDS client/client.glsl
)
set_source_files_properties(client.glsl.h PROPERTIES
    GENERATED TRUE
)

if(CMAKE_SYSTEM_NAME STREQUAL Windows)
    add_executable(client WIN32 client/main.c client.glsl.h ./formpack/build/formpack.h)

    set_target_properties(client PROPERTIES
        LINK_FLAGS /SUBSYSTEM:CONSOLE
    )
else()
    add_executable(client client/main.c client.glsl.h ./formpack/build/formpack.h)
endif()

target_link_libraries(client sokol)
target_link_libraries(client bq_websocket)
target_link_libraries(client microui)
if (CMAKE_SYSTEM_NAME STREQUAL Emscripten)
    target_link_options(client PUBLIC --shell-file ../sokol/shell.html)
    target_link_options(client PUBLIC -sNO_FILESYSTEM=1 -sASSERTIONS=0 -sMALLOC=emmalloc --closure=1)
endif()

add_executable(server server/main.c ./formpack/build/formpack.h)
if (CMAKE_SYSTEM_NAME STREQUAL Linux)
    target_link_libraries(server m)
endif()
target_link_libraries(server bq_websocket)

foreach (targ client server)
    if(MSVC)
      target_compile_options(${targ} PRIVATE /W4 /WX)
    else()
      target_compile_options(${targ} PRIVATE -Wall -Wextra -Werror
                                             -Wno-missing-braces
                                             -Wno-missing-field-initializers)
    endif()

    # explicitly strip dead code
    if (CMAKE_C_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_SYSTEM_NAME STREQUAL Emscripten)
        target_link_options(${targ} PRIVATE LINKER:-dead_strip)
    endif()
endforeach()
