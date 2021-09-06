#include <sstream>
#include <windows.h>
#include "handmade_hero_config.h"

std::string version();

struct alignas(32) projectile {
    // These are the members of this structure
    char unsigned is_on_fire;
    int damage;
    int particles_per_second;
    short how_many;
} hello[3];     // this define an array of projectiles right after the definition

int WinMain(HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow)
{
    unsigned short test;

    hello[1].is_on_fire = true;

    projectile project;
    project.is_on_fire = 1;
    project.damage = 100;
    project.particles_per_second = 200;
    project.how_many = 4;
    
    size_t char_size = sizeof(char);
    size_t int_size = sizeof(int);
    size_t short_size = sizeof(short);
    
    size_t projectile_size = sizeof(projectile);    // 16?


}

std::string version()
{
    std::stringstream ss;
    ss << "Handmade Hero VERSION "
       << HANDMADE_HERO_VERSION_MAJOR << '.'
       << HANDMADE_HERO_VERSION_MINOR << '.'
       << HANDMADE_HERO_VERSION_PATCH << std::endl;
    return ss.str();
}