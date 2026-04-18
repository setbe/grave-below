cmake_minimum_required(VERSION 3.22)
# ---------------------------------------------------------------------------
# Custom configs for Visual Studio (must be BEFORE project())
# NOTE: Don't use if(MSVC) here, MSVC is not known yet.
# ---------------------------------------------------------------------------
set(GB_CUSTOM_CONFIGS "ReleaseMini;ReleaseNoConsole;ReleaseMiniNoConsole")
if(CMAKE_GENERATOR MATCHES "Visual Studio" OR CMAKE_GENERATOR MATCHES "Ninja Multi-Config")
    set(CMAKE_CONFIGURATION_TYPES
        "Debug;Release;${GB_CUSTOM_CONFIGS}"
        CACHE STRING "Available build configurations" FORCE
    )
endif()

project(grave_below LANGUAGES CXX)

# Common include dirs for whole repo
add_library(gb_includes INTERFACE)
target_include_directories(gb_includes INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party
    ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/hi/3rd_party
)


# ---------------------------------------------------------------------------
# Seed missing per-config flag variables (must be AFTER project())
# to avoid: CMAKE_EXE_LINKER_FLAGS_<CFG> not set
# ---------------------------------------------------------------------------
if(CMAKE_GENERATOR MATCHES "Visual Studio")
    foreach(cfg IN ITEMS ReleaseMini ReleaseNoConsole ReleaseMiniNoConsole)
        string(TOUPPER "${cfg}" CFGU)

        foreach(var IN ITEMS
            CMAKE_C_FLAGS
            CMAKE_CXX_FLAGS
            CMAKE_EXE_LINKER_FLAGS
            CMAKE_SHARED_LINKER_FLAGS
            CMAKE_MODULE_LINKER_FLAGS
        )
            # If not defined, copy from Release as baseline.
            if(NOT DEFINED ${var}_${CFGU})
                set(${var}_${CFGU} "${${var}_RELEASE}" CACHE STRING "" FORCE)
            endif()
        endforeach()
    endforeach()
endif()
# Options
option(LEGACY_CPP14 "Use legacy C++14 compiler (default ON)" ON)
option(BUILD_SHARED_LIBS "Build shared libs" OFF)
# Language standard
if(LEGACY_CPP14)
    set(CMAKE_CXX_STANDARD 14)
else()
    set(CMAKE_CXX_STANDARD 20)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Config groups
set(FREESTANDING_CONFIGS ReleaseMini ReleaseNoConsole ReleaseMiniNoConsole)
# Helper generator expression: true when freestanding config
set(_GE_FREESTANDING
    $<OR:$<CONFIG:ReleaseMini>,$<CONFIG:ReleaseNoConsole>,$<CONFIG:ReleaseMiniNoConsole>>
)
# Flags (cleanly separated: common vs freestanding-only)
set(msvc_common_compile
    "/permissive-"
    "/diagnostics:column"
    "/nologo"
)
# Freestanding base (applies to all Release* freestanding configs)
set(msvc_freestanding_base
    "/TP"
    "/W3"
    "/FC"
    "/Gs9999999"
    "/GS-"
    "/GR-"
    "/Zl"
    "/Zc:threadSafeInit-"
    "/Oi-"     # disable intrinsics (essential for freestanding)
    "/EHs-"    # disable C++ exceptions (no unwind tables)
    "/EHc-"    # no extern "C" unwind
    "/Gm-"
    "/Gy"
    "/Zc:inline"
    "/Zc:wchar_t"
    "/Zc:forScope"
    "/sdl-"
    "/errorReport:none"
    "/WX-"
    "/D;NDEBUG"
    "/D_HAS_EXCEPTIONS=0"
)
# Freestanding "full" (ReleaseNoConsole)
set(msvc_freestanding_full_opt
    "/O2"
    "/Ob2"
    "/fp:precise"
    "/Ot"
    "/Oy"
)
# Freestanding "mini" (ReleaseMini, ReleaseMiniNoConsole)
set(msvc_freestanding_mini_opt
    "/O1"
    "/Ob1"
    "/fp:fast"
    "/Os"
    "/Oy"
)
# Debug
set(msvc_debug_compile
    "/Zi"
    "/W3"
    "/GR"
    "/Od"
)
# GCC/Clang
set(unix_common_compile
    "-pipe"
)
set(unix_freestanding_base
    "-ffunction-sections"
    "-fdata-sections"
    "-fno-exceptions"
    "-fno-rtti"
    "-fno-math-errno"
    "-fno-signaling-nans"
    "-fomit-frame-pointer"
)
set(unix_freestanding_full_opt
    "-O3"
    "-flto"
    "-ffast-math"
)
set(unix_freestanding_mini_opt
    "-Os"
    "-ffunction-sections"
    "-fdata-sections"
    "-fno-exceptions"
    "-fno-rtti"
    "-fno-unwind-tables"
    "-fno-asynchronous-unwind-tables"
    "-fno-ident"
    "-fvisibility=hidden"
    "-fvisibility-inlines-hidden"
)
set(unix_debug_compile
    "-O0"
    "-g"
    "-fno-omit-frame-pointer"
)
set(unix_hosted_link_opt
    "-Wl,--gc-sections"
    "-Wl,--hash-style=gnu"
    "-Wl,--build-id=none"   # optional
    "-Wl,-z,relro"          # optional security
    "-Wl,-z,now"            # optional security (slower startup)
)

