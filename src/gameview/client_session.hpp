#pragma once

#include <memory>

#include "worldview.hpp"

#include "gfx/draw_list.hpp"
#include "net/defs.hpp"
#include "net/inmessage.hpp"

class App;

namespace game::view
{

class ClientSession
{
public:
    ClientSession(App& app);

    bool ProcessMessage(net::InMessage& msg);
    bool ProcessSingleMessage(net::MessageType type, net::InMessage& msg);

    void ProcessMouseMove(float delta_yaw, float delta_pitch);

    void Update(const UpdateInfo& info);

    const WorldView* GetWorld() const { return world_.get(); } 

    glm::mat4 GetViewMatrix() const;

private:
    // msg handlers
    bool ProcessWorldMsg(net::InMessage& msg);
    bool ProcessControlMsg(net::InMessage& msg);

private:
    App& app_;
    
    std::unique_ptr<WorldView> world_;

    float yaw_ = 0.0f, pitch_ = 0.0f;
    net::EntNum ctl_ = 0;

};

} // namespace game::view