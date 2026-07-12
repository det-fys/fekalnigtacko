#include "model.hpp"

#include "cmdfile.hpp"

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

std::shared_ptr<assets::Model> assets::Model::Load(const std::string& name)
{
    return LoadFromFile("data/" + name + ".mdl");
}

std::shared_ptr<assets::Model> assets::Model::LoadFromFile(const std::string& filename)
{
    auto model = std::make_shared<Model>();
    std::vector<glm::vec3> vert_pos; // rember for collision trimesh
    
    std::unique_ptr<btConvexHullShape> temp_hull;
    std::unique_ptr<btCompoundShape> compound;

    collision::Material col_material = collision::PM_NONE;

    LoadCMDFile(filename, [&](const std::string& command, CmdLineStream& iss) {
        if (command == "v")
        {
            glm::vec3 pos{};
            iss >> pos.x >> pos.y >> pos.z;

#ifdef CLIENT
            glm::vec3 normal{};
            glm::vec2 uv{};
            iss >> normal.x >> normal.y >> normal.z;
            iss >> uv.x >> uv.y;

            uv.y = 1.0f - uv.y; // FLIP FOR GL // TODO: rly?

            auto& vert_data = model->vertices_;
            vert_data.positions.emplace_back(pos);
            vert_data.normals.emplace_back(normal);
            vert_data.uvs.emplace_back(uv);

            if (model->skeleton_)
            {
                gfx::MeshVertexBoneData bones{};

                size_t num_bones = 0;
                iss >> num_bones;
                for (size_t i = 0; i < gfx::MAX_VERTEX_BONE_INFLUENCES; ++i)
                {
                    gfx::BoneIndex bone_idx = gfx::NO_BONE;
                    float bone_weight = 0.0f;
                    if (i < num_bones)
                    {
                        iss >> bone_idx >> bone_weight;
                    }
                    bones.bone_indices[i] = bone_idx;
                    bones.bone_weights[i] = bone_weight;
                }
                vert_data.bones.emplace_back(bones);
            }

#endif // CLIENT

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
            gfx::MeshTriangle t;
            iss >> t.vertices[0] >> t.vertices[1] >> t.vertices[2];
            
#ifdef CLIENT
            if (model->surfaces_.empty())
            {
                throw std::runtime_error("Face without surface in model");
            }

            model->tris_.emplace_back(t);
            ++model->surfaces_.back().tri_count;
#endif // CLIENT

            if (model->cmesh_)
            {
                glm::vec3 p[3];
                for (size_t i = 0; i < 3; ++i)
                {
                    size_t index = t.vertices[i];
                    if (index >= vert_pos.size())
                        throw std::runtime_error("Vertex index out of bounds in model");
                    
                    p[i] = vert_pos[index] - model->col_offset_;
                }

                model->cmesh_->AddTriangle(p[0], p[1], p[2]);
            }
        }
        else if (command == "surface")
        {
            std::string surface_name;
            iss >> surface_name;

            size_t first = 0;
            if (!model->surfaces_.empty())
            {
                first = model->surfaces_.back().tri_offset + model->surfaces_.back().tri_count;
            }

            model->surface_indices_[surface_name] = model->surfaces_.size();
            auto& surface = model->surfaces_.emplace_back();
            surface.tri_offset = first;

            // Optional flags
            std::string flag;
            while (!iss.Eol())
            {
                iss >> flag;

                if (flag == "+texture")
                {
                    iss >> surface.texture_name;
                }
                else if (flag == "+2sided")
                {
                    surface.properties.twosided = true;
                }
                else if (flag == "+ocolor")
                {
                    surface.properties.color = gfx::MATERIAL_OBJECT_COLOR_TYPE_BACKGROUND;
                }
                else if (flag == "+ocolor_mult")
                {
                    surface.properties.color = gfx::MATERIAL_OBJECT_COLOR_TYPE_MULTIPLY;
                }
                else if (flag == "+multicolor")
                {
                    surface.properties.color = gfx::MATERIAL_OBJECT_COLOR_TYPE_MULTICOLOR;
                }
                else if (flag == "+blend")
                {
                    std::string blend_str;
                    iss >> blend_str;

                    if (blend_str == "additive")
                        surface.properties.blend = gfx::MATERIAL_BLEND_TYPE_ADDITIVE;
                    else
                        surface.properties.blend = gfx::MATERIAL_BLEND_TYPE_OPACITY;
                }
                else if (flag == "+unlit")
                {
                    surface.properties.lighting = gfx::MATERIAL_LIGHTING_TYPE_UNLIT;
                }
                else if (flag == "+translucent")
                {
                    surface.properties.translucent = true;
                }
            }
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
            model->skeleton_ = AssetManager::GetInstance().Get<Skeleton>(skel_name);
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
        else if (command == "loc")
        {
            std::string loc_name;
            iss >> loc_name;
            ParseTransform(iss, model->locations_[loc_name]);
        }
        else
        {
            throw std::runtime_error("Unknown command in model file: " + command);
        }
    });
    
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

bool assets::Model::GetSurfaceIndex(const std::string& name, size_t & idx) const
{
    auto it = surface_indices_.find(name);
    if (it == surface_indices_.end())
        return false;

    idx = it->second;
    return true;
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

const Transform* assets::Model::GetLocation(const std::string& key) const
{
    auto it = locations_.find(key);
    if (it == locations_.end())
        return nullptr;

    return &it->second;
}
