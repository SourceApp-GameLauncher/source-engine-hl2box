//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#define liclientshadowmgr_cpp

#include "cbase.h"
#include "luamanager.h"
#include "mathlib/lvector.h"
#include "materialsystem/imaterialsystem.h"  // FlashlightState_t, materials
#include "materialsystem/itexture.h"         // ITexture (AddRef/Release on SetTexture)
#include "texture_group_names.h"             // TEXTURE_GROUP_OTHER
#include "lColor.h"                          // luaL_checkcolor / lua_pushcolor
#include "litexture.h"                       // luaL_checkitexture / lua_pushitexture

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"



static int g_pClientShadowMgr_AdvanceFrame (lua_State *L) {
  g_pClientShadowMgr->AdvanceFrame();
  return 0;
}

static int g_pClientShadowMgr_GetShadowDirection (lua_State *L) {
  Vector v = g_pClientShadowMgr->GetShadowDirection();
  lua_pushvector(L, v);
  return 1;
}

static int g_pClientShadowMgr_RenderShadowTexture (lua_State *L) {
  g_pClientShadowMgr->RenderShadowTexture(luaL_checkint(L, 1), luaL_checkint(L, 2));
  return 0;
}

static int g_pClientShadowMgr_SetShadowBlobbyCutoffArea (lua_State *L) {
  g_pClientShadowMgr->SetShadowBlobbyCutoffArea(luaL_checknumber(L, 1));
  return 0;
}

static int g_pClientShadowMgr_SetShadowColor (lua_State *L) {
  g_pClientShadowMgr->SetShadowColor(luaL_checkint(L, 1), luaL_checkint(L, 2), luaL_checkint(L, 3));
  return 0;
}

static int g_pClientShadowMgr_SetShadowDirection (lua_State *L) {
  g_pClientShadowMgr->SetShadowDirection(luaL_checkvector(L, 1));
  return 0;
}

static int g_pClientShadowMgr_SetShadowDistance (lua_State *L) {
  g_pClientShadowMgr->SetShadowDistance(luaL_checknumber(L, 1));
  return 0;
}

static int g_pClientShadowMgr_SetShadowsDisabled (lua_State *L) {
  g_pClientShadowMgr->SetShadowsDisabled(luaL_checkboolean(L, 1));
  return 0;
}

static int g_pClientShadowMgr_UnlockAllShadowDepthTextures (lua_State *L) {
  g_pClientShadowMgr->UnlockAllShadowDepthTextures();
  return 0;
}


static const luaL_Reg g_pClientShadowMgrlib[] = {
  {"AdvanceFrame",   g_pClientShadowMgr_AdvanceFrame},
  {"GetShadowDirection",   g_pClientShadowMgr_GetShadowDirection},
  {"RenderShadowTexture",   g_pClientShadowMgr_RenderShadowTexture},
  {"SetShadowBlobbyCutoffArea",   g_pClientShadowMgr_SetShadowBlobbyCutoffArea},
  {"SetShadowColor",   g_pClientShadowMgr_SetShadowColor},
  {"SetShadowDirection",   g_pClientShadowMgr_SetShadowDirection},
  {"SetShadowDistance",   g_pClientShadowMgr_SetShadowDistance},
  {"UnlockAllShadowDepthTextures",   g_pClientShadowMgr_UnlockAllShadowDepthTextures},
  {NULL, NULL}
};


// ---------------------------------------------------------------------------
// HL2SB: ProjectedTexture() -- GMod's client-side spotlight object.
//
// Engine side it is the same flashlight handle the env_projectedtexture
// entity drives (CreateFlashlight / UpdateFlashlightState /
// DestroyFlashlight on g_pClientShadowMgr).  The Lua object stores the light
// parameters; Update() folds them into one FlashlightState_t and pushes the
// state to the shadow manager, so every setter takes effect on the next
// Update -- which is exactly how the nvg addon drives its NVG light
// (set parameters, call Update every frame while the goggles are down,
// Remove() when they flip up or the battery dies).
//
// FindMetaTable("ProjectedTexture") resolves through the alias table in
// lsrcinit, so only the metatable name and the global constructor are
// registered here (luaopen_g_pClientShadowMgr, client realm only).
// ---------------------------------------------------------------------------
#define HL2SB_PROJECTEDTEXTURE_METATABLE "ProjectedTexture"

