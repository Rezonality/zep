#pragma once

#include <float.h>
#include <math.h>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159862f
#endif

// ----------------------------
// Notes from: www.github.com/cmaughan
// Ported from AntTweakBar
// This is a really nice implementation of an orientation widget; all due respect to the original author ;) 
// Dependencies kept to a minimum.  I basically vectorized the original code, added a few math types, cleaned things up and 
// made it clearer what the maths was doing.
// I tried to make it more imgui-like, and removed all the excess stuff not needed here.  This still needs work.
// I also added triangle culling because ImGui doesn't support winding clip 
// The widget works by transforming the 3D object to screen space and clipping the triangles.  This makes it work with any 
// imgui back end, without modifications to the renderers.
// 
// TODO:
// More cleanup.
// Figure out what ShowDir is for.
// Test direction vectors more

// -------------------------- 
// Firstly, a little math, missing from ImGui but needed for this widget 
// A Vec3, Matrix 3x3, Dot & Cross products, A Quaternion.  Some helper functions, bare minimum
struct ImVec3
{
    float x, y, z;
    ImVec3() { x = y = z = 0.0f; }
    ImVec3(float _x, float _y, float _z) { x = _x; y = _y; z = _z; }

    ImVec3 RotY() const { return ImVec3(-z, y, x); }
    ImVec3 RotZ() const { return ImVec3(-y, x, z); }
    ImVec3 Cross(const ImVec3& b) const { return ImVec3(y * b.z - z * b.y, z * b.x - x * b.z, x * b.y - y * b.x); }
    float Dot(const ImVec3& b) const { return x * b.x + y * b.y + z * b.z; }
    ImVec3 Mult(float val) const { return ImVec3(x * val, y * val, z * val); }
    ImVec3 Div(float val) const { return ImVec3(x / val, y / val, z / val); }
    float Length() const { return sqrt(x * x + y * y + z * z); }
#ifdef IM_VEC3_CLASS_EXTRA          // Define constructor and implicit cast operators in imconfig.h to convert back<>forth from your math types and ImVec2.
    IM_VEC3_CLASS_EXTRA
#endif
};

// Added to the existing ImVec2 vector
inline ImVec2 ImVec2Subtract(const ImVec2& left, const ImVec2& right) { return ImVec2(left.x - right.x, left.y - right.y); }
inline float ImVec2Cross(const ImVec2& left, const ImVec2& right) { return (left.x * right.y) - (left.y * right.x); }

struct ImQuat
{
    float x, y, z, w;
    ImQuat() { x = y = z = 0.0f; w = 1.0f; }
    ImQuat(float _x, float _y, float _z, float _w) { x = _x; y = _y; z = _z; w = _w; }

    ImQuat Div(float val) { return ImQuat(x / val, y / val, z / val, w / val); }
    ImVec3 Rotate(const ImVec3& dir)
    {
        float ps = -x * dir.x - y * dir.y - z * dir.z;
        float px = w * dir.x + y * dir.z - z * dir.y;
        float py = w * dir.y + z * dir.x - x * dir.z;
        float pz = w * dir.z + x * dir.y - y * dir.x;
        return ImVec3(-ps * x + px * w - py * z + pz * y, -ps * y + py * w - pz * x + px * z, -ps * z + pz * w - px * y + py * x);
    }

    ImQuat Mult(const ImQuat& q2)
    {
        ImQuat out;
        out.x = w * q2.x + x * q2.w + y * q2.z - z * q2.y;
        out.y = w * q2.y + y * q2.w + z * q2.x - x * q2.z;
        out.z = w * q2.z + z * q2.w + x * q2.y - y * q2.x;
        out.w = w * q2.w - (x * q2.x + y * q2.y + z * q2.z);
        return out;
    }
#ifdef IM_QUAT_CLASS_EXTRA          // Define constructor and implicit cast operators in imconfig.h to convert back<>forth from your math types and ImVec2.
    IM_QUAT_CLASS_EXTRA
#endif
};

// Matrix used to allow user to specify axis orientation
struct ImMat3x3
{
    float m[3][3];
    ImMat3x3()
    {
        for (int x = 0; x < 3; x++)
        {
            for (int y = 0; y < 3; y++)
            {
                m[y][x] = (x == y) ? 1.0f : 0.0f;
            }
        }
    }

