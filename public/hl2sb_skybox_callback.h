#ifndef HL2SB_SKYBOX_CALLBACK_H
#define HL2SB_SKYBOX_CALLBACK_H

#define HL2SB_SKYBOX_CALLBACK_VERSION "HL2SBSkyboxCallback001"

abstract_class IHL2SBSkyboxCallback
{
public:
	virtual void PostDraw2DSkyBox() = 0;
};

#endif
