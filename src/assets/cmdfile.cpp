#include "cmdfile.hpp"

void assets::LoadCMDStream(std::istream& is, CmdCallback handler)
{
    std::string line, command;

    if (is.eof())
        return;

    while (std::getline(is, line))
    {                
        CmdLineStream iss(line);

        if (iss.Eol())
            continue; // empty or comment

        iss >> command;

        if (!handler(command, iss))
            return;
    }
}

void assets::LoadCMDFile(const std::string& filename, CmdCallbackVoid handler)
{
    std::istringstream file = fs::ReadFileAsStream(filename);
    LoadCMDStream(file, [handler](const std::string& command, CmdLineStream& iss) {
        handler(command, iss);
        return true;
    });
}

// DEPRECATED - with CmdLineStream no longer needed for parsing quoted strings
std::string assets::ParseString(CmdLineStream& iss)
{
    std::string str;
    iss >> str;
    return str;
}
