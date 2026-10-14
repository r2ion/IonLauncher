#pragma once

#include "mathlib/vector.h"

struct WeaponSway_Transform
{
    Vector3D translate;
    QAngle rotate;
};

struct WeaponSway_Move
{
    WeaponSway_Transform forward;
    WeaponSway_Transform back;
    WeaponSway_Transform left;
    WeaponSway_Transform right;
    WeaponSway_Transform up;
    WeaponSway_Transform down;
};

struct WeaponSway_Turn
{
    WeaponSway_Transform left;
    WeaponSway_Transform right;
    WeaponSway_Transform up;
    WeaponSway_Transform down;
};

struct WeaponSway_Clamp
{
    char rotateAttachName[80];
    float rotateAttachBlendTime;
    Vector3D translateMin;
    Vector3D translateMax;
    QAngle rotateMin;
    QAngle rotateMax;
    float translateGain;
    float rotateGain;
    float bobCycleTime;
    float bobMinSpeed;
    float bobMaxSpeed;
    float bobVertDist;
    float bobHorzDist;
    float bobPitch;
    float bobYaw;
    float bobRoll;
    float bobGain;
    WeaponSway_Move move;
    WeaponSway_Turn turn;
};

struct WeaponSway_Spec
{
    WeaponSway_Clamp unzoomed;
    WeaponSway_Clamp zoomed;
    bool zoomedSway;
};
