#include "material.hpp"

assets::ModelMaterial assets::ParseMaterial(CmdLineStream& iss)
{
    ModelMaterial mat{};

    // parse name
    iss >> mat.name;

    // parse properties
    std::string flag;
    while (!iss.Eol())
    {
        iss >> flag;

        if (flag == "+texture")
        {
            iss >> mat.texture_name;
        }
        else if (flag == "+2sided")
        {
            mat.properties.twosided = true;
        }
        else if (flag == "+ocolor")
        {
            mat.properties.color = gfx::MATERIAL_OBJECT_COLOR_TYPE_BACKGROUND;
        }
        else if (flag == "+ocolor_mult")
        {
            mat.properties.color = gfx::MATERIAL_OBJECT_COLOR_TYPE_MULTIPLY;
        }
        else if (flag == "+multicolor")
        {
            mat.properties.color = gfx::MATERIAL_OBJECT_COLOR_TYPE_MULTICOLOR;
        }
        else if (flag == "+blend")
        {
            std::string blend_str;
            iss >> blend_str;

            if (blend_str == "additive")
                mat.properties.blend = gfx::MATERIAL_BLEND_TYPE_ADDITIVE;
            else
                mat.properties.blend = gfx::MATERIAL_BLEND_TYPE_OPACITY;
        }
        else if (flag == "+unlit")
        {
            mat.properties.lighting = gfx::MATERIAL_LIGHTING_TYPE_UNLIT;
        }
        else if (flag == "+translucent")
        {
            mat.properties.translucent = true;
        }
    }

    return mat;
}
