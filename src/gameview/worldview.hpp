#pragma once

#include <any>

#include "assets/map.hpp"
#include "draw_args.hpp"
#include "net/defs.hpp"
#include "net/inmessage.hpp"
#include "collision/dynamicsworld.hpp"
#include "entityview.hpp"
#include "mapinstanceview.hpp"

namespace game::view
{

class ClientSession;

class WorldView : public collision::DynamicsWorld
{
public:
    WorldView(ClientSession& session, net::InMessage& msg);

    bool ProcessMsg(net::MessageType type, net::InMessage& msg);

    void Update(const UpdateInfo& info);
    void Draw(const DrawArgs& args) const;

    glm::vec3 CameraSweep(const glm::vec3& start, const glm::vec3& end);

    EntityView* GetEntity(net::EntNum entnum);

    float GetTime() const { return time_; }
    audio::Master& GetAudioMaster() const { return audiomaster_; }

private:
    // msg handlers
    bool ProcessEntSpawnMsg(net::InMessage& msg);
    bool ProcessEntMsgMsg(net::InMessage& msg);
    bool ProcessUpdateEntsMsg(net::InMessage& msg);
    bool ProcessEntDestroyMsg(net::InMessage& msg);

    bool ProcessObjDestroyOrRespawnMsg(net::InMessage& msg, bool enable);

    void Cache(std::any val);

private:
    ClientSession& session_;
    
    MapInstanceView map_;
    std::map<net::EntNum, std::unique_ptr<EntityView>> ents_;
    
    float time_ = 0.0f;
    
    audio::Master& audiomaster_;

    std::vector<std::any> cache_;
};

} // namespace game::view