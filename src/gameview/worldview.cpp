#include "worldview.hpp"

#include "assets/cache.hpp"

#include "simple_entity_view.hpp"
#include "characterview.hpp"
#include "vehicleview.hpp"
#include "markerview.hpp"
#include "client_session.hpp"
#include "draw_args.hpp"
#include "net/utils.hpp"

game::view::WorldView::WorldView(ClientSession& session, net::InMessage& msg) : 
    session_(session), audiomaster_(session_.GetAudioMaster()), audioplayer_(audiomaster_), emitter_(&audioplayer_)
{
    net::MapName mapname;
    if (!msg.Read(mapname))
        throw EntityInitError();

    map_ = std::make_unique<MapInstanceView>(*this, std::string(mapname));

    // init destroyed objs
    net::ObjCount objcount;
    if (!msg.Read(objcount))
        throw EntityInitError();

    for (net::ObjCount i = 0; i < objcount; ++i)
    {
        net::ObjNum objnum;
        if (!msg.Read(objnum))
            throw EntityInitError();

        map_->EnableObj(objnum, false);
    }

    // break sounds
    Cache(assets::CacheManager::GetSound("data/breakpatnik.snd"));
    Cache(assets::CacheManager::GetSound("data/breaksign.snd"));
    Cache(assets::CacheManager::GetSound("data/breakwindow.snd"));
    Cache(assets::CacheManager::GetSound("data/breakwood.snd"));
    Cache(assets::CacheManager::GetSound("data/cardoor.snd"));
    Cache(assets::CacheManager::GetSound("data/crash.snd"));

    // vehicles
    Cache(assets::CacheManager::GetVehicleModel("data/avia.veh"));
    Cache(assets::CacheManager::GetVehicleModel("data/passat.veh"));
    Cache(assets::CacheManager::GetVehicleModel("data/pickup_hd.veh"));
    Cache(assets::CacheManager::GetVehicleModel("data/polskifiat.veh"));
    Cache(assets::CacheManager::GetVehicleModel("data/twingo.veh"));
    
    // models
    Cache(assets::CacheManager::GetModel("data/marker_base.mdl"));
    Cache(assets::CacheManager::GetModel("data/marker_tuning.mdl"));

    // items
    Cache(assets::CacheManager::GetItem("data/airrifle.item"));
    Cache(assets::CacheManager::GetItem("data/airsniper.item"));
    Cache(assets::CacheManager::GetItem("data/ak47.item"));
    Cache(assets::CacheManager::GetItem("data/uzi.item"));
    
    // fire sounds
    Cache(assets::CacheManager::GetSound("data/airrifle_fire.snd"));
    Cache(assets::CacheManager::GetSound("data/ak_fire.snd"));
    Cache(assets::CacheManager::GetSound("data/uzi_fire.snd"));

    // other sounds
    Cache(assets::CacheManager::GetSound("data/pickup_ammo.snd"));
    Cache(assets::CacheManager::GetSound("data/cow-01.snd"));
    Cache(assets::CacheManager::GetSound("data/cow-02.snd"));
    Cache(assets::CacheManager::GetSound("data/cow-04.snd"));
    Cache(assets::CacheManager::GetSound("data/cow-05.snd"));
    
    // fx
    Cache(assets::CacheManager::GetEffect("data/firefx.fx"));
    Cache(assets::CacheManager::GetEffect("data/impact_dirt.fx"));
    Cache(assets::CacheManager::GetEffect("data/impact_flesh.fx"));
    Cache(assets::CacheManager::GetEffect("data/impact_grass.fx"));
    Cache(assets::CacheManager::GetEffect("data/impact_metal.fx"));
    Cache(assets::CacheManager::GetEffect("data/impact_stone.fx"));
    Cache(assets::CacheManager::GetEffect("data/impact_wood.fx"));

    env_ = std::make_unique<WorldEnv>();
    env_->SetDayTime(12.0f);
}

