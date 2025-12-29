#include "model.hpp"

#include "cmdfile.hpp"
#include "cache.hpp"

std::shared_ptr<const assets::Model> assets::Model::LoadFromFile(const std::string& filename)
{
    auto model = std::make_shared<Model>();

    MeshBuilder mb(gfx::MF_NONE);

    LoadCMDFile(filename, [&](const std::string& command, std::istringstream& iss) {
        if (command == "v")
        {
            MeshVertex v;
            iss >> v.pos.x >> v.pos.y >> v.pos.z;
            iss >> v.normal.x >> v.normal.y >> v.normal.z;
            iss >> v.uv.x >> v.uv.y;

            // TODO: LUV & bone data

            mb.AddVertex(v);
        }
        else if (command == "f")
        {
            MeshTriangle t;
            iss >> t.vert[0] >> t.vert[1] >> t.vert[2];

            mb.AddTriangle(t);
        }
        else if (command == "surface")
        {
            std::string surface_name, texture_name;
            gfx::SurfaceFlags sflags = gfx::SF_NONE;

            iss >> surface_name;

            // Optional flags
            std::string flag;
            while (iss >> flag)
            {
                if (flag == "+texture")
                    iss >> texture_name;
                else if (flag == "+doublesided")
                    sflags |= gfx::SF_DOUBLE_SIDED;
                else if (flag == "+transparent")
                    sflags |= gfx::SF_TRANSPARENT;
                else if (flag == "+ocolor")
                    sflags |= gfx::SF_OBJECT_COLOR;
            }

            std::shared_ptr<const gfx::Texture> texture;
            if (!texture_name.empty())
            {
                texture = CacheManager::GetTexture("data/" + surface_name + ".png");
            }

            mb.BeginSurface(sflags, surface_name, texture);
        }
        else
        {
            throw std::runtime_error("Unknown command in model file: " + command);
        }

        // TODO: skeleton
    });
    


    return model;
}
