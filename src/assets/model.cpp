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

assets::Model::Model(ModelDescriptor desc)
    : skeleton_(std::move(desc.skeleton)), vertices_(std::move(desc.verts)), tris_(std::move(desc.tris)),
      surfaces_(std::move(desc.surfaces)), col_offset_(desc.col_offset), params_(std::move(desc.params)),
      locations_(std::move(desc.locations))
{
    // surface map
    for (size_t i = 0; i < surfaces_.size(); ++i)
    {
        surface_indices_[surfaces_[i].name] = i;
    }

    // aabb
    for (const auto& v : vertices_.positions)
    {
        aabb_.AddPoint(v);
    }

    // tri mesh
    if (desc.make_triangle_mesh)
    {
        cmesh_ = std::make_unique<collision::TriangleMesh>();
        for (const auto& col_surface : desc.col_surfaces)
        {
            if (col_surface.material == collision::PM_NONE)
                continue;

            if (col_surface.tri_count == 0)
                continue;

            cmesh_->BeginMaterial(col_surface.material);

            for (size_t i = 0; i < col_surface.tri_count; ++i)
            {
                const auto& tri = tris_[col_surface.tri_offset + i];
                glm::vec3 p[3];
                for (size_t j = 0; j < 3; ++j)
                {
                    size_t index = tri.vertices[j];
                    if (index >= vertices_.positions.size())
                        throw std::runtime_error("Vertex index out of bounds in model");
                    p[j] = vertices_.positions[index] - col_offset_;
                }
                cmesh_->AddTriangle(p[0], p[1], p[2]);
            }
        }

        cmesh_->Build();
    }

    // convex hull
    if (desc.make_convex_hull)
    {
        auto temp_hull = std::make_unique<btConvexHullShape>();

        for (const auto& v : vertices_.positions)
        {
            auto offset_pos = v - col_offset_;
            temp_hull->addPoint(btVector3(offset_pos.x, offset_pos.y, offset_pos.z), false);
        }

        temp_hull->recalcLocalAabb();

        auto shape_hull = std::make_unique<btShapeHull>(temp_hull.get());
        shape_hull->buildHull(temp_hull->getMargin());

        cshape_ = std::make_unique<btConvexHullShape>((btScalar*)shape_hull->getVertexPointer(),
                                                      shape_hull->numVertices(), sizeof(btVector3));
    }
    else if (!desc.col_shapes.empty())
    {
        auto compound = std::make_unique<btCompoundShape>();

        for (const auto& col_shape : desc.col_shapes)
        {
            std::unique_ptr<btCollisionShape> shape;
            switch (col_shape.type)
            {
            case MODEL_COLLISION_SHAPE_BOX:
                shape = std::make_unique<btBoxShape>(btVector3(col_shape.size.x, col_shape.size.y, col_shape.size.z));
                break;
            case MODEL_COLLISION_SHAPE_SPHERE:
                shape = std::make_unique<btSphereShape>(col_shape.size.x);
                break;
            default:
                throw std::runtime_error("Unknown collision shape type in model");
            }

            auto transform = col_shape.transform;
            transform.position -= col_offset_;

            compound->addChildShape(transform.ToBtTransform(), shape.get());
            subshapes_.emplace_back(std::move(shape));
        }

        cshape_ = std::move(compound);
    }

    if (cshape_)
    {
        collision::SetShapeMaterial(*cshape_, desc.col_material);
        if (desc.col_material != collision::PM_NONE)
            cshape_is_bullet_target_ = true;
    }
}

std::shared_ptr<assets::Model> assets::Model::Load(const std::string& name)
{
    return LoadFromFile("data/" + name + ".mdl");
}

std::shared_ptr<assets::Model> assets::Model::LoadFromFile(const std::string& filename)
{
    ModelDescriptor desc{};

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

            auto& vert_data = desc.verts;
            vert_data.positions.emplace_back(pos);
            vert_data.normals.emplace_back(normal);
            vert_data.uvs.emplace_back(uv);

            if (desc.skeleton)
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
        }
        else if (command == "f")
        {
            gfx::MeshTriangle t;
            iss >> t.vertices[0] >> t.vertices[1] >> t.vertices[2];
            
#ifdef CLIENT
            if (desc.surfaces.empty())
            {
                throw std::runtime_error("Face without surface in model");
            }

            desc.tris.emplace_back(t);
            ++desc.surfaces.back().tri_count;
#endif // CLIENT

            if (!desc.col_surfaces.empty())
            {
                ++desc.col_surfaces.back().tri_count;
            }
        }
        else if (command == "surface")
        {
            std::string surface_name;
            iss >> surface_name;

            uint32_t first = 0;
            if (!desc.surfaces.empty())
            {
                first = desc.surfaces.back().tri_offset + desc.surfaces.back().tri_count;
            }

            auto& surface = desc.surfaces.emplace_back();
            surface.tri_offset = first;
            surface.name = surface_name;

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
            desc.make_triangle_mesh = true;
        }
        else if (command == "makeconvexhull")
        {
            desc.make_convex_hull = true;
        }
        else if (command == "skeleton")
        {
            std::string skel_name;
            iss >> skel_name;
            desc.skeleton = AssetManager::GetInstance().Get<Skeleton>(skel_name);
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

            ModelCollisionShape model_shape{};
            model_shape.size = scale;
            model_shape.transform = trans;

            if (shape_type == "box")
            {
                model_shape.type = MODEL_COLLISION_SHAPE_BOX;
            }
            else if (shape_type == "sphere")
            {
                model_shape.type = MODEL_COLLISION_SHAPE_SPHERE;
            }
            else
            {
                throw std::runtime_error("Unknown collision shape type: " + shape_type);
            }

            desc.col_shapes.emplace_back(model_shape);
        }
        else if (command == "centerofmass")
        {
            glm::vec3 com;
            iss >> com.x >> com.y >> com.z;
            desc.col_offset = com;
        }
        else if (command == "param")
        {
            std::string key, val;
            iss >> key >> val;
            desc.params[key] = val;
        }
        else if (command == "pm")
        {
            std::string pm_name;
            iss >> pm_name;

            ModelCollisionSurface col_surface{};
            col_surface.material = GetMaterialByName(pm_name);

            if (!desc.col_surfaces.empty())
            {
                col_surface.tri_offset = desc.col_surfaces.back().tri_offset + desc.col_surfaces.back().tri_count;
            }

            desc.col_surfaces.emplace_back(col_surface);
        }
        else if (command == "cpm")
        {
            std::string pm_name;
            iss >> pm_name;
            desc.col_material = GetMaterialByName(pm_name);
        }
        else if (command == "loc")
        {
            std::string loc_name;
            iss >> loc_name;
            ParseTransform(iss, desc.locations[loc_name]);
        }
        else
        {
            throw std::runtime_error("Unknown command in model file: " + command);
        }
    });
    
    return std::make_shared<Model>(std::move(desc));
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
