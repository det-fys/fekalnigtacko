#include "item.hpp"

#include "cache.hpp"
#include "cmdfile.hpp"

std::shared_ptr<assets::Item> assets::Item::LoadFromFile(const std::string& path)
{
    auto item = std::make_shared<Item>();

    LoadCMDFile(path, [&](const std::string& command, std::istringstream& iss) {
        if (command == "type")
        {
            std::string type_str;
            iss >> type_str;

            if (type_str == "consumable")
                item->type = ITEM_CONSUMABLE;
            else if (type_str == "weapon")
                item->type = ITEM_WEAPON;
            else
                throw std::runtime_error("Unknown item type " + type_str);
        }
        else if (command == "name")
        {
            iss >> item->name;
        }
        else if (command == "displayname")
        {
            item->displayname = ParseString(iss);
        }
        else if (command == "anim")
        {
            std::string anim_type, anim_name;
            iss >> anim_type >> anim_name;

            if (anim_type == "idle")
                item->idle_anim = anim_name;
            else if (anim_type == "raise")
                item->raise_anim = anim_name;
            else if (anim_type == "use" || anim_type == "fire")
                item->use_anim = anim_name;
            else if (anim_type == "aim")
                item->aim_anim = anim_name;
            else if (anim_type == "aiming")
                item->aiming_anim = anim_name;
            else if (anim_type == "reload")
                item->reload_anim = anim_name;
            else if (anim_type == "legs")
                item->legs_anim = anim_name;
            else
                throw std::runtime_error("Unknown item anim type " + anim_type);
        }
        else if (command == "model")
        {
            iss >> item->model_name;
            item->model = CacheManager::GetModel("data/" + item->model_name + ".mdl");    
        }
        else if (command == "attach")
        {
            std::string target;
            iss >> target;

            if (target == "loc")
            {
                std::string sk_name, loc_name;
                iss >> sk_name >> loc_name;
                
                auto sk = assets::CacheManager::GetSkeleton("data/" + sk_name + ".sk");
                auto loc = sk->GetLocation(loc_name);
                if (!loc)
                    throw std::runtime_error("Invalid skeleton location: " + loc_name);

                item->bone = loc->bone_name;
                item->bone_offset = loc->offset;
            }
            else
            {
                item->bone = target;
                glm::vec3 position;
                glm::vec3 angles;
                iss >> position.x >> position.y >> position.z >> angles.x >> angles.y >> angles.z;
    
                item->bone_offset.position = position;
                item->bone_offset.rotation = glm::quat(glm::radians(angles));
            }

            // ParseTransform(iss, item->bone_offset);
        }
        else if (command == "weapontype")
        {
            std::string type_str;
            iss >> type_str;

            if (type_str == "manual")
                item->weapon_type = WEAPON_MANUAL;
            else if (type_str == "semiauto")
                item->weapon_type = WEAPON_SEMIAUTO;
            else if (type_str == "auto")
                item->weapon_type = WEAPON_AUTO;
            else
                throw std::runtime_error("Unknown weapon type " + type_str);
        }
        else if (command == "firetype")
        {
            std::string type_str;
            iss >> type_str;   

            if (type_str == "melee")
                item->fire_type = FIRETYPE_MELEE;
            else if (type_str == "bullet")
                item->fire_type = FIRETYPE_BULLET;
            else if (type_str == "projectile")
                item->fire_type = FIRETYPE_PROJECTILE;
            else
                throw std::runtime_error("Unknown weapon type: " + type_str);
        }
        else if (command == "ammotype")
        {
            iss >> item->ammo_type;
        }
        else if (command == "clipsize")
        {
            iss >> item->clip_size;
        }
        else if (command == "firedelay")
        {
            iss >> item->fire_delay;
        }
        else if (command == "firesnd")
        {
            iss >> item->fire_snd;
        }
        else if (command == "firefx")
        {
            iss >> item->fire_fx >> item->fire_fx_loc;
        }
        else if (command == "dispersion")
        {
            iss >> item->dispersion_min >> item->dispersion_max >> item->dispersion_shot >> item->dispersion_decay;
        }
        else if (command == "slot")
        {
            iss >> item->slot;
        }
        else if (command == "twohanded")
        {
            item->twohanded = true;
        }
        else if (command == "walkspeedmult")
        {
            iss >> item->walk_speed_mult;
        }
        else if (command == "damage")
        {
            iss >> item->damage;
        }
        else if (command == "aimtype")
        {
            std::string aimtype_str;
            iss >> aimtype_str;

            if (aimtype_str == "none")
                item->aim_type = AIMTYPE_NONE;
            else if (aimtype_str == "crosshair")
                item->aim_type = AIMTYPE_CROSSHAIR;
            else if (aimtype_str == "scope")
                item->aim_type = AIMTYPE_SCOPE;
            else
                throw std::runtime_error("Invalid aim type: " + aimtype_str);
        }
        else
        {
            throw std::runtime_error("Unknown item command: " + command);
        }
    });

    if (item->displayname.empty())
    {
        item->displayname = item->name;
    }
    
    return item;
}