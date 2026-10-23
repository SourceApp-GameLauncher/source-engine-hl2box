#ifndef HL2SB_MOTIONCONTROLLER_H
#define HL2SB_MOTIONCONTROLLER_H
class CBaseEntity;
class IPhysicsObject;
void HL2SB_StartMotionController(CBaseEntity *entity);
void HL2SB_StopMotionController(CBaseEntity *entity);
void HL2SB_EditMotionController(CBaseEntity *entity, IPhysicsObject *object, bool attach);
#endif
