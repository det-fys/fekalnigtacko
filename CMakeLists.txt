cmake_minimum_required(VERSION 3.22)
project(FekalniGtacko)

# ------------------------------------------------------------------------------
# Build Options
# ------------------------------------------------------------------------------
option(BUILD_CLIENT "Build the client application" ON)
option(BUILD_SERVER "Build the server application" ON)

# ------------------------------------------------------------------------------
# Version
# ------------------------------------------------------------------------------
execute_process(
    COMMAND git describe --tags --always --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE PROJECT_VERSION_GIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# fallback
if(NOT PROJECT_VERSION_GIT)
    set(PROJECT_VERSION_GIT "unknown")
endif()

configure_file(
    "${CMAKE_SOURCE_DIR}/src/utils/version.hpp.in"
    "${CMAKE_BINARY_DIR}/generated/version.hpp"
    @ONLY
)

# ------------------------------------------------------------------------------
# Global Compiler Settings (Set early for submodules to inherit)
# ------------------------------------------------------------------------------
# Enable C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    message(STATUS "Target platform: WebAssembly (Emscripten)")
    add_compile_definitions(EMSCRIPTEN=1)
    add_compile_options("-pthread")
    add_link_options("-pthread")
else()
    message(STATUS "Target platform: Native")
endif()

# ------------------------------------------------------------------------------
# Source Definitions (Headers removed)
# ------------------------------------------------------------------------------
set(COMMON_SOURCES
    "src/assets/animation.cpp"
    "src/assets/asset_manager.cpp"
    "src/assets/cmdfile.cpp"
    "src/assets/item.cpp"
    "src/assets/map.cpp"
    "src/assets/map_navmesh.cpp"
    "src/assets/model.cpp"
    "src/assets/skeleton.cpp"
    "src/assets/vehiclemdl.cpp"
    "src/collision/dynamicsworld.cpp"
    "src/collision/trianglemesh.cpp"
    "src/game/camera_controller.cpp"
    "src/game/character_anim_state.cpp"
    "src/game/deform_grid.cpp"
    "src/game/skeletoninstance.cpp"
    "src/net/msg_producer.cpp"
    "src/utils/cvars.cpp"
    "src/db/db.cpp"
    "src/db/db_memory.cpp"
    "src/fs/archive_fetch.cpp"
    "src/fs/fs.cpp"
    "src/fs/zip_archive_kubazip.cpp"
    "src/game/animal.cpp"
    "src/game/character.cpp"
    "src/game/commands.cpp"
    "src/game/cow.cpp"
    "src/game/destroyed_object.cpp"
    "src/game/drivable_vehicle.cpp"
    "src/game/enterable_world.cpp"
    "src/game/entity.cpp"
    "src/game/game.cpp"
    "src/game/human_character.cpp"
    "src/game/item_instance.cpp"
    "src/game/mapinstance.cpp"
    "src/game/marker.cpp"
    "src/game/npc_character.cpp"
    "src/game/openworld.cpp"
    "src/game/player_character.cpp"
    "src/game/player.cpp"
    "src/game/projectile.cpp"
    "src/game/remote_menu.cpp"
    "src/game/rideable.cpp"
    "src/game/simple_entity.cpp"
    "src/game/tuning_world.cpp"
    "src/game/vehicle_tuning.cpp"
    "src/game/vehicle.cpp"
    "src/game/world.cpp"
    "src/mapgen/common.cpp"
    "src/mapgen/chunk.cpp"
    "src/mapgen/global_layout.cpp"
    "src/mapgen/heightmesh.cpp"
    "src/server/client.cpp"
    "src/server/server.cpp"
    "src/server/server_cfg.cpp"
)

# ------------------------------------------------------------------------------
# Common Dependencies (Needed by both Client and Server)
# ------------------------------------------------------------------------------
# GLM
add_subdirectory(external/glm)

# Bullet3
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_BULLET2_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_PYBULLET OFF CACHE BOOL "" FORCE)
set(BUILD_OPENGL3_DEMOS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/bullet3)

