cmake_minimum_required(VERSION 3.16)

# HANDMADE HERO PROJECT
project(HandmadeHero
        VERSION 0.1.0
        LANGUAGES C CXX)
configure_file(handmade_hero_config.h.in handmade_hero_config.h)
set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        ON)

# NOTE(daniel): PlatformDebug only generate debug info for the main platform
# code, this is to allow to compile non-platform code(mainly game code
# in this case(handmade_hero) at run-time of the platform and make it update
# dinamically) this is a work arround to the .pdb file lock that VS debuger
# imposes making it impossible or useless to use the debug information
if(CMAKE_CONFIGURATION_TYPES)
    if(NOT "DebugPlatform" IN_LIST CMAKE_CONFIGURATION_TYPES)
        list(APPEND CMAKE_CONFIGURATION_TYPES DebugPlatform)
        set(CMAKE_EXE_LINKER_FLAGS_DEBUGPLATFORM "")
        set(CMAKE_SHARED_LINKER_FLAGS_DEBUGPLATFORM "")
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
#
# GLOBAL COMPILER DEFINITIONS
#
add_compile_definitions(
    PRIVATE HANDMADE_WIN32_BUILD
    PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugPlatform>>:HANDMADE_DEVELOPER_BUILD=1>
    PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugPlatform>>:HANDMADE_DEVELOPER_PERFORMANCE=1>
    
    PRIVATE $<$<CONFIG:Release>:HANDMADE_DEVELOPER_BUILD=0>
    PRIVATE $<$<CONFIG:Release>:HANDMADE_DEVELOPER_PERFORMANCE=0>
)
#
# RELEASE BIN DIRECTORY
#
set(DATA_DIRECTORY "${CMAKE_SOURCE_DIR}/data/")
#
# HANDMADE_HERO TARGET
#
add_library(handmade_hero SHARED
    src/handmade_hero.cpp)
target_include_directories(handmade_hero
    PUBLIC ${PROJECT_BINARY_DIR})


if(WIN32)
    #
    # WINDOWS GLOBAL COMPILER/LINKER FLAGS
    #
    if(MSVC)
        add_compile_options(
            /MP     # enable multiprocessor compilation
            /EHsc   # enable c++ exceptions
            /nologo # disable compiler logo

            # RELEASE FLAGS
            # x86
            # TODO(daniel): query the cpu arch before implement any AVX extension
            $<$<CONFIG:Release>>:arch:AVX>


            # DEBUG FLAGS
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugPlatform>>:/ZI>    # edit and continue debug info
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugPlatform>>:/Od>    # disable optimization
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugPlatform>>:/Ob1>   # only expand functions marked as inline
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugPlatform>>:/RTC1>
            # display full path in diagnostic text [to use QuickFix and others]
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugPlatform>>:/FC>
            # enable all warnings [overkill warnings]
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugPlatform>>:-Wall>
            # DESABLED WARNINGS: 
            # disable SAL annotation warnings
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugPlatform>>:-wd28251>
            # "bytes" paddind added after "member_name" warning 4
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugPlatform>>:-wd4820>
            # nonstandard extension used: nameless struct/union
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugPlatform>>:-wd4201>
            # "symbol" is not defined as a preprocessor macro, replacing with "0" for "directives"
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugPlatform>>:-wd4668>
            # /Qspectre information about the code that can be changed by this command
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugPlatform>>:-wd5045>
            # this function may throw an exception
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugPlatform>>:-wd5039>
            # "function" unreferenced inline function has been removed
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugPlatform>>:-wd4514>
        )
        add_link_options(
            $<$<OR:$<CONFIG:Debug>,$<CONFIG:DebugPlatform>>:/MAP>
        )
    endif()
    #
    # WIN32 MAIN TARGET [THE MAIN LOOP]
    #
    add_executable(win32_main WIN32
        src/win32_main.cpp)
    target_include_directories(win32_main
        PRIVATE ${PROJECT_BINARY_DIR})
    target_link_options(win32_main
        PRIVATE $<$<CONFIG:DebugPlatform>:/DEBUG>)

    #
    # VISUAL STUDIO CONFIGURATIONS
    #
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT win32_main)
    set_target_properties(win32_main PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:win32_main>")

    set(win32_link)
    get_target_property(win32_link win32_main COMPILE_OPTIONS)
    message("${CMAKE_SHARED_COMPILE_FLAGS_DEBUG}")
                               
    target_link_libraries(win32_main
                          Winmm.lib)
    add_dependencies(win32_main
        handmade_hero)

    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT win32_main)
    set_target_properties(win32_main PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:win32_main>")

    target_link_options(win32_main
        PRIVATE $<$<CONFIG:DebugPlatform>:/DEBUG>)
    set(win32_link)
    get_target_property(win32_link win32_main COMPILE_OPTIONS)
    message("${CMAKE_SHARED_COMPILE_FLAGS_DEBUG}")
    target_include_directories(win32_main
                               PRIVATE ${PROJECT_BINARY_DIR})
                               
    target_link_libraries(win32_main
                          Winmm.lib)
    add_dependencies(win32_main
        handmade_hero)


else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()


if(MSVC)
    set_target_properties(handmade_hero PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:handmade_hero>")
    # NOTE(daniel): target_link_option need to be after the set_target_properties
    # to not to rewrite the configuration
    target_link_options(handmade_hero
        PRIVATE /EXPORT:game_update_and_render
        PRIVATE /EXPORT:game_get_sound_samples
    )

elseif(UNIX)
    # TODO: Make unix build
endif()
