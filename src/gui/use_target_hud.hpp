#pragma once

#include <string>

#include "context.hpp"

namespace gui
{

class UseTargetHud
{
public:
    UseTargetHud(const float& time);
 
    void SetData(std::string text, std::string error_text, float delay);

    void Draw(Context& ctx) const;
    
private:    
    const float& time_; 
    std::string text_;
    std::string error_text_;
    float start_time_ = 0.0f;
    float end_time_ = 0.0f;
};


}