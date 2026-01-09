#include "sound_source.hpp"

#include <AL/al.h>
#include <AL/alc.h>

audio::SoundSource::SoundSource(Player* player, std::shared_ptr<const Sound> sound)
    : Super(sound->GetCategoryName(), player), sound_(std::move(sound))
{
    SetVolume(1.0f);
    SetPitch(1.0f);

    alSourcei(source_, AL_BUFFER, sound_->GetBufferId());
}

void audio::SoundSource::SetLooping(bool looping)
{
    alSourcei(source_, AL_LOOPING, looping ? AL_TRUE : AL_FALSE);
    // TsrDebugf(DML_2, "Set source %p looping to %s\n", this, looping ? "true" : "false");
}

void audio::SoundSource::SetPitch(float pitch)
{
    Super::SetSourcePitch(sound_->GetPitch() * pitch);
}

void audio::SoundSource::SetVolume(float volume)
{
    Super::SetSourceVolume(sound_->GetVolume() * volume);
}

void audio::SoundSource::Update()
{
    Super::Update();

    ALint state;
    alGetSourcei(source_, AL_SOURCE_STATE, &state);

    finished_ = state == AL_STOPPED;

    switch (state)
    {
    case AL_PLAYING: {
        if (!should_play_)
            alSourcePause(source_);

        break;
    }

    case AL_INITIAL:
    case AL_STOPPED:
    case AL_PAUSED: {
        if (should_play_)
            alSourcePlay(source_);

        break;
    }

    default:
        break;
    }
}

audio::SoundSource::~SoundSource()
{
    alSourceStop(source_);
    alSourcei(source_, AL_BUFFER, 0);
}
