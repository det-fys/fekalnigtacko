#include "server_cfg.hpp"
#include "assets/cmdfile.hpp"

#include <iostream>

#include <set>

static sv::Cfg s_config{};

static std::set<std::string> s_conds;

static void InitConds() 
{
    s_conds.clear();

#ifdef NDEBUG
    s_conds.insert("release");
#else
    s_conds.insert("debug");
#endif
}

static void ProcessCmd(const std::string& cmd, std::istringstream& iss);

static void ProcessIfCmd(std::istringstream& iss)
{
    std::string cond_name, next_cmd;
    iss >> cond_name >> next_cmd;

    if (cond_name.empty())
        throw std::runtime_error("server cfg: if without condition");

    if (!s_conds.contains(cond_name))
        return;

    ProcessCmd(next_cmd, iss);
}

static void ProcessSetCmd(std::istringstream& iss)
{
    std::string var_name;
    iss >> var_name;

    if (var_name == "port")
    {
        iss >> s_config.port;
    }
    else if (var_name == "broadphase")
    {
        iss >> s_config.broadphase >> s_config.bp_bounds_min.x >> s_config.bp_bounds_min.y >>
            s_config.bp_bounds_min.z >> s_config.bp_bounds_max.x >> s_config.bp_bounds_max.y >>
            s_config.bp_bounds_max.z;
    }
    else
    {
        throw std::runtime_error("server cfg: unknown var: " + var_name);
    }
}

static void ProcessCmd(const std::string& cmd, std::istringstream& iss)
{
    if (cmd == "set")
    {
        ProcessSetCmd(iss);
    }
    else if (cmd == "if")
    {
        ProcessIfCmd(iss);
    }
    else
    {
        throw std::runtime_error("server cfg: unknown command: " + cmd);
    }
}

void sv::LoadCfg()
{
    std::cout << "Loading server.cfg..." << std::endl;

    InitConds();

    assets::LoadCMDFile("server.cfg", [](const std::string& cmd, std::istringstream& iss){
        ProcessCmd(cmd, iss);
    });

    std::cout << "... server.cfg loaded" << std::endl;

}

const sv::Cfg& sv::GetCfg()
{
    return s_config;
}
