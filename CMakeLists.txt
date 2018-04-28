cmake_minimum_required(VERSION 3.10)
project(rasterizer)

set(RASTERIZER_SRC
    src/main.c

    kernel/tasksys.cpp
)

add_custom_command(OUTPUT clear.o
                   COMMAND ispc --target=sse2 ${CMAKE_SOURCE_DIR}/kernel/clear.ispc -o clear.o
                   DEPENDS kernel/clear.ispc)

add_executable(rasterizer WIN32 ${RASTERIZER_SRC} clear.o)

#target_link_libraries(fishball glfw ${VULKAN_LIBRARY})