# -----------------------------------------------------------------------------
# Functions
# -----------------------------------------------------------------------------
function(gb_apply_common target)
    target_link_libraries(${target} PRIVATE gb_includes)

    target_compile_definitions(${target} PRIVATE
        $<$<CONFIG:Release>:NDEBUG>
        # IO_HAS_STD when NOT freestanding
        $<$<NOT:${_GE_FREESTANDING}>:IO_HAS_STD=1>
    )
    # compiler-id common flags
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:${msvc_common_compile}>
        $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:${unix_common_compile}>
    )
    # MSVC linker opts for all Release-like configs
    if(MSVC)
        target_link_options(${target} PRIVATE
            $<$<OR:$<CONFIG:Release>,$<CONFIG:ReleaseMini>,$<CONFIG:ReleaseNoConsole>,$<CONFIG:ReleaseMiniNoConsole>>:/OPT:REF>
            $<$<OR:$<CONFIG:Release>,$<CONFIG:ReleaseMini>,$<CONFIG:ReleaseNoConsole>,$<CONFIG:ReleaseMiniNoConsole>>:/OPT:ICF>
        )
    endif()
endfunction()

function(gb_apply_freestanding target)
    # MSVC: apply ONLY for freestanding configs
    target_compile_options(${target} PRIVATE
        $<$<AND:$<CXX_COMPILER_ID:MSVC>,${_GE_FREESTANDING}>:${msvc_freestanding_base}>
        $<$<AND:$<CXX_COMPILER_ID:MSVC>,${_GE_FREESTANDING}>:/EHs- /EHc- /D_HAS_EXCEPTIONS=0>

        $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:ReleaseNoConsole>>:${msvc_freestanding_full_opt}>
        $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:ReleaseMini>>:${msvc_freestanding_mini_opt}>
        $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:ReleaseMiniNoConsole>>:${msvc_freestanding_mini_opt}>
    )

    if(MSVC)
        # 1) DO NOT select CRT runtime for freestanding
        #    (so CMake/MSVC won't inject /DEFAULTLIB:libcpmt/msvcrt)
        set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY
          "$<IF:${_GE_FREESTANDING},MultiThreaded,$<IF:$<CONFIG:Debug>,MultiThreadedDebug,MultiThreadedDLL>>"
        )

        # 2) No default libs at link time for freestanding configs
        target_link_options(${target} PRIVATE
          $<$<AND:${_GE_FREESTANDING},$<CXX_COMPILER_ID:MSVC>>:/NODEFAULTLIB>
        )


        # 3) Keep only what you explicitly link (Win32 API libs etc.)
        # (add `psapi` for `SetProcessWorkingSetSize`, `K32EmptyWorkingSet`)
        target_link_libraries(${target} PRIVATE user32 gdi32 opengl32 kernel32 bcrypt ws2_32)
    endif()

    # GCC/Clang freestanding compile options
    target_compile_options(${target} PRIVATE
        $<$<AND:$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>,${_GE_FREESTANDING}>:${unix_freestanding_base}>

        $<$<AND:$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>,$<CONFIG:ReleaseNoConsole>>:${unix_freestanding_full_opt}>
        $<$<AND:$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>,$<CONFIG:ReleaseMini>>:${unix_freestanding_mini_opt}>
        $<$<AND:$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>,$<CONFIG:ReleaseMiniNoConsole>>:${unix_freestanding_mini_opt}>
    )

    # GCC/Clang no-std
    get_property(_hosted TARGET ${target} PROPERTY GB_HOSTED_LINK)
    # linux: not hosted link
    if(UNIX AND NOT APPLE AND NOT _hosted)
      target_link_options(${target} PRIVATE
        $<$<AND:${_GE_FREESTANDING},$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>>:
          -s -nostdlib -nostartfiles -nodefaultlibs -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables
          -Wl,--hash-style=gnu
          -Wl,--build-id=none
          -Wl,--gc-sections
          -Wl,-e,_start
          -Wl,-z,norelro
        >
      )
    endif()
    # linux: hosted link
    if(UNIX AND NOT APPLE AND _hosted)
        target_link_options(${target} PRIVATE
            $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:${unix_hosted_link_opt}>
        )
    endif()
