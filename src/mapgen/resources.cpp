#include "resources.hpp"

#include <set>

#include "assets/asset_manager.hpp"
#include "assets/model.hpp"

std::shared_ptr<mg::ResourceSet> mg::ResourceSet::Load(const std::string& path)
{
    return LoadFromFile("data/" + path + ".mg");
}

static mg::TemplateMaterialRef GetMaterialRef(const mg::ResourceSet& res, const std::string& name)
{
    mg::TemplateMaterialRef ref{};
    if (name == "terrain")
    {
        ref.type = mg::TPL_MATERIAL_TERRAIN;
    }
    else if (name == "heightmesh")
    {
        ref.type = mg::TPL_MATERIAL_HEIGHTMESH;
    }
    else
    {
        ref.type = mg::TPL_MATERIAL_NORMAL;
        ref.id = res.GetMaterialIndexByName(name);
    }
    return ref;
}

static mg::TemplateProfile LoadTemplateProfile(const mg::ResourceSet& res, CmdLineStream& iss)
{
    mg::TemplateProfile profile;
    iss >> profile.name;

    auto model = assets::AssetManager::GetInstance().Get<assets::Model>("tpl_" + profile.name);

    const auto& surfaces = model->GetSurfaces();
    const auto& verts = model->GetVertices();
    const auto& tris = model->GetTriangles();

    struct Quad
    {
        std::array<uint32_t, 4> v{}; // 0: bot-left, 1: bot-right, 2: top-left, 3: top-right
        mg::TemplateMaterialRef material{};
    };

    std::vector<Quad> quads;

    // A diagonal edge spans across both X and Y
    auto is_diagonal_edge = [&](uint32_t u, uint32_t v) {
        const auto& p1 = verts.positions[u];
        const auto& p2 = verts.positions[v];
        return std::abs(p1.x - p2.x) > 0.001f && std::abs(p1.y - p2.y) > 0.001f;
    };

    using Edge = std::pair<uint32_t, uint32_t>;
    auto make_edge = [](uint32_t u, uint32_t v) { return std::make_pair(std::min(u, v), std::max(u, v)); };

    for (const auto& surface : surfaces)
    {
        auto material = GetMaterialRef(res, surface.material.name);

        // Maps diagonal edge -> 3 vertices of the first half-quad triangle
        std::map<Edge, std::array<uint32_t, 3>> diag_map;

        for (uint32_t i = 0; i < surface.tri_count; ++i)
        {
            const auto& tri = tris[surface.tri_offset + i];
            const auto& tv = tri.vertices;

            Edge diag_edge{0, 0};
            bool found_diag = false;

            for (int j = 0; j < 3; ++j)
            {
                uint32_t u = tv[j];
                uint32_t v = tv[(j + 1) % 3];
                if (is_diagonal_edge(u, v))
                {
                    diag_edge = make_edge(u, v);
                    found_diag = true;
                    break;
                }
            }

            if (!found_diag)
            {
                throw std::runtime_error("Invalid profile mesh: triangle has no diagonal edge");
            }

            auto it = diag_map.find(diag_edge);
            if (it != diag_map.end())
            {
                // Find the non-shared vertex in the second triangle
                uint32_t extra_v = (tv[0] != diag_edge.first && tv[0] != diag_edge.second)   ? tv[0]
                                   : (tv[1] != diag_edge.first && tv[1] != diag_edge.second) ? tv[1]
                                                                                             : tv[2];

                std::array<uint32_t, 4> quad_verts = {it->second[0], it->second[1], it->second[2], extra_v};

                // Sort all 4 vertices spatially by Y, then X
                std::sort(quad_verts.begin(), quad_verts.end(), [&](uint32_t a, uint32_t b) {
                    const auto& posA = verts.positions[a];
                    const auto& posB = verts.positions[b];
                    if (std::abs(posA.y - posB.y) > 0.001f)
                    {
                        return posA.y < posB.y;
                    }
                    return posA.x < posB.x;
                });

                Quad quad;
                quad.v = quad_verts;
                quad.material = material;
                quads.push_back(quad);

                diag_map.erase(it);
            }
            else
            {
                diag_map[diag_edge] = {tv[0], tv[1], tv[2]};
            }
        }

        if (!diag_map.empty())
        {
            throw std::runtime_error("Invalid profile mesh: unmatched triangles remaining");
        }
    }

    // Convert quads to profile edges
    std::map<uint32_t, uint32_t> vert_map;
    for (const auto& quad : quads)
    {
        auto& edge = profile.edges.emplace_back();
        edge.material = quad.material;

        // Add bottom-edge vertices (0 and 1) to profile
        for (uint32_t i = 0; i < 2; ++i)
        {
            auto model_vert_idx = quad.v[i];
            auto model_vert_idx_next = quad.v[i + 2];
            
            auto it = vert_map.find(model_vert_idx);
            if (it != vert_map.end())
            {
                edge.verts[i] = it->second;
                continue;
            }

            mg::TemplateProfileVertex profile_vert{};
            // pos
            profile_vert.pos = verts.positions[model_vert_idx];
            profile_vert.pos.y = 0.0f;
            
            // normal
            profile_vert.normal = verts.normals[model_vert_idx];
            
            // uv and calc advance
            profile_vert.uv = verts.uvs[model_vert_idx];
            auto pos_diff_y = verts.positions[model_vert_idx_next].y - verts.positions[model_vert_idx].y;
            auto uv_diff = verts.uvs[model_vert_idx_next] - profile_vert.uv;
            profile_vert.uv_advance = glm::abs(pos_diff_y) > 0.001f ? uv_diff / pos_diff_y : glm::vec2(0.0f, 0.0f);
            
            // add to profile and map
            edge.verts[i] = static_cast<uint32_t>(profile.verts.size());
            profile.verts.push_back(profile_vert);
            vert_map[model_vert_idx] = edge.verts[i];
        }

        float dy = verts.positions[quad.v[2]].y - verts.positions[quad.v[0]].y;
        float duv = verts.uvs[quad.v[2]].y - verts.uvs[quad.v[0]].y;
    }

    // find reversed edges
    for (const auto& edge : profile.edges)
    {
        std::array<glm::vec3, 2> search_pos;
        for (int i = 0; i < 2; ++i)
        {
            search_pos[i] = profile.verts[edge.verts[i]].pos;
            search_pos[i].x = -search_pos[i].x; // x mirror
        }

        bool found = false;

        for (const auto& other_edge : profile.edges)
        {
            if (edge.material != other_edge.material)
                continue;

            if ((glm::length(search_pos[0] - profile.verts[other_edge.verts[1]].pos) < 0.001f) &&
                (glm::length(search_pos[1] - profile.verts[other_edge.verts[0]].pos) < 0.001f))
            {
                mg::TemplateProfileEdge reversed_edge{};
                reversed_edge.verts[0] = other_edge.verts[1];
                reversed_edge.verts[1] = other_edge.verts[0];
                reversed_edge.material = other_edge.material;
                profile.edges_reverse.push_back(reversed_edge);
                
                found = true;
                break;
            }
        }

        if (!found)
        {
            throw std::runtime_error("Invalid profile mesh: no matching reversed edge found (profile asymmetric)");
        }
    }

    return profile;
}