struct HL2SB_ProjectedTexture_t
{
  Vector        origin;
  QAngle        angles;
  float         fovH;
  float         fovV;
  float         brightness;
  Color         color;
  bool          enableShadows;
  float         nearZ;
  float         farZ;
  float         quadraticAtten;
  float         linearAtten;
  float         constantAtten;
  bool          lightWorld;
  float         shadowSlopeScaleDepthBias;
  float         shadowDepthBias;
  int           shadowQuality;
  char          textureName[256];
  int           textureFrame;
  ITexture     *pTexture;   // borrowed ITexture (AddRef'd) when SetTexture got one
  bool          alive;      // cleared by Remove(); the light may exist before first Update
  ClientShadowHandle_t handle;
};

static HL2SB_ProjectedTexture_t *HL2SB_ProjectedTexture_Check( lua_State *L, int nArg )
{
  return (HL2SB_ProjectedTexture_t *)luaL_checkudata( L, nArg, HL2SB_PROJECTEDTEXTURE_METATABLE );
}

static void HL2SB_ProjectedTexture_Destroy( HL2SB_ProjectedTexture_t *pPT )
{
  if ( pPT->handle != CLIENTSHADOW_INVALID_HANDLE && g_pClientShadowMgr != NULL )
  {
    g_pClientShadowMgr->DestroyFlashlight( pPT->handle );
  }
  pPT->handle = CLIENTSHADOW_INVALID_HANDLE;

  if ( pPT->pTexture != NULL )
  {
    pPT->pTexture->Release();
    pPT->pTexture = NULL;
  }
  pPT->alive = false;
}

static void HL2SB_ProjectedTexture_Apply( HL2SB_ProjectedTexture_t *pPT )
{
  if ( !pPT->alive || g_pClientShadowMgr == NULL )
    return;

  FlashlightState_t state;
  Vector fwd, right, up;
  AngleVectors( pPT->angles, &fwd, &right, &up );

  // Reference Update() clamps the projected frustum on every push: FOV to
  // [0.1, 179.9] degrees, near plane above zero, far plane past the near
  // plane -- a zeroed or inverted parameter cannot degenerate the cone.  The
  // clamped values are locals only; getters keep reporting what the script
  // set, matching the reference behaviour.
  float flNearZ = max( pPT->nearZ, 0.01f );
  float flFarZ = max( pPT->farZ, flNearZ + 0.01f );

  state.m_vecLightOrigin = pPT->origin;
  BasisToQuaternion( fwd, right, up, state.m_quatOrientation );
  state.m_fHorizontalFOVDegrees = clamp( pPT->fovH, 0.1f, 179.9f );
  state.m_fVerticalFOVDegrees = clamp( pPT->fovV, 0.1f, 179.9f );
  state.m_fQuadraticAtten = pPT->quadraticAtten;
  state.m_fLinearAtten = pPT->linearAtten;
  state.m_fConstantAtten = pPT->constantAtten;
  // GMod stores the colour premultiplied by brightness; same result here.
  state.m_Color[0] = ( pPT->color.r() / 255.0f ) * pPT->brightness;
  state.m_Color[1] = ( pPT->color.g() / 255.0f ) * pPT->brightness;
  state.m_Color[2] = ( pPT->color.b() / 255.0f ) * pPT->brightness;
  state.m_Color[3] = 1.0f;
  state.m_NearZ = flNearZ;
  state.m_FarZ = flFarZ;
  state.m_bEnableShadows = pPT->enableShadows;
  state.m_flShadowSlopeScaleDepthBias = pPT->shadowSlopeScaleDepthBias;
  state.m_flShadowDepthBias = pPT->shadowDepthBias;
  state.m_nShadowQuality = pPT->shadowQuality;
  state.m_pSpotlightTexture = ( pPT->pTexture != NULL )
    ? pPT->pTexture
    : materials->FindTexture( pPT->textureName, TEXTURE_GROUP_OTHER, false );
  state.m_nSpotlightTextureFrame = pPT->textureFrame;

  if ( pPT->handle == CLIENTSHADOW_INVALID_HANDLE )
    pPT->handle = g_pClientShadowMgr->CreateFlashlight( state );
  else
    g_pClientShadowMgr->UpdateFlashlightState( pPT->handle, state );

  g_pClientShadowMgr->SetFlashlightTarget( pPT->handle, NULL );
  g_pClientShadowMgr->SetFlashlightLightWorld( pPT->handle, pPT->lightWorld );
  g_pClientShadowMgr->UpdateProjectedTexture( pPT->handle, true );
}

