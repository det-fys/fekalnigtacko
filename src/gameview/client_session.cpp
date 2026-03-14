#include "client_session.hpp"

#include "client/app.hpp"
#include <iostream>
// #include <glm/gtx/common.hpp>

#include "vehicleview.hpp"
#include "utils/version.hpp"

game::view::ClientSession::ClientSession(App& app) : app_(app)
{
	// send login
	auto msg = BeginMsg(net::MSG_ID);
    msg.Write<net::Version>(FEKAL_VERSION);
	msg.Write(net::PlayerName(app.GetUserName()));
}

bool game::view::ClientSession::ProcessMessage(net::InMessage& msg)
{
    while (true)
    {
        net::MessageType type = net::MSG_NONE;
        if (!msg.Read(type))
            return true;

        if (type == net::MSG_NONE || type >= net::MSG_COUNT)
            return false;

        if (!ProcessSingleMessage(type, msg))
            return false;
    }
}

bool game::view::ClientSession::ProcessSingleMessage(net::MessageType type, net::InMessage& msg)
{
    MSGDEBUG(std::cout << "[MSG] received " << (uint32_t)type << std::endl;)

    switch (type)
    {
    case net::MSG_CHWORLD:
        return ProcessWorldMsg(msg);

    case net::MSG_CAM:
        return ProcessCameraMsg(msg);

    case net::MSG_CHAT:
        return ProcessChatMsg(msg);

    default:
        // try pass the msg to world
        if (world_ && world_->ProcessMsg(type, msg))
            return true;

        return false;
    }
}

void game::view::ClientSession::Input(game::PlayerInputType in, bool pressed, bool repeated)
{


    if (repeated)
        return;

    SendInput(in, pressed);

}

void game::view::ClientSession::ProcessMouseMove(float delta_yaw, float delta_pitch)
{
    yaw_ = glm::mod(yaw_ + delta_yaw, glm::two_pi<float>());

    pitch_ += delta_pitch;
    // Clamp pitch to avoid gimbal lock
    if (pitch_ > glm::radians(89.0f))
    {
        pitch_ = glm::radians(89.0f);
    }
    else if (pitch_ < glm::radians(-89.0f))
    {
        pitch_ = glm::radians(-89.0f);
    }
}

void game::view::ClientSession::Update(const UpdateInfo& info)
{
    if (world_)
    {
        world_->Update(info);
        SendViewAngles(info.time);
    }
}

void game::view::ClientSession::Draw(gfx::DrawList& dlist, gfx::DrawListParams& params, gui::Context& gui)
{
    if (world_)
    {
        DrawWorld(dlist, params, gui);
    }
}

void game::view::ClientSession::GetViewInfo(glm::vec3& eye, glm::mat4& view) const
{
    glm::vec3 start(0.0f, 0.0f, 2.0f);
    float distance = 5.0f;

    if (follow_ent_)
    {
        auto ent = world_->GetEntity(follow_ent_);
        if (ent)
        {
            start += ent->GetRoot().GetGlobalPosition();
        
            if (dynamic_cast<const VehicleView*>(ent))
                distance = 8.0f;
        }
    }

    float yaw_cos = glm::cos(yaw_);
    float yaw_sin = glm::sin(yaw_);
    float pitch_cos = glm::cos(pitch_);
    float pitch_sin = glm::sin(pitch_);
    glm::vec3 dir(yaw_cos * pitch_cos, yaw_sin * pitch_cos, pitch_sin);

    glm::vec3 end = start - dir * distance;

    //start.z -= 0.5f; // shift this a bit to make it better when occluded
    eye = world_->CameraSweep(start, end);
    view = glm::lookAt(eye, eye + dir, glm::vec3(0, 0, 1));
}

audio::Master& game::view::ClientSession::GetAudioMaster() const
{
    return app_.GetAudioMaster();
}

bool game::view::ClientSession::ProcessWorldMsg(net::InMessage& msg)
{
    try
    {
        world_ = std::make_unique<WorldView>(*this, msg);
    }
    catch (const EntityInitError&)
    {
        return false;
    }

    return true;
}

bool game::view::ClientSession::ProcessCameraMsg(net::InMessage& msg)
{
    if (!msg.Read(follow_ent_))
        return false;

    return true;
}

bool game::view::ClientSession::ProcessChatMsg(net::InMessage& msg)
{
    net::ChatMessage chatm;
    if (!msg.Read(chatm))
        return false;

    app_.AddChatMessagePrefix("Server", chatm);
    return true;
}

void game::view::ClientSession::DrawWorld(gfx::DrawList& dlist, gfx::DrawListParams& params, gui::Context& gui)
{
    // glm::mat4 view = glm::lookAt(glm::vec3(15.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, -13.0f), glm::vec3(0.0f,
    // 0.0f, 1.0f));
    float aspect = static_cast<float>(params.screen_width) / static_cast<float>(params.screen_height);

    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 3000.0f);
    glm::vec3 eye;
    glm::mat4 view;
    GetViewInfo(eye, view);

    params.view_proj = proj * view;
    params.cam_pos = eye;

    // glm::mat4 fake_view_proj = glm::perspective(glm::radians(30.0f), aspect, 0.1f, 3000.0f) * view;

    game::view::DrawArgs draw_args(dlist, gui, params.view_proj, eye, glm::ivec2(params.screen_width, params.screen_height), 500.0f);
    world_->Draw(draw_args);

    glm::mat4 camera_world = glm::inverse(view);
    GetAudioMaster().SetListenerOrientation(camera_world);
}

void game::view::ClientSession::SendInput(game::PlayerInputType type, bool enable)
{
    auto msg = BeginMsg(net::MSG_IN);
	uint8_t val = type;
	if (enable)
		val |= 128;
	msg.Write(val);
}

void game::view::ClientSession::SendViewAngles(float time)
{
    if (time - last_send_time_ < 0.040f)
        return;

    net::ViewYawQ yaw_q;
    net::ViewPitchQ pitch_q;
    yaw_q.Encode(yaw_);
    pitch_q.Encode(pitch_);

    if (yaw_q.value == view_yaw_q_.value && pitch_q.value == view_pitch_q_.value)
        return;

    auto msg = BeginMsg(net::MSG_VIEWANGLES);
    msg.Write(yaw_q.value);
    msg.Write(pitch_q.value);

    view_yaw_q_.value = yaw_q.value;
    view_pitch_q_.value = pitch_q.value;
    last_send_time_ = time;
}
