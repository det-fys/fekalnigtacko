#include "openworld.hpp"

#include <coroutine>
#include <iostream>

#include "player.hpp"
#include "vehicle.hpp"

namespace game
{

struct ControllableCharacter : public Character
{
    using Super = Character;

    Vehicle* vehicle = nullptr;
    bool is_driver = false;

    ControllableCharacter(World& world) : Character(world, CharacterInfo{}) {}

    virtual void VehicleChanged() = 0;
};

struct PlayerCharacter : public ControllableCharacter
{
    using Super = ControllableCharacter;

    Player& player;

    PlayerCharacter(World& world, Player& player) : ControllableCharacter(world), player(player) {
        VehicleChanged();
    }

    virtual void VehicleChanged() override
    {
        if (vehicle)
        {
            player.SetCamera(vehicle->GetEntNum());
        }
        else
        {
            player.SetCamera(GetEntNum());
        }
    }

    void UpdateInputs()
    {
        auto in = player.GetInput();
        CharacterInputFlags c_in = 0;
        VehicleInputFlags v_in = 0;

        if (in & (1 << IN_FORWARD))
        {
            c_in |= 1 << CIN_FORWARD;
            v_in |= 1 << VIN_FORWARD;
        }

        if (in & (1 << IN_BACKWARD))
        {
            c_in |= 1 << CIN_BACKWARD;
            v_in |= 1 << VIN_BACKWARD;
        }

        if (in & (1 << IN_LEFT))
        {
            c_in |= 1 << CIN_LEFT;
            v_in |= 1 << VIN_LEFT;
        }

        if (in & (1 << IN_RIGHT))
        {
            c_in |= 1 << CIN_RIGHT;
            v_in |= 1 << VIN_RIGHT;
        }

        if (in & (1 << IN_JUMP))
        {
            c_in |= 1 << CIN_JUMP;
        }

        if (vehicle && is_driver)
        {
            SetInputs(0);
            vehicle->SetInputs(v_in);
        }
        else
        {
            SetInputs(c_in);
        }

    }
};


static float GetTurnAngle2D(const glm::vec2& forward, const glm::vec2& to_target)
{
    glm::vec2 forward_xy = glm::normalize(forward);
    glm::vec2 to_target_xy = glm::normalize(to_target);
    float dot = glm::dot(forward_xy, to_target_xy);
    float cross = forward_xy.x * to_target_xy.y - forward_xy.y * to_target_xy.x;
    float angle = acosf(glm::clamp(dot, -1.0f, 1.0f)); // in [0, pi]

    if (cross < 0)
        angle = -angle;

    return angle; // in [-pi, pi]
}

static float GetTurnAngle(const glm::vec3& pos, const glm::quat& rot, const glm::vec3& target)
{
    glm::vec3 forward = rot * glm::vec3{0.0f, 1.0f, 0.0f};
    glm::vec3 to_target = target - pos;
    glm::vec2 forward_xy = glm::vec2{forward.x, forward.y};
    glm::vec2 to_target_xy = glm::vec2{to_target.x, to_target.y};
    return GetTurnAngle2D(forward_xy, to_target_xy);
}

struct NpcCharacter : public ControllableCharacter
{
    using Super = ControllableCharacter;

    // driver
    const assets::MapGraph* roads;
    glm::vec3 seg_start;
    std::deque<size_t> path;
    bool gas = false;
    size_t stuck_counter = 0;
    glm::vec3 last_pos = glm::vec3(0.0f);
    float speed_limit = 0.0f;

    NpcCharacter(World& world) : ControllableCharacter(world)
    {
        VehicleChanged();
    }

    virtual void VehicleChanged() override
    {
        roads = nullptr;
        path.clear();
        gas = false;
        stuck_counter = 0;
        speed_limit = 0.0f;

        if (vehicle && is_driver)
        {
            roads = world_.GetMap().GetGraph("roads");

            seg_start = vehicle->GetPosition();
            
            size_t start_node = 0;
            float min_dist = std::numeric_limits<float>().infinity();

            for (size_t i = 0; i < roads->nodes.size(); ++i)
            {
                auto& node = roads->nodes[i];
                float dist = glm::distance(node.position, seg_start);
                if (dist < min_dist)
                {
                    min_dist = dist;
                    start_node = i;
                }
            }

            path.push_back(start_node);

        }
        else
        {
        }
    }

    void SelectNextNode()
    {
        size_t node = path.back();
        size_t num_nbs = roads->nodes[node].num_nbs;

        if (num_nbs < 1)
        {
            const auto& pos = roads->nodes[node].position;
            std::cout << "node " << node << " has no neighbors!!!1 position: " << pos.x << " " << pos.y << " " << pos.z
                      << std::endl;
            throw std::runtime_error("no neighbors");
        }

        path.push_back(roads->nbs[roads->nodes[node].nbs + (rand() % num_nbs)]);
    }

    virtual void Update() override
    {
        Super::Update();

        VehicleThink();
    }

