#include "npc_character.hpp"
#include "openworld.hpp"
#include "drivable_vehicle.hpp"
#include "utils/random.hpp"
#include "player_character.hpp"

#include <array>
#include <iostream>

static constexpr size_t PATH_NEXT_WAYPOINTS = 16;

game::NpcCharacter::NpcCharacter(World& world, const HumanCharacterTuning& tuning) : Super(world, tuning)
{
    roads_ = world_.GetMap().GetGraph("roads");

    EnterThinkState(THINKSTATE_IDLE);
    EnterDriverThinkState(DRIVERSTATE_NONE);
}

void game::NpcCharacter::Update()
{
    UpdateEnemy();
    Think();
    DriverThink();
    Super::Update();
}

void game::NpcCharacter::ReceiveDamage(const DamageInfo& damage)
{
    Super::ReceiveDamage(damage);

    if (IsAlive() && damage.inflictor)
    {
        MakeEnemy(damage.inflictor->GetEntNum());
    }
}

void game::NpcCharacter::SetWeapon(std::shared_ptr<ItemInstance> weapon)
{
    weapon_ = std::move(weapon);
}

bool game::NpcCharacter::IsBored(int64_t time) const
{
    if (think_state_ != THINKSTATE_IDLE || GetCurrentThinkStateDuration() < time)
    {
        return false; // not idle for long
    }

    if (driver_state_ != DRIVERSTATE_NONE || GetCurrentDriverThinkStateDuration() < time)
    {
        return false; // driving or was driving recently
    }

    auto rideable = GetRideable();
    if (rideable->GetPassenger(0))
    {
        return false; // passenger in a ride with a driver
    }

    return true; // BORING
}

void game::NpcCharacter::Die()
{
    DamageInfo damage{};
    damage.type = DAMAGE_OTHER;
    damage.damage = 100000.0f;
    ReceiveDamage(damage);
}

void game::NpcCharacter::OnRideableChanged()
{
    EnterThinkState(THINKSTATE_IDLE);
}

void game::NpcCharacter::OnRideableDamaged(const DamageInfo& damage)
{
    if (damage.inflictor && damage.inflictor->IsAlive())
    {
        MakeEnemy(damage.inflictor->GetEntNum());
    }
}

void game::NpcCharacter::SpawnLoot()
{
    if (weapon_)
    {
        size_t ammo = weapon_->def->clip_size * 5;
        GetWorld().CreateItemPickup(root_.GetGlobalPosition(), std::move(weapon_), 60000, 0, ammo);
    }
}

void game::NpcCharacter::MakeEnemy(net::EntNum enemy_num)
{
    // it must have been a mistake
    // if (Chance(0.05f))
    // {
    //     return;
    // }

    // cant switch enemies that fast
    auto time = GetWorld().GetTime();
    if (enemy_num_ > 0 && enemy_num != enemy_num_ && time - enemy_time_ < 5000)
    {
        return; 
    }

    // increase anger
    // if (Chance(0.85f))
    // {
    //     follow_enemy_ = true;
    // }

    // this mf is already my current enemy
    if (enemy_num == enemy_num_)
    {
        return;
    }

    // already have another enemy, likely stay focused on him
    if (enemy_num_ != 0 && Chance(0.3f))
    {
        return;
    }

    // he is npc and i am not his target, was SURELY a missclick
    auto enemy_npc = dynamic_cast<NpcCharacter*>(GetWorld().GetEntity(enemy_num));
    if (enemy_npc && enemy_npc->enemy_num_ != GetEntNum() && Chance(0.1f))
    {
        return;
    }

    // OK this one is now my enemy
    enemy_num_ = enemy_num;
    enemy_time_ = time;
    follow_enemy_ = true;
    UpdateEnemy();
}