bool game::view::WorldView::ProcessMsg(net::MessageType type, net::InMessage& msg)
{
    switch (type)
    {
    case net::MSG_ENV:
        return ProcessEnvMsg(msg);

    case net::MSG_ENTSPAWN:
        return ProcessEntSpawnMsg(msg);

    case net::MSG_ENTMSG:
        return ProcessEntMsgMsg(msg);
        
    case net::MSG_UPDATEENTS:
        return ProcessUpdateEntsMsg(msg);

    case net::MSG_ENTDESTROY:
        return ProcessEntDestroyMsg(msg);

    case net::MSG_OBJDESTROY:
        return ProcessObjDestroyOrRespawnMsg(msg, false);

    case net::MSG_OBJRESPAWN:
        return ProcessObjDestroyOrRespawnMsg(msg, true);

    case net::MSG_BEAM:
        return ProcessBeamMsg(msg);

    case net::MSG_FX:
        return ProcessFxMsg(msg);

    default:
        return false;
    }
}

void game::view::WorldView::Update(const UpdateInfo& info)
{
    time_ = info.time;

    if (!map_->IsLoaded())
        map_->LoadNext();

    for (const auto& [entnum, ent] : ents_)
    {
        ent->TryUpdate(info);
    }

    UpdateEnv();
    UpdateBeams();
    audioplayer_.Update();
    emitter_.Update(info.delta_time);
}

void game::view::WorldView::Draw(const DrawArgs& args)
{
    if (!map_->IsLoaded())
    {
        DrawLoadingScreen(args);
        return;
    }

    DrawEnv(args);

    map_->Draw(args);

    for (const auto& [entnum, ent] : ents_)
    {
        if (args.frustum.IsSphereVisible(ent->GetBoundingSphere()))
            ent->Draw(args);
    }

    DrawBeams(args);
    emitter_.Draw(args);
}

game::view::EntityView* game::view::WorldView::GetEntity(net::EntNum entnum)
{
    auto it = ents_.find(entnum);
    if (it != ents_.end())
        return it->second.get();

    return nullptr;
}

bool game::view::WorldView::IsLoaded() const
{
    return map_ && map_->IsLoaded();
}

void game::view::WorldView::DrawLoadingScreen(const DrawArgs& args) const
{
    float margin = 50.0f;
    glm::vec2 size(400.0f, 15.0f);
    glm::vec2 pos(margin, args.screen_size.y - margin - size.y);

    int loaded_percent = map_->GetLoadingPercent();
    float loaded = static_cast<float>(loaded_percent) * 0.01f;

    args.gui.DrawRect(pos, pos + size, 0x77FFFFFF);
    args.gui.DrawRect(pos, pos + glm::vec2(size.x * loaded, size.y), 0xFF00FFFF);

    std::string load_text = std::to_string(loaded_percent) + "%";
    args.gui.DrawTextAligned(load_text, pos + glm::vec2(size.x + 50.0f, size.y * 0.5f), glm::vec2(-0.5f, -0.5f));
}

void game::view::WorldView::UpdateEnv()
{
    if (!env_)
        return;

    env_->SetDayTime(glm::mix(daytime0_, daytime1_, glm::clamp(GetTime() - env_msg_time_, 0.0f, 1.0f)));
}

void game::view::WorldView::DrawEnv(const DrawArgs& args) const
{
    if (env_)
    {
        env_->Draw(args);
        return;
    }

    // args.env.clear_color = glm::vec3(0.5f, 0.7f, 1.0f);
    
    args.env.clear_color = glm::vec3(0.1f, 0.15f, 0.3f);
    args.env.ambient_light = glm::vec3(0.4f, 0.4f, 0.4f);
    args.env.sun_color = glm::vec3(0.5f, 0.8f, 1.0f) * 0.4f;
    args.env.sun_direction = glm::normalize(glm::vec3(1.0f, 1.0f, -1.0f));


}

bool game::view::WorldView::ProcessEnvMsg(net::InMessage& msg)
{
    float new_daytime;

    if (!msg.Read<net::DayTimeQ>(new_daytime))
        return false;

    if (!env_)
        return true;

    env_msg_time_ = GetTime();
    daytime0_ = env_->GetDayTime();
    daytime1_ = new_daytime;

    if (daytime1_ < daytime0_)
        daytime0_ -= 24.0f;


    return true;
}