# Recast Navigation
set(RECASTNAVIGATION_DEMO OFF CACHE BOOL "" FORCE)
set(RECASTNAVIGATION_TESTS OFF CACHE BOOL "" FORCE)
set(RECASTNAVIGATION_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(external/recastnavigation)

# Zip
set(ZIP_ENABLE_INFLATE ON)
set(ZIP_ENABLE_DEFLATE OFF)
set(ZIP_HAVE_SYMLINK OFF)
add_subdirectory(external/zip)

# TrianglePP
add_subdirectory(external/trianglepp EXCLUDE_FROM_ALL)

# ------------------------------------------------------------------------------
# Client Target
# ------------------------------------------------------------------------------
if(BUILD_CLIENT)
    set(CLIENT_ONLY_SOURCES
        "src/assets/effect.cpp"
        "src/assets/precache.cpp"
        "src/audio/master.cpp"
        "src/audio/ogg.cpp"
        "src/audio/player.cpp"
        "src/audio/sound_source.cpp"
        "src/audio/sound.cpp"
        "src/audio/source.cpp"
        "src/client/app.cpp"
        "src/client/key_map.cpp"
        "src/client/main.cpp"
        "src/client/settings.cpp"
        "src/edit/map_edit.cpp"
        "src/edit/map_project.cpp"
        "src/edit/map_viewport.cpp"
        "src/edit/map_viewport_2d.cpp"
        "src/edit/map_viewport_3d.cpp"
        "src/edit/object.cpp"
        "src/edit/static_object.cpp"
        "src/edit/waypoint.cpp"
        "src/gameview/characterview.cpp"
        "src/gameview/client_session.cpp"
        "src/gameview/entityview.cpp"
        "src/gameview/mapinstanceview.cpp"
        "src/gameview/markerview.cpp"
        "src/gameview/modelview.cpp"
        "src/gameview/particle_emitter.cpp"
        "src/gameview/remote_menu_view.cpp"
        "src/gameview/simple_entity_view.cpp"
        "src/gameview/spz_texture.cpp"
        "src/gameview/vehicleview.cpp"
        "src/gameview/worldenv.cpp"
        "src/gameview/worldview.cpp"
        "src/gfx/common/vertex_pack.cpp"
        "src/gfx/renderer_gl/buffer_object.cpp"
        "src/gfx/renderer_gl/deform_texture_gl.cpp"
        "src/gfx/renderer_gl/mesh_gl.cpp"
        "src/gfx/renderer_gl/renderer_gl.cpp"
        "src/gfx/renderer_gl/shader.cpp"
        "src/gfx/renderer_gl/shader_sources.cpp"
        "src/gfx/renderer_gl/skeleton_pose_gl.cpp"
        "src/gfx/renderer_gl/surface_shader.cpp"
        "src/gfx/renderer_gl/texture_gl.cpp"
        "src/gfx/renderer_gl/vertex_array.cpp"
        "src/gfx/renderer_gl/viewport_gl.cpp"
        "src/gfx/renderer_wgpu/renderer_wgpu.cpp"
        "src/gfx/renderer_wgpu/surface_sdl.cpp"
        "src/gfx/renderer_wgpu/surface_shader_wgpu.cpp"
        "src/gfx/deform_texture.cpp"
        "src/gfx/frustum.cpp"
        "src/gfx/material.cpp"
        "src/gfx/mesh.cpp"
        "src/gfx/renderer.cpp"
        "src/gfx/skeleton_pose.cpp"
        "src/gfx/texture.cpp"
        "src/gfx/viewport.cpp"
        "src/gui/chat.cpp"
        "src/gui/context.cpp"
        "src/gui/font.cpp"
        "src/gui/loading_screen.cpp"
        "src/gui/menu.cpp"
        "src/gui/player_hud.cpp"
        "src/im/imgui_integration.cpp"
        "src/im/scene_view.cpp"
        "src/im/viewport.cpp"
        "src/im/viewport_2d_controls.cpp"
        "src/net/client_local.cpp"
        "src/net/client_ws_easywsclient.cpp"
        "src/net/client_ws_emscripten.cpp"
        "src/net/client.cpp"
        "src/net/local_channel.cpp"
        "src/net/server_local.cpp"
        "src/utils/image.cpp"
    )

    set(MAIN_NAME "FekalniGtacko")
    add_executable(${MAIN_NAME} ${COMMON_SOURCES} ${CLIENT_ONLY_SOURCES})
    
    target_include_directories(${MAIN_NAME} PRIVATE 
        "src"
        "external/stb"
        "external/bullet3/src"
        "${CMAKE_BINARY_DIR}/generated"
        "external/zip/src"
    )

    target_compile_definitions(${MAIN_NAME} PRIVATE CLIENT GLM_ENABLE_EXPERIMENTAL)
    target_link_libraries(${MAIN_NAME} PRIVATE 
        glm 
        BulletDynamics BulletCollision LinearMath Bullet3Common 
        RecastNavigation::Recast RecastNavigation::Detour
        zip
        TrianglePP
    )

    # Client-only external libraries
    add_subdirectory(external/ogg)
    add_subdirectory(external/vorbis)
    target_link_libraries(${MAIN_NAME} PRIVATE vorbisfile vorbis)

    # ImGui
    set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui)
    set(IMGUIZMO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imguizmo)

    # Core ImGui sources
    set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        # backends
        ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
        ${IMGUI_DIR}/backends/imgui_impl_wgpu.cpp
        # imguizmo
        ${IMGUIZMO_DIR}/src/ImGuizmo.cpp
    )

    set(IMGUI_INCLUDE_DIRS
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
        ${IMGUIZMO_DIR}/src
    )

    target_sources(${MAIN_NAME} PRIVATE ${IMGUI_SOURCES})
    target_include_directories(${MAIN_NAME} PRIVATE ${IMGUI_INCLUDE_DIRS})

    target_compile_definitions(${MAIN_NAME} PRIVATE PG_IMGUI)

    # Dawn setup
    set(DAWN_FETCH_DEPENDENCIES ON)
    set(DAWN_ENABLE_D3D11 OFF)
    set(DAWN_ENABLE_METAL OFF)
    set(DAWN_ENABLE_NULL OFF)
    set(DAWN_ENABLE_DESKTOP_GL OFF)
    set(DAWN_ENABLE_OPENGLES OFF)
    set(TINT_BUILD_SPV_READER OFF)
    set(DAWN_BUILD_SAMPLES OFF)
    set(DAWN_USE_GLFW OFF)
    set(TINT_BUILD_TINT OFF)
    set(TINT_BUILD_CMD_TOOLS OFF)
    set(TINT_BUILD_SAMPLES OFF)
    set(TINT_BUILD_DOCS OFF)
    set(TINT_BUILD_TESTS OFF)
    set(TINT_BUILD_FUZZERS OFF)
    set(TINT_BUILD_SPIRV_TOOLS_FUZZER OFF)
    set(TINT_BUILD_AST_FUZZER OFF)
    set(TINT_BUILD_REGEX_FUZZER OFF)
    set(TINT_BUILD_BENCHMARKS OFF)
    set(TINT_BUILD_AS_OTHER_OS OFF)
    set(TINT_BUILD_REMOTE_COMPILE OFF)
    
    if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
        set(DAWN_ENABLE_D3D12 OFF)
        set(DAWN_ENABLE_VULKAN OFF)
    else()
        set(DAWN_ENABLE_D3D12 ON)
        set(DAWN_ENABLE_VULKAN ON)
    endif()

    add_subdirectory(external/dawn EXCLUDE_FROM_ALL)

    # Emscripten vs Native Client Logic
    if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
        set_target_properties(${MAIN_NAME} PROPERTIES SUFFIX ".html")
        target_compile_options(${MAIN_NAME} PRIVATE 
            "-sUSE_SDL=2"
            "-sNO_DISABLE_EXCEPTION_CATCHING=1"
        )
        target_link_options(${MAIN_NAME} PRIVATE 
            "-lwebsocket.js"
            "-lidbfs.js"
            "-lopenal"
            "-sPTHREAD_POOL_SIZE=4"
            "-sUSE_SDL=2"
            "-sASYNCIFY"
            "-sUSE_WEBGL2=1"
            "-sFETCH"
            "-sNO_DISABLE_EXCEPTION_CATCHING=1"
            "-sALLOW_MEMORY_GROWTH=1"
            "-sGROWABLE_ARRAYBUFFERS=0"
            "-sEXPORTED_FUNCTIONS=_RunMain,_SetName,_SetUrl,_SetRenderer"
            "-sEXPORTED_RUNTIME_METHODS=ccall,cwrap"
            "--shell-file" "${CMAKE_SOURCE_DIR}/shell.html"
            "--preload-file" "${CMAKE_SOURCE_DIR}/assets/@/"
        )
        target_link_libraries(${MAIN_NAME} PRIVATE emdawnwebgpu_cpp)
    else()
        # Native Client setup
        set(SDL_TEST OFF CACHE BOOL "" FORCE)
        set(SDL_SHARED OFF CACHE BOOL "" FORCE)
        set(SDL_STATIC ON CACHE BOOL "" FORCE)

        add_subdirectory(external/SDL)
        target_include_directories(${MAIN_NAME} PRIVATE "external/SDL/include")
        target_link_libraries(${MAIN_NAME} PRIVATE SDL2main SDL2-static)

        add_subdirectory(external/glad)
        target_link_libraries(${MAIN_NAME} PRIVATE glad)

        # EasyWSClient
        set(EASYWSCLIENT_PATH "external/easywsclient")
        add_library(easywsclient STATIC ${EASYWSCLIENT_PATH}/easywsclient.cpp)
        target_include_directories(easywsclient PUBLIC ${EASYWSCLIENT_PATH})
        target_link_libraries(${MAIN_NAME} PRIVATE easywsclient)

        # OpenAL
        set(ALSOFT_EXAMPLES OFF CACHE BOOL "" FORCE)
        add_subdirectory(external/openal-soft)
        target_link_libraries(${MAIN_NAME} PRIVATE OpenAL::OpenAL)

        # Copy OpenAL dll/so on build
        add_custom_command(TARGET ${MAIN_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:OpenAL>
                $<TARGET_FILE_DIR:${MAIN_NAME}>
        )

        target_link_libraries(${MAIN_NAME} PRIVATE webgpu_dawn)

        target_compile_definitions(${MAIN_NAME} PRIVATE IMGUI_IMPL_WEBGPU_BACKEND_DAWN)
    endif()
endif()

# ------------------------------------------------------------------------------
# Server Target (Native Only)
# ------------------------------------------------------------------------------
if(BUILD_SERVER)
    if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
        message(WARNING "Skipping server build: Cannot build the server on Emscripten.")
    else()
        set(SERVER_ONLY_SOURCES
            "src/net/server_ws_crow.cpp"
            "src/server/main.cpp"
        )

        set(SERVER_NAME "server")
        add_executable(${SERVER_NAME} ${COMMON_SOURCES} ${SERVER_ONLY_SOURCES})
        
        set(ASIO_INCLUDE_DIR "external/asio/include")

        target_include_directories(${SERVER_NAME} PRIVATE 
            "src" 
            "external/bullet3/src"
            ${ASIO_INCLUDE_DIR}
            "${CMAKE_BINARY_DIR}/generated"
            "external/zip/src"
        )
        target_compile_definitions(${SERVER_NAME} PRIVATE SERVER GLM_ENABLE_EXPERIMENTAL)
        
        # Add Server-specific libraries
        add_subdirectory(external/Crow)

        target_link_libraries(${SERVER_NAME} PRIVATE 
            Crow 
            glm 
            BulletDynamics BulletCollision LinearMath Bullet3Common 
            RecastNavigation::Recast RecastNavigation::Detour
            zip
            TrianglePP
        )
    endif()
endif()
