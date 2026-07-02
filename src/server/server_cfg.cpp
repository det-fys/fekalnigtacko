#include "server_cfg.hpp"

#include <iostream>
#include <set>

#include "assets/cmdfile.hpp"
#include "utils/cvars.hpp"

static std::set<std::string> s_conds;

static void InitConds() 
{
    s_conds.clear();

#ifdef NDEBUG
    s_conds.insert("RELEASE");
#else
    s_conds.insert("DEBUG");
#endif
}

static void ProcessCmd(const std::string& cmd, CmdLineStream& iss);

static void ProcessIfCmd(CmdLineStream& iss)
{
    std::string cond_name, next_cmd;
    iss >> cond_name >> next_cmd;

    if (cond_name.empty())
        throw std::runtime_error("server cfg: if without condition");

    if (!s_conds.contains(cond_name))
        return;

    ProcessCmd(next_cmd, iss);
}

static void ProcessEnableCmd(bool enable, CmdLineStream& iss)
{
    std::string cond_name;
    iss >> cond_name;

    if (enable)
    {
        s_conds.insert(cond_name);
    }
    else
    {
        s_conds.erase(cond_name);
    }
}

static void ProcessSetCmd(CmdLineStream& iss)
{
    std::string var_name, value_str;
    iss >> var_name;
    value_str = assets::ParseString(iss);

    try
    {
        CVarRegistry::GetServerInstance().Set(var_name, value_str);       
    }
    catch(const std::exception& e)
    {
        std::cerr << "server cfg: error setting " << var_name << ": " << e.what() << std::endl;
    }
}

static void ProcessCmd(const std::string& cmd, CmdLineStream& iss)
{
    if (cmd == "set")
    {
        ProcessSetCmd(iss);
    }
    else if (cmd == "if")
    {
        ProcessIfCmd(iss);
    }
    else if (cmd == "enable")
    {
        ProcessEnableCmd(true, iss);
    }
    else if (cmd == "disable")
    {
        ProcessEnableCmd(false, iss);
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

    assets::LoadCMDFile("server.cfg", [](const std::string& cmd, CmdLineStream& iss){
        ProcessCmd(cmd, iss);
    });

    std::cout << "... server.cfg loaded" << std::endl;

}
