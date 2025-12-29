#include "files.hpp"

#include <SDL.h>
#include <SDL_rwops.h>

std::string fs::ReadFileAsString(const std::string& path)
{
    SDL_RWops *rw = SDL_RWFromFile(path.c_str(), "rb");
    
    if (!rw)
    {
        throw std::runtime_error("Failed to open file: " + path);
    }

    Sint64 res_size = SDL_RWsize(rw);
    if (res_size < 0)
    {
        SDL_RWclose(rw);
        throw std::runtime_error("Failed to get file size: " + path);
    }

    std::string content;
    content.resize(static_cast<size_t>(res_size));
    SDL_RWread(rw, content.data(), 1, res_size);
    SDL_RWclose(rw);
    return content;
}

std::istringstream fs::ReadFileAsStream(const std::string& path)
{
    std::string content = ReadFileAsString(path);
    return std::istringstream(content);
}
