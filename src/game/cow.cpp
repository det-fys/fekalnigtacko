#include "cow.hpp"

static game::CharacterTuning GetCowTuning()
{
    game::CharacterTuning ct{};
    ct.shape = game::CharacterShape(0.3f, 0.75f);
    ct.model_name = "cow";
    return ct;
}

game::Cow::Cow(World& world, const glm::vec3& position, float yaw) : Animal(world, GetCowTuning(), position, yaw)
{
    SetUseMessage("vlízt na krávu");

    AddAnimalSeat(glm::vec3(0.0f, -0.098926f, 0.447576f));
    AddAnimalSeat(glm::vec3(0.0f, 0.394027f, 0.458536f));

    SetIdleAnim("idle");
    SetWalkAnim("walk");
}
