#include "heightmesh.hpp"

#include "assets/cmdfile.hpp"

std::shared_ptr<mg::HeightMesh> mg::HeightMesh::Load(const std::string& name)
{
    return LoadFromFile("data/" + name + ".hm");
}

std::shared_ptr<mg::HeightMesh> mg::HeightMesh::LoadFromFile(const std::string& name)
{
    auto hm = std::make_shared<HeightMesh>();

    assets::LoadCMDFile(name, [&](const std::string& command, CmdLineStream& iss) {
        if (command == "hv")
        {
            auto& v = hm->verts_.emplace_back(0.0f);
            iss >> v.x >> v.y >> v.z;
        }
        else if (command == "ht")
        {
            auto& t = hm->tris_.emplace_back();
            iss >> t[0] >> t[1] >> t[2];
        }
    });

    return hm;
}
