#pragma once

#include "utils/transform.hpp"
#include <btBulletDynamicsCommon.h>

namespace collision
{

class MotionState : public btMotionState
{
public:
    MotionState(Transform& transform) : transform_(transform) {}

    void getWorldTransform(btTransform& bt_trans) const override {
        bt_trans = transform_.ToBtTransform();
    }

    void setWorldTransform(const btTransform& bt_trans) override {
        transform_.SetBtTransform(bt_trans);
    }

private:
    Transform& transform_;
};

}