void game::NpcCharacter::UpdateEnemy()
{
    if (enemy_num_ == 0)
    {
        enemy_ = nullptr;
        return;
    }

    enemy_ = dynamic_cast<HumanCharacter*>(GetWorld().GetEntity(enemy_num_));

    if (!enemy_ || CheckEnemyLost())
    {
        ClearEnemy();
        return;
    }

    if (GetAiming())
    {        
        const auto& held_item = GetHeldItem(); 
        if (held_item && held_item->def->fire_type == assets::FIRETYPE_PROJECTILE)
        {
            glm::vec3 aim_target;
            // aim at vehicle and predict
            if (enemy_->GetRideable())
            {
                aim_target = enemy_->GetRideable()->GetEntity().GetRoot().GetGlobalPosition();
            }
            else
            {
                aim_target = enemy_->GetRoot().GetGlobalPosition() + glm::vec3(0.0f, 0.0f, 0.2f);
            }

            auto velocity = (aim_target - last_enemy_pos_) * 20.0f;
            last_enemy_pos_ = aim_target;
        
            aim_target += velocity * 0.5f; 

            SetAimTargetSmart(aim_target, velocity);
        }
        else
        {
            glm::vec3 aim_target = enemy_->GetRoot().matrix * glm::vec4(0.0f, 0.0f, 1.7f, 1.0f);
            SetAimTarget(aim_target);
        }

    }

    // in vehicle with me?????
    if (IsAlive() && GetRideable() && enemy_->GetRideable() == GetRideable())
    {
        Ride(nullptr, 0);
    }
}

bool game::NpcCharacter::CheckEnemyLost()
{
    if (!IsAlive())
    {
        return true;
    }

    if (!enemy_->IsAlive())
    {
        return true; // may he rest in peace
    }

    const float max_dist = 250.0f;
    auto dist2 = glm::distance2(root_.GetGlobalPosition(), enemy_->GetRoot().GetGlobalPosition());
    if (dist2 > (max_dist * max_dist))
        return true; // too far

    return false;
}

bool game::NpcCharacter::HasEnemy() const
{
    return enemy_;
}

void game::NpcCharacter::ClearEnemy()
{
    enemy_num_ = 0;
    enemy_ = nullptr;
    enemy_time_ = 0;
    follow_enemy_ = false;
}

bool game::NpcCharacter::WantsToFollowEnemy()
{
    return HasEnemy() && follow_enemy_ && IsArmed();
}

bool game::NpcCharacter::IsArmed() const
{
    return weapon_.get() != nullptr;
}

bool game::NpcCharacter::IsVehicleDriver() const
{
    return GetVehicle() && IsDriver() && IsAlive();
}

void game::NpcCharacter::ResetVehiclePath()
{
    path_.clear();
    last_waypoint_idx_= 0;
}

void game::NpcCharacter::FindVehiclePath(const glm::vec3& position)
{
    if (path_.empty())
    {
        // path_.clear(); // make sure there is not 1 left
        // path_.push_back(position); // take current pos as first waypoint

        // find closest waypoint as first
        size_t closest = 0;
        float closest_dist2 = 10000000000.0f;
        for (size_t i = 0; i < roads_->nodes.size(); ++i)
        {
            auto d = position - roads_->nodes[i].position;
            auto dist2 = glm::dot(d, d);
            if (dist2 < closest_dist2)
            {
                closest_dist2 = dist2;
                closest = i;
            }
        }

        path_.push_back(roads_->nodes[closest].position);
        last_waypoint_idx_ = closest;
    }

    while (path_.size() < PATH_NEXT_WAYPOINTS)
    {
        const auto& last = roads_->nodes[last_waypoint_idx_];
        if (last.num_nbs == 0)
        {
            throw std::runtime_error("dead end from waypoint: " + last_waypoint_idx_);
        }
        
        size_t random_next_idx = rand() % last.num_nbs;
        size_t nb_idx = roads_->nbs[last.nbs + random_next_idx];
        path_.push_back(roads_->nodes[nb_idx].position);
        last_waypoint_idx_ = nb_idx;
    }
}

static float FindClosestPointOnSegment(const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& pos)
{
    glm::vec3 seg_dir = p1 - p0;
    float seg_len2 = glm::dot(seg_dir, seg_dir);
    if (seg_len2 < 0.0001f)
        return 0.0f; // segment is too short

    float t = glm::dot(pos - p0, seg_dir) / seg_len2;
    // t = glm::clamp(t, 0.0f, 1.0f);
    return t;
}

