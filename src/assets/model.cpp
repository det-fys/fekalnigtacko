#include "model.hpp"

#include "cmdfile.hpp"
#include "cache.hpp"

std::shared_ptr<const assets::Model> assets::Model::LoadFromFile(const std::string& filename)
{
    auto model = std::make_shared<Model>();
    std::vector<glm::vec3> vert_pos; // rember for collision trimesh
    
    CLIENT_ONLY(MeshBuilder mb(gfx::MF_NONE);)

    LoadCMDFile(filename, [&](const std::string& command, std::istringstream& iss) {
        if (command == "v")
        {
            glm::vec3 pos;
            iss >> pos.x >> pos.y >> pos.z;

            CLIENT_ONLY(
                MeshVertex v;
                v.pos = pos;
                iss >> v.normal.x >> v.normal.y >> v.normal.z;
                iss >> v.uv.x >> v.uv.y;
    
                // TODO: LUV & bone data
    
                mb.AddVertex(v);
            )

            if (model->cmesh_)
                vert_pos.emplace_back(pos);
        }
        else if (command == "f")
        {
            uint32_t indices[3];
            iss >> indices[0] >> indices[1] >> indices[2];
            
            CLIENT_ONLY(
                MeshTriangle t;
                t.vert[0] = indices[0];
                t.vert[1] = indices[1];
                t.vert[2] = indices[2];
                mb.AddTriangle(t);
            )

            if (model->cmesh_)
            {
                // FIXME: possible index segfault
                model->cmesh_->AddTriangle(vert_pos[indices[0]], vert_pos[indices[1]], vert_pos[indices[2]]);
            }
        }
        else if (command == "surface")
        {
            std::string surface_name, texture_name;
            CLIENT_ONLY(gfx::SurfaceFlags sflags = gfx::SF_NONE;)

            iss >> surface_name;

            // Optional flags
            std::string flag;
            while (iss >> flag)
            {
                if (flag == "+texture")
                {
                    iss >> texture_name;
                }
                else if (flag == "+doublesided")
                {
                    CLIENT_ONLY(sflags |= gfx::SF_DOUBLE_SIDED;)
                }
                else if (flag == "+transparent")
                {   
                    CLIENT_ONLY(sflags |= gfx::SF_TRANSPARENT;)
                }
                else if (flag == "+ocolor")
                {
                    CLIENT_ONLY(sflags |= gfx::SF_OBJECT_COLOR;)
                }
            }

            CLIENT_ONLY(
                std::shared_ptr<const gfx::Texture> texture;
                if (!texture_name.empty())
                {
                    texture = CacheManager::GetTexture("data/" + surface_name + ".png");
                }
    
                mb.BeginSurface(sflags, surface_name, texture);
            )
        }
        else if (command == "makecoltrimesh")
        {
            model->cmesh_ = std::make_unique<collision::TriangleMesh>();
        }
        else
        {
            throw std::runtime_error("Unknown command in model file: " + command);
        }

        // TODO: skeleton
    });
    
    CLIENT_ONLY(
        mb.Build();
        model->mesh_ = mb.GetMesh();
    )

    if (model->cmesh_)
        model->cmesh_->Build();

    return model;
}
