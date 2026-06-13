#include "player.hpp"

audio::Player::Player(Master& master) : master_(master) {}

audio::SoundSource* audio::Player::PlaySound(const std::shared_ptr<const Sound>& sound,
                                             const game::TransformNode* attach_node)
{
    SoundSource* source = new SoundSource(this, sound);
    source->AttachToNode(attach_node);

    return source;
}

void audio::Player::Update()
{
    Source* current = first_source_.get();
    while (current)
    {
        current->Update();

        Source* next = current->player_next_.get();

        if (current->ShouldBeDeleted())
        {
            current->Delete();
        }

        current = next;
    }
}