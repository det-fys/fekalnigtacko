#include "vehicle_tuning.hpp"

#include "assets/cmdfile.hpp"

#include <iomanip>

static float game::VehicleTuningContext::* GetCtxVariablePointer(const std::string& name)
{
    if (name == "mass")
        return &game::VehicleTuningContext::mass;
    if (name == "engine_force")
        return &game::VehicleTuningContext::engine_force;
    if (name == "braking_force")
        return &game::VehicleTuningContext::braking_force;

    throw std::runtime_error("tuning list: invalid variable " + name);
}

static float game::VehicleWheelTuningContext::* GetWheelVariablePointer(const std::string& name)
{
    if (name == "radius")
        return &game::VehicleWheelTuningContext::radius;
    if (name == "z_offset")
        return &game::VehicleWheelTuningContext::z_offset;
    if (name == "friction")
        return &game::VehicleWheelTuningContext::friction;
    if (name == "suspension_stiffness")
        return &game::VehicleWheelTuningContext::suspension_stiffness;
    if (name == "suspension_max_force")
        return &game::VehicleWheelTuningContext::suspension_max_force;
    if (name == "suspension_rest_length")
        return &game::VehicleWheelTuningContext::suspension_rest_length;
    if (name == "suspension_travel")
        return &game::VehicleWheelTuningContext::suspension_travel;
    if (name == "roll_influence")
        return &game::VehicleWheelTuningContext::roll_influence;
    if (name == "steering_factor")
        return &game::VehicleWheelTuningContext::steering_factor;
    if (name == "braking_factor")
        return &game::VehicleWheelTuningContext::braking_factor;
    if (name == "engine_factor")
        return &game::VehicleWheelTuningContext::engine_factor;

    throw std::runtime_error("tuning list: invalid wheel variable " + name);
}

static void ApplyOp(float& var, const std::string& op, float value)
{
    if (op == "+=")
        var += value;
    else if (op == "-=")
        var -= value;
    else if (op == "*=")
        var *= value;
    else if (op == "/=")
        var /= value;
    else
        var = value;
}

static bool CheckWheelCond(const game::VehicleWheelTuningContext& wheel_ctx, const std::string& cond)
{
    if (cond == "front" && !wheel_ctx.front)
        return false;
    
    if (cond == "rear" && wheel_ctx.front)
        return false;
    
    return true;
}

static uint32_t ParseColor(uint32_t c)
{
    auto r = (c >> 16) & 0xFF;
    auto g = (c >> 8) & 0xFF;
    auto b = c & 0xFF;
    return 0xFF000000 | (b << 16) | (g << 8) | r;
}

static game::VehicleTuningFunction ParseTuningFunction(std::istringstream& iss)
{
    std::string func_name;
    iss >> func_name;

    if (func_name == "set")
    {
        std::string var_name, op;
        float value;
        iss >> var_name >> op >> value;

        auto var_ptr = GetCtxVariablePointer(var_name);

        return [var_ptr, op, value](game::VehicleTuningContext& ctx) {
            ApplyOp(ctx.*var_ptr, op, value); 
        };
    }
    else if (func_name == "setcolor")
    {
        size_t color_idx;
        uint32_t color;
        iss >> color_idx >> std::hex >> color >> std::dec;

        if (color_idx >= 4)
            throw std::runtime_error("tuning list: invalid color index");

        color = ParseColor(color);

        return [color_idx, color](game::VehicleTuningContext& ctx) {
            ctx.colors[color_idx] = color;
        };
    }
    else if (func_name == "setwheel")
    {
        std::string wheel_cond, var_name, op;
        float value;

        iss >> wheel_cond >> var_name >> op >> value;

        auto var_ptr = GetWheelVariablePointer(var_name);

        return [wheel_cond, var_ptr, op, value](game::VehicleTuningContext& ctx) {
            for (auto& wheel : ctx.wheels)
            {
                if (CheckWheelCond(wheel, wheel_cond))
                    ApplyOp(wheel.*var_ptr, op, value);
            }
        };
    }
    else if (func_name == "setwheelmodel")
    {
        std::string wheel_cond, modelname;
        iss >> wheel_cond >> modelname;

        return [wheel_cond, modelname](game::VehicleTuningContext& ctx) {
            for (auto& wheel : ctx.wheels)
            {
                if (CheckWheelCond(wheel, wheel_cond))
                    wheel.modelname = modelname;
            }
        };
    }
    else
    {
        throw std::runtime_error("tuning list: unknown function " + func_name);
    }
}

std::unique_ptr<const game::VehicleTuningList> game::VehicleTuningList::LoadFromFile(const std::string& filename)
{
    auto tuninglist = std::make_unique<VehicleTuningList>();

    if (!fs::FileExists(filename))
        return tuninglist; // empty

    VehicleTuningGroup* current_group = nullptr;
    VehicleTuningPart* current_part = nullptr;

    auto process_command = [&](const std::string& command, std::istringstream& iss) {
        if (command == "group")
        {
            VehicleTuningGroup group;
            iss >> group.id;
            group.displayname = assets::ParseString(iss);

            tuninglist->groups.emplace_back(std::move(group));
            current_group = &tuninglist->groups.back();

            return true;
        }
        else if (command == "part")
        {
            if (!current_group)
                throw std::runtime_error("tuning list: part without active group");

            VehicleTuningPart part;
            iss >> part.id >> part.price;
            part.displayname = assets::ParseString(iss);

            current_part = &(current_group->parts[part.id] = std::move(part));

            return true;
        }
        else if (command == "default")
        {
            tuninglist->default_funcs.emplace_back(ParseTuningFunction(iss));
            return true;
        }
        else if (command == "mod")
        {
            if (!current_part)
                throw std::runtime_error("tuning list: mod without active part");

            current_part->funcs.emplace_back(ParseTuningFunction(iss));
            return true;
        }

        return false;
    };

    assets::LoadCMDFile(filename, [&](const std::string& command, std::istringstream& iss) {

        if (process_command(command, iss))
            return;

        if (command == "include")
        {
            std::string include_name;
            iss >> include_name;

            assets::LoadCMDFile("data/" + include_name + ".tun", [&](const std::string& command, std::istringstream& iss) {
                process_command(command, iss);
            });

        }
    });

    return tuninglist;
}