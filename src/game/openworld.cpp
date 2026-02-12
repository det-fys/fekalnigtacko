#include "openworld.hpp"

#include <coroutine>
#include <iostream>

#include "player.hpp"
#include "vehicle.hpp"

// /* --------------- awaitable machinery --------------- */

// struct CoroWrapper
// {
//     bool destroy = true;
//     std::coroutine_handle<> handle;

//     CoroWrapper(std::coroutine_handle<> h) : handle(h) {}
//     ~CoroWrapper() { if (handle && destroy) handle.destroy(); }
// };

// struct CoroTask
// {
//     std::shared_ptr<CoroWrapper> wrapper;

//     explicit CoroTask(std::coroutine_handle<> h) : wrapper(std::make_shared<CoroWrapper>(h)) {}
    
//     void operator()() const
//     {
//         if (wrapper && wrapper->handle && !wrapper->handle.done())
//         {
//             wrapper->destroy = false;
//             wrapper->handle.resume();
//         }
//     }
// };

// class SchedulerAwaiter
// {
//     Scheduler& sch_;
//     int64_t    when_;
// public:
//     explicit SchedulerAwaiter(Scheduler& s, int64_t t) noexcept
//         : sch_(s), when_(t) {}

//     // --- await interface ---
//     bool await_ready() const noexcept { return when_ <= 0; }

//     void await_suspend(std::coroutine_handle<> h) const
//     {
//         sch_.Schedule(when_, CoroTask{h});   // resume coro later
//     }

//     void await_resume() const noexcept {}
// };

// /* --------------- task type (minimal) --------------- */

// template<typename T = void>
// struct task
// {
//     struct promise_type
//     {
//         std::suspend_never initial_suspend() noexcept { return {}; }
//         std::suspend_never final_suspend() noexcept { return {}; }
//         task get_return_object() { return {}; }
//         void return_void() {}
//         void unhandled_exception() { std::terminate(); }
//     };
// };

// static task<> BotThink(game::Vehicle& vehicle)
// {
//     while (true)
//     {
//         vehicle.SetInput(game::VIN_FORWARD, true);
//         co_await SchedulerAwaiter{vehicle, 1000};
//         vehicle.SetInput(game::VIN_FORWARD, false);
//         co_await SchedulerAwaiter{vehicle, 1000};
//     }
// }


game::OpenWorld::OpenWorld() : World("openworld")
{
    srand(time(NULL));

    // // spawn test vehicles
    // for (size_t i = 0; i < 3; ++i)
    // {
    //     auto& vehicle = Spawn<Vehicle>("twingo", glm::vec3{0.3f, 0.3f, 0.3f});
    //     vehicle.SetPosition({ static_cast<float>(i * 3), 150.0f, 5.0f });
    //     // vehicle.SetInput(VIN_FORWARD, true);    
    //     BotThink(vehicle);
    //     bots_.push_back(&vehicle);
    // }

    // spawn bots
    for (size_t i = 0; i < 100; ++i)
    {
        SpawnBot();
    }
}

void game::OpenWorld::Update(int64_t delta_time)
{
    World::Update(delta_time);

    // for (auto bot : bots_)
    // {
    //     bot->SetInput(VIN_FORWARD, true);
    
    //     if (rand() % 1000 < 10)
    //     {
    //         bool turn_left = rand() % 2;
    //         bot->SetInput(VIN_LEFT, turn_left);
    //         bot->SetInput(VIN_RIGHT, !turn_left);
    //     }
    //     else
    //     {
    //         bot->SetInput(VIN_LEFT, false);
    //         bot->SetInput(VIN_RIGHT, false);
    //     }

    //     auto pos = bot->GetPosition();
    //     if (glm::distance(pos, glm::vec3(0.0f, 0.0f, 0.0f)) > 1000.0f || pos.z < -20.0f)
    //     {
    //         bot->SetPosition({ rand() % 30 * 3 + 100.0f, 200.0f, 10.0f });
    //     }
    
    // }
}

void game::OpenWorld::PlayerJoined(Player& player)
{
    // SpawnVehicle(player);
    SpawnCharacter(player);
}

