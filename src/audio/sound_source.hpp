#pragma once

#include "sound.hpp"
#include "source.hpp"

namespace audio
{

class SoundSource : public Source
{

public:
    SoundSource(Player* player, std::shared_ptr<const Sound> sound);

    virtual void SetLooping(bool looping) override;
    virtual void SetPitch(float pitch) override;
    virtual void SetVolume(float volume) override;

    virtual void Update() override;

    virtual ~SoundSource() override;

private:
    using Super = Source;

    std::shared_ptr<const Sound> sound_;
};

} // namespace audio