#include "texture.hpp"
#include "utils/files.hpp"
#include "utils/image.hpp"
#include "assets/cmdfile.hpp"

gfx::Texture::Texture(const TextureDescriptor& desc)
{
    id_ = Renderer::GetInstance().CreateTexture(desc);
}

std::shared_ptr<gfx::Texture> gfx::Texture::Load(const std::string& name)
{
    return LoadFromFile("data/" + name + ".png");
}

std::shared_ptr<gfx::Texture> gfx::Texture::LoadFromFile(const std::string& filename)
{
    printf("Loading texture from file: %s\n", filename.c_str());

    auto image = LoadImage(filename);

    bool mipmaps = true;
    bool linear = false;

    std::string config_path = filename + ".cfg";
    if (fs::FileExists(config_path))
    {
        assets::LoadCMDFile(config_path, [&](const std::string& command, CmdLineStream& iss) {
            if (command == "nomipmaps")
            {
                mipmaps = false;
            }
            else if (command == "linear")
            {
                linear = true;
            }
        });
    }

    TextureDescriptor desc{};
    desc.width = image.width;
    desc.height = image.height;
    desc.filter = linear ? TEXTURE_FILTER_LINEAR : TEXTURE_FILTER_NEAREST;
    desc.mipmaps = mipmaps ? TEXTURE_MIPMAP_TYPE_LINEAR : TEXTURE_MIPMAP_TYPE_NONE;

    auto texture = std::make_shared<Texture>(desc);
    texture->SetData(image.data);

    return texture;
}

void gfx::Texture::SetData(std::span<const uint8_t> data)
{
    Renderer::GetInstance().SetTextureData(id_, data);
}

gfx::Texture::~Texture()
{
    Renderer::GetInstance().ReleaseTexture(id_);
}

