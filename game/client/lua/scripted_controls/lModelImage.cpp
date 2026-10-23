#include "cbase.h"
#include "luamanager.h"
#include "vgui_int.h"
#include "vgui_controls/lPanel.h"
#include "scripted_controls/lPanel.h"
#include "vgui_controls/PHandle.h"
#include "vgui/ISurface.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imaterialsystem.h"
#include "filesystem.h"
#include "tier1/utlbuffer.h"
#include "lbaseflex_shared.h"
#include "lbaseentity_shared.h"
#include "mathlib/lvector.h"
#include "iviewrender.h"
#include "view_shared.h"
#include "vtf/vtf.h"
#include "bitmap/imageformat.h"
#include <deque>
#include <map>
#include <string>

#include "tier0/memdbgon.h"

extern IMaterial *HL2SB_FindImageMaterial(const char *name, const char *digits);
extern bool HL2SB_RenderSpawnIcon(C_BaseAnimating *entity, const CViewSetup &camera,
    int width, int height, const Vector *lightbox, CUtlBuffer &png, CUtlVector<unsigned char> &pixels);

namespace
{
int IconSize(int size)
{
    int result = 32;
    while (result < size && result < 512) result *= 2;
    return result;
}

std::string IconPath(const std::string &model, int skin, const std::string &body, int width, int height)
{
    char path[MAX_PATH];
    Q_strncpy(path, model.c_str(), sizeof(path));
    Q_FixSlashes(path, '/');
    Q_strlower(path);
    Q_StripExtension(path, path, sizeof(path));
    std::string result = "spawnicons/";
    result += path;
    if (body.size() == 9 && body != "000000000") result += "_" + body;
    if (skin > 0) { Q_snprintf(path, sizeof(path), "_skin%i", skin); result += path; }
    if (width != 64 || height != 64)
    {
        if (width == height) Q_snprintf(path, sizeof(path), "_%i", width);
        else Q_snprintf(path, sizeof(path), "_%ix%i", width, height);
        result += path;
    }
    return result + ".png";
}

struct IconImage
{
    IconImage() : material(NULL), texture(-1), queued(false), failed(false) {}
    IMaterial *material;
    int texture;
    bool queued, failed;
};

class IconPixels : public ITextureRegenerator
{
public:
    IconPixels(int width, int height, const CUtlVector<unsigned char> &pixels) : m_Width(width), m_Height(height)
    {
        m_Pixels.SetCount(pixels.Count());
        Q_memcpy(m_Pixels.Base(), pixels.Base(), pixels.Count());
    }
    virtual void RegenerateTextureBits(ITexture *, IVTFTexture *texture, Rect_t *)
    {
        for (int mip = 0; mip < texture->MipCount(); ++mip)
        {
            int width = MAX(1, texture->Width() >> mip), height = MAX(1, texture->Height() >> mip);
            CUtlVector<unsigned char> scaled;
            scaled.SetCount(width * height * 4);
            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x)
                    Q_memcpy(scaled.Base() + (y * width + x) * 4,
                        m_Pixels.Base() + ((y * m_Height / height) * m_Width + x * m_Width / width) * 4, 4);
            ImageLoader::ConvertImageFormat(scaled.Base(), IMAGE_FORMAT_RGBA8888,
                texture->ImageData(0, 0, mip), texture->Format(), width, height);
        }
    }
    virtual void Release() { delete this; }
private:
    int m_Width, m_Height;
    CUtlVector<unsigned char> m_Pixels;
};

class LModelImage : public vgui::LPanel
{
    DECLARE_CLASS_SIMPLE(LModelImage, vgui::LPanel);
public:
    LModelImage(vgui::Panel *parent, const char *name, lua_State *state)
        : BaseClass(parent, name, state), m_Body("000000000"), m_Skin(0), m_Width(0), m_Height(0), m_Custom(false), m_Force(false)
    {
        SetSize(64, 64);
        SetMouseInputEnabled(false);
        SetKeyBoardInputEnabled(false);
        SetPaintBackgroundEnabled(false);
        SetPaintBorderEnabled(false);
    }

    void SetModel(const char *model, int skin, const char *body)
    {
        m_Model = model;
        m_Skin = skin;
        m_Body = body;
        m_Custom = false;
        m_Force = false;
        m_Path.clear();
    }

    void SetSpawnIcon(const char *path)
    {
        m_Model.clear();
        m_Path = path;
        for (size_t i = 0; i < m_Path.size(); ++i)
            if (m_Path[i] == '\\') m_Path[i] = '/';
        m_Custom = false;
    }

