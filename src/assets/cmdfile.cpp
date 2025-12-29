#include "cmdfile.hpp"

void assets::LoadCMDFile(const std::string& filename,
                         const std::function<void(const std::string& command, std::istringstream& iss)>& handler)
{
    std::istringstream file = fs::ReadFileAsStream(filename);

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#') // Skip empty lines and comments
            continue;

        // skip whitespace
        line.erase(0, line.find_first_not_of(" \t"));

        std::istringstream iss(line);

        std::string command;
        iss >> command;

        handler(command, iss);
    }

}
