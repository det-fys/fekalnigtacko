#pragma once

#include "assets/map.hpp"
#include "human_character.hpp"
#include "vehicle.hpp"

namespace game
{

constexpr size_t INVALID_WAYPOINT_IDX = std::numeric_limits<size_t>::max();

class OpenWorld;

enum ThinkState
{
    THINKSTATE_IDLE,
    THINKSTATE_MAD_IDLE,
    THINKSTATE_MAD_AIM,
    THINKSTATE_MAD_FIRE,
    THINKSTATE_SCARED,
    THINKSTATE_BRAINDEAD,
};

enum DriverThinkState
{
    DRIVERSTATE_NONE,
    DRIVERSTATE_PATH_BEGIN,
    DRIVERSTATE_PATH,
    DRIVERSTATE_REVERSE,
    DRIVERSTATE_FOLLOW_ENEMY,
};

class NpcCharacter : public HumanCharacter
{
public:
    using Super = HumanCharacter;

    NpcCharacter(World& world, const HumanCharacterTuning& tuning);

    virtual void Update() override;

    virtual void ReceiveDamage(const DamageInfo& damage) override;

    void SetWeapon(std::shared_ptr<ItemInstance> weapon);
    void SetMoney(int64_t amount) { money_ = amount; }

    bool IsBored(int64_t time) const;
    void Die();

    // net::EntNum GetEnemyNum() const { return enemy_num_; }
    bool IsArmed() const;
    

protected:
    virtual void OnRideableChanged() override;
    virtual void OnRideableDamaged(const DamageInfo& damage) override;
    virtual void SpawnLoot() override;

private:
    void MakeEnemy(net::EntNum enemy_num);
    void UpdateEnemy();
    bool CheckEnemyLost();
    bool HasEnemy() const;
    void ClearEnemy();
    bool WantsToFollowEnemy();

    bool IsVehicleDriver() const;
    void ResetVehiclePath();
    void SetPathMode(bool to_target);
    void FindVehiclePath(const glm::vec3& position);
    void FindVehiclePathRoads(const glm::vec3& position);
    void FindVehiclePathToTarget(const glm::vec3& position);
    void UpdateVehicleInput(std::span<glm::vec3> path);
    void UpdateVehicleInputToFollowPath();
    void UpdateVehicleInputToFollowEnemy();
    bool CheckStuck();

    void Think();
    void EnterThinkState(ThinkState state);
    ThinkState CheckThinkStateTransition();
    int64_t GetCurrentThinkStateDuration() const;
    void SetTargetThinkStateDuration(int duration_min, int duration_max = 0);
    bool HasThinkStateDurationElapsed() const;

    void DriverThink();
    void EnterDriverThinkState(DriverThinkState state);
    DriverThinkState CheckDriverThinkStateTransition();
    int64_t GetCurrentDriverThinkStateDuration() const;

    // void UpdateVehicleState();
    // void SelectNextNode();
    // void VehicleThink();

private:
    const assets::MapGraph* roads_;

    ThinkState think_state_ = THINKSTATE_IDLE;
    int64_t think_state_time_ = 0;
    int64_t think_state_target_duration_ = 0;

    // driver
    DriverThinkState driver_state_ = DRIVERSTATE_NONE;
    int64_t driver_state_time_ = 0;
    DriverThinkState prev_driver_state_ = DRIVERSTATE_NONE;
    
    std::deque<glm::vec3> path_;
    size_t last_waypoint_idx_ = INVALID_WAYPOINT_IDX;
    glm::vec3 last_pos_ = glm::vec3(0.0f);
    size_t stuck_counter_ = 0;
    
    VehicleInputFlags vehicle_in_ = 0;
    float vehicle_steer_ = 0.0f;

    bool path_to_target_ = false;
    int64_t last_pathfind_time_ = 0;

    bool in_hurry_ = false;

    bool reverse_ = false;
    int64_t last_direction_change_time_ = 0;

    // mad
    std::shared_ptr<ItemInstance> weapon_;

    net::EntNum enemy_num_ = 0;
    HumanCharacter* enemy_ = nullptr;
    int64_t enemy_time_ = 0;
    glm::vec3 last_enemy_pos_{};

    bool follow_enemy_ = false;

    int64_t money_ = 0;
};

}