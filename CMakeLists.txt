cmake_minimum_required(VERSION 3.16)

project(handmade_hero
        VERSION 0.1.0
        LANGUAGES C CXX)
configure_file(handmade_hero_config.h.in handmade_hero_config.h)

if(MSVC)
    add_compile_options(-W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(MAIN_APP_SOURCES src/main.cpp)
add_executable(handmade_hero_app WIN32 ${MAIN_APP_SOURCES})
set_property(TARGET handmade_hero_app PROPERTY CXX_STANDARD 17)
target_include_directories(handmade_hero_app
                           PRIVATE ${PROJECT_BINARY_DIR})

if(MSVC)
    target_compile_options(handmade_hero_app
        PRIVATE -wd28251    # disable SAL annotation warnings
        PRIVATE /MP)        # enable multiprocessor compilation
    target_link_libraries(handmade_hero_app
                          PRIVATE Xinput.lib
                          PRIVATE Winmm.lib)
endif()
