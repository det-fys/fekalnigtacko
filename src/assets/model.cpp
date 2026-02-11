#include "model.hpp"

#include "cmdfile.hpp"
#include "cache.hpp"

#include <BulletCollision/CollisionShapes/btShapeHull.h>

std::shared_ptr<const assets::Model> assets::Model::LoadFromFile(const std::string& filename)
{
    auto model = std::make_shared<Model>();
    std::vector<glm::vec3> vert_pos; // rember for collision trimesh
    
    CLIENT_ONLY(MeshBuilder mb(gfx::MF_NONE);)
    std::unique_ptr<btConvexHullShape> temp_hull;

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
    
                v.uv.y = 1.0f - v.uv.y; // FLIP FOR GL
                
                if (model->skeleton_)
                {
                    size_t num_bones = 0;
                    iss >> num_bones;
                    for (size_t i = 0; i < num_bones; ++i)
                    {
                        iss >> v.bones[i].bone_index >> v.bones[i].weight;
                    }
                }
    
                mb.AddVertex(v);
            )

            if (model->cmesh_)
                vert_pos.emplace_back(pos);

            if (temp_hull)
                temp_hull->addPoint(btVector3(pos.x, pos.y, pos.z), false);
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
                else if (flag == "+2sided")
                {
                    CLIENT_ONLY(sflags |= gfx::SF_2SIDED;)
                }
                else if (flag == "+ocolor")
                {
                    CLIENT_ONLY(sflags |= gfx::SF_OBJECT_COLOR;)
                }
                else if (flag == "+blend")
                {
                    std::string blend_str;
                    iss >> blend_str;

                    CLIENT_ONLY(
                        sflags |= gfx::SF_BLEND;
                        if (blend_str == "additive")
                            sflags |= gfx::SF_BLEND_ADDITIVE;
                    )
                }
            }

            CLIENT_ONLY(
                std::shared_ptr<const gfx::Texture> texture;
                if (!texture_name.empty())
                {
                    texture = CacheManager::GetTexture("data/" + texture_name + ".png");
                }
    
                mb.BeginSurface(sflags, surface_name, texture);
            )
        }
        else if (command == "makecoltrimesh")
        {
            model->cmesh_ = std::make_unique<collision::TriangleMesh>();
        }
        else if (command == "makeconvexhull")
        {
            temp_hull = std::make_unique<btConvexHullShape>();
        }
        else if (command == "skeleton")
        {
            std::string skel_name;
            iss >> skel_name;
            model->skeleton_ = CacheManager::GetSkeleton("data/" + skel_name + ".sk");
            CLIENT_ONLY(mb.SetMeshFlag(gfx::MF_SKELETAL));
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

    // tri mesh
    if (model->cmesh_)
        model->cmesh_->Build();

    // convex hull
    if (temp_hull)
    {
        temp_hull->recalcLocalAabb();

        auto shape_hull = std::make_unique<btShapeHull>(temp_hull.get());
        shape_hull->buildHull(temp_hull->getMargin());

        model->cshape_ = std::make_unique<btConvexHullShape>((btScalar*)shape_hull->getVertexPointer(), shape_hull->numVertices(), sizeof(btVector3));
    }

    return model;
}
