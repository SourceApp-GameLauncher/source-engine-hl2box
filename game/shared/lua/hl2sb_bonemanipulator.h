#ifndef HL2SB_BONEMANIPULATOR_H
#define HL2SB_BONEMANIPULATOR_H

class CBaseEntity;

Vector HL2SB_GetBonePositionOffset(CBaseEntity *entity, int bone);
QAngle HL2SB_GetBoneAngleOffset(CBaseEntity *entity, int bone);
Vector HL2SB_GetBoneScale(CBaseEntity *entity, int bone);
void HL2SB_SetBonePositionOffset(CBaseEntity *entity, int bone, const Vector &value, bool network = true);
void HL2SB_SetBoneAngleOffset(CBaseEntity *entity, int bone, const QAngle &value, bool network = true);
void HL2SB_SetBoneScale(CBaseEntity *entity, int bone, const Vector &value, bool network = true);

#endif
