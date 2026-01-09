#pragma once

#include "master.hpp"
#include <memory>

namespace audio
{

class Player;

class Source
{
protected:
    Source(const std::string& category_name, Player* player);

public:
    DELETE_COPY_MOVE(Source)

    void SetPlay(bool play) { should_play_ = play; }
    void Play() { SetPlay(true); }
    void Stop() { SetPlay(false); }

    void SetDeleteOnFinish(bool destroy) { delete_on_finish_ = destroy; }

    void SetPosition(const glm::vec3& position);
    void SetVelocity(const glm::vec3& velocity);
    void AttachToPosition(const glm::vec3* position);
    void SetRelativeToListener(bool relative);

    virtual void SetLooping(bool looping) = 0;
    virtual void SetPitch(float pitch) = 0;
    virtual void SetVolume(float volume) = 0;

    virtual void Update();

    bool ShouldBeDeleted() { return finished_ && delete_on_finish_; }

    void Delete();

    virtual ~Source();

protected:
    void SetSourceVolume(float volume);
    void SetSourcePitch(float pitch);

    unsigned int source_ = 0;

    const glm::vec3* attach_position_ = nullptr;
    bool should_play_ = true; // auto play when created
    bool finished_ = false;
    bool delete_on_finish_ = true; // auto delete when finished

private:
    void UpdateVolume(); // on this or category volume change

    friend class Category;
    Category* category_ = nullptr;
    Source** cat_prev_next_ = nullptr;
    Source* cat_next_ = nullptr;

    friend class Player;
    Player* player_ = nullptr;
    std::unique_ptr<Source>* player_prev_next_ = nullptr;
    std::unique_ptr<Source> player_next_ = nullptr;

    float volume_ = 1.0f; // volume set by source
};

} // namespace audio