    void VehicleThink()
    {
        if (!is_driver || !vehicle || !roads)
            return;

        glm::vec3 pos = vehicle->GetPosition();
        glm::quat rot = vehicle->GetRotation();
        glm::vec3 forward = rot * glm::vec3{0.0f, 1.0f, 0.0f};

        // glm::vec3 target = s->roads.nodes[s->node].position;

        //
        std::array<glm::vec3, 8> waypoints;

        while (path.size() < waypoints.size() - 1)
        {
            SelectNextNode();
        }

        glm::vec3 node_pos = roads->nodes[path[0]].position;
        if (glm::distance(glm::vec2(pos), glm::vec2(node_pos)) < 6.0f && path.size() > 1)
        {
            seg_start = node_pos;
            path.pop_front();
            SelectNextNode();
        }

        glm::vec3 target_node_pos = roads->nodes[path[0]].position;

        waypoints[0] = pos - glm::normalize(forward) * 3.0f;
        waypoints[1] = pos;

        // find closest point on segment [seg_start -> target_node_pos]
        glm::vec3 seg_end = target_node_pos;
        glm::vec3 seg_dir = seg_end - seg_start;
        float seg_len = glm::length(seg_dir);
        if (seg_len > 5.0f)
        {
            glm::vec3 seg_dir_norm = seg_dir / seg_len;
            float t = glm::clamp(glm::dot(pos - seg_start, seg_dir_norm) / seg_len, 0.0f, 1.0f);
            waypoints[2] = seg_start + t * seg_dir;
            if (glm::distance(waypoints[1], target_node_pos) > 10.0f)
            {
                waypoints[2] += seg_dir_norm * 10.0f; // look a bit ahead on segment
            }
            else
            {
                waypoints[2] = target_node_pos;
            }
        }
        else
        {
            waypoints[2] = target_node_pos;
        }

        for (size_t i = 3; i < waypoints.size(); ++i)
        {
            size_t path_idx = glm::min(i - 3, path.size() - 1);
            waypoints[i] = roads->nodes[path[path_idx]].position;
        }

        // decrease speed based on curvature
        const float base_speed = 100.0f;
        float target_speed = base_speed;
        float dist_accum = 0.0f;
        for (size_t i = 1; i < waypoints.size() - 1; ++i)
        {
            glm::vec3 dir1 = waypoints[i] - waypoints[i - 1];
            glm::vec3 dir2 = waypoints[i + 1] - waypoints[i];
            float dist = glm::length(dir1);
            dist_accum += dist;

            glm::vec2 dir1_xy = glm::vec2{dir1.x, dir1.y};
            glm::vec2 dir2_xy = glm::vec2{dir2.x, dir2.y};

            const float min_dir_length = 0.001f;
            float angle = glm::length(dir1_xy) > min_dir_length && glm::length(dir2_xy) > min_dir_length
                              ? GetTurnAngle2D(dir1_xy, dir2_xy)
                              : 0.0f;
            // std::cout << "angle: " << angle << "\n";
            float abs_angle = fabsf(angle);
            if (abs_angle > glm::radians(7.0f))
            {
                // float speed_limit = 50.0f / abs_angle; // sharper turn -> lower speed
                // speed_limit *= dist_accum / 20.0f; // more distance to turn -> higher speed
                // speed_limit = glm::max(speed_limit, 20.0f);
                // max_speed = glm::min(max_speed, speed_limit);
                target_speed -=
                    abs_angle * (base_speed / glm::pi<float>() / 2.0f) * 50.0f / glm::max(dist_accum - 1.0f, 1.0f);
            }

            if (dist_accum > 200.0f)
                break;
        }

        target_speed = glm::clamp(target_speed, 25.0f, 100.0f);
        speed_limit = target_speed;

        // std::cout << "target speed: " << target_speed << "\n";

        float angle = GetTurnAngle(pos, rot, waypoints[2]);

        if (glm::distance(pos, last_pos) < 2.0f)
        {
            stuck_counter++;
            if (stuck_counter > 20)
            {
                //s->state_str = "stuck (reverse)";
                //s->stuck_counter = 0;
                //s->vehicle.SetSteering(true, -angle); // try turn away

                //s->vehicle.SetInputs(0); // stop
                //// stuck, go reverse for a while
                //s->vehicle.SetInput(game::VIN_BACKWARD, true);
                //s->vehicle.Schedule(2000, [s]() {
                //    s->vehicle.SetInput(game::VIN_BACKWARD, false);
                //    BotThink(s);
                //});

                vehicle->SetInputs(0); // stop
                is_driver = false; // TODO: fix 
                return;
            }
        }
        else
        {
            stuck_counter = 0;
            last_pos = pos;
        }

        vehicle->SetSteering(true, angle);

        game::VehicleInputFlags vin = 0;

        float speed = vehicle->GetSpeed();

        // if (glm::distance(pos, target) < 10.0f)
        // {
        //     target_speed = 20.0f;
        // }

        if (speed < target_speed * 0.9f && !gas)
        {
            gas = true;
        }
        else if (speed > target_speed * 1.1f && gas)
        {
            gas = false;
        }

        if (gas)
        {
            vin |= 1 << game::VIN_FORWARD;
        }

        if (speed > target_speed * 1.4f)
        {
            vin |= 1 << game::VIN_BACKWARD;
        }

        vehicle->SetInputs(vin);
    }
};

struct VehicleSeat
{
    glm::vec3 position;
    ControllableCharacter* occupant;
};

struct DrivableVehicle : public Vehicle
{
    std::vector<VehicleSeat> seats;