static mg::TemplateMesh LoadTemplateMesh(const mg::ResourceSet& res, CmdLineStream & iss)
{
    mg::TemplateMesh mesh{};
    iss >> mesh.name;

    auto model = assets::AssetManager::GetInstance().Get<assets::Model>("tpl_" + mesh.name);

    const auto& surfaces = model->GetSurfaces();
    const auto& verts = model->GetVertices();
    const auto& tris = model->GetTriangles();

    // copy vertices
    for (uint32_t i = 0; i < verts.positions.size(); ++i)
    {
        mg::TemplateMeshVertex tpl_vert{};
        tpl_vert.pos = verts.positions[i];
        tpl_vert.normal = verts.normals[i];
        tpl_vert.uv = verts.uvs[i];
        tpl_vert.link_weights = { 0.0f, 0.0f, 0.0f, 0.0f };
        mesh.verts.push_back(tpl_vert);
    }

    struct Edge
    {
        std::array<uint32_t, 2> verts;
        mg::TemplateMaterialRef material;

        Edge(uint32_t v0, uint32_t v1, const mg::TemplateMaterialRef& mat) : verts{v0, v1}, material(mat) 
        {
            if (verts[0] > verts[1])
            {
                std::swap(verts[0], verts[1]);
            }
        }

        bool operator<(const Edge& other) const
        {
            return std::tie(verts[0], verts[1], material.type, material.id) <
                   std::tie(other.verts[0], other.verts[1], other.material.type, other.material.id);
        }
    };

    std::set<Edge> edges;

    // copy triangles
    for (const auto& surface : surfaces)
    {
        auto material = GetMaterialRef(res, surface.material.name);
        for (uint32_t i = 0; i < surface.tri_count; ++i)
        {
            const auto& tri = tris[surface.tri_offset + i];
            const auto& tv = tri.vertices;
            mg::TemplateMeshTriangle tpl_tri{};
            tpl_tri.material = material;
            tpl_tri.tri[0] = tv[0];
            tpl_tri.tri[1] = tv[1];
            tpl_tri.tri[2] = tv[2];
            mesh.tris.push_back(tpl_tri);

            // add edges
            edges.insert(Edge(tv[0], tv[1], material));
            edges.insert(Edge(tv[1], tv[2], material));
            edges.insert(Edge(tv[2], tv[0], material));
        }
    }

    // process links
    std::string loc_name = "link0";
    for (uint32_t i = 0; i < 4; ++i)
    {
        loc_name[4] = '0' + i;
        auto loc = model->GetLocation(loc_name);
        if (!loc)
            break;

        mg::TemplateMeshLink link{};

        // TODO: get profile from model param
        link.profile_id = 0;
        auto& profile = res.GetProfiles()[link.profile_id];

        link.pos = loc->position;

        auto loc_matrix = loc->ToMatrix();
        auto inv_loc_matrix = glm::inverse(loc_matrix);
        link.margin = -inv_loc_matrix[3].y;
        link.start_matrix = inv_loc_matrix;
        link.start_matrix[3].y = 0.0f;
        link.end_matrix =
            glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)) * link.start_matrix;

        link.profile_vert_mappings.resize(profile.verts.size(), UINT32_MAX);

        // find matching edges in mesh
        for (uint32_t j = 0; j < profile.edges.size(); ++j)
        {
            const auto& edge = profile.edges[j];

            auto [i0, i1] = edge.verts;
            auto& p0 = profile.verts[i0].pos;
            auto& p1 = profile.verts[i1].pos;

            bool found = false;

            for (const auto& mesh_edge : edges)
            {
                if (mesh_edge.material != edge.material)
                    continue;

                auto& m0 = mesh.verts[mesh_edge.verts[0]].pos;
                auto& m1 = mesh.verts[mesh_edge.verts[1]].pos;
                
                glm::vec3 l0 = inv_loc_matrix * glm::vec4(m0, 1.0f);
                glm::vec3 l1 = inv_loc_matrix * glm::vec4(m1, 1.0f);

                constexpr float max_dist = 0.01f;
                constexpr float max_dist2 = max_dist * max_dist;

                // Test direct alignment
                auto d00 = p0 - l0;
                auto d11 = p1 - l1;
                bool match_direct = (glm::dot(d00, d00) <= max_dist2 && glm::dot(d11, d11) <= max_dist2);

                // Test swapped alignment
                auto d01 = p0 - l1;
                auto d10 = p1 - l0;
                bool match_swapped = (glm::dot(d01, d01) <= max_dist2 && glm::dot(d10, d10) <= max_dist2);

                if (!match_direct && !match_swapped)
                {
                    // does not match either orientation
                    continue;
                }

                bool swapped = match_swapped && !match_direct;

                auto mapped_verts = mesh_edge.verts;
                if (swapped)
                {
                    std::swap(mapped_verts[0], mapped_verts[1]);
                }

                link.profile_vert_mappings[i0] = mapped_verts[0];
                link.profile_vert_mappings[i1] = mapped_verts[1];

                found = true;
                break;
            }
            
            if (!found)
            {
                // no matching edge found
                throw std::runtime_error("No matching edge found");
            }
        }

        mesh.links.emplace_back(std::move(link));
    }

    // calc vertex weights for links
    for (auto& v : mesh.verts)
    {
        for (auto& w : v.link_weights)
        {
            w = 0.0001f;
        }

        if (mesh.links.empty())
            continue;

        uint32_t closest_link = 0;
        float closest_dist2 = std::numeric_limits<float>::max();

        for (uint32_t i = 0; i < mesh.links.size(); ++i)
        {
            //const auto& link = mesh.links[i];
            //auto lpos = link.start_matrix * glm::vec4(v.pos, 1.0f);
            //float weight = 1.0f - glm::clamp(lpos.y / link.margin, 0.0f, 1.0f);
            //v.link_weights[i] = weight;

            auto d = v.pos - mesh.links[i].pos;
            float d2 = glm::dot(d, d);
            if (d2 < closest_dist2)
            {
                closest_dist2 = d2;
                closest_link = i;
            }
        }

        v.link_weights[closest_link] = 1.0f;

        float sum = 0.0f;
        for (auto w : v.link_weights)
        {
            sum += w;
        }

        if (sum > 0.0f)
        {
            for (auto& w : v.link_weights)
            {
                w /= sum;
            }
        }
    }

    return mesh;
}

