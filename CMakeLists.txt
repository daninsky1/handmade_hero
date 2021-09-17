cmake_minimum_required(VERSION 3.16)

project(handmade_hero
        VERSION 0.1.0
        LANGUAGES C CXX)
configure_file(handmade_hero_config.h.in handmade_hero_config.h)

# warnings
if(MSVC)
    add_compile_options(-W4 -Wall)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(MAIN_APP_SOURCES
    src/win32_handmade_hero.cpp src/win32_handmade_hero.h
    src/handmade_hero.cpp src/handmade_hero.h)
add_executable(handmade_hero_app WIN32 ${MAIN_APP_SOURCES})
set_property(TARGET handmade_hero_app PROPERTY CXX_STANDARD 17)
target_include_directories(handmade_hero_app
                           PRIVATE ${PROJECT_BINARY_DIR})

if(MSVC)
    set_target_properties(handmade_hero_app PROPERTIES
                          VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/data/")
    target_compile_options(handmade_hero_app
        PRIVATE -wd28251    # disable SAL annotation warnings
		PRIVATE -wd4820		# disable "bytes" paddind added after "member_name" warning 4
		PRIVATE -wd4201		# disable nonstandard extension used: nameless struct/union
		PRIVATE -wd4668		# disable "symbol" is not defined as a preprocessor macro, replacing with "0" for "directives"
		PRIVATE -wd5045		# disable /Qspectre information about the code that can be changed by this command
		PRIVATE -wd5039		# disable this function may throw an exception
		PRIVATE -wd4514
        PRIVATE /MP)        # enable multiprocessor compilation
    # target definitions
    target_compile_definitions(handmade_hero_app
        PRIVATE $<$<CONFIG:Debug>:DEVELOPER_BUILD=1>
        PRIVATE $<$<CONFIG:Debug>:DEVELOPER_PERFORMANCE=1>)
    target_compile_definitions(handmade_hero_app
        PRIVATE $<$<CONFIG:Release>:DEVELOPER_BUILD=0>
        PRIVATE $<$<CONFIG:Release>:DEVELOPER_PERFORMANCE=0>)
    target_link_libraries(handmade_hero_app
                          PRIVATE Xinput.lib
                          PRIVATE Winmm.lib)
endif()