    void Rebuild(lua_State *state, int options)
    {
        m_Custom = options > 0 && lua_istable(state, options);
        m_Entity = NULL;
        if (m_Custom)
        {
            lua_getfield(state, options, "ent");
            m_Entity = dynamic_cast<C_BaseAnimating *>(lua_toentity(state, -1)); lua_pop(state, 1);
            lua_getfield(state, options, "cam_pos");
            m_Origin = luaL_checkvector(state, -1); lua_pop(state, 1);
            lua_getfield(state, options, "cam_ang");
            m_Angles = luaL_checkangle(state, -1); lua_pop(state, 1);
            lua_getfield(state, options, "cam_fov");
            m_Fov = luaL_optnumber(state, -1, 20); lua_pop(state, 1);
            for (int i = 0; i < 6; ++i) m_Lightbox[i].Init(0, 0, 0);
            lua_getfield(state, options, "lightbox");
            if (lua_istable(state, -1))
            {
                for (int i = 0; i < 6; ++i)
                {
                    lua_rawgeti(state, -1, i + 1);
                    if (lua_isuserdata(state, -1)) m_Lightbox[i] = lua_tovector(state, -1);
                    lua_pop(state, 1);
                }
            }
            lua_pop(state, 1);
        }
        m_Force = true;
    }

    virtual void Paint();
    bool Generate(const std::string &path, int width, int height);
    bool VisibleInTree()
    {
        for (vgui::Panel *p = this; p; p = p->GetParent())
            if (!p->IsVisible() || p->IsMarkedForDeletion()) return false;
        return true;
    }

    std::string m_Model, m_Body, m_Path;
    int m_Skin, m_Width, m_Height;
    bool m_Custom, m_Force;
    Vector m_Origin;
    QAngle m_Angles;
    float m_Fov;
    CHandle<C_BaseAnimating> m_Entity;
    Vector m_Lightbox[6];
};

struct IconJob
{
    vgui::DHANDLE<LModelImage> panel;
    std::string path;
    int width, height;
};

std::map<std::string, IconImage> s_Images;
std::deque<IconJob> s_Jobs;

bool LoadImage(const std::string &path, IconImage &image)
{
    IMaterial *material = HL2SB_FindImageMaterial(path.c_str(), "0000000");
    if (!material || material->IsErrorMaterial()) return false;
    material->GetMappingWidth();
    bool found = false;
    IMaterialVar *baseTexture = material->FindVar("$basetexture", &found, false);
    if (!found || !baseTexture || !baseTexture->IsTexture() || IsErrorTexture(baseTexture->GetTextureValue())) return false;
    if (image.texture < 0) image.texture = vgui::surface()->CreateNewTextureID(true);
    g_pMatSystemSurface->DrawSetTextureMaterial(image.texture, material);
    image.material = material;
    return true;
}

void LModelImage::Paint()
{
    int left, top, right, bottom;
    GetClipRect(left, top, right, bottom);
    if (left >= right || top >= bottom) return;
    int width = IconSize(GetWide()), height = IconSize(GetTall());
    if (!m_Model.empty() && (m_Path.empty() || m_Width != width || m_Height != height))
    {
        m_Path = IconPath(m_Model, m_Skin, m_Body, width, height);
        m_Width = width;
        m_Height = height;
    }
    if (m_Path.empty()) return;
    IconImage &image = s_Images[m_Path];
    if (m_Force)
    {
        image.failed = false;
        image.material = NULL;
    }
    if (!image.material && !image.queued && !image.failed)
    {
        IconJob job;
        job.panel = this;
        job.path = m_Path;
        job.width = width;
        job.height = height;
        s_Jobs.push_back(job);
        image.queued = true;
    }
    if (image.material)
    {
        vgui::surface()->DrawSetColor(255, 255, 255, 255);
        vgui::surface()->DrawSetTexture(image.texture);
        vgui::surface()->DrawTexturedRect(0, 0, GetWide(), GetTall());
    }
}