static int HL2SB_ProjectedTexture_Create( lua_State *L )
{
  HL2SB_ProjectedTexture_t *pPT =
    (HL2SB_ProjectedTexture_t *)lua_newuserdata( L, sizeof( HL2SB_ProjectedTexture_t ) );

  pPT->origin.Init();
  pPT->angles.Init();
  pPT->fovH = 90.0f;
  pPT->fovV = 90.0f;
  pPT->brightness = 1.0f;
  pPT->color.SetColor( 255, 255, 255, 255 );
  pPT->enableShadows = false;
  // Reference constructor defaults (aligned 2026-10-13): near plane 4,
  // far plane 750, stock flashlight cookie when no SetTexture arrives.
  // The NVG addon overrides the texture and the far plane itself
  // (SetTexture "effects/flashlight/soft", SetFarZ 10000), so these only
  // shape scripts that rely on the defaults.
  pPT->nearZ = 4.0f;
  pPT->farZ = 750.0f;
  pPT->quadraticAtten = 0.0f;
  pPT->linearAtten = 100.0f;
  pPT->constantAtten = 0.0f;
  pPT->lightWorld = true;
  pPT->shadowSlopeScaleDepthBias = 2.0f;
  pPT->shadowDepthBias = 0.0001f;
  pPT->shadowQuality = 1;
  Q_strncpy( pPT->textureName, "effects/flashlight001", sizeof( pPT->textureName ) );
  pPT->textureFrame = 0;
  pPT->pTexture = NULL;
  pPT->alive = true;
  pPT->handle = CLIENTSHADOW_INVALID_HANDLE;

  luaL_getmetatable( L, HL2SB_PROJECTEDTEXTURE_METATABLE );
  lua_setmetatable( L, -2 );
  return 1;
}

// -- position / orientation ------------------------------------------------

static int HL2SB_ProjectedTexture_SetPos( lua_State *L )
{
  HL2SB_ProjectedTexture_Check( L, 1 )->origin = luaL_checkvector( L, 2 );
  return 0;
}

static int HL2SB_ProjectedTexture_GetPos( lua_State *L )
{
  lua_pushvector( L, HL2SB_ProjectedTexture_Check( L, 1 )->origin );
  return 1;
}

static int HL2SB_ProjectedTexture_SetAngles( lua_State *L )
{
  HL2SB_ProjectedTexture_Check( L, 1 )->angles = luaL_checkangle( L, 2 );
  return 0;
}

static int HL2SB_ProjectedTexture_GetAngles( lua_State *L )
{
  lua_pushangle( L, HL2SB_ProjectedTexture_Check( L, 1 )->angles );
  return 1;
}

// -- field of view ---------------------------------------------------------

static int HL2SB_ProjectedTexture_SetFOV( lua_State *L )
{
  HL2SB_ProjectedTexture_t *pPT = HL2SB_ProjectedTexture_Check( L, 1 );
  const float fov = (float)luaL_checknumber( L, 2 );
  pPT->fovH = fov;   // GMod's SetFOV writes both axes.
  pPT->fovV = fov;
  return 0;
}

static int HL2SB_ProjectedTexture_SetHorizontalFOV( lua_State *L )
{
  HL2SB_ProjectedTexture_Check( L, 1 )->fovH = (float)luaL_checknumber( L, 2 );
  return 0;
}

static int HL2SB_ProjectedTexture_GetHorizontalFOV( lua_State *L )
{
  lua_pushnumber( L, HL2SB_ProjectedTexture_Check( L, 1 )->fovH );
  return 1;
}

static int HL2SB_ProjectedTexture_SetVerticalFOV( lua_State *L )
{
  HL2SB_ProjectedTexture_Check( L, 1 )->fovV = (float)luaL_checknumber( L, 2 );
  return 0;
}

static int HL2SB_ProjectedTexture_GetVerticalFOV( lua_State *L )
{
  lua_pushnumber( L, HL2SB_ProjectedTexture_Check( L, 1 )->fovV );
  return 1;
}

// -- colour / brightness ---------------------------------------------------

static int HL2SB_ProjectedTexture_SetBrightness( lua_State *L )
{
  HL2SB_ProjectedTexture_Check( L, 1 )->brightness = (float)luaL_checknumber( L, 2 );
  return 0;
}

