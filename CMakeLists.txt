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
    "src/assets/item.hpp"
    "src/assets/item.cpp"
    "src/assets/map.hpp"
    "src/assets/map.cpp"
    "src/assets/model.hpp"
    "src/assets/model.cpp"
    "src/assets/skeleton.hpp"
    "src/assets/skeleton.cpp"
    "src/assets/vehiclemdl.hpp"
    "src/assets/vehiclemdl.cpp"
    "src/collision/dynamicsworld.hpp"
    "src/collision/dynamicsworld.cpp"
    "src/collision/motionstate.hpp"
    "src/collision/trianglemesh.hpp"
    "src/collision/trianglemesh.cpp"
    "src/game/camera_controller.hpp"
    "src/game/camera_controller.cpp"
    "src/game/character_anim_state.hpp"
    "src/game/character_anim_state.cpp"
    "src/game/deform_grid.hpp"
    "src/game/deform_grid.cpp"
    "src/game/player_input.hpp"
    "src/game/simple_entity_sync.hpp"
    "src/game/skeletoninstance.hpp"
    "src/game/skeletoninstance.cpp"
    "src/game/transform_node.hpp"
    "src/game/vehicle_sync.hpp"
    "src/net/defs.hpp"
    "src/net/fixed_str.hpp"
    "src/net/inmessage.hpp"
    "src/net/msg_producer.hpp"
    "src/net/msg_producer.cpp"
    "src/net/outmessage.hpp"
    "src/net/quantized.hpp"
    "src/net/utils.hpp"
    "src/utils/aabb.hpp"
    "src/utils/allocnum.hpp"
    "src/utils/cvars.hpp"
    "src/utils/cvars.cpp"
    "src/utils/defs.hpp"
    "src/utils/files.hpp"
    "src/utils/scheduler.hpp"
    "src/utils/sphere.hpp"
    "src/utils/transform.hpp"
    "src/utils/validate.hpp"
)

set(CLIENT_ONLY_SOURCES
    "src/assets/effect.hpp"
    "src/assets/effect.cpp"
    "src/assets/mesh_builder.hpp"
    "src/assets/mesh_builder.cpp"
    "src/assets/precache.hpp"
    "src/assets/precache.cpp"
    "src/audio/defs.hpp"
    "src/audio/master.hpp"
    "src/audio/master.cpp"
    "src/audio/ogg.hpp"
    "src/audio/ogg.cpp"
    "src/audio/player.hpp"
    "src/audio/player.cpp"
    "src/audio/sound_source.hpp"
    "src/audio/sound_source.cpp"
    "src/audio/sound.hpp"
    "src/audio/sound.cpp"
    "src/audio/source.hpp"
    "src/audio/source.cpp"
    "src/client/app.hpp"
    "src/client/app.cpp"
    "src/client/gl.hpp"
    "src/client/main.cpp"
    "src/client/settings.hpp"
    "src/client/settings.cpp"
    "src/client/utils.hpp"
    "src/client/wsclient.hpp"
    "src/client/wsclient_easywsclient.cpp"
    "src/client/wsclient_emscripten.cpp"
    "src/gameview/characterview.hpp"
    "src/gameview/characterview.cpp"
    "src/gameview/client_session.hpp"
    "src/gameview/client_session.cpp"
    "src/gameview/entityview.hpp"
    "src/gameview/entityview.cpp"
    "src/gameview/mapinstanceview.hpp"
    "src/gameview/mapinstanceview.cpp"
    "src/gameview/markerview.hpp"
    "src/gameview/markerview.cpp"
    "src/gameview/particle_emitter.hpp"
    "src/gameview/particle_emitter.cpp"
    "src/gameview/remote_menu_view.hpp"
    "src/gameview/remote_menu_view.cpp"
    "src/gameview/simple_entity_view.hpp"
    "src/gameview/simple_entity_view.cpp"
    "src/gameview/skinning_ubo.hpp"
    "src/gameview/skinning_ubo.cpp"
    "src/gameview/vehicleview.hpp"
    "src/gameview/vehicleview.cpp"
    "src/gameview/worldenv.hpp"
    "src/gameview/worldenv.cpp"
    "src/gameview/worldview.hpp"
    "src/gameview/worldview.cpp"
    "src/gfx/buffer_object.cpp"
    "src/gfx/buffer_object.hpp"
    "src/gfx/deform_texture.hpp"
    "src/gfx/deform_texture.cpp"
    "src/gfx/draw_list.hpp"
    "src/gfx/frustum.hpp"
    "src/gfx/frustum.cpp"
    "src/gfx/renderer.hpp"
    "src/gfx/renderer.cpp"
    "src/gfx/shader_common.hpp"
    "src/gfx/shader_defs.hpp"
    "src/gfx/shader_sources.hpp"
    "src/gfx/shader_sources.cpp"
    "src/gfx/shader.hpp"
    "src/gfx/shader.cpp"
    "src/gfx/surface_render_flags.hpp"
    "src/gfx/surface_shader.hpp"
    "src/gfx/surface_shader.cpp"
    "src/gfx/surface.hpp"
    "src/gfx/texture.cpp"
    "src/gfx/texture.hpp"
    "src/gfx/uniform_buffer.hpp"
    "src/gfx/vertex_array.cpp"
    "src/gfx/vertex_array.hpp"
    "src/gui/context.hpp"
    "src/gui/context.cpp"
    "src/gui/font.hpp"
    "src/gui/font.cpp"
    "src/gui/loading_screen.hpp"
    "src/gui/loading_screen.cpp"
    "src/gui/menu.hpp"
    "src/gui/menu.cpp"
    "src/gui/player_hud.hpp"
    "src/gui/player_hud.cpp"
    "src/utils/files.cpp"
)

