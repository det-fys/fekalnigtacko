#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <glm/glm.hpp>

namespace sv
{

struct Cfg
{
    uint16_t port = 11200;

    // broadphase
    std::string broadphase;
    glm::vec3 bp_bounds_min;
    glm::vec3 bp_bounds_max;




};

void LoadCfg();
const Cfg& GetCfg();

}