static int HL2SB_ProjectedTexture_GetBrightness( lua_State *L )
{
  lua_pushnumber( L, HL2SB_ProjectedTexture_Check( L, 1 )->brightness );
  return 1;
}

static int HL2SB_ProjectedTexture_SetColor( lua_State *L )
{
  HL2SB_ProjectedTexture_Check( L, 1 )->color = luaL_checkcolor( L, 2 );
  return 0;
}

static int HL2SB_ProjectedTexture_GetColor( lua_State *L )
{
  // GMod's binding pushes the stored colour as one packed integer
  // (byte order r,g,b,a), not as a Color table -- match that contract.
  const Color &c = HL2SB_ProjectedTexture_Check( L, 1 )->color;
  const unsigned int packed = (unsigned int)c.r()
    | ( (unsigned int)c.g() << 8 )
    | ( (unsigned int)c.b() << 16 )
    | ( (unsigned int)c.a() << 24 );
  lua_pushinteger( L, (lua_Integer)packed );
  return 1;
}

// -- texture ---------------------------------------------------------------

static int HL2SB_ProjectedTexture_SetTexture( lua_State *L )
{
  HL2SB_ProjectedTexture_t *pPT = HL2SB_ProjectedTexture_Check( L, 1 );

  if ( lua_type( L, 2 ) == LUA_TSTRING )
  {
    Q_strncpy( pPT->textureName, luaL_checkstring( L, 2 ), sizeof( pPT->textureName ) );
    if ( pPT->pTexture != NULL )
    {
      pPT->pTexture->Release();
      pPT->pTexture = NULL;
    }
  }
  else
  {
    // GMod also accepts an ITexture object; keep a reference so the pointer
    // outlives material-system reloads between the setter and the next Update.
    ITexture *pTexture = luaL_checkitexture( L, 2 );
    if ( pTexture != pPT->pTexture )
    {
      if ( pPT->pTexture != NULL )
        pPT->pTexture->Release();
      pTexture->AddRef();
      pPT->pTexture = pTexture;
    }
  }
  return 0;
}

static int HL2SB_ProjectedTexture_GetTexture( lua_State *L )
{
  HL2SB_ProjectedTexture_t *pPT = HL2SB_ProjectedTexture_Check( L, 1 );
  ITexture *pTexture = ( pPT->pTexture != NULL )
    ? pPT->pTexture
    : materials->FindTexture( pPT->textureName, TEXTURE_GROUP_OTHER, false );
  lua_pushitexture( L, pTexture );
  return 1;
}

static int HL2SB_ProjectedTexture_SetTextureFrame( lua_State *L )
{
  HL2SB_ProjectedTexture_Check( L, 1 )->textureFrame = (int)luaL_checknumber( L, 2 );
  return 0;
}

static int HL2SB_ProjectedTexture_GetTextureFrame( lua_State *L )
{
  lua_pushinteger( L, HL2SB_ProjectedTexture_Check( L, 1 )->textureFrame );
  return 1;
}

// -- shadows / range / attenuation / world light ---------------------------

static int HL2SB_ProjectedTexture_SetEnableShadows( lua_State *L )
{
  HL2SB_ProjectedTexture_Check( L, 1 )->enableShadows = luaL_checkboolean( L, 2 ) ? true : false;
  return 0;
}

static int HL2SB_ProjectedTexture_GetEnableShadows( lua_State *L )
{
  lua_pushboolean( L, HL2SB_ProjectedTexture_Check( L, 1 )->enableShadows );
  return 1;
}

// The two bias fields always existed in the state struct and were always
// pushed in Apply, but no Lua setter ever reached them -- the reference
// binding exposes both (aligned 2026-10-13).
static int HL2SB_ProjectedTexture_SetShadowDepthBias( lua_State *L )
{
  HL2SB_ProjectedTexture_Check( L, 1 )->shadowDepthBias = (float)luaL_checknumber( L, 2 );
  return 0;
}

static int HL2SB_ProjectedTexture_GetShadowDepthBias( lua_State *L )
{
  lua_pushnumber( L, HL2SB_ProjectedTexture_Check( L, 1 )->shadowDepthBias );
  return 1;
}

static int HL2SB_ProjectedTexture_SetShadowSlopeScaleDepthBias( lua_State *L )
{
  HL2SB_ProjectedTexture_Check( L, 1 )->shadowSlopeScaleDepthBias = (float)luaL_checknumber( L, 2 );
  return 0;
}