void game::OpenWorld::PlayerInput(Player& player, PlayerInputType type, bool enabled)
{
    // auto vehicle = player_vehicles_.at(&player);
    // // player.SendChat("input zmenen: " + std::to_string(static_cast<int>(type)) + "=" + (enabled ? "1" : "0"));

    // switch (type)
    // {
    // case IN_FORWARD:
    //     vehicle->SetInput(VIN_FORWARD, enabled);
    //     break;

    // case IN_BACKWARD:
    //     vehicle->SetInput(VIN_BACKWARD, enabled);
    //     break;

    // case IN_LEFT:
    //     vehicle->SetInput(VIN_LEFT, enabled);
    //     break;

    // case IN_RIGHT:
    //     vehicle->SetInput(VIN_RIGHT, enabled);
    //     break;

    // case IN_DEBUG1:
    //     if (enabled)
    //         vehicle->SetPosition({ 100.0f, 100.0f, 5.0f });
    //     break;
    
    // case IN_DEBUG2:
    //     if (enabled)
    //         SpawnVehicle(player);
    //     break;

    // default:
    //     break;
    // }

    auto character = player_characters_.at(&player);

    switch (type)
    {
    case IN_FORWARD:
        character->SetInput(CIN_FORWARD, enabled);
        break;

    case IN_BACKWARD:
        character->SetInput(CIN_BACKWARD, enabled);
        break;

    case IN_LEFT:
        character->SetInput(CIN_LEFT, enabled);
        break;

    case IN_RIGHT:
        character->SetInput(CIN_RIGHT, enabled);
        break;

    case IN_JUMP:
        character->SetInput(CIN_JUMP, enabled);
        break;

    case IN_DEBUG1:
        if (enabled)
            character->SetPosition({ 100.0f, 100.0f, 5.0f });
        break;
    
    case IN_DEBUG2:
        if (enabled)
            SpawnCharacter(player);
        break;

    default:
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
    // RemoveVehicle(player);
    RemoveCharacter(player);
}

void game::OpenWorld::RemoveVehicle(Player& player)
{
    auto it = player_vehicles_.find(&player);
    if (it != player_vehicles_.end())
    {
        it->second->Remove();
        player_vehicles_.erase(it);
    }
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

void game::OpenWorld::SpawnCharacter(Player& player)
{
    RemoveCharacter(player);

    CharacterInfo cinfo;
    auto& character = Spawn<Character>(cinfo);
    character.SetNametag("player (" + std::to_string(character.GetEntNum()) + ")");
    character.SetPosition({ 100.0f, 100.0f, 5.0f });

    // add clothes
    character.AddClothes("tshirt", GetRandomColor());
    character.AddClothes("shorts", GetRandomColor());

    player.SetCamera(character.GetEntNum());

    player_characters_[&player] = &character;
}

void game::OpenWorld::RemoveCharacter(Player& player)
{
    auto it = player_characters_.find(&player);
    if (it != player_characters_.end())
    {
        it->second->Remove();
        player_characters_.erase(it);
    }
}

// static void BotThink(game::Vehicle& vehicle)
// {
//     int direction = rand() % 3; // 0=none, 1=forward, 2=backward
//     int steer = 0; // 0=none, 1=left, 2=right
//     if (direction && rand() % 1000 < 300)
//     {
//         steer = (rand() % 2) ? 1 : 2;
//     }

//     game::VehicleInputFlags vin = 0;
//     if (direction == 1)
//         vin |= 1 << game::VIN_FORWARD;
//     else if (direction == 2)
//         vin |= 1 << game::VIN_BACKWARD;

//     if (steer == 1)
//         vin |= 1 << game::VIN_LEFT;
//     else if (steer == 2)
//         vin |= 1 << game::VIN_RIGHT;

//     vehicle.SetInputs(vin);

//     int time = 300 + (rand() % 3000);

//     vehicle.Schedule(time, [&vehicle]() {
//         BotThink(vehicle);
//     } );
// }


struct BotThinkState
{
    game::Vehicle& vehicle;
    const assets::MapGraph& roads;
    glm::vec3 seg_start;
    std::deque<size_t> path;
    bool gas = false;
    size_t stuck_counter = 0;
    glm::vec3 last_pos = glm::vec3(0.0f);
    float speed_limit = 0.0f;
    const char* state_str = "init";

    BotThinkState(game::Vehicle& v, const assets::MapGraph& g, size_t n)
        : vehicle(v), roads(g), path({n})
    {
        seg_start = v.GetPosition();
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

static void SelectNextNode(BotThinkState& s)
{
    size_t node = s.path.back();
    size_t num_nbs = s.roads.nodes[node].num_nbs;

    if (num_nbs < 1)
    {
        const auto& pos = s.roads.nodes[node].position;
        std::cout << "node " << node << " has no neighbors!!!1 position: " << pos.x << " " << pos.y << " " << pos.z << std::endl;
        throw std::runtime_error("no neighbors");
    }

    s.path.push_back(s.roads.nbs[s.roads.nodes[node].nbs + (rand() % num_nbs)]);
}

static void BotThink(std::shared_ptr<BotThinkState> s)
{
    s->state_str = "path";

    glm::vec3 pos = s->vehicle.GetPosition();
    glm::quat rot = s->vehicle.GetRotation();
    glm::vec3 forward = rot * glm::vec3{0.0f, 1.0f, 0.0f};

    //glm::vec3 target = s->roads.nodes[s->node].position;

    // 
    std::array<glm::vec3, 8> waypoints;

    while (s->path.size() < waypoints.size() - 1)
    {
        SelectNextNode(*s); 
    }

    glm::vec3 node_pos = s->roads.nodes[s->path[0]].position;
    if (glm::distance(glm::vec2(pos), glm::vec2(node_pos)) < 6.0f && s->path.size() > 1)
    {
        s->seg_start = node_pos;
        s->path.pop_front();
        SelectNextNode(*s);
    }

    glm::vec3 target_node_pos = s->roads.nodes[s->path[0]].position;

    waypoints[0] = pos - glm::normalize(forward) * 3.0f;
    waypoints[1] = pos;

    // find closest point on segment [seg_start -> target_node_pos]
    glm::vec3 seg_end = target_node_pos;
    glm::vec3 seg_dir = seg_end - s->seg_start;
    float seg_len = glm::length(seg_dir);
    if (seg_len > 5.0f)
    {
        glm::vec3 seg_dir_norm = seg_dir / seg_len;
        float t = glm::clamp(glm::dot(pos - s->seg_start, seg_dir_norm) / seg_len, 0.0f, 1.0f);
        waypoints[2] = s->seg_start + t * seg_dir;
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
        size_t path_idx = glm::min(i - 3, s->path.size() - 1);
        waypoints[i] = s->roads.nodes[s->path[path_idx]].position;
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
        float angle = glm::length(dir1_xy) > min_dir_length && glm::length(dir2_xy) > min_dir_length ? GetTurnAngle2D(dir1_xy, dir2_xy) : 0.0f;
        // std::cout << "angle: " << angle << "\n";
        float abs_angle = fabsf(angle);
        if (abs_angle > glm::radians(7.0f))
        {
            // float speed_limit = 50.0f / abs_angle; // sharper turn -> lower speed
            // speed_limit *= dist_accum / 20.0f; // more distance to turn -> higher speed
            // speed_limit = glm::max(speed_limit, 20.0f);
            // max_speed = glm::min(max_speed, speed_limit);
            target_speed -= abs_angle * (base_speed / glm::pi<float>() / 2.0f) * 50.0f / glm::max(dist_accum - 1.0f, 1.0f);

        }

        if (dist_accum > 200.0f)
            break;
    }

    target_speed = glm::clamp(target_speed, 25.0f, 100.0f);
    s->speed_limit = target_speed;

    // std::cout << "target speed: " << target_speed << "\n";

    float angle = GetTurnAngle(pos, rot, waypoints[2]);
    
    if (glm::distance(pos, s->last_pos) < 2.0f)
    {
        s->stuck_counter++;
        if (s->stuck_counter > 20)
        {
            s->state_str = "stuck (reverse)";
            s->stuck_counter = 0;
            s->vehicle.SetSteering(true, -angle); // try turn away
            
            s->vehicle.SetInputs(0); // stop
            // stuck, go reverse for a while
            s->vehicle.SetInput(game::VIN_BACKWARD, true);
            s->vehicle.Schedule(2000, [s]() {
                s->vehicle.SetInput(game::VIN_BACKWARD, false);
                BotThink(s);
            } );
            return;
        }
    }
    else
    {
        s->stuck_counter = 0;
        s->last_pos = pos;
    }
    
    s->vehicle.SetSteering(true, angle);

    game::VehicleInputFlags vin = 0;

    float speed = s->vehicle.GetSpeed();

    // if (glm::distance(pos, target) < 10.0f)
    // {
    //     target_speed = 20.0f;
    // }

    if (speed < target_speed * 0.9f && !s->gas)
    {
        s->gas = true;
    }
    else if (speed > target_speed * 1.1f && s->gas)
    {
        s->gas = false;
    }

    if (s->gas)
    {
        vin |= 1 << game::VIN_FORWARD;
    }

    if (speed > target_speed * 1.4f)
    {
        vin |= 1 << game::VIN_BACKWARD;
    }

    s->vehicle.SetInputs(vin);

    s->vehicle.Schedule(rand() % 120 + 40, [s]() {
        BotThink(s);
    } );
}

static void BotNametagThink(std::shared_ptr<BotThinkState> s)
{
    std::string nametag;
    nametag += "s=" + std::to_string(static_cast<int>(s->vehicle.GetSpeed())) + "/" + std::to_string(static_cast<int>(s->speed_limit)) + " ";
    nametag += "sc=" + std::to_string(s->stuck_counter) + " ";
    nametag += "st=" + std::string(s->state_str); // + " ";

    // nametag += "path=";
    // for (auto n : path)
    // {
    //     nametag += std::to_string(n) + " ";
    // }
    s->vehicle.SetNametag(nametag);

    s->vehicle.Schedule(240, [s]() {
        BotNametagThink(s);
    } );
}

static const char* GetRandomCarModel()
{
    const char* vehicles[] = {"pickup_hd", "passat", "twingo", "polskifiat"};
    return vehicles[rand() % (sizeof(vehicles) / sizeof(vehicles[0]))];
}


void game::OpenWorld::SpawnBot()
{
    auto roads = GetMap().GetGraph("roads");

    if (!roads)
    {
        std::cerr << "OpenWorld::SpawnBot: no roads graph in map\n";
        return;
    }

    size_t start_node = rand() % roads->nodes.size();
    // auto color = glm::vec3{0.3f, 0.3f, 0.3f};
    auto color = GetRandomColor();
    auto& vehicle = Spawn<Vehicle>(GetRandomCarModel(), color);
    //vehicle.SetNametag("bot (" + std::to_string(vehicle.GetEntNum()) + ")");
    vehicle.SetPosition(roads->nodes[start_node].position + glm::vec3{0.0f, 0.0f, 5.0f});

    auto think_state = std::make_shared<BotThinkState>(vehicle, *roads, start_node);
    BotThink(think_state);
    vehicle.Schedule(rand() % 500, [think_state]() {
        //BotNametagThink(think_state);
    } );
}

void game::OpenWorld::SpawnVehicle(Player& player)
{
    RemoveVehicle(player);

    // spawn him car
    // ranodm color


    auto vehicle_name = GetRandomCarModel();
    auto& vehicle = Spawn<Vehicle>(vehicle_name, GetRandomColor());
    vehicle.SetNametag("player (" + std::to_string(vehicle.GetEntNum()) + ")");
    vehicle.SetPosition({ 100.0f, 100.0f, 5.0f });

    player.SetCamera(vehicle.GetEntNum());

    player_vehicles_[&player] = &vehicle;

    player.SendChat("dostals " + std::string(vehicle_name));
}