endfunction()

function(gb_apply_debug target)
    target_compile_options(${target} PRIVATE
        $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:${msvc_debug_compile}>
        $<$<AND:$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>,$<CONFIG:Debug>>:${unix_debug_compile}>
    )
endfunction()

function(gb_console target)
    if(MSVC)
        target_link_options(${target} PRIVATE
            $<$<CONFIG:ReleaseNoConsole>:/SUBSYSTEM:WINDOWS>
            $<$<CONFIG:ReleaseNoConsole>:/ENTRY:WinMain>
            $<$<CONFIG:ReleaseMiniNoConsole>:/SUBSYSTEM:WINDOWS>
            $<$<CONFIG:ReleaseMiniNoConsole>:/ENTRY:WinMain>
        )
    endif()

    # CONSOLE define only for configs where it set
    target_compile_definitions(${target} PRIVATE
        $<$<OR:$<CONFIG:Debug>,$<CONFIG:Release>,$<CONFIG:ReleaseMini>>:CONSOLE>
    )
endfunction()


function(gb_add_console_exe name)
    # usage:
    # gb_add_console_exe(target
    #     CPPS file1.cpp file2.cpp ...
    #     HEADERS file1.hpp file2.hpp ...
    #     LIBS lib1 lib2 ...
    # )
    set(options)
    set(oneValueArgs)
    set(multiValueArgs CPPS HEADERS LIBS)
    cmake_parse_arguments(T "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_executable(${name} ${T_CPPS} ${T_HEADERS})
    set_property(TARGET ${name} PROPERTY GB_HOSTED_LINK 1)

    if(T_LIBS)
        target_link_libraries(${name} PRIVATE ${T_LIBS})
    endif()

    gb_apply_common(${name})
    gb_apply_freestanding(${name})
    gb_apply_debug(${name})
    gb_console(${name})
endfunction()

function(gb_add_hosted_console_exe name)
    # usage:
    # gb_add_hosted_console_exe(target
    #     CPPS file1.cpp file2.cpp ...
    #     HEADERS file1.hpp file2.hpp ...
    #     LIBS lib1 lib2 ...
    # )
    set(options)
    set(oneValueArgs)
    set(multiValueArgs CPPS HEADERS LIBS)
    cmake_parse_arguments(T "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_executable(${name} ${T_CPPS} ${T_HEADERS})
    set_property(TARGET ${name} PROPERTY GB_HOSTED_LINK 1)
    target_compile_definitions(${name} PRIVATE IO_HAS_STD=1)

    if(T_LIBS)
        target_link_libraries(${name} PRIVATE ${T_LIBS})
    endif()

    gb_apply_common(${name})
    gb_apply_debug(${name})
    gb_console(${name})

    if(MSVC)
        target_link_libraries(${name} PRIVATE user32 gdi32 opengl32 kernel32 bcrypt ws2_32)
    endif()
endfunction()

function(gb_add_gui_exe name)
    # usage:
    # gb_add_gui_exe(target
    #     CPPS file1.cpp file2.cpp ...
    #     HEADERS file1.hpp file2.hpp ...
    #     LIBS lib1 lib2 ...
    # )
    set(options)
    set(oneValueArgs)
    set(multiValueArgs CPPS HEADERS LIBS)
    cmake_parse_arguments(T "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_executable(${name} ${T_CPPS} ${T_HEADERS})
    set_property(TARGET ${name} PROPERTY GB_HOSTED_LINK 1)

    target_compile_definitions(${name} PRIVATE
        GB_GUI_APP=1
    )

    if(T_LIBS)
        target_link_libraries(${name} PRIVATE ${T_LIBS})
    endif()

    gb_apply_common(${name})
    gb_apply_freestanding(${name})
    gb_apply_debug(${name})

    # GUI target: console define only where needed, and no-console subsystem for *NoConsole configs
    gb_console(${name})

    if(MSVC)
        target_link_options(${name} PRIVATE
            $<$<CONFIG:ReleaseNoConsole>:/SUBSYSTEM:WINDOWS>
            $<$<CONFIG:ReleaseNoConsole>:/ENTRY:WinMain>
            $<$<CONFIG:ReleaseMiniNoConsole>:/SUBSYSTEM:WINDOWS>
            $<$<CONFIG:ReleaseMiniNoConsole>:/ENTRY:WinMain>
        )
    endif()

    if(UNIX AND NOT APPLE)
        target_link_libraries(${name} PRIVATE
            $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:X11 GL>
        )
    endif()
endfunction()

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------

set(GB_HEADERS_SERVER
    "src/game/server/server_app.hpp"
    "src/game/server/server_bootstrap.hpp"
    "src/game/server/app/server_app_members.hpp"
    "src/game/server/app/server_app_inventory.hpp"
    "src/game/server/app/server_app_world_sim_sand.hpp"
    "src/game/server/app/server_app_fluid_and_gameplay.hpp"
    "src/game/server/app/server_app_runtime_pipeline.hpp"
    "src/game/server/app/server_app_tick_and_callbacks.hpp"
    "src/game/server/chunk/server_stream_budget.hpp"
    "src/game/server/chunk/server_stream_policy.hpp")
set(GB_SOURCES_SERVER
    "src/game/server/main.cpp")

set(GB_HEADERS_CLIENT
    # core
    "src/engine/core/camera.hpp"
    "src/engine/core/key_bindings.hpp"
    "src/engine/core/modeller.hpp"
    # render
    "src/game/client/render/client_defs.hpp"
    "src/game/client/render/client_ui.hpp"
    "src/game/client/render/window.hpp"
    "src/game/client/render/window_world.hpp"
    "src/game/client/render/ui_common.hpp"
    "src/game/client/render/ui_contexts.hpp"
    "src/game/client/render/ui_resources.hpp"
    "src/game/client/render/scene.hpp"
    "src/game/client/render/ui.hpp"
    "src/game/shared/world/item.hpp"
    # state
    "src/game/client/state/state_session_actor.hpp"
    "src/game/client/state/state_storage_queue.hpp"
    "src/game/client/state/state_network.hpp"
    # chunk/player/world/context modules
    "src/game/client/chunk/request.hpp"
    "src/game/client/chunk/frustum.hpp"
    "src/game/client/chunk/mesh_job.hpp"
    "src/game/client/player/block_build_destroy.hpp"
    "src/game/client/player/camera.hpp"
    "src/game/client/player/collider.hpp"
    "src/game/client/world/world_common.hpp"
    "src/game/client/world/world_chunk_cache.hpp"
    "src/game/client/world/world_uv_far.hpp"
    "src/game/client/world/world_runtime.hpp"
    "src/game/client/world/world_entity_sky.hpp"
    "src/game/client/context/main_menu.hpp"
    "src/game/client/context/settings.hpp"
    "src/game/client/context/graphics.hpp"
    "src/game/client/context/key_bindings.hpp"
    "src/game/client/context/multiplayer.hpp"
    "src/game/client/context/connecting.hpp"
    "src/game/client/context/in_game.hpp"
    "src/game/client/context/in_game_dead.hpp")
set(GB_SOURCES_CLIENT
    "src/game/client/main.cpp")

set(GB_HEADERS_TEST_TEXTURE_ATLAS
    "src/engine/core/config.hpp"
    "src/engine/core/resource_manager.hpp")

set(GB_SOURCES_TEST_TEXTURE_ATLAS
    "src/game/client/tests/test_texture_atlas.cpp")

set(GB_HEADERS_TEST_CHUNK_CLIENT
    "src/game/shared/net/protocol.hpp"
    "src/game/shared/world/chunk_stream.hpp")

set(GB_SOURCES_TEST_CHUNK_CLIENT
    "src/game/client/tests/test_chunk_client.cpp")

set(GB_HEADERS_TEST_MODELLER
    "src/engine/core/modeller.hpp"
    "src/engine/core/resource_manager.hpp")

set(GB_SOURCES_TEST_MODELLER
    "src/game/client/tests/test_modeller.cpp")

set(GB_HEADERS_TEST_SERVER_CHAOS_CLIENT
    "src/game/shared/net/protocol.hpp")

set(GB_SOURCES_TEST_SERVER_CHAOS_CLIENT
    "src/game/server/tests/test_server_chaos_client.cpp")

# -----------------------------------------------------------------------------
# Freestanding GUI Applications.
# (On Linux it's still hosted binary because of X11, pthread linkage)
# -----------------------------------------------------------------------------

# server
gb_add_console_exe(server
    CPPS ${GB_SOURCES_SERVER}
    HEADERS ${GB_HEADERS_SERVER}
    LIBS
)

if(MSVC)
    # ReleaseMini freestanding x86 disables stack probes; server hot paths
    # (net/event loop + world/chunk simulation) can use large frames.
    # Commit a larger stack upfront to avoid guard-page jump AVs (C0000005).
    target_link_options(server PRIVATE
        $<$<CONFIG:ReleaseMini>:/STACK:4194304,4194304>
        $<$<CONFIG:ReleaseNoConsole>:/STACK:4194304,4194304>
        $<$<CONFIG:ReleaseMiniNoConsole>:/STACK:4194304,4194304>
    )
endif()

# client
gb_add_gui_exe(client
    CPPS ${GB_SOURCES_CLIENT}
    HEADERS ${GB_HEADERS_CLIENT}
    LIBS
)

if(MSVC)
    target_link_options(client PRIVATE
        $<$<CONFIG:ReleaseMini>:/STACK:4194304>
        $<$<CONFIG:ReleaseMiniNoConsole>:/STACK:4194304>
    )
endif()


# texture atlas validation (PASS1/PASS2)
gb_add_hosted_console_exe(test_texture_atlas
    CPPS ${GB_SOURCES_TEST_TEXTURE_ATLAS}
    HEADERS ${GB_HEADERS_TEST_TEXTURE_ATLAS}
    LIBS
)
set_target_properties(test_texture_atlas PROPERTIES
    EXCLUDE_FROM_ALL TRUE
    EXCLUDE_FROM_DEFAULT_BUILD_ReleaseMini TRUE
    EXCLUDE_FROM_DEFAULT_BUILD_ReleaseNoConsole TRUE
    EXCLUDE_FROM_DEFAULT_BUILD_ReleaseMiniNoConsole TRUE
)

# client net chunk streaming validation (reliable request + part reassembly)
gb_add_console_exe(test_chunk_client
    CPPS ${GB_SOURCES_TEST_CHUNK_CLIENT}
    HEADERS ${GB_HEADERS_TEST_CHUNK_CLIENT}
    LIBS
)

gb_add_console_exe(test_modeller
    CPPS ${GB_SOURCES_TEST_MODELLER}
    HEADERS ${GB_HEADERS_TEST_MODELLER}
    LIBS
)

gb_add_hosted_console_exe(test_server_chaos_client
    CPPS ${GB_SOURCES_TEST_SERVER_CHAOS_CLIENT}
    HEADERS ${GB_HEADERS_TEST_SERVER_CHAOS_CLIENT}
    LIBS
)
target_compile_definitions(test_server_chaos_client PRIVATE
    $<$<OR:$<CONFIG:Release>,$<CONFIG:ReleaseMini>,$<CONFIG:ReleaseNoConsole>,$<CONFIG:ReleaseMiniNoConsole>>:NDEBUG;_ITERATOR_DEBUG_LEVEL=0>
)
if(MSVC)
    # Hosted stress tool must always use CRT in every config (including ReleaseMini).
    set_property(TARGET test_server_chaos_client PROPERTY MSVC_RUNTIME_LIBRARY
        "$<IF:$<CONFIG:Debug>,MultiThreadedDebugDLL,MultiThreadedDLL>")
    target_link_libraries(test_server_chaos_client PRIVATE ucrt vcruntime msvcrt)
endif()
set_target_properties(test_server_chaos_client PROPERTIES
    EXCLUDE_FROM_ALL TRUE
    EXCLUDE_FROM_DEFAULT_BUILD_ReleaseMini TRUE
    EXCLUDE_FROM_DEFAULT_BUILD_ReleaseNoConsole TRUE
    EXCLUDE_FROM_DEFAULT_BUILD_ReleaseMiniNoConsole TRUE
)

message(STATUS "Configured ${PROJECT_NAME} (C++${CMAKE_CXX_STANDARD})")
message(STATUS "Freestanding configs: ${FREESTANDING_CONFIGS}")