static int HL2SB_ProjectedTexture_GetShadowSlopeScaleDepthBias( lua_State *L )
{
  lua_pushnumber( L, HL2SB_ProjectedTexture_Check( L, 1 )->shadowSlopeScaleDepthBias );
  return 1;
}

static int HL2SB_ProjectedTexture_SetNearZ( lua_State *L )
{
  HL2SB_ProjectedTexture_Check( L, 1 )->nearZ = (float)luaL_checknumber( L, 2 );
  return 0;
}

static int HL2SB_ProjectedTexture_GetNearZ( lua_State *L )
{
  lua_pushnumber( L, HL2SB_ProjectedTexture_Check( L, 1 )->nearZ );
  return 1;
}

static int HL2SB_ProjectedTexture_SetFarZ( lua_State *L )
{
  HL2SB_ProjectedTexture_Check( L, 1 )->farZ = (float)luaL_checknumber( L, 2 );
  return 0;
}

static int HL2SB_ProjectedTexture_GetFarZ( lua_State *L )
{
  lua_pushnumber( L, HL2SB_ProjectedTexture_Check( L, 1 )->farZ );
  return 1;
}

static int HL2SB_ProjectedTexture_SetQuadraticAttenuation( lua_State *L )
{
  HL2SB_ProjectedTexture_Check( L, 1 )->quadraticAtten = (float)luaL_checknumber( L, 2 );
  return 0;
}

static int HL2SB_ProjectedTexture_GetQuadraticAttenuation( lua_State *L )
{
  lua_pushnumber( L, HL2SB_ProjectedTexture_Check( L, 1 )->quadraticAtten );
  return 1;
}

static int HL2SB_ProjectedTexture_SetLinearAttenuation( lua_State *L )
{
  HL2SB_ProjectedTexture_Check( L, 1 )->linearAtten = (float)luaL_checknumber( L, 2 );
  return 0;
}

static int HL2SB_ProjectedTexture_GetLinearAttenuation( lua_State *L )
{
  lua_pushnumber( L, HL2SB_ProjectedTexture_Check( L, 1 )->linearAtten );
  return 1;
}

static int HL2SB_ProjectedTexture_SetConstantAttenuation( lua_State *L )
{
  HL2SB_ProjectedTexture_Check( L, 1 )->constantAtten = (float)luaL_checknumber( L, 2 );
  return 0;
}

static int HL2SB_ProjectedTexture_GetConstantAttenuation( lua_State *L )
{
  lua_pushnumber( L, HL2SB_ProjectedTexture_Check( L, 1 )->constantAtten );
  return 1;
}

static int HL2SB_ProjectedTexture_SetLightWorld( lua_State *L )
{
  HL2SB_ProjectedTexture_Check( L, 1 )->lightWorld = luaL_checkboolean( L, 2 ) ? true : false;
  return 0;
}

static int HL2SB_ProjectedTexture_GetLightWorld( lua_State *L )
{
  lua_pushboolean( L, HL2SB_ProjectedTexture_Check( L, 1 )->lightWorld );
  return 1;
}

// -- lifecycle -------------------------------------------------------------

static int HL2SB_ProjectedTexture_Update( lua_State *L )
{
  HL2SB_ProjectedTexture_Apply( HL2SB_ProjectedTexture_Check( L, 1 ) );
  return 0;
}

static int HL2SB_ProjectedTexture_Remove( lua_State *L )
{
  HL2SB_ProjectedTexture_Destroy( HL2SB_ProjectedTexture_Check( L, 1 ) );
  return 0;
}

static int HL2SB_ProjectedTexture_IsValid( lua_State *L )
{
  lua_pushboolean( L, HL2SB_ProjectedTexture_Check( L, 1 )->alive );
  return 1;
}

static int HL2SB_ProjectedTexture_GC( lua_State *L )
{
  HL2SB_ProjectedTexture_Destroy( HL2SB_ProjectedTexture_Check( L, 1 ) );
  return 0;
}

static int HL2SB_ProjectedTexture_ToString( lua_State *L )
{
  lua_pushfstring( L, "ProjectedTexture: %p", (void *)HL2SB_ProjectedTexture_Check( L, 1 ) );
  return 1;
}