bool game::view::WorldView::ProcessEntSpawnMsg(net::InMessage& msg)
{
    net::EntNum entnum;
    net::EntType type;

    if (!msg.Read(entnum) || !msg.Read(type))
        return false;

    auto& entslot = ents_[entnum];
    if (entslot)
        entslot.reset();

    try
    {
        switch (type)
        {
        case net::ET_SIMPLE:
            entslot = std::make_unique<SimpleEntityView>(*this, msg);
            break;

        case net::ET_CHARACTER:
            entslot = std::make_unique<CharacterView>(*this, msg);
            break;
    
        case net::ET_VEHICLE:
            entslot = std::make_unique<VehicleView>(*this, msg);
            break;

        case net::ET_MARKER:
            entslot = std::make_unique<MarkerView>(*this, msg);
            break;

        default:
            ents_.erase(entnum);
            return false; // unknown type
        }

        return true;

    } catch (const EntityInitError& e) // failed
    {
        ents_.erase(entnum);
        return false;
    }
}

bool game::view::WorldView::ProcessEntMsgMsg(net::InMessage& msg)
{
    net::EntNum entnum;
    net::EntMsgType type;

    if (!msg.Read(entnum) || !msg.Read(type))
        return false;

    auto ent_it = ents_.find(entnum);

    if (ent_it == ents_.end())
        return false;

    return ent_it->second->ProcessMsg(type, msg);
}

bool game::view::WorldView::ProcessUpdateEntsMsg(net::InMessage& msg)
{
    net::EntCount count;
    if (!msg.Read(count))
        return false;

    if (count > ents_.size())
        return false; // wanna update more than we have

    net::EntNum current = 0;
    auto it = ents_.begin();

    for (net::EntNum i = 0; i < count; ++i)
    {
        int64_t diff;
        if (!msg.ReadVarInt(diff))
            return false;
        
        current += static_cast<net::EntNum>(diff);

        while (it->first < current)
        {
            it->second->ProcessUpdateMsg(nullptr); // blank update
            ++it;
            if (it == ents_.end())
                return false;
        }

        if (it->first != current)
        {
            return false; // exact num wasnt found and it overrun 
        }

        if (!it->second->ProcessUpdateMsg(&msg))
            return false;
    }

    // process remaining
    for (; it != ents_.end(); ++it)
    {
        it->second->ProcessUpdateMsg(nullptr);
    }

    return true;
}

bool game::view::WorldView::ProcessEntDestroyMsg(net::InMessage& msg)
{
    net::EntNum entnum;
    if (!msg.Read(entnum))
        return false;

    ents_.erase(entnum);
    return true;
}

bool game::view::WorldView::ProcessObjDestroyOrRespawnMsg(net::InMessage& msg, bool enable)
{
    net::ObjNum objnum;
    if (!msg.Read(objnum))
        return false;

    map_->EnableObj(objnum, enable);
    return true;
}

bool game::view::WorldView::ProcessBeamMsg(net::InMessage& msg)
{
    BeamView beam;
    if (!net::ReadPosition(msg, beam.start) || !net::ReadPosition(msg, beam.end) || !net::ReadRGB(msg, beam.color) || !msg.Read<net::BeamTimeQ>(beam.expiration))
    return false;
    
    beam.expiration += GetTime();
    beam.width = 0.02f;
    
    beams_.emplace_back(beam);

    return true;
}

bool game::view::WorldView::ProcessFxMsg(net::InMessage& msg)
{
    net::ModelName name;
    glm::vec3 pos, dir;

    if (!msg.Read(name) || !net::ReadPosition(msg, pos) || !msg.Read<net::DirQ>(dir.x) || !msg.Read<net::DirQ>(dir.y) ||
        !msg.Read<net::DirQ>(dir.z))
        return false;

    if (glm::length2(dir) < 0.3f)
        return true;// weird

    emitter_.Emit(assets::CacheManager::GetEffect("data/" + std::string(name) + ".fx"), pos, glm::normalize(dir));
    return true;

}

void game::view::WorldView::Cache(std::any val)
{
    cache_.emplace_back(std::move(val));
}

void game::view::WorldView::UpdateBeams()
{
    beams_.erase(std::remove_if(beams_.begin(), beams_.end(),
                                [this](const BeamView& beam) { return beam.expiration <= GetTime(); }),
                 beams_.end());
}

void game::view::WorldView::DrawBeams(const DrawArgs& args) const
{
    for (const auto& beam : beams_)
    {
        args.dlist.AddBeam(beam.start, beam.end, beam.color | 0xFF000000, beam.width);
    }
}
