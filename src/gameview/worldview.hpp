#pragma once

#include <any>

#include "assets/map.hpp"
#include "draw_args.hpp"
#include "net/defs.hpp"
#include "net/inmessage.hpp"
#include "collision/dynamicsworld.hpp"
#include "entityview.hpp"
#include "mapinstanceview.hpp"
#include "worldenv.hpp"

namespace game::view
{

class ClientSession;

struct BeamView
{
    float expiration;
    glm::vec3 start;
    glm::vec3 end;
    float width;
    uint32_t color;
};

class WorldView : public collision::DynamicsWorld
{
public:
    WorldView(ClientSession& session, net::InMessage& msg);

    bool ProcessMsg(net::MessageType type, net::InMessage& msg);

    void Update(const UpdateInfo& info);
    void Draw(const DrawArgs& args) const;

    EntityView* GetEntity(net::EntNum entnum);

    float GetTime() const { return time_; }
    audio::Master& GetAudioMaster() const { return audiomaster_; }

private:
    void DrawLoadingScreen(const DrawArgs& args) const;

    void UpdateEnv();
    void DrawEnv(const DrawArgs& args) const;

    // msg handlers
    bool ProcessEnvMsg(net::InMessage& msg);
    bool ProcessEntSpawnMsg(net::InMessage& msg);
    bool ProcessEntMsgMsg(net::InMessage& msg);
    bool ProcessUpdateEntsMsg(net::InMessage& msg);
    bool ProcessEntDestroyMsg(net::InMessage& msg);
    bool ProcessObjDestroyOrRespawnMsg(net::InMessage& msg, bool enable);
    bool ProcessBeamMsg(net::InMessage& msg);

    void Cache(std::any val);

    void UpdateBeams();
    void DrawBeams(const DrawArgs& args) const;

private:
    ClientSession& session_;
    
    std::unique_ptr<MapInstanceView> map_;
    std::map<net::EntNum, std::unique_ptr<EntityView>> ents_;
    std::unique_ptr<WorldEnv> env_;

    float time_ = 0.0f;
    
    float daytime0_ = 0.0f;
    float daytime1_ = 0.0f;
    float env_msg_time_ = 0.0f;

    audio::Master& audiomaster_;

    std::vector<std::any> cache_;

    std::vector<BeamView> beams_;
};

} // namespace game::view