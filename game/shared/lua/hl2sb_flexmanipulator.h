#ifndef HL2SB_FLEXMANIPULATOR_H
#define HL2SB_FLEXMANIPULATOR_H

class CBaseEntity;

bool HL2SB_HasFlexManipulator(CBaseEntity *entity);
float HL2SB_GetFlexWeight(CBaseEntity *entity, int index);
void HL2SB_SetFlexWeight(CBaseEntity *entity, int index, float weight);
float HL2SB_GetFlexScale(CBaseEntity *entity);
void HL2SB_SetFlexScale(CBaseEntity *entity, float scale);
#ifdef CLIENT_DLL
bool HL2SB_ApplyFlexManipulation(CBaseEntity *entity, int count, float *weights, float *delayed);
#endif

#endif