    DrivableVehicle(World& world, std::string model_name, const glm::vec3& color)
        : Vehicle(world, std::move(model_name), color)
    {
        InitSeats();
    }

    void InitSeats()
    {
        const auto& veh = *GetModel();
        for (char c = '0'; c <= '9'; ++c)
        {
            auto trans = veh.GetLocation(std::string("seat") + c);
            if (!trans)
                break;

            VehicleSeat seat{};
            seat.position = trans->position;
            seats.emplace_back(seat);
        }
    }
};

} // namespace game

game::OpenWorld::OpenWorld() : World("openworld")
{
    srand(time(NULL));

    // spawn bots
    for (size_t i = 0; i < 100; ++i)
    {
        SpawnBot();
    }
}

void game::OpenWorld::Update(int64_t delta_time)
{
    World::Update(delta_time);

}

void game::OpenWorld::PlayerJoined(Player& player)
{
    CreatePlayerCharacter(player);
}

void game::OpenWorld::PlayerInput(Player& player, PlayerInputType type, bool enabled)
{
    auto character = player_characters_.at(&player);

    switch (type)
    {
    case IN_DEBUG1:
        if (enabled)
        {
            if (character->vehicle)
                character->vehicle->SetPosition({100.0f, 100.0f, 5.0f});
            else
                character->SetPosition({100.0f, 100.0f, 5.0f});
        }
        break;

    case IN_DEBUG2:
        if (enabled)
            CreatePlayerCharacter(player);
        break;

    default:
        character->UpdateInputs();
        break;
    }
}

void game::OpenWorld::PlayerViewAnglesChanged(Player& player, float yaw, float pitch)
{
    auto character = player_characters_.at(&player);
    character->SetForwardYaw(yaw);
}

void game::OpenWorld::PlayerLeft(Player& player)
{
    RemovePlayerCharacter(player);
}


static glm::vec3 GetRandomColor()
{
    glm::vec3 color;
    // shittiest way to do it
    for (int i = 0; i < 3; ++i)
    {
        net::ColorQ qcol;
        qcol.value = rand() % 256;
        color[i] = qcol.Decode();
    }

    return color;
}

template <class T, typename... TArgs>
static T& SpawnRandomCharacter(game::World& world, TArgs&&... args)
{
    auto& character = world.Spawn<T>(std::forward<TArgs>(args)...);

    // add clothes
    character.AddClothes("tshirt", GetRandomColor());
    character.AddClothes("shorts", GetRandomColor());

    return character;
}

void game::OpenWorld::CreatePlayerCharacter(Player& player)
{
    RemovePlayerCharacter(player);

    auto& character = SpawnRandomCharacter<PlayerCharacter>(*this, player);
    character.SetNametag("player (" + std::to_string(character.GetEntNum()) + ")");
    character.SetPosition({100.0f, 100.0f, 5.0f});
    character.EnablePhysics(true);

    player.SetCamera(character.GetEntNum());

    player_characters_[&player] = &character;
}

void game::OpenWorld::RemovePlayerCharacter(Player& player)
{
    auto it = player_characters_.find(&player);
    if (it != player_characters_.end())
    {
        it->second->Remove();
        player_characters_.erase(it);
    }
}

static const char* GetRandomCarModel()
{
    const char* vehicles[] = {"pickup_hd", "passat", "twingo", "polskifiat"};
    return vehicles[rand() % (sizeof(vehicles) / sizeof(vehicles[0]))];
}

static game::DrivableVehicle& SpawnRandomVehicle(game::World& world)
{
    auto roads = world.GetMap().GetGraph("roads");

    if (!roads)
    {
        throw std::runtime_error("SpawnRandomVehicle: no roads graph in map");
    }

    size_t start_node = rand() % roads->nodes.size();
    // auto color = glm::vec3{0.3f, 0.3f, 0.3f};
    auto color = GetRandomColor();
    auto& vehicle = world.Spawn<game::DrivableVehicle>(GetRandomCarModel(), color);
    // vehicle.SetNametag("bot (" + std::to_string(vehicle.GetEntNum()) + ")");
    vehicle.SetPosition(roads->nodes[start_node].position + glm::vec3{0.0f, 0.0f, 5.0f});

    return vehicle;
}

void game::OpenWorld::SpawnBot()
{
    auto& vehicle = SpawnRandomVehicle(*this);
    auto& driver = SpawnRandomCharacter<NpcCharacter>(*this);

    driver.Attach(vehicle.GetEntNum());

    if (vehicle.seats.size() > 0)
    {
        driver.SetPosition(vehicle.seats[0].position);
    }

    driver.SetMainAnim("vehicle_drive");
    driver.SetYaw(0.5f * glm::pi<float>());

    driver.vehicle = &vehicle;
    driver.is_driver = true;
    driver.VehicleChanged();
}
