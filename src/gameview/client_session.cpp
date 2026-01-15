#include "client_session.hpp"

#include <iostream>
#include "client/app.hpp"
// #include <glm/gtx/common.hpp>

game::view::ClientSession::ClientSession(App& app) : app_(app) {}

bool game::view::ClientSession::ProcessMessage(net::InMessage& msg)
{
    while (true)
    {
        net::MessageType type = net::MSG_NONE;
        if (!msg.Read(type))
            return true;

        if (type == net::MSG_NONE || type >= net::MSG_COUNT)
            return false;

        ProcessSingleMessage(type, msg);
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

void game::view::ClientSession::ProcessMouseMove(float delta_yaw, float delta_pitch)
{
    yaw_ += delta_yaw;
    // yaw_ = glm::fmod(yaw_, 2.0f * glm::pi<float>());

    pitch_ += delta_pitch;
    // Clamp pitch to avoid gimbal lock
	if (pitch_ > glm::radians(89.0f)) {
		pitch_ = glm::radians(89.0f);
	} else if (pitch_ < glm::radians(-89.0f)) {
		pitch_ = glm::radians(-89.0f);
	}
}

void game::view::ClientSession::Update(const UpdateInfo& info)
{
    if (world_)
        world_->Update(info);
}

glm::mat4 game::view::ClientSession::GetViewMatrix() const
{
    glm::vec3 center(0, 0, 3);

    if (world_ && follow_ent_)
    {
        auto ent = world_->GetEntity(follow_ent_);
        if (ent)
            center += ent->GetRoot().local.position;
    }

	float yaw_cos = glm::cos(yaw_);
	float yaw_sin = glm::sin(yaw_);
	float pitch_cos = glm::cos(pitch_);
	float pitch_sin = glm::sin(pitch_);
    glm::vec3 dir(yaw_sin * pitch_cos, yaw_cos * pitch_cos, pitch_sin);

    float distance = 10.0f;

    auto eye = center - dir * distance;

    return glm::lookAt(eye, center, glm::vec3(0, 0, 1));
}

audio::Master& game::view::ClientSession::GetAudioMaster() const
{
    return app_.GetAudioMaster();
}

bool game::view::ClientSession::ProcessWorldMsg(net::InMessage& msg)
{
    net::MapName mapname;
    if (!msg.Read(mapname))
        return false;

    // TODO: pass mapname
    world_ = std::make_unique<WorldView>(*this);

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