set(SERVER_ONLY_SOURCES
    "src/game/animal.hpp"
    "src/game/animal.cpp"
    "src/game/character.hpp"
    "src/game/character.cpp"
    "src/game/cow.hpp"
    "src/game/cow.cpp"
    "src/game/destroyed_object.hpp"
    "src/game/destroyed_object.cpp"
    "src/game/drivable_vehicle.hpp"
    "src/game/drivable_vehicle.cpp"
    "src/game/enterable_world.hpp"
    "src/game/enterable_world.cpp"
    "src/game/entity.hpp"
    "src/game/entity.cpp"
    "src/game/game.hpp"
    "src/game/game.cpp"
    "src/game/human_character.hpp"
    "src/game/human_character.cpp"
    "src/game/item_instance.hpp"
    "src/game/item_instance.cpp"
    "src/game/mapinstance.hpp"
    "src/game/mapinstance.cpp"
    "src/game/marker.hpp"
    "src/game/marker.cpp"
    "src/game/npc_character.hpp"
    "src/game/npc_character.cpp"
    "src/game/openworld.hpp"
    "src/game/openworld.cpp"
    "src/game/player_character.hpp"
    "src/game/player_character.cpp"
    "src/game/player.hpp"
    "src/game/player.cpp"
    "src/game/projectile.hpp"
    "src/game/projectile.cpp"
    "src/game/remote_menu.hpp"
    "src/game/remote_menu.cpp"
    "src/game/rideable.hpp"
    "src/game/rideable.cpp"
    "src/game/simple_entity.hpp"
    "src/game/simple_entity.cpp"
    "src/game/tuning_world.hpp"
    "src/game/tuning_world.cpp"
    "src/game/usable.hpp"
    "src/game/vehicle_tuning.hpp"
    "src/game/vehicle_tuning.cpp"
    "src/game/vehicle.hpp"
    "src/game/vehicle.cpp"
    "src/game/world.hpp"
    "src/game/world.cpp"
    "src/server/client.hpp"
    "src/server/client.cpp"
    "src/server/main.cpp"
    "src/server/server.hpp"
    "src/server/server.cpp"
    "src/server/server_cfg.hpp"
    "src/server/server_cfg.cpp"
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

target_compile_definitions(${MAIN_NAME} PRIVATE CLIENT)

# add glm
add_subdirectory(external/glm)
target_link_libraries(${MAIN_NAME} PRIVATE glm)
target_include_directories(${MAIN_NAME} PRIVATE "external/stb")

# add bullet3
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_BULLET2_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_PYBULLET OFF CACHE BOOL "" FORCE)
set(BUILD_OPENGL3_DEMOS OFF CACHE BOOL "" FORCE)

add_subdirectory(external/bullet3)
target_link_libraries(${MAIN_NAME} PRIVATE BulletDynamics BulletCollision LinearMath Bullet3Common)
target_include_directories(${MAIN_NAME} PRIVATE "external/bullet3/src")

# add ogg
add_subdirectory(external/ogg)

# add vorbis
add_subdirectory(external/vorbis)
target_link_libraries(${MAIN_NAME} PRIVATE vorbisfile vorbis)

# Platform-specific SDL2 handling
if (CMAKE_SYSTEM_NAME STREQUAL Emscripten)

    # Emscripten provides SDL2 via its system libraries
    message(STATUS "Target platform: WebAssembly (Emscripten)")
    set(CMAKE_EXECUTABLE_SUFFIX ".html")  # Optional: build HTML page

    target_compile_options(${MAIN_NAME} PRIVATE 
        "-sUSE_SDL=2"
        "-sNO_DISABLE_EXCEPTION_CATCHING=1"
    )

    target_compile_definitions(${MAIN_NAME} PRIVATE EMSCRIPTEN)

    target_link_options(${MAIN_NAME} PRIVATE 
        "-lwebsocket.js"
        "-lidbfs.js"
        "-lopenal"
        "-sUSE_SDL=2"
        "-sASYNCIFY"
        "-sUSE_WEBGL2=1"
        "-sNO_DISABLE_EXCEPTION_CATCHING=1"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sEXPORTED_FUNCTIONS=_RunMain,_SetName,_SetUrl"
        "-sEXPORTED_RUNTIME_METHODS=ccall,cwrap"
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

    # add openal
    set(ALSOFT_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(external/openal-soft)
    target_link_libraries(${MAIN_NAME} PRIVATE OpenAL::OpenAL)

    add_custom_command(TARGET ${MAIN_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:OpenAL>
            $<TARGET_FILE_DIR:${MAIN_NAME}>
    )

    # build server
    set(ASIO_INCLUDE_DIR "external/asio/include")
    include_directories(${ASIO_INCLUDE_DIR})

    add_subdirectory(external/Crow)

    set(SERVER_NAME server)
    add_executable(${SERVER_NAME} ${COMMON_SOURCES} ${SERVER_ONLY_SOURCES})
    target_link_libraries(${SERVER_NAME} PRIVATE Crow glm BulletDynamics BulletCollision LinearMath Bullet3Common)
    target_include_directories(${SERVER_NAME} PRIVATE "src" "external/bullet3/src")
    target_compile_definitions(${SERVER_NAME} PRIVATE SERVER)

endif()
