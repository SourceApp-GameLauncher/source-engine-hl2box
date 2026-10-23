#ifndef PHYSCONSTRAINT_LUA_H
#define PHYSCONSTRAINT_LUA_H

class CBaseEntity;
class IPhysicsObject;
class IPhysicsConstraintGroup;

void HL2SB_SetPhysConstraintSystem(CBaseEntity *system);
void HL2SB_CapturePhysConstraintSystem(CBaseEntity *constraint);
void HL2SB_SetPhysConstraintObjects(CBaseEntity *constraint, IPhysicsObject *first, IPhysicsObject *second);
bool HL2SB_GetPhysConstraintObject(CBaseEntity *constraint, int index, IPhysicsObject **object);
IPhysicsObject *HL2SB_GetAttachedPhysicsObject(CBaseEntity *constraint, CBaseEntity *attached);
IPhysicsConstraintGroup *HL2SB_GetPhysConstraintGroup(CBaseEntity *constraint);
void HL2SB_ClearPhysConstraintData(CBaseEntity *constraint);

#endif
