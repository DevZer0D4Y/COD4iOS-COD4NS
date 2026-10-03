#ifndef KISAK_MP
#error This File is MultiPlayer Only
#endif

#include <universal/q_shared.h>
#include "cg_local_mp.h"
#include "cg_public_mp.h"
#include <xanim/dobj_utils.h>
#include <ragdoll/ragdoll.h>
#include <gfx_d3d/r_scene.h>
#include <universal/profile.h>


// Keep skeleton addresses native on arm64. The old decompilation packed
// base-pose pointers into float4's 32-bit lanes before reading wheel bones.
void CG_VehPoseControllers(const cpose_t *pose, const DObj_s *obj, int32_t *partBits)
{
    if (!obj || !pose)
        return;
    constexpr float shortToAngle = 0.0054931640625f;
    float bodyAngles[3] = {pose->vehicle.pitch * shortToAngle, 0.0f, pose->vehicle.roll * shortToAngle};
    float turretAngles[3] = {0.0f, pose->vehicle.yaw * shortToAngle, 0.0f};
    float barrelAngles[3] = {pose->vehicle.barrelPitch * shortToAngle, 0.0f, 0.0f};
    if (pose->eType == ET_HELICOPTER)
        barrelAngles[2] = pose->turret.barrelPitch;
    float steerAngles[3] = {0.0f, pose->vehicle.steerYaw * shortToAngle, 0.0f};
    DObjSetLocalTag((DObj_s *)obj, partBits, pose->vehicle.tag_body, vec3_origin, bodyAngles);
    DObjSetLocalTag((DObj_s *)obj, partBits, pose->vehicle.tag_turret, vec3_origin, turretAngles);
    DObjSetLocalTag((DObj_s *)obj, partBits, pose->vehicle.tag_barrel, vec3_origin, barrelAngles);

    const XModel *model = DObjGetModel(obj, 0);
    if (!model)
        return;
    const unsigned boneCount = XModelNumBones(model);
    const DObjAnimMat *basePose = XModelGetBasePose(model);
    if (!basePose)
        return;
    for (int wheel = 0; wheel < 4; ++wheel)
    {
        const unsigned bone = pose->vehicle.wheelBoneIndex[wheel];
        if (bone >= 0xFE || bone >= boneCount
            || !DObjSetRotTransIndex((DObj_s *)obj, partBits, bone))
            continue;
        const float fraction = pose->vehicle.wheelFraction[wheel] / 65535.0f;
        const float drop = fraction * (pose->vehicle.time + 40.0f);
        const float minimumDrop = 40.0f - pose->vehicle.time;
        const float suspension = drop >= minimumDrop ? drop : minimumDrop;
        // World up transformed back into the vehicle's orthonormal basis is
        // local Z; the base-pose translation cancels when computing the offset.
        const float offset[3] = {0.0f, 0.0f, 40.0f - suspension};
        DObjSetLocalTagInternal(obj, offset,
            wheel < 2 && steerAngles[1] != 0.0f ? steerAngles : nullptr, bone);
    }
}

void __cdecl CG_DoControllers(const cpose_t *pose, const DObj_s *obj, int32_t *partBits)
{
    int32_t setPartBits[4]; // [esp+34h] [ebp-10h] BYREF

    PROF_SCOPED("CG_DoControllers");

    DObjGetSetBones(obj, setPartBits);
    switch (pose->eType)
    {
    case ET_PLAYER:
        CG_Player_DoControllers(pose, obj, partBits);
        break;
    case ET_MG42:
        CG_mg42_DoControllers(pose, obj, partBits);
        break;
    case ET_HELICOPTER:
    case ET_VEHICLE:
        CG_VehPoseControllers(pose, obj, partBits);
        break;
    default:
        break;
    }
    CG_DoBaseOriginController(pose, obj, setPartBits);
    if (pose->isRagdoll && (pose->ragdollHandle || pose->killcamRagdollHandle))
        Ragdoll_DoControllers(pose, (DObj_s*)obj, partBits);
}

void __cdecl CG_Player_DoControllers(const cpose_t *pose, const DObj_s *obj, int32_t *partBits)
{
    if (pose->player.control)
        BG_Player_DoControllers(&pose->player, obj, partBits);
}

void __cdecl CG_mg42_DoControllers(const cpose_t *pose, const DObj_s *obj, int32_t *partBits)
{
    float angles[3]; // [esp+10h] [ebp-10h] BYREF
    const float *viewAngles; // [esp+1Ch] [ebp-4h]

    if (pose->turret.playerUsing)
    {
        viewAngles = pose->turret.viewAngles;
        angles[0] = AngleDelta(viewAngles[0], pose->angles[0]);
        angles[1] = AngleDelta(viewAngles[1], pose->angles[1]);
    }
    else
    {
        //angles[0] = pose->turret.$9D88A49AD898204B3D6E378457DD8419::angles.pitch;
        angles[0] = pose->turret.angles.pitch;
        //angles[1] = pose->turret.$9D88A49AD898204B3D6E378457DD8419::angles.yaw;
        angles[1] = pose->turret.angles.yaw;
    }
    angles[2] = 0.0;
    DObjSetControlTagAngles((DObj_s*)obj, partBits, pose->turret.tag_aim, angles);
    DObjSetControlTagAngles((DObj_s *)obj, partBits, pose->turret.tag_aim_animated, angles);
    angles[0] = pose->turret.barrelPitch;
    angles[1] = 0.0;
    DObjSetControlTagAngles((DObj_s *)obj, partBits, pose->turret.tag_flash, angles);
}

