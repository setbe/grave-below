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
    "")

set(GB_SOURCES_SERVER
    "src/game/server/main.cpp")

set(GB_HEADERS_CLIENT
    "src/engine/core/camera.hpp")

set(GB_SOURCES_CLIENT
    "src/game/client/main.cpp")

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

# client
gb_add_gui_exe(client
    CPPS ${GB_SOURCES_CLIENT}
    HEADERS ${GB_HEADERS_CLIENT}
    LIBS
)

message(STATUS "Configured ${PROJECT_NAME} (C++${CMAKE_CXX_STANDARD})")
message(STATUS "Freestanding configs: ${FREESTANDING_CONFIGS}")
