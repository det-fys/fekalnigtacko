cmake_minimum_required(VERSION 3.15)
project(FekalniGtacko)

# Enable C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(COMMON_SOURCES
    "src/assets/animation.hpp"
    "src/assets/animation.cpp"
    "src/assets/cache.hpp"
    "src/assets/cache.cpp"
    "src/assets/cmdfile.hpp"
    "src/assets/cmdfile.cpp"
    "src/assets/map.hpp"
    "src/assets/map.cpp"
    "src/assets/model.hpp"
    "src/assets/model.cpp"
    "src/assets/skeleton.hpp"
    "src/assets/skeleton.cpp"
    "src/assets/vehiclemdl.hpp"
    "src/assets/vehiclemdl.cpp"
    "src/collision/aabb.hpp"
    "src/collision/dynamicsworld.hpp"
    "src/collision/dynamicsworld.cpp"
    "src/collision/motionstate.hpp"
    "src/collision/trianglemesh.hpp"
    "src/collision/trianglemesh.cpp"
    "src/game/player_input.hpp"
    "src/game/transform_node.hpp"
    "src/net/defs.hpp"
    "src/net/fixed_str.hpp"
    "src/net/inmessage.hpp"
    "src/net/msg_producer.hpp"
    "src/net/msg_producer.cpp"
    "src/net/outmessage.hpp"
    "src/net/quantized.hpp"
    "src/utils/allocnum.hpp"
    "src/utils/defs.hpp"
    "src/utils/files.hpp"
    "src/utils/transform.hpp"
    "src/utils/validate.hpp"
)

set(CLIENT_ONLY_SOURCES
    "src/assets/mesh_builder.hpp"
    "src/assets/mesh_builder.cpp"
    "src/client/app.hpp"
    "src/client/app.cpp"
    "src/client/gl.hpp"
    "src/client/main.cpp"
    "src/client/utils.hpp"
    "src/gameview/client_session.hpp"
    "src/gameview/client_session.cpp"
    "src/gameview/entityview.hpp"
    "src/gameview/vehicleview.hpp"
    "src/gameview/vehicleview.cpp"
    "src/gameview/worldview.hpp"
    "src/gameview/worldview.cpp"
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
    "src/utils/files.cpp"
)

set(SERVER_ONLY_SOURCES
    "src/game/controllable.cpp"
    "src/game/controllable.hpp"
    "src/game/entity.hpp"
    "src/game/entity.cpp"
    "src/game/game.hpp"
    "src/game/game.cpp"
    "src/game/openworld.hpp"
    "src/game/openworld.cpp"
    "src/game/player.hpp"
    "src/game/player.cpp"
    "src/game/vehicle.hpp"
    "src/game/vehicle.cpp"
    "src/game/world.hpp"
    "src/game/world.cpp"
    "src/server/client.hpp"
    "src/server/client.cpp"
    "src/server/main.cpp"
    "src/server/server.hpp"
    "src/server/server.cpp"
    "src/server/wsserver.hpp"
    "src/server/wsserver.cpp"
    "src/utils/files_server.cpp"
)

if(ANDROID)
    # Android-specific setup
    set(MAIN_NAME "main")
    add_library(${MAIN_NAME} SHARED ${COMMON_SOURCES} ${CLIENT_ONLY_SOURCES})
    target_link_libraries(${MAIN_NAME} PRIVATE GLESv3 log android)
else()
    # Desktop build
    set(MAIN_NAME "FekalniGtacko")
    add_executable(${MAIN_NAME} ${COMMON_SOURCES} ${CLIENT_ONLY_SOURCES})
endif()


# Include directories
target_include_directories(${MAIN_NAME} PRIVATE "src")

add_subdirectory(external/glm)
target_link_libraries(${MAIN_NAME} PRIVATE glm)
target_include_directories(${MAIN_NAME} PRIVATE "external/stb")

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_BULLET2_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_PYBULLET OFF CACHE BOOL "" FORCE)
set(BUILD_OPENGL3_DEMOS OFF CACHE BOOL "" FORCE)

add_subdirectory(external/bullet3)
target_link_libraries(${MAIN_NAME} PRIVATE BulletCollision BulletDynamics LinearMath Bullet3Common)
target_include_directories(${MAIN_NAME} PRIVATE "external/bullet3/src")

target_compile_definitions(${MAIN_NAME} PRIVATE CLIENT)

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

    # add easywsclient
    set(EASYWSCLIENT_PATH "external/easywsclient")

    add_library(easywsclient STATIC 
        ${EASYWSCLIENT_PATH}/easywsclient.cpp
    )

    target_include_directories(easywsclient PUBLIC ${EASYWSCLIENT_PATH})
    
    target_link_libraries(${MAIN_NAME} PRIVATE easywsclient)

    # build server
    set(ASIO_INCLUDE_DIR "external/asio/include")
    include_directories(${ASIO_INCLUDE_DIR})

    add_subdirectory(external/Crow)
    set(SERVER_SOURCES

    )

    set(SERVER_NAME server)
    add_executable(${SERVER_NAME} ${COMMON_SOURCES} ${SERVER_ONLY_SOURCES})
    target_link_libraries(${SERVER_NAME} PRIVATE Crow glm BulletCollision BulletDynamics LinearMath Bullet3Common)
    target_include_directories(${SERVER_NAME} PRIVATE "src" "external/bullet3/src")
    target_compile_definitions(${SERVER_NAME} PRIVATE SERVER)

endif()
