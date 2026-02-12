#pragma once

#include <memory>

#include "worldview.hpp"

#include "gfx/draw_list.hpp"
#include "gfx/renderer.hpp"
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
    void Draw(gfx::DrawList& dlist, gfx::DrawListParams& params);

    const WorldView* GetWorld() const { return world_.get(); } 

    void GetViewInfo(glm::vec3& eye, glm::mat4& view) const;

    audio::Master& GetAudioMaster() const;

private:
    // msg handlers
    bool ProcessWorldMsg(net::InMessage& msg);
    bool ProcessCameraMsg(net::InMessage& msg);
    bool ProcessChatMsg(net::InMessage& msg);

    void DrawWorld(gfx::DrawList& dlist, gfx::DrawListParams& params);
    void SendViewAngles(float time);

private:
    App& app_;
    
    std::unique_ptr<WorldView> world_;

    float yaw_ = 0.0f, pitch_ = 0.0f;
    net::EntNum follow_ent_ = 0;

    net::ViewYawQ view_yaw_q_;
    net::ViewPitchQ view_pitch_q_;
    float last_send_time_ = 0.0f;
};

} // namespace game::view