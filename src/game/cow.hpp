#pragma once

#include "animal.hpp"

namespace game
{

class Cow : public Animal
{
public:
    using Super = Animal;

    Cow(World& world, const glm::vec3& position, float yaw);

protected:
    virtual void OnPassengerChanged(size_t seat_idx, HumanCharacter* passenger) override;

    virtual void MakeSound() override;
    virtual void MakeHurtSound() override;

private:
    void PlayRandomMoo();
    void PlayUseSound();


};


}