#pragma once

#include "entityview.hpp"
#include "game/marker_info.hpp"
#include "assets/model.hpp"

namespace game::view
{

class MarkerView : public EntityView
{
public:
    using Super = EntityView;

    MarkerView(WorldView& world, net::InMessage& msg);

    // virtual bool ProcessMsg(net::EntMsgType type, net::InMessage& msg) override;
    // virtual bool ProcessUpdateMsg(net::InMessage* msg) override;
    virtual void Update(const UpdateInfo& info) override;
    virtual void Draw(const DrawArgs& args) override;

private:
    bool Init(net::InMessage& msg);

    void DrawModel(const DrawArgs& args, const assets::Model& model, const TransformNode& node);

private:
    MarkerType marker_type_;
    glm::vec4 color_;

    std::shared_ptr<const assets::Model> base_model_;
    std::shared_ptr<const assets::Model> model_;
    TransformNode icon_node_;

};


}