bool LModelImage::Generate(const std::string &path, int width, int height)
{
    if (m_Model.empty() || path.find("..") != std::string::npos || !m_lua_State || m_lua_State != L) return false;
    C_BaseAnimating *entity = m_Custom ? m_Entity.Get() : NULL;
    bool ownsEntity = !entity;
    if (!entity) entity = g_pClientSideEntityManager->CreateClientSideEntity(m_Model.c_str(), RENDER_GROUP_OTHER);
    if (!entity) return false;
    if (ownsEntity)
    {
        entity->AddEffects(EF_NODRAW);
        entity->m_nSkin = m_Skin;
        for (size_t i = 0; i < m_Body.size(); ++i)
            if (m_Body[i] >= '0' && m_Body[i] <= '9') entity->SetBodygroup(i, m_Body[i] - '0');
        int sequence = entity->LookupSequence("idle_all_01");
        if (sequence < 0) sequence = entity->LookupSequence("ragdoll");
        if (sequence >= 0) entity->ResetSequence(sequence);
        entity->SetAbsOrigin(vec3_origin);
        entity->SetAbsAngles(vec3_angle);
    }

    CViewSetup camera = *view->GetViewSetup();
    camera.x = camera.y = 0;
    camera.width = width;
    camera.height = height;
    camera.m_flAspectRatio = float(width) / height;
    camera.zNear = 0.1f;
    camera.zFar = 16000.0f;
    bool positioned = m_Custom;
    if (m_Custom)
    {
        camera.origin = m_Origin;
        camera.angles = m_Angles;
        camera.fov = m_Fov;
    }
    else
    {
        int top = lua_gettop(L);
        lua_checkstack(L, 16);
        lua_getglobal(L, "PositionSpawnIcon");
        if (lua_isfunction(L, -1))
        {
            lua_pushentity(L, entity);
            lua_pushvector(L, vec3_origin);
            if (luasrc_pcall(L, 2, 1, 0) == 0 && lua_istable(L, -1))
            {
                lua_getfield(L, -1, "origin"); bool hasOrigin = lua_isuserdata(L, -1); if (hasOrigin) camera.origin = lua_tovector(L, -1); lua_pop(L, 1);
                lua_getfield(L, -1, "angles"); bool hasAngles = lua_isuserdata(L, -1); if (hasAngles) camera.angles = lua_toangle(L, -1); lua_pop(L, 1);
                lua_getfield(L, -1, "fov"); camera.fov = luaL_optnumber(L, -1, 20); lua_pop(L, 1);
                positioned = hasOrigin && hasAngles;
            }
        }
        lua_settop(L, top);
    }
    CUtlBuffer png;
    CUtlVector<unsigned char> pixels;
    bool rendered = positioned && HL2SB_RenderSpawnIcon(entity, camera, width, height,
        m_Custom ? m_Lightbox : NULL, png, pixels);
    if (ownsEntity) entity->Release();
    if (!rendered) return false;
    std::string filename = "materials/" + path;
    char directory[MAX_PATH];
    Q_ExtractFilePath(filename.c_str(), directory, sizeof(directory));
    filesystem->CreateDirHierarchy(directory, "MOD");
    if (!filesystem->WriteFile(filename.c_str(), "MOD", png)) return false;
    IconImage &image = s_Images[path];
    bool hadTexture = image.texture >= 0;
    if (LoadImage(path, image) && hadTexture)
    {
        bool found = false;
        IMaterialVar *baseTexture = image.material->FindVar("$basetexture", &found, false);
        if (found && baseTexture && baseTexture->IsTexture())
        {
            ITexture *texture = baseTexture->GetTextureValue();
            texture->SetTextureRegenerator(new IconPixels(width, height, pixels));
            texture->Download();
        }
    }
    {
        BEGIN_LUA_CALL_HOOK("SpawniconGenerated");
            lua_pushstring(L, m_Model.c_str());
            lua_pushstring(L, path.c_str());
        END_LUA_CALL_HOOK(2, 0);
    }
    return true;
}

LModelImage *CheckImage(lua_State *state)
{
    LModelImage *image = dynamic_cast<LModelImage *>(luaL_checkpanel(state, 1));
    if (!image) luaL_argerror(state, 1, "ModelImage expected");
    return image;
}
int SetModel(lua_State *state)
{
    CheckImage(state)->SetModel(luaL_checkstring(state, 2), luaL_optint(state, 3, 0), luaL_optstring(state, 4, "000000000"));
    return 0;
}
int SetSpawnIcon(lua_State *state) { CheckImage(state)->SetSpawnIcon(luaL_checkstring(state, 2)); return 0; }
int Rebuild(lua_State *state) { CheckImage(state)->Rebuild(state, 0); return 0; }
int RebuildEx(lua_State *state) { CheckImage(state)->Rebuild(state, 2); return 0; }
int CreateImage(lua_State *state)
{
    lua_pushpanel(state, new LModelImage(luaL_optpanel(state, 1, VGui_GetClientLuaRootPanel()),
        luaL_optstring(state, 2, "ModelImage"), state));
    return 1;
}
}

void HL2SB_ProcessModelImages()
{
    // Existing files load without creating model entities or rendering a snapshot.
    const double deadline = Plat_FloatTime() + 0.003;
    while (!s_Jobs.empty())
    {
        IconJob job = s_Jobs.front();
        s_Jobs.pop_front();
        IconImage &image = s_Images[job.path];
        image.queued = false;
        LModelImage *panel = job.panel.Get();
        if (!panel || panel->m_lua_State != L || panel->m_Path != job.path || !panel->VisibleInTree()) continue;
        if (!panel->m_Force && LoadImage(job.path, image))
        {
            if (Plat_FloatTime() >= deadline) break;
            continue;
        }
        bool generated = panel->Generate(job.path, job.width, job.height);
        panel->m_Force = false;
        image.failed = !generated || !LoadImage(job.path, image);
        break;
    }
}

void HL2SB_RegisterModelImage(lua_State *state)
{
    lua_getglobal(state, "vgui");
    lua_pushcfunction(state, CreateImage); lua_setfield(state, -2, "ModelImage");
    lua_pop(state, 1);
    luaL_getmetatable(state, "Panel");
    const luaL_Reg methods[] = {
        {"SetModel", SetModel}, {"SetSpawnIcon", SetSpawnIcon},
        {"RebuildSpawnIcon", Rebuild}, {"RebuildSpawnIconEx", RebuildEx}, {NULL, NULL}
    };
    luaL_register(state, NULL, methods);
    lua_pop(state, 1);
}
