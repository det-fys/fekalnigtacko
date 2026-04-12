#include "cmdfile.hpp"

void assets::LoadCMDStream(std::istream& is, CmdCallback handler)
{
    std::string line, command;

    if (is.eof())
        return;

    while (std::getline(is, line))
    {        
        // skip whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        
        if (line.empty() || line[0] == '#') // Skip empty lines and comments
            continue;

        std::istringstream iss(line);
        iss >> command;

        if (!handler(command, iss))
            return;
    }
}

void assets::LoadCMDFile(const std::string& filename, CmdCallbackVoid handler)
{
    std::istringstream file = fs::ReadFileAsStream(filename);
    LoadCMDStream(file, [handler](const std::string& command, std::istringstream& iss) {
        handler(command, iss);
        return true;
    });
}

std::string assets::ParseString(std::istringstream& iss)
{
    std::string str;
    iss >> str;

    if (!str.starts_with('"'))
        return str;

    str = str.substr(1);

    if (str.ends_with('"'))
    {
        str.pop_back();
        return str;
    }

    while (iss)
    {
        std::string tmp;
        iss >> tmp;
        str += ' ';
        str += tmp;

        if (str.ends_with('"'))
        {
            str.pop_back();
            break;
        }
    }

    return str;
}
