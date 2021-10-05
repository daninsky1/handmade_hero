cmake_minimum_required(VERSION 3.16)

project(HandmadeHero
        VERSION 0.1.0
        LANGUAGES C CXX)
configure_file(handmade_hero_config.h.in handmade_hero_config.h)
set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        ON)

# TODO(daniel): PlatformDebug only generate debug info for the main platform
# code, this is to allow to compile non-platform code(mainly game code
# in this case(handmade_hero) at run-time of the platform and make it update
# dinamically) this is a work arround to the .pdb file lock that VS debuger
# imposes making it impossible or useless to use the debug information
if(CMAKE_CONFIGURATION_TYPES)
    if(NOT "DebugPlatform" IN_LIST CMAKE_CONFIGURATION_TYPES)
        list(APPEND CMAKE_CONFIGURATION_TYPES DebugPlatform)
        set(CMAKE_SHARED_LINKER_FLAGS_DEBUGPLATFORM ${CMAKE_SHARED_LINKER_FLAGS_RELEASE})
        set(CMAKE_EXE_LINKER_FLAGS_DEBUGPLATFORM ${CMAKE_EXE_LINKER_FLAGS_RELEASE})
    endif()
else()
    set(BuildTypes Debug Release DebugPlatform)
    set(CACHE CMAKE_BUILD_TYPE PROPERTY
        STRINGS "${BuildTypes}")
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
    elseif(NOT CMAKE_BUILD_TYPE IN_LIST BuildTypes)
        message(FATAL_ERROR "Invalid build type: ${CMAKE_BUILD_TYPE}")
    endif()
endif()

# The main directory of the project
set(DATA_DIRECTORY "${CMAKE_SOURCE_DIR}/data/")

# warnings
if(WIN32)
    if(MSVC)
        add_compile_options(
            $<$<CONFIG:Debug>:/ZI>
            /FC
            -Wall   # enable all warnings
            /MP         # enable multiprocessor compilation
            # Desabled Warnings: 
            -wd28251    # SAL annotation warnings
            -wd4820		# "bytes" paddind added after "member_name" warning 4
            -wd4201		# nonstandard extension used: nameless struct/union
            -wd4668		# "symbol" is not defined as a preprocessor macro, replacing with "0" for "directives"
            -wd5045		# /Qspectre information about the code that can be changed by this command
            -wd5039		# this function may throw an exception
            -wd4514     # "function" unreferenced inline function has been removed
        )
        add_link_options(
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugPlatform>>:/MAP>
        )
    endif()

    add_compile_definitions(
        PRIVATE HANDMADE_WIN32_BUILD
        PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugPlatform>>:HANDMADE_DEVELOPER_BUILD=1>
        PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugPlatform>>:HANDMADE_DEVELOPER_PERFORMANCE=1>
        
        PRIVATE $<$<CONFIG:Release>:HANDMADE_DEVELOPER_BUILD=0>
        PRIVATE $<$<CONFIG:Release>:HANDMADE_DEVELOPER_PERFORMANCE=0>
    )

else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# The game'll will stand in its own lib independently of the platform layer
# HANDMADE_HERO
add_library(handmade_hero SHARED
    src/handmade_hero.cpp src/handmade_hero.h)

if(MSVC)
    set_target_properties(handmade_hero PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:handmade_hero>")
    # NOTE(daniel): target_link_option need to be after the set_target_properties
    # to not to rewrite the configuration
    target_link_options(handmade_hero
        PRIVATE /EXPORT:game_update_and_render
        PRIVATE /EXPORT:game_get_sound_samples
    )

# WIN32_HANDMADE_HERO
    add_executable(win32_handmade_hero WIN32
        src/win32_handmade_hero.cpp src/win32_handmade_hero.h)

    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT win32_handmade_hero)
    set_target_properties(win32_handmade_hero PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:win32_handmade_hero>")

    target_compile_options(win32_handmade_hero
        PRIVATE $<$<CONFIG:DebugPlatform>:/ZI>
        PRIVATE $<$<CONFIG:DebugPlatform>:/Od>
        PRIVATE $<$<CONFIG:DebugPlatform>:/Ob0>
        PRIVATE $<$<CONFIG:DebugPlatform>:/RTC1>)
    target_link_options(win32_handmade_hero
        PRIVATE $<$<CONFIG:DebugPlatform>:/DEBUG>)
    set(win32_link)
    get_target_property(win32_link win32_handmade_hero COMPILE_OPTIONS)
    message("${CMAKE_SHARED_COMPILE_FLAGS_DEBUG}")
    target_include_directories(win32_handmade_hero
                               PRIVATE ${PROJECT_BINARY_DIR})
                               
    target_link_libraries(win32_handmade_hero
                          Winmm.lib)
    add_dependencies(win32_handmade_hero
        handmade_hero)
elseif(UNIX)
    # TODO: Make unix build
endif()
