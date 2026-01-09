#include "source.hpp"

#include <stdexcept>

#include <AL/al.h>
#include <AL/alc.h>

#include "defs.hpp"
#include "player.hpp"

audio::Source::Source(const std::string& category_name, Player* player)
{
    alGenSources(1, &source_);

    if (!source_)
        throw std::runtime_error("Failed to create audio source");

    // link to category
    category_ = player->GetMaster().GetCategory(category_name);
    cat_next_ = category_->first_source_;
    category_->first_source_ = this;
    cat_prev_next_ = &category_->first_source_;
    if (cat_next_)
        cat_next_->cat_prev_next_ = &cat_next_;

    // link to player
    player_ = player;
    player_next_ = std::move(player->first_source_);
    player->first_source_.reset(this);
    player_prev_next_ = &player->first_source_;
    if (player_next_)
        player_next_->player_prev_next_ = &player_next_;

    // TsrDebugf(DML_2, "Created audio source %p in category '%s'\n", this, category->GetName().c_str());

    // alSourcef(m_source, AL_ROLLOFF_FACTOR, 0.0f); // disable rolloff
    // alSourcef(m_source, AL_REFERENCE_DISTANCE, 1.0f); // set reference distance
}

void audio::Source::SetPosition(const glm::vec3& position)
{
    alSource3f(source_, AL_POSITION, position.x, position.y, position.z);
}

void audio::Source::SetVelocity(const glm::vec3& velocity)
{
    alSource3f(source_, AL_VELOCITY, velocity.x, velocity.y, velocity.z);
}

void audio::Source::AttachToPosition(const glm::vec3* position)
{
    attach_position_ = position;
    if (attach_position_)
        SetPosition(*attach_position_);
    // TsrDebugf(DML_2, "Attached source %p to position %p\n", this, position);
}

void audio::Source::SetRelativeToListener(bool relative)
{
    if (relative)
    {
        alSourcei(source_, AL_SOURCE_RELATIVE, AL_TRUE);
        // TsrDebugf(DML_2, "Set source %p to be relative to listener\n", this);
    }
    else
    {
        alSourcei(source_, AL_SOURCE_RELATIVE, AL_FALSE);
        // TsrDebugf(DML_2, "Set source %p to be absolute\n", this);
    }
}

void audio::Source::Update()
{
    if (attach_position_)
        SetPosition(*attach_position_);
}

void audio::Source::Delete()
{
    // unlink from player (causes destruction)
    if (player_next_)
    {
        player_next_->player_prev_next_ = player_prev_next_;
    }
    std::unique_ptr<Source>& player_prev_next = *player_prev_next_;
    player_prev_next = std::move(player_next_);

    // TsrDebugf(DML_2, "Deleted audio source %p\n", this);
}

void audio::Source::SetSourceVolume(float volume)
{
    volume_ = volume;
    UpdateVolume();
}

void audio::Source::SetSourcePitch(float pitch)
{
    alSourcef(source_, AL_PITCH, pitch);
    // TsrDebugf(DML_2, "Set source %p pitch to %.2f\n", this, pitch);
}

void audio::Source::UpdateVolume()
{
    float cat_volume = category_->GetVolume();
    float result_volume = volume_ * cat_volume;
    alSourcef(source_, AL_GAIN, result_volume);
    // TsrDebugf(DML_2, "Set source %p volume to %.2f (this %.2f, category %.2f)\n", this, result_volume, m_volume,
    // cat_volume);
}

audio::Source::~Source()
{
    alDeleteSources(1, &source_);

    // unlink from category
    if (cat_next_)
    {
        cat_next_->cat_prev_next_ = cat_prev_next_;
    }
    *cat_prev_next_ = cat_next_;
}