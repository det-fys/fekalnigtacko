#include "cow.hpp"
#include "utils/random.hpp"

static game::CharacterTuning GetCowTuning()
{
    game::CharacterTuning ct{};
    ct.shape = game::CharacterShape(0.3f, 0.75f);
    ct.model_name = "cow";
    return ct;
}

game::Cow::Cow(World& world, const glm::vec3& position, float yaw) : Animal(world, GetCowTuning(), position, yaw)
{
    walk_speed_ = 3.0f;
    turn_speed_ = 3.0f;

    SetUseMessage("vlízt na krávu");

    AddAnimalSeat(glm::vec3(0.0f, 0.098926f, 0.447576f));
    AddAnimalSeat(glm::vec3(0.0f, -0.394027f, 0.458536f));

    SetIdleAnim("idle");
    SetWalkAnim("walk");

    ScheduleRandomMoo();
}

void game::Cow::OnPassengerChanged(size_t seat_idx, HumanCharacter* passenger)
{
    Super::OnPassengerChanged(seat_idx, passenger);
    
    if (passenger)
    {
        PlayUseSound();
    }
}

void game::Cow::ScheduleRandomMoo()
{
    Schedule(rand() % 15000 + 5000, [this]() {
        PlayRandomMoo();
        ScheduleRandomMoo();
    });
}

void game::Cow::PlayRandomMoo()
{
    float volume = RandomFloat(0.9f, 1.1f);
    float pitch = RandomFloat(0.9f, 1.0f);

    switch (rand() % 4)
    {
    case 0:
        PlaySound("cow-01", volume, pitch);
        break;
    case 1:
        PlaySound("cow-02", volume, pitch);
        break;
    case 2:
        PlaySound("cow-04", volume, pitch);
        break;
    case 3:
        PlaySound("cow-05", volume, pitch);
        break;
    }
}

void game::Cow::PlayUseSound()
{
    PlaySound("cow-06", 1.0f, RandomFloat(0.9f, 1.1f));
}
