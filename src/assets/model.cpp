#include "model.hpp"

#include "cmdfile.hpp"
#include "cache.hpp"

#include <BulletCollision/CollisionShapes/btShapeHull.h>

static collision::Material GetMaterialByName(const std::string& name)
{
    if (name == "stone")
        return collision::PM_STONE;
    else if (name == "dirt")
        return collision::PM_DIRT;
    else if (name == "grass")
        return collision::PM_GRASS;
    else if (name == "wood")
        return collision::PM_WOOD;
    else if (name == "metal")
        return collision::PM_METAL;
    else if (name == "glass")
        return collision::PM_GLASS;
    else if (name == "flesh")
        return collision::PM_FLESH;
    else if (name == "car") // TODO: make new material for cars
        return collision::PM_METAL;
    else if (name == "carwindow")
        return collision::PM_NONE;
    else
        return collision::PM_STONE;
}

std::shared_ptr<const assets::Model> assets::Model::LoadFromFile(const std::string& filename)
{
    auto model = std::make_shared<Model>();
    model->name_ = filename; // TODO: name not filename
    std::vector<glm::vec3> vert_pos; // rember for collision trimesh
    
    CLIENT_ONLY(MeshBuilder mb(gfx::MF_NONE);)
    std::unique_ptr<btConvexHullShape> temp_hull;
    std::unique_ptr<btCompoundShape> compound;

    collision::Material col_material = collision::PM_NONE;

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
            {
                auto offset_pos = pos - model->col_offset_;

                temp_hull->addPoint(btVector3(offset_pos.x, offset_pos.y, offset_pos.z), false);

            }

            model->aabb_.AddPoint(pos);
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
                glm::vec3 p[3];
                for (size_t i = 0; i < 3; ++i)
                {
                    size_t index = indices[i];
                    if (index >= vert_pos.size())
                        throw std::runtime_error("Vertex index out of bounds in model");
                    
                    p[i] = vert_pos[index] - model->col_offset_;
                }

                model->cmesh_->AddTriangle(p[0], p[1], p[2]);
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
                else if (flag == "+ocolor_mult")
                {
                    CLIENT_ONLY(sflags |= gfx::SF_OBJECT_COLOR;)
                    CLIENT_ONLY(sflags |= gfx::SF_OBJECT_COLOR_MULT;)
                }
                else if (flag == "+multicolor")
                {
                    CLIENT_ONLY(sflags |= gfx::SF_MULTICOLOR;)
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
                else if (flag == "+unlit")
                {
                    CLIENT_ONLY(sflags |= gfx::SF_UNLIT;)
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
        else if (command == "col")
        {
            std::string shape_type;
            Transform trans;
            float sy, sz;
            iss >> shape_type;
            ParseTransform(iss, trans);
            iss >> sy >> sz;
            glm::vec3 scale(trans.scale, sy, sz);
            trans.scale = 1.0f; 

            trans.position -= model->col_offset_; // apply offset

            if (!compound)
            {
                compound = std::make_unique<btCompoundShape>();
            }

            if (shape_type == "box")
            {
                auto box_shape = std::make_unique<btBoxShape>(btVector3(scale.x, scale.y, scale.z));
                compound->addChildShape(trans.ToBtTransform(), box_shape.get());
                model->subshapes_.push_back(std::move(box_shape));
            }
            else
            {
                throw std::runtime_error("Unknown collision shape type: " + shape_type);
            }
        }
        else if (command == "centerofmass")
        {
            glm::vec3 com;
            iss >> com.x >> com.y >> com.z;
            model->col_offset_ = com;
        }
        else if (command == "param")
        {
            std::string key, val;
            iss >> key >> val;
            model->params_[key] = val;
        }
        else if (command == "pm")
        {
            std::string pm_name;
            iss >> pm_name;

            if (model->cmesh_)
            {
                model->cmesh_->BeginMaterial(GetMaterialByName(pm_name));
            }
        }
        else if (command == "cpm")
        {
            std::string pm_name;
            iss >> pm_name;
            col_material = GetMaterialByName(pm_name);
        }
        else
        {
            throw std::runtime_error("Unknown command in model file: " + command);
        }
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
    else
    {
        model->cshape_ = std::move(compound);
    }

    if (model->cshape_)
    {
        collision::SetShapeMaterial(*model->cshape_, col_material);
        if (col_material != collision::PM_NONE)
            model->cshape_is_bullet_target_ = true;
    }

    return model;
}

const std::string* assets::Model::GetParam(const std::string& key) const
{
    auto it = params_.find(key);
    if (it == params_.end())
        return nullptr;

    return &it->second;
}

bool assets::Model::GetParamFloat(const std::string& key, float& out) const
{
    auto str_val = GetParam(key);
    if (!str_val)
        return false;

    std::string str = *str_val;
    
    auto dashpos = str.find(',');
    if (dashpos != std::string::npos)
        str[dashpos] = '.';

    out = std::strtof(str.c_str(), nullptr);

    return true;
}