static const luaL_Reg projectedtexturelib[] = {
  {"SetPos", HL2SB_ProjectedTexture_SetPos},
  {"GetPos", HL2SB_ProjectedTexture_GetPos},
  {"SetAngles", HL2SB_ProjectedTexture_SetAngles},
  {"GetAngles", HL2SB_ProjectedTexture_GetAngles},
  {"SetFOV", HL2SB_ProjectedTexture_SetFOV},
  {"SetHorizontalFOV", HL2SB_ProjectedTexture_SetHorizontalFOV},
  {"GetHorizontalFOV", HL2SB_ProjectedTexture_GetHorizontalFOV},
  {"SetVerticalFOV", HL2SB_ProjectedTexture_SetVerticalFOV},
  {"GetVerticalFOV", HL2SB_ProjectedTexture_GetVerticalFOV},
  {"SetBrightness", HL2SB_ProjectedTexture_SetBrightness},
  {"GetBrightness", HL2SB_ProjectedTexture_GetBrightness},
  {"SetColor", HL2SB_ProjectedTexture_SetColor},
  {"GetColor", HL2SB_ProjectedTexture_GetColor},
  {"SetTexture", HL2SB_ProjectedTexture_SetTexture},
  {"GetTexture", HL2SB_ProjectedTexture_GetTexture},
  {"SetTextureFrame", HL2SB_ProjectedTexture_SetTextureFrame},
  {"GetTextureFrame", HL2SB_ProjectedTexture_GetTextureFrame},
  {"SetEnableShadows", HL2SB_ProjectedTexture_SetEnableShadows},
  {"GetEnableShadows", HL2SB_ProjectedTexture_GetEnableShadows},
  {"SetShadowDepthBias", HL2SB_ProjectedTexture_SetShadowDepthBias},
  {"GetShadowDepthBias", HL2SB_ProjectedTexture_GetShadowDepthBias},
  {"SetShadowSlopeScaleDepthBias", HL2SB_ProjectedTexture_SetShadowSlopeScaleDepthBias},
  {"GetShadowSlopeScaleDepthBias", HL2SB_ProjectedTexture_GetShadowSlopeScaleDepthBias},
  {"SetNearZ", HL2SB_ProjectedTexture_SetNearZ},
  {"GetNearZ", HL2SB_ProjectedTexture_GetNearZ},
  {"SetFarZ", HL2SB_ProjectedTexture_SetFarZ},
  {"GetFarZ", HL2SB_ProjectedTexture_GetFarZ},
  {"SetQuadraticAttenuation", HL2SB_ProjectedTexture_SetQuadraticAttenuation},
  {"GetQuadraticAttenuation", HL2SB_ProjectedTexture_GetQuadraticAttenuation},
  {"SetLinearAttenuation", HL2SB_ProjectedTexture_SetLinearAttenuation},
  {"GetLinearAttenuation", HL2SB_ProjectedTexture_GetLinearAttenuation},
  {"SetConstantAttenuation", HL2SB_ProjectedTexture_SetConstantAttenuation},
  {"GetConstantAttenuation", HL2SB_ProjectedTexture_GetConstantAttenuation},
  {"SetLightWorld", HL2SB_ProjectedTexture_SetLightWorld},
  {"GetLightWorld", HL2SB_ProjectedTexture_GetLightWorld},
  {"Update", HL2SB_ProjectedTexture_Update},
  {"Remove", HL2SB_ProjectedTexture_Remove},
  {"IsValid", HL2SB_ProjectedTexture_IsValid},
  {"__tostring", HL2SB_ProjectedTexture_ToString},
  {NULL, NULL}
};


/*
** Open g_pClientShadowMgr library
*/
LUALIB_API int luaopen_g_pClientShadowMgr (lua_State *L) {
  luaL_register(L, "g_pClientShadowMgr", g_pClientShadowMgrlib);

  // HL2SB: the ProjectedTexture metatable + global constructor (client-only
  // state, same list entry that has always opened this library).
  luaL_newmetatable(L, HL2SB_PROJECTEDTEXTURE_METATABLE);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");   // methods resolve through the metatable
  luaL_register(L, NULL, projectedtexturelib);
  lua_pushcfunction(L, HL2SB_ProjectedTexture_GC);
  lua_setfield(L, -2, "__gc");
  lua_pushstring(L, HL2SB_PROJECTEDTEXTURE_METATABLE);
  lua_setfield(L, -2, "__type");    // type( p ) returns "ProjectedTexture"
  lua_pop(L, 1);

  lua_pushcfunction(L, HL2SB_ProjectedTexture_Create);
  lua_setglobal(L, "ProjectedTexture");

  return 1;
}

