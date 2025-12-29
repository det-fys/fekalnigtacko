cmake_minimum_required(VERSION 3.15)
project(FekalniGtacko)

# Enable C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES 
    "src/client/main.cpp"
    "src/client/app.hpp"
    "src/client/app.cpp"
    "src/client/gl.hpp"
    "src/client/utils.hpp"
    "src/assets/animation.hpp"
    "src/assets/animation.cpp"
    "src/assets/cache.hpp"
    "src/assets/cache.cpp"
    "src/assets/cmdfile.hpp"
    "src/assets/cmdfile.cpp"
    "src/assets/model.hpp"
    "src/assets/model.cpp"
    "src/assets/mesh_builder.hpp"
    "src/assets/mesh_builder.cpp"
    "src/assets/map.hpp"
    "src/assets/map.cpp"
    "src/assets/skeleton.hpp"
    "src/assets/skeleton.cpp"
    "src/collision/trianglemesh.cpp"
    # "src/game/entity.hpp"
    # "src/game/entity.cpp"
    # "src/game/meshinstance.hpp"
    # "src/game/meshinstance.cpp"
    "src/game/player_input.hpp"
    # "src/game/player.hpp"
    # "src/game/player.cpp"
    "src/game/transform_node.hpp"
    # "src/game/world.hpp"
    # "src/game/world.cpp"
    "src/gameview/client_session.hpp"
    "src/gameview/entityview.hpp"
    "src/gameview/worldview.hpp"
    "src/gfx/buffer_object.cpp"
    "src/gfx/buffer_object.hpp"
    "src/gfx/draw_list.hpp"
    "src/gfx/renderer.hpp"
    "src/gfx/renderer.cpp"
    "src/gfx/shader_defs.hpp"
    "src/gfx/shader_sources.hpp"
    "src/gfx/shader_sources.cpp"
    "src/gfx/shader.hpp"
    "src/gfx/shader.cpp"
    "src/gfx/surface.hpp"
    "src/gfx/texture.cpp"
    "src/gfx/texture.hpp"
    "src/gfx/uniform_buffer.hpp"
    "src/gfx/vertex_array.cpp"
    "src/gfx/vertex_array.hpp"
    "src/net/defs.hpp"
    "src/net/fixed_str.hpp"
    "src/net/inmessage.hpp"
    "src/net/outmessage.hpp"
    "src/net/quantized.hpp"
    "src/utils/files.hpp"
    "src/utils/files.cpp"
    "src/utils/transform.hpp"
)


if(ANDROID)
    # Android-specific setup
    set(MAIN_NAME "main")
    add_library(${MAIN_NAME} SHARED ${SOURCES})
    target_link_libraries(${MAIN_NAME} PRIVATE GLESv3 log android)
else()
    # Desktop build
    set(MAIN_NAME "FekalniGtacko")
    add_executable(${MAIN_NAME} ${SOURCES})
endif()


# Include directories
target_include_directories(${MAIN_NAME} PRIVATE 
    "src"
)

# Platform-specific SDL2 handling
if (CMAKE_SYSTEM_NAME STREQUAL Emscripten)

    # Emscripten provides SDL2 via its system libraries
    message(STATUS "Target platform: WebAssembly (Emscripten)")
    set(CMAKE_EXECUTABLE_SUFFIX ".html")  # Optional: build HTML page

    target_compile_options(${MAIN_NAME} PRIVATE 
        "-sUSE_SDL=2"
        "-sNO_DISABLE_EXCEPTION_CATCHING=1"
    )

    target_link_options(${MAIN_NAME} PRIVATE 
        "-sUSE_SDL=2"
        "-sASYNCIFY"
        "-sUSE_WEBGL2=1"
        "-sNO_DISABLE_EXCEPTION_CATCHING=1"
        "-sALLOW_MEMORY_GROWTH=1"
        "--shell-file" "${CMAKE_SOURCE_DIR}/shell.html"
        "--preload-file" "${CMAKE_SOURCE_DIR}/assets/@/"
        )
        
else()
    message(STATUS "Target platform: Native")
    # Native platform
    # find_package(SDL2 REQUIRED)
    # SDL2 build options to avoid unwanted components
    set(SDL_TEST OFF CACHE BOOL "" FORCE)

if(ANDROID)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
else()
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
endif()

    add_subdirectory(external/SDL)
    target_include_directories(${MAIN_NAME} PRIVATE "external/SDL/include")

    target_link_libraries(${MAIN_NAME} PRIVATE SDL2main)

if(ANDROID)
    target_link_libraries(${MAIN_NAME} PRIVATE SDL2)
else()
    target_link_libraries(${MAIN_NAME} PRIVATE SDL2-static)
endif()

    add_subdirectory(external/glad)
    target_link_libraries(${MAIN_NAME} PRIVATE glad)

endif()

add_subdirectory(external/glm)
target_link_libraries(${MAIN_NAME} PRIVATE glm)
target_include_directories(${MAIN_NAME} PRIVATE "external/stb")

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_BULLET2_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_PYBULLET OFF CACHE BOOL "" FORCE)
set(BUILD_OPENGL3_DEMOS OFF CACHE BOOL "" FORCE)

add_subdirectory(external/bullet3)
target_link_libraries(${MAIN_NAME} PRIVATE BulletCollision LinearMath Bullet3Common)
target_include_directories(${MAIN_NAME} PRIVATE "external/bullet3/src")