static glm::vec3 LookAhead(std::span<glm::vec3> path, float distance)
{
    for (size_t i = 0; i < path.size() - 1; ++i)
    {
        const auto& p0 = path[i];
        const auto& p1 = path[i + 1];
        auto seg_dist = glm::distance(p0, p1);
        if (distance < seg_dist)
        {
            return glm::mix(p0, p1, distance / seg_dist);
        }

        distance -= seg_dist;
    }

    return path.back();
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

float CalculateNPCTargetSpeed(std::span<const glm::vec3> waypoints, const glm::quat& vehicleOrientation)
{
    // Configuration parameters (Tweak these to match your game's driving feel)
    const float MAX_SPEED_KMH = 100.0f;      // Speed on a straight road
    const float MIN_SPEED_KMH = 20.0f;       // Speed in a very sharp turn
    const float LOOK_AHEAD_DISTANCE = 200.0f; // How far ahead (in meters) the NPC cares about

    // Safety check: We need at least the current position (0) and the next waypoint (1)
    // to calculate a heading. More waypoints improve the curve estimation.
    if (waypoints.size() < 2)
    {
        return MIN_SPEED_KMH;
    }

    // Initialize vehicle forward vector from orientation
    glm::vec3 vehicleForward = vehicleOrientation * glm::vec3(0.0f, 1.0f, 0.0f);

    float accumulatedSharpness = 0.0f;
    float distanceEvaluated = 0.0f;

    // We start by checking the angle between the vehicle's current heading and the first segment
    glm::vec3 currentDir = vehicleForward;

    for (size_t i = 0; i < waypoints.size() - 1; ++i)
    {
        glm::vec3 segment = waypoints[i + 1] - waypoints[i];
        float segmentLength = glm::length(segment);

        if (segmentLength < 0.1f)
            continue; // Skip duplicate waypoints

        // Accumulate distance. Stop looking ahead if it's too far down the road.
        distanceEvaluated += segmentLength;
        if (distanceEvaluated > LOOK_AHEAD_DISTANCE)
            break;

        glm::vec3 nextDir = segment / segmentLength; // Normalized direction

        // Dot product gives the cosine of the angle between segments (range: -1 to 1)
        // 1.0 means perfectly straight, 0.0 means 90-degree turn, -1.0 is a hairpin U-turn.
        float dot = glm::dot(currentDir, nextDir);

        // Clamp to avoid float precision issues with acos/sqrt logic
        dot = glm::clamp(dot, -1.0f, 1.0f);

        // Turn sharpness: 0.0 (straight) to 2.0 (180-degree turn)
        float sharpness = 1.0f - dot;

        // Distance weighting: turns further away matter less
        float distanceWeight = 1.0f - (distanceEvaluated / LOOK_AHEAD_DISTANCE);
        distanceWeight = glm::max(distanceWeight, 0.0f);

        // Accumulate weighted sharpness
        accumulatedSharpness += sharpness * distanceWeight;

        // Move to the next segment
        currentDir = nextDir;
    }

    // Map the sharpness to a speed limit.
    // If accumulatedSharpness is 0, t = 0 -> MAX_SPEED.
    // We cap sharpness sensitivity at 1.0 (roughly a sharp 90-degree turn nearby).
    float t = glm::clamp(accumulatedSharpness, 0.0f, 1.0f);

    // Linear interpolation between max and min speed based on sharpness
    float targetSpeedKMH = glm::mix(MAX_SPEED_KMH, MIN_SPEED_KMH, t);

    return targetSpeedKMH;
}

void game::NpcCharacter::UpdateVehicleInput(std::span<glm::vec3> actual_path)
{
    auto vehicle = GetVehicle();
    auto vehicle_pos = vehicle->GetRoot().GetGlobalPosition();
    const auto& vehicle_rot = vehicle->GetRoot().local.rotation;

    auto target_pos = LookAhead(actual_path, glm::max(2.0f, vehicle->GetSpeed() * 0.2f));
    // auto target_speed = 10.0f;
    std::span<glm::vec3> speed_path = actual_path;
    if (glm::distance2(actual_path[0], actual_path[1]) < 4.0f)
    {
        speed_path = { actual_path.begin() + 1, actual_path.end() };
    }

    auto target_speed = CalculateNPCTargetSpeed(speed_path, vehicle_rot);

    if (in_hurry_)
    {
        target_speed *= 4.0f;
    }

    // set steering
    vehicle_steer_ = GetTurnAngle(vehicle_pos, vehicle_rot, target_pos);

    // set input
    float speed = vehicle->GetSpeed();

    if (speed < target_speed * 0.9f && ((vehicle_in_ & (1<<VIN_FORWARD)) == 0))
    {
        // gas
        vehicle_in_ |= (1<<VIN_FORWARD);
    }
    else if (speed > target_speed * 1.1f && ((vehicle_in_ & (1<<VIN_FORWARD)) > 0))
    {
        // no gas
        vehicle_in_ &= ~(1<<VIN_FORWARD);
    }

    if (speed > target_speed * 1.4f)
    {
        // brake
        vehicle_in_ |= (1<<VIN_BACKWARD);
    }
    else
    {
        // dont brake
        vehicle_in_ &= ~(1<<VIN_BACKWARD);
    }


    // debug draw path
    // float beam_dist = 30.0f;
    // for (size_t i = 0; i < actual_path.size() - 1; ++i)
    // {
    //     const auto& p0 = actual_path[i];
    //     auto p1 = actual_path[i + 1];

    //     auto dist = glm::distance(p0, p1);
    //     if (beam_dist < dist)
    //     {
    //         p1 = glm::mix(p0, p1, beam_dist / dist);
    //     }

    //     GetWorld().Beam(p0, p1, i % 2 == 0 ? 0xFF0000FF : 0xFF0077FF, 1.5f / 25.0f);
    
    //     beam_dist -= dist;
    //     if (beam_dist <= 0.0f)
    //         break;
    // }
    // GetWorld().Beam(vehicle_pos, target_pos, 0xFFFF00FF, 1.5f / 25.0f);
    // GetWorld().BeamBox(target_pos - 0.05f, target_pos + 0.05f, 0xFFFF00FF, 1.5f / 25.0f);
}

void game::NpcCharacter::UpdateVehicleInputToFollowPath() 
{
    auto vehicle = GetVehicle();
    if (!vehicle)
        return;

    auto vehicle_pos = vehicle->GetRoot().GetGlobalPosition();
    
    FindVehiclePath(vehicle_pos);

    float seg_t;

    // check if we reached next waypoint
    while (true)
    {
        auto seg_dir = path_[1] - path_[0];
        auto seg_len2 = glm::dot(seg_dir, seg_dir);
        auto seg_len = glm::sqrt(seg_len2);
        if (seg_len > 0.1f)
        {
            seg_t = glm::dot(vehicle_pos - path_[0], seg_dir) / seg_len2;
            if (seg_t < (1.0f - 3.0f / seg_len))
            {
                break;
            }
        }

        path_.pop_front();
        FindVehiclePath(vehicle_pos);
        
    }

    seg_t = glm::clamp(seg_t, 0.0f, 1.0f);

    const auto& p0 = path_[0];
    const auto& p1 = path_[1];
    auto on_segment = glm::mix(p0, p1, seg_t);

    std::array<glm::vec3, PATH_NEXT_WAYPOINTS + 1> actual_path; // pos,on_segment,path[1],path[2]...
    actual_path[0] = vehicle_pos;
    actual_path[1] = on_segment;
    for (size_t i = 1; i < PATH_NEXT_WAYPOINTS; ++i)
    {
        actual_path[i + 2 - 1] = path_[i];
    }

    UpdateVehicleInput(actual_path);
}

void game::NpcCharacter::UpdateVehicleInputToFollowEnemy()
{
    if (!enemy_)
    {
        vehicle_in_ = 0;
        vehicle_steer_ = 0.0f;
        return;   
    }

    auto vehicle = GetVehicle();
    if (!vehicle)
        return;

    std::array<glm::vec3, 2> actual_path;
    actual_path[0] = vehicle->GetRoot().GetGlobalPosition();;
    actual_path[1] = enemy_->GetRoot().GetGlobalPosition();;

    UpdateVehicleInput(actual_path);
}

bool game::NpcCharacter::CheckStuck()
{
    auto pos = root_.GetGlobalPosition();
    auto dist2 = glm::distance2(pos, last_pos_);

    // far, not stuck
    if (dist2 > 4.0f)
    {
        last_pos_ = pos;
        stuck_counter_ = 0;
        return false;
    }

    ++stuck_counter_;

    // close but only short time yet
    if (stuck_counter_ < 100)
        return false;

    // stuck
    last_pos_ = pos;
    stuck_counter_ = 0;
    return true;
}

static game::CharacterInputFlags GetRandomStrafeDir(float strafe_chance)
{
    auto dir_choice = RandomFloat(0.0f, 1.0f);

    if (dir_choice > strafe_chance)
        return 0;

    return (dir_choice < strafe_chance * 0.5f) ? (1 << game::CIN_RIGHT) : (1 << game::CIN_LEFT);
}

void game::NpcCharacter::Think()
{
    while (true)
    {
        auto new_state = CheckThinkStateTransition();
        if (new_state == think_state_)
            break;

        EnterThinkState(new_state);
    }
}

void game::NpcCharacter::EnterThinkState(ThinkState state)
{
    think_state_ = state;
    think_state_time_ = GetWorld().GetTime();

    switch (state)
    {
    case THINKSTATE_IDLE:
        SetAimHeld(false);
        SetFireHeld(false);
        Equip(nullptr);
        SetInputs(0);
        in_hurry_ = false;
        break;

    case THINKSTATE_MAD_IDLE:
        SetAimHeld(false);
        SetFireHeld(false);
        Equip(weapon_);
        SetInputs(0);
        SetTargetThinkStateDuration(100, 300);
        break;

    case THINKSTATE_MAD_AIM:
        SetAimHeld(true);
        SetFireHeld(false);
        Equip(weapon_);
        SetInputs(GetRandomStrafeDir(0.8f));
        SetTargetThinkStateDuration(200, 800);
        break;

    case THINKSTATE_MAD_FIRE:
        SetAimHeld(true);
        SetFireHeld(true);
        Equip(weapon_);
        SetInputs(GetRandomStrafeDir(0.4f));
        SetTargetThinkStateDuration(200, 1200);
        break;

    case THINKSTATE_SCARED:
        SetAimHeld(false);
        SetFireHeld(false);
        Equip(nullptr);
        SetInputs(0);
        in_hurry_ = true;
        SetTargetThinkStateDuration(5000, 10000);
        break;

    case THINKSTATE_BRAINDEAD:
        SetAimHeld(true);
        SetFireHeld(true);
        SetInputs(0);
        in_hurry_ = false;
        break;
    }

}

game::ThinkState game::NpcCharacter::CheckThinkStateTransition()
{
    if (!IsAlive())
        return THINKSTATE_BRAINDEAD;

    switch (think_state_)
    {
    case THINKSTATE_IDLE:
        if (HasEnemy())
            return IsArmed() ? THINKSTATE_MAD_IDLE : THINKSTATE_SCARED;

        return THINKSTATE_IDLE;

    case THINKSTATE_MAD_IDLE:
        if (!HasEnemy() || !IsArmed())
            return THINKSTATE_IDLE;

        if (HasThinkStateDurationElapsed() && CanAim())
            return THINKSTATE_MAD_AIM;

        return THINKSTATE_MAD_IDLE;

    case THINKSTATE_MAD_AIM:
        if (!HasEnemy() || !IsArmed())
            return THINKSTATE_IDLE;

        if (GetCurrentThinkStateDuration() > 0 && !GetAiming()) // reload or sth
            return THINKSTATE_MAD_IDLE;

        if (HasThinkStateDurationElapsed() && CanTurnToTarget())
            return THINKSTATE_MAD_FIRE;

        return THINKSTATE_MAD_AIM;

    case THINKSTATE_MAD_FIRE:
        if (HasThinkStateDurationElapsed())
            return THINKSTATE_MAD_AIM;

        return THINKSTATE_MAD_FIRE;
    
    case THINKSTATE_SCARED:
        if (HasThinkStateDurationElapsed())
            return THINKSTATE_IDLE;

        return THINKSTATE_SCARED;

    default:
        return THINKSTATE_IDLE;
    }
}

int64_t game::NpcCharacter::GetCurrentThinkStateDuration() const
{
    return GetWorld().GetTime() - think_state_time_;
}

void game::NpcCharacter::SetTargetThinkStateDuration(int duration_min, int duration_max)
{
    if (duration_max == 0)
    {
        think_state_target_duration_ = duration_min;
    }
    else
    {
        think_state_target_duration_ = RandomInt(duration_min, duration_max);
    }
}

bool game::NpcCharacter::HasThinkStateDurationElapsed() const
{
    return GetCurrentThinkStateDuration() >= think_state_target_duration_;
}

void game::NpcCharacter::DriverThink()
{
    while (true)
    {
        auto new_state = CheckDriverThinkStateTransition();
        if (new_state == driver_state_)
            break;

        EnterDriverThinkState(new_state);
    }

    auto vehicle = GetVehicle();
    if (vehicle && IsDriver())
    {
        vehicle->SetSteering(true, vehicle_steer_);
        vehicle->SetInputs(vehicle_in_);
    }
}

void game::NpcCharacter::EnterDriverThinkState(DriverThinkState state)
{
    prev_driver_state_ = driver_state_;
    driver_state_ = state;
    driver_state_time_ = GetWorld().GetTime();

    stuck_counter_ = 0;

    switch (state)
    {
    case DRIVERSTATE_NONE:
        vehicle_in_ = 0;
        vehicle_steer_ = 0.0f;
        break;

    case DRIVERSTATE_PATH_BEGIN:
        vehicle_in_ = 0;
        vehicle_steer_ = 0.0f;
        ResetVehiclePath();
        break;

    case DRIVERSTATE_PATH:
        break;

    case DRIVERSTATE_REVERSE:
        vehicle_in_ = 1<<VIN_BACKWARD; // go reverse
        vehicle_steer_ = -vehicle_steer_; // try turn away
        break;

    case DRIVERSTATE_FOLLOW_ENEMY:
        break;

    default:
        break;
    }
}

game::DriverThinkState game::NpcCharacter::CheckDriverThinkStateTransition()
{
    switch (driver_state_)
    {
    case DRIVERSTATE_NONE:
        if (IsVehicleDriver() && GetCurrentDriverThinkStateDuration() > 1000) // take some time to think
        {
            if (WantsToFollowEnemy())
                return DRIVERSTATE_FOLLOW_ENEMY;

            return DRIVERSTATE_PATH_BEGIN;
        }

        return DRIVERSTATE_NONE;

    case DRIVERSTATE_PATH_BEGIN:
        return DRIVERSTATE_PATH;
    
    case DRIVERSTATE_PATH:
        if (!IsVehicleDriver())
            return DRIVERSTATE_NONE;

        if (WantsToFollowEnemy())
            return DRIVERSTATE_NONE;

        if (CheckStuck())
            return DRIVERSTATE_REVERSE;

        // update input
        UpdateVehicleInputToFollowPath();

        return DRIVERSTATE_PATH;

    case DRIVERSTATE_FOLLOW_ENEMY:
        if (!IsVehicleDriver())
            return DRIVERSTATE_NONE;

        if (!WantsToFollowEnemy())
            return DRIVERSTATE_NONE;

        if (CheckStuck())
            return DRIVERSTATE_REVERSE;

        UpdateVehicleInputToFollowEnemy();

        return DRIVERSTATE_FOLLOW_ENEMY;

    case DRIVERSTATE_REVERSE:
        if (!IsVehicleDriver())
            return DRIVERSTATE_NONE;

        if (GetCurrentDriverThinkStateDuration() > 3000)
            return prev_driver_state_;
    
        return DRIVERSTATE_REVERSE;

    default:
        return DRIVERSTATE_NONE;
    }
}

int64_t game::NpcCharacter::GetCurrentDriverThinkStateDuration() const
{
    return GetWorld().GetTime() - driver_state_time_;
}
