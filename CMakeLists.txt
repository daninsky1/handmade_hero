cmake_minimum_required(VERSION 3.16)

project(HandmadeHero
        VERSION 0.1.0
        LANGUAGES C CXX)
configure_file(handmade_hero_config.h.in handmade_hero_config.h)
set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        ON)

# warnings
if(MSVC)
    add_compile_options(
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

else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# The game'll will stand in its own lib independently of the platform layer
add_library(handmade_hero_lib
    src/handmade_hero.cpp src/handmade_hero.h)

if(MSVC)
    add_executable(win32_handmade_hero
        src/win32_handmade_hero.cpp src/win32_handmade_hero.h)

    target_compile_definitions(win32_handmade_hero
        PUBLIC WIN32_HANDMADE
        PRIVATE $<$<CONFIG:Debug>:DEVELOPER_BUILD=1>
        PRIVATE $<$<CONFIG:Debug>:DEVELOPER_PERFORMANCE=1>
        
        PRIVATE $<$<CONFIG:Release>:DEVELOPER_BUILD=0>
        PRIVATE $<$<CONFIG:Release>:DEVELOPER_PERFORMANCE=0>)
  
    target_include_directories(win32_handmade_hero
                               PRIVATE ${PROJECT_BINARY_DIR})
    target_link_libraries(win32_handmade_hero
                          handmade_hero_lib
                          Xinput.lib
                          Winmm.lib)

#    set_target_properties(handmade_hero_app PROPERTIES
#        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/data/")
endif()