std::shared_ptr<mg::ResourceSet> mg::ResourceSet::LoadFromFile(const std::string& path)
{
    auto res = std::make_shared<ResourceSet>();

    assets::LoadCMDFile(path, [&](const std::string& command, CmdLineStream& iss) {
        if (command == "mat")
        {
            res->materials_.emplace_back(assets::ParseMaterial(iss));
            res->material_map_[res->materials_.back().name] = static_cast<uint32_t>(res->materials_.size() - 1);
        }
        else if (command == "tpl_profile")
        {
            res->profiles_.emplace_back(LoadTemplateProfile(*res, iss));
            res->profile_map_[res->profiles_.back().name] = static_cast<uint32_t>(res->profiles_.size() - 1);
        }
        else if (command == "tpl_mesh")
        {
            res->meshes_.emplace_back(LoadTemplateMesh(*res, iss));
            res->mesh_map_[res->meshes_.back().name] = static_cast<uint32_t>(res->meshes_.size() - 1);
        }
    });

    return res;
}

const assets::ModelMaterial* mg::ResourceSet::GetMaterialByName(const std::string& name) const
{
    auto it = material_map_.find(name);
    return it != material_map_.end() ? &materials_[it->second] : nullptr;
}

uint32_t mg::ResourceSet::GetMaterialIndexByName(const std::string& name) const
{
    auto it = material_map_.find(name);
    return it != material_map_.end() ? it->second : 0;
}

const mg::TemplateProfile* mg::ResourceSet::GetProfileByName(const std::string& name) const
{
    auto it = profile_map_.find(name);
    return it != profile_map_.end() ? &profiles_[it->second] : nullptr;
}

uint32_t mg::ResourceSet::GetProfileIndexByName(const std::string& name) const
{
    auto it = profile_map_.find(name);
    return it != profile_map_.end() ? it->second : 0;
}

const mg::TemplateMesh* mg::ResourceSet::GetMeshByName(const std::string& name) const
{
    auto it = mesh_map_.find(name);
    return it != mesh_map_.end() ? &meshes_[it->second] : nullptr;
}

uint32_t mg::ResourceSet::GetMeshIndexByName(const std::string& name) const
{
    auto it = mesh_map_.find(name);
    return it != mesh_map_.end() ? it->second : 0;
}