    ImVec3 Transform(const ImVec3& vec)
    {
        ImVec3 out;
        out.x = m[0][0] * vec.x + m[1][0] * vec.y + m[2][0] * vec.z;
        out.y = m[0][1] * vec.x + m[1][1] * vec.y + m[2][1] * vec.z;
        out.z = m[0][2] * vec.x + m[1][2] * vec.y + m[2][2] * vec.z;
        return out;
    }

    ImVec3 TransformInv(const ImVec3& vec)
    {
        ImVec3 out;
        out.x = m[0][0] * vec.x + m[0][1] * vec.y + m[0][2] * vec.z;
        out.y = m[1][0] * vec.x + m[1][1] * vec.y + m[1][2] * vec.z;
        out.z = m[2][0] * vec.x + m[2][1] * vec.y + m[2][2] * vec.z;
        return out;
    }
};

inline float ImDegToRad(float degree) { return degree * (float(M_PI) / 180.0f); }
inline float ImRadToDeg(float radian) { return radian * (180.0f / float(M_PI)); }

// The data structure that holds the orientation among other things
struct ImOrient
{
    ImQuat Qt;            // Quaternion value

    ImVec3 Axis;          // Axis and Angle
    float Angle;

    ImVec3 Dir;           // Dir value set when used as a direction
    bool m_AAMode;        // Axis & angle mode
    bool m_IsDir;         // Mapped to a dir vector instead of a quat
    ImVec3 m_ShowDir;     // CM: Not sure what this is all about? 
    ImU32 m_DirColor;        // Direction vector color

    ImMat3x3 AxisTransform;  // Transform to required axis frame

    // For the geometry
    enum EArrowParts { ARROW_CONE, ARROW_CONE_CAP, ARROW_CYL, ARROW_CYL_CAP };
    static ImVector<ImVec3> s_SphTri;
    static ImVector<ImU32> s_SphCol;
    static ImVector<ImVec2> s_SphTriProj;
    static ImVector<ImU32> s_SphColLight;
    static ImVector<ImVec3> s_ArrowTri[4];
    static ImVector<ImVec2> s_ArrowTriProj[4];
    static ImVector<ImVec3> s_ArrowNorm[4];
    static ImVector<ImU32> s_ArrowColLight[4];
    static void CreateSphere();
    static void CreateArrow();

    IMGUI_API bool Draw(const char* label);
    IMGUI_API void DrawTriangles(ImDrawList* draw_list, const ImVec2& offset, const ImVector<ImVec2>& triProj, const ImVector<ImU32>& colLight, int numVertices, float cullDir);
    IMGUI_API void ConvertToAxisAngle();
    IMGUI_API void ConvertFromAxisAngle();

    // Quaternions
    inline float QuatD(float w, float h) { return (float)std::min(std::abs(w), std::abs(h)) - 4.0f; }
    inline float QuatPX(float x, float w, float h) { return (x*0.5f*QuatD(w, h) + w*0.5f + 0.5f); }
    inline float QuatPY(float y, float w, float h) { return (-y*0.5f*QuatD(w, h) + h*0.5f - 0.5f); }
    inline float QuatIX(int x, float w, float h) { return (2.0f*x - w - 1.0f) / QuatD(w, h); }
    inline float QuatIY(int y, float w, float h) { return (-2.0f*y + h - 1.0f) / QuatD(w, h); }
    IMGUI_API void QuatFromDir(ImQuat& quat, const ImVec3& dir);
    IMGUI_API static void QuatFromAxisAngle(ImQuat& qt, const ImVec3& axis, float angle);

    // Useful colors
    IMGUI_API static ImU32 ColorBlend(ImU32 _Color1, ImU32 _Color2, float _S);

    typedef unsigned int color32;
    const ImU32 COLOR32_BLACK = 0xff000000;   // Black 
    const ImU32 COLOR32_WHITE = 0xffffffff;   // White 
    const ImU32 COLOR32_ZERO = 0x00000000;    // Zero 
    const ImU32 COLOR32_RED = 0xffff0000;     // Red 
    const ImU32 COLOR32_GREEN = 0xff00ff00;   // Green 
    const ImU32 COLOR32_BLUE = 0xff0000ff;    // Blue 

    const int GIZMO_SIZE = 200;
};

// The API
namespace ImGui
{

IMGUI_API bool QuaternionGizmo(const char* label, ImQuat& quat);
IMGUI_API bool AxisAngleGizmo(const char* label, ImVec3& axis, float& angle);
IMGUI_API bool DirectionGizmo(const char* label, ImVec3& dir);

};

