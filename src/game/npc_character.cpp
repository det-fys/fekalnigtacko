#include "npc_character.hpp"
#include "openworld.hpp"
#include "drivable_vehicle.hpp"

#include <array>
#include <iostream>

game::NpcCharacter::NpcCharacter(World& world, const CharacterTuning& tuning) : Super(world, tuning) {
    UpdateVehicleState();
}

void game::NpcCharacter::Update()
{
    Super::Update();

    if (GetVehicle() && IsDriver())
        VehicleThink();
}

void game::NpcCharacter::OnRideableChanged()
{
    UpdateVehicleState();
}

void game::NpcCharacter::UpdateVehicleState()
{
    roads_ = nullptr;
    path_.clear();
    gas_ = false;
    stuck_counter_ = 0;
    vehicle_state_ = NVT_NORMAL;
    speed_limit_ = 0.0f;

    if (GetVehicle() && IsDriver())
    {
        roads_ = world_.GetMap().GetGraph("roads");

        seg_start_ = GetVehicle()->GetRootTransform().position;
        
        size_t start_node = 0;
        float min_dist = std::numeric_limits<float>().infinity();

        for (size_t i = 0; i < roads_->nodes.size(); ++i)
        {
            auto& node = roads_->nodes[i];
            float dist = glm::distance(node.position, seg_start_);
            if (dist < min_dist)
            {
                min_dist = dist;
                start_node = i;
            }
        }

        path_.push_back(start_node);

    }
}

void game::NpcCharacter::SelectNextNode()
{
    size_t node = path_.back();
    size_t num_nbs = roads_->nodes[node].num_nbs;

    if (num_nbs < 1)
    {
        const auto& pos = roads_->nodes[node].position;
        std::cout << "node " << node << " has no neighbors!!!1 position: " << pos.x << " " << pos.y << " " << pos.z
                    << std::endl;
        throw std::runtime_error("no neighbors");
    }

    path_.push_back(roads_->nbs[roads_->nodes[node].nbs + (rand() % num_nbs)]);
}


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

void game::NpcCharacter::VehicleThink()
{
    if (!roads_)
        return;

    auto vehicle = GetVehicle();

    if (vehicle_state_ == NVT_REVERSING)
    {
        if (reversing_frames_ > 0)
        {
            --reversing_frames_;
        }
        else
        {
            vehicle_state_ = NVT_NORMAL;
            stuck_counter_ = 0;
            vehicle->SetInput(game::VIN_BACKWARD, false);
        }
        return;
    }


    const auto& vehicle_trans = vehicle->GetRootTransform();

    const glm::vec3& pos = vehicle_trans.position;
    const glm::quat& rot = vehicle_trans.rotation;
    glm::vec3 forward = rot * glm::vec3{0.0f, 1.0f, 0.0f};

    // glm::vec3 target = s->roads.nodes[s->node].position;

    //
    std::array<glm::vec3, 8> waypoints;

    while (path_.size() < waypoints.size() - 1)
    {
        SelectNextNode();
    }

    glm::vec3 node_pos = roads_->nodes[path_.front()].position;
    if (glm::distance(glm::vec2(pos), glm::vec2(node_pos)) < 6.0f && path_.size() > 1)
    {
        seg_start_ = node_pos;
        path_.pop_front();
        SelectNextNode();
    }

    glm::vec3 target_node_pos = roads_->nodes[path_.front()].position;

    waypoints[0] = pos - glm::normalize(forward) * 3.0f;
    waypoints[1] = pos;

    // find closest point on segment [seg_start -> target_node_pos]
    glm::vec3 seg_end = target_node_pos;
    glm::vec3 seg_dir = seg_end - seg_start_;
    float seg_len = glm::length(seg_dir);
    if (seg_len > 5.0f)
    {
        glm::vec3 seg_dir_norm = seg_dir / seg_len;
        float t = glm::clamp(glm::dot(pos - seg_start_, seg_dir_norm) / seg_len, 0.0f, 1.0f);
        waypoints[2] = seg_start_ + t * seg_dir;
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
        size_t path_idx = glm::min(i - 3, path_.size() - 1);
        waypoints[i] = roads_->nodes[path_[path_idx]].position;
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
    speed_limit_ = target_speed;

    // std::cout << "target speed: " << target_speed << "\n";

    float angle = GetTurnAngle(pos, rot, waypoints[2]);

    if (glm::distance(pos, last_pos_) < 2.0f)
    {
        stuck_counter_++;
        if (stuck_counter_ > 100)
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

            vehicle->SetSteering(true, -angle); // try turn away while reversing
            vehicle->SetInputs(0); // stop
            vehicle->SetInput(game::VIN_BACKWARD, true);
            vehicle_state_ = NVT_REVERSING;
            reversing_frames_ = 50; // reverse for 50 frames

            // GetVehicleOld()->SetInputs(0); // stop
            // is_driver_ = false; // TODO: fix 
            return;
        }
    }
    else
    {
        stuck_counter_ = 0;
        last_pos_ = pos;
    }

    vehicle->SetSteering(true, angle);

    game::VehicleInputFlags vin = 0;

    float speed = vehicle->GetSpeed();

    // if (glm::distance(pos, target) < 10.0f)
    // {
    //     target_speed = 20.0f;
    // }

    if (speed < target_speed * 0.9f && !gas_)
    {
        gas_ = true;
    }
    else if (speed > target_speed * 1.1f && gas_)
    {
        gas_ = false;
    }

    if (gas_)
    {
        vin |= 1 << game::VIN_FORWARD;
    }

    if (speed > target_speed * 1.4f)
    {
        vin |= 1 << game::VIN_BACKWARD;
    }

    vehicle->SetInputs(vin);
}
