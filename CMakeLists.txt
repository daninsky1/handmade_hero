cmake_minimum_required(VERSION 3.16)

project(HandmadeHero
        VERSION 0.1.0
        LANGUAGES C CXX)
configure_file(handmade_hero_config.h.in handmade_hero_config.h)
set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        ON)

# TODO(daniel): Maybe do a DebugNobuild
# not finished
#if(CMAKE_CONFIGURATION_TYPES)
#    if(NOT "Daniel" IN_LIST CMAKE_CONFIGURATION_TYPES)
#        list(APPEND CMAKE_CONFIGURATION_TYPES DebugNoBuild)
#        set(CMAKE_SHARED_LINKER_FLAGS_DEBUGNOBUILD CMAKE_SHARED_LINKER_FLAGS_DEBUG)
#        set(CMAKE_EXE_LINKER_FLAGS_DEBUGNOBUILD CMAKE_EXE_LINKER_FLAGS_DEBUG)
#    endif()
#else()
#    set(BuildTypes Debug Release)
#    set(CACHE CMAKE_BUILD_TYPE PROPERTY
#        STRINGS "${BuildTypes}")
#    if(NOT CMAKE_BUILD_TYPE)
#        set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
#    elseif(NOT CMAKE_BUILD_TYPE IN_LIST BuildTypes)
#        message(FATAL_ERROR "Invalid build type: ${CMAKE_BUILD_TYPE}")
#    endif()
#endif()

# The main directory of the project
set(DATA_DIRECTORY "${CMAKE_SOURCE_DIR}/data/")

# warnings
if(WIN32)
    if(MSVC)
        add_compile_options(
            /FC
            -W4 -Wall   # enable all warnings
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
            $<$<CONFIG:Debug>:/MAP>
        )
    endif()

    add_compile_definitions(
        PRIVATE HANDMADE_WIN32_BUILD
        PRIVATE $<$<CONFIG:Debug>:HANDMADE_DEVELOPER_BUILD=1>
        PRIVATE $<$<CONFIG:Debug>:HANDMADE_DEVELOPER_PERFORMANCE=1>
        
        PRIVATE $<$<CONFIG:Release>:HANDMADE_DEVELOPER_BUILD=0>
        PRIVATE $<$<CONFIG:Release>:HANDMADE_DEVELOPER_PERFORMANCE=0>
    )

else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# The game'll will stand in its own lib independently of the platform layer
add_library(handmade_hero SHARED
    src/handmade_hero.cpp src/handmade_hero.h)

target_link_options(handmade_hero
    PRIVATE /EXPORT:game_update_and_render
    PRIVATE /EXPORT:game_get_sound_samples
)
set_target_properties(handmade_hero PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:handmade_hero>")

if(WIN32)
    add_executable(win32_handmade_hero WIN32
        src/win32_handmade_hero.cpp src/win32_handmade_hero.h)

    if(MSVC)
        set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT win32_handmade_hero)
        set_target_properties(win32_handmade_hero PROPERTIES
            VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:win32_handmade_hero>")
    endif()

    target_include_directories(win32_handmade_hero
                               PRIVATE ${PROJECT_BINARY_DIR})
    target_link_libraries(win32_handmade_hero
                          Winmm.lib)
    add_dependencies(win32_handmade_hero
        handmade_hero)
elseif(UNIX)
    # TODO: Make unix build
endif()