void __cdecl CG_DoBaseOriginController(const cpose_t *pose, const DObj_s *obj, int32_t *setPartBits)
{
    uint32_t rootBoneMask; // [esp+90h] [ebp-7Ch]
    float baseQuat[4]; // [esp+94h] [ebp-78h] BYREF
    float viewOffset[3]; // [esp+A4h] [ebp-68h] BYREF
    float origin[3]; // [esp+B0h] [ebp-5Ch] BYREF
    int32_t partIndex; // [esp+BCh] [ebp-50h]
    DObjAnimMat animMat; // [esp+C0h] [ebp-4Ch] BYREF
    int32_t rootBoneCount; // [esp+E0h] [ebp-2Ch]
    uint32_t maxHighIndex; // [esp+E4h] [ebp-28h]
    DObjAnimMat *mat; // [esp+E8h] [ebp-24h]
    uint32_t highIndex; // [esp+ECh] [ebp-20h]
    int32_t partBits[7];
    cg_s *cgameGlob;

    rootBoneCount = DObjGetRootBoneCount(obj);
    iassert(rootBoneCount);

    maxHighIndex = --rootBoneCount >> 5;
    for (highIndex = 0; highIndex < maxHighIndex; ++highIndex)
    {
        if (setPartBits[highIndex] != -1)
            goto notSet;
    }

    rootBoneMask = 0xFFFFFFFF >> ((rootBoneCount & 0x1F) + 1);
    if ((rootBoneMask | setPartBits[maxHighIndex]) == 0xFFFFFFFF)
        return;
notSet:
    mat = DObjGetRotTransArray(obj);
    if (mat)
    {
        AnglesToQuat(pose->angles, baseQuat);
        memset(partBits, 0, sizeof(partBits));
        partBits[3] = 0x80000000;
        cgameGlob = CG_GetLocalClientGlobals(R_GetLocalClientNum());
        viewOffset[0] = cgameGlob->refdef.viewOffset[0];
        viewOffset[1] = cgameGlob->refdef.viewOffset[1];
        viewOffset[2] = cgameGlob->refdef.viewOffset[2];
        partIndex = 0;
        while (partIndex <= rootBoneCount)
        {
            highIndex = partIndex >> 5;
            if ((setPartBits[partIndex >> 5] & partBits[3]) == 0)
            {
                if (DObjSetRotTransIndex((DObj_s*)obj, &partBits[3 - highIndex], partIndex))
                {
                    mat->quat[0] = baseQuat[0];
                    mat->quat[1] = baseQuat[1];
                    mat->quat[2] = baseQuat[2];
                    mat->quat[3] = baseQuat[3];

                    origin[0] = pose->origin[0];
                    origin[1] = pose->origin[1];
                    origin[2] = pose->origin[2];
                }
                else
                {
                    animMat.quat[0] = baseQuat[0];
                    animMat.quat[1] = baseQuat[1];
                    animMat.quat[2] = baseQuat[2];
                    animMat.quat[3] = baseQuat[3];
                    DObjSetTrans(&animMat, pose->origin);
                    float len = Vec4LengthSq(animMat.quat);
                    if (len == 0.0f)
                    {
                        animMat.quat[3] = 1.0f;
                        animMat.transWeight = 2.0f;
                    }
                    else
                    {
                        animMat.transWeight = 2.0f / len;
                    }
                    QuatMultiplyEquals(baseQuat, mat->quat);
                    MatrixTransformVectorQuatTrans(mat->trans, &animMat, origin);
                }
                Vec3Sub(origin, viewOffset, origin);
                DObjSetTrans(mat, origin);
            }
            ++partIndex;
            partBits[3] = (partBits[3] << 31) | ((uint32_t)partBits[3] >> 1);
            ++mat;
        }
    }
}

DObjAnimMat *__cdecl CG_DObjCalcPose(const cpose_t *pose, const DObj_s *obj, int32_t *partBits)
{
    DObjAnimMat *boneMatrix; // [esp+0h] [ebp-4h] BYREF

    iassert(obj);
    iassert(pose);

    if (!CL_DObjCreateSkelForBones(obj, partBits, &boneMatrix))
    {
        DObjCompleteHierarchyBits(obj, partBits);
        CG_DoControllers(pose, obj, partBits);
        DObjCalcSkel(obj, partBits);
    }

    return boneMatrix;
}
