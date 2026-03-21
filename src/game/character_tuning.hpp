#pragma once

#include <string>
#include <vector>

namespace game
{

struct CharacterShape
{
    float radius;
    float height;

    CharacterShape(float radius, float height) : radius(radius), height(height) {}
};

struct CharacterConfigClothes
{
    std::string name;
    uint32_t color = 0xFFFFFF;
};

struct CharacterTuning
{
    CharacterShape shape = CharacterShape(0.3f, 0.75f);
    std::vector<CharacterConfigClothes> clothes;
};


}