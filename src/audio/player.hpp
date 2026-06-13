#pragma once

#include <memory>

#include "master.hpp"
#include "sound_source.hpp"

namespace audio
{

class Player
{
public:
    Player(Master& master);
    DELETE_COPY_MOVE(Player)

    SoundSource* PlaySound(const std::shared_ptr<const Sound>& sound, const game::TransformNode* attach_node);

    void Update();

    Master& GetMaster() const { return master_; }

    ~Player() = default;

private:
    Master& master_;

    std::unique_ptr<Source> first_source_;
    friend class Source;
};

} // namespace audio