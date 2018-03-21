#include "imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h" // ImSaturate
#include "imgui_orient.h"

#include <algorithm>
#include <cassert>
ImVector<ImVec3> ImOrient::s_SphTri;
ImVector<ImU32> ImOrient::s_SphCol;
ImVector<ImVec2>   ImOrient::s_SphTriProj;
ImVector<ImU32> ImOrient::s_SphColLight;
ImVector<ImVec3> ImOrient::s_ArrowTri[4];
ImVector<ImVec2>   ImOrient::s_ArrowTriProj[4];
ImVector<ImVec3> ImOrient::s_ArrowNorm[4];
ImVector<ImU32> ImOrient::s_ArrowColLight[4];
 
namespace ImGui
{
IMGUI_API bool QuaternionGizmo(const char* label, ImQuat& quat)
{
    ImOrient orient;
    orient.Qt = quat;
    orient.Axis = ImVec3(1.0f, 0.0f, 0.0f);
    orient.Angle = 0;
    orient.Dir.x = orient.Dir.y = orient.Dir.z = 0;

    orient.m_AAMode = false; // Axis & angle mode hidden
    orient.m_IsDir = false;
    orient.m_ShowDir = ImVec3(0.0f, 0.0f, 0.0f);
    orient.m_DirColor = 0xff00ffff;
    orient.ConvertToAxisAngle();

    bool ret = orient.Draw(label);
    if (ret)
    {
        quat = orient.Qt;
    }
    return ret;
}

IMGUI_API bool AxisAngleGizmo(const char* label, ImVec3& axis, float& angle)
{
    ImOrient orient;
    orient.Qt = ImQuat();
    orient.Axis = axis;
    orient.Angle = angle;
    orient.Dir = ImVec3(0.0f, 0.0f, 0.0f);

    orient.m_AAMode = true; // Axis & angle mode hidden
    orient.m_IsDir = true;
    orient.m_ShowDir = ImVec3(0.0f, 0.0f, 0.0f);
    orient.m_DirColor = 0xff00ffff;
    orient.ConvertFromAxisAngle();

    bool ret = orient.Draw(label);
    if (ret)
    {
        orient.ConvertToAxisAngle();
        axis = orient.Axis;
        angle = orient.Angle;
    }
    return ret;
}

IMGUI_API bool DirectionGizmo(const char* label, ImVec3& dir)
{
    ImOrient orient;
    orient.Qt = ImQuat();
    orient.Dir = dir;
    orient.Axis = ImVec3(1.0f, 0.0f, 0.0f);
    orient.Angle = 0.0f;

    orient.m_AAMode = false; // Axis & angle mode hidden
    orient.m_IsDir = true;
    orient.m_ShowDir = ImVec3(1.0f, 0.0f, 0.0f);
    orient.m_DirColor = 0xffff0000;
    orient.QuatFromDir(orient.Qt, dir);
    orient.ConvertToAxisAngle();

    bool ret = orient.Draw(label);
    if (ret)
    {
        ImVec3 d = orient.Qt.Rotate(ImVec3(1, 0, 0));
        d = d.Div(d.Length());
        orient.Dir = d;
        dir = orient.Dir;
    }
    return ret;
}

} // ImGui namespace

bool ImOrient::Draw(const char* label)
{
    //    ImGuiIO& io = ImGui::GetIO();
    ImGuiStyle& style = ImGui::GetStyle();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    if (ImOrient::s_SphTri.empty())
    {
        ImOrient::CreateArrow();
        ImOrient::CreateSphere();
    }

    ImGui::PushID(label);
    ImGui::BeginGroup();

    bool value_changed = false;

    ImGui::Text("%s", label);
    // Summary 
    if (m_AAMode)
    {
        ImGui::Text("Axis={%.2f,%.2f,%.2f} Angle=%.0f%c", Axis.x, Axis.y, Axis.z, Angle, 176);
    }
    else if (m_IsDir)
    {
        ImGui::Text("Dir={%.2f,%.2f,%.2f}", Dir.x, Dir.y, Dir.z);
    }
    else
    {
        ImGui::Text("Quat={x:%.2f,y:%.2f,z:%.2f,s:%.2f}", Qt.x, Qt.y, Qt.z, Qt.w);
    }

    ImVec2 orient_pos = ImGui::GetCursorScreenPos();

    float sv_orient_size = std::min(ImGui::CalcItemWidth(), float(GIZMO_SIZE));
    float w = sv_orient_size;
    float h = sv_orient_size;

    // We want to generate quaternion rotations relative to the quaternion in the 'down' press state.
    // This gives us cleaner control over rotation (it feels better/more natural)
    static ImQuat origQuat;
    static ImVec3 coordOld;
    bool highlighted = false;
    ImGui::InvisibleButton("orient", ImVec2(sv_orient_size, sv_orient_size));
    if (ImGui::IsItemActive())
    {
        highlighted = true;
        ImVec2 mouse = ImGui::GetMousePos() - orient_pos;
        if (ImGui::IsMouseClicked(0))
        {
            origQuat = Qt;
            coordOld = ImVec3(QuatIX((int)mouse.x, w, h), QuatIY((int)mouse.y, w, h), 1.0f);
        }
        else if (ImGui::IsMouseDragging(0))
        {
            //ImGui::ResetMouseDragDelta(0);
            ImVec3 coord(QuatIX((int)mouse.x, w, h), QuatIY((int)mouse.y, w, h), 1.0f);
            ImVec3 pVec = AxisTransform.Transform(coord);
            ImVec3 oVec = AxisTransform.Transform(coordOld);
            coord.z = 0.0f;
            float n0 = oVec.Length();
            float n1 = pVec.Length();
            if (n0 > FLT_EPSILON && n1 > FLT_EPSILON)
            {
                ImVec3 v0 = oVec.Div(n0);
                ImVec3 v1 = pVec.Div(n1);
                ImVec3 axis = v0.Cross(v1);
                float sa = axis.Length();
                float ca = v0.Dot(v1);
                float angle = atan2(sa, ca);
                if (coord.x*coord.x + coord.y*coord.y > 1.0)
                    angle *= 1.0f + 1.5f*(coord.Length() - 1.0f);
                ImQuat qrot, qres, qorig;
                QuatFromAxisAngle(qrot, axis, angle);
                float nqorig = sqrt(origQuat.x * origQuat.x + origQuat.y * origQuat.y + origQuat.z * origQuat.z + origQuat.w * origQuat.w);
                if (fabs(nqorig) > FLT_EPSILON * FLT_EPSILON)
                {
                    qorig = origQuat.Div(nqorig);
                    qres = qrot.Mult(qorig);
                    Qt = qres;
                }
                else
                {
                    Qt = qrot;
                }
                //origQuat = Qt;
                value_changed = true;
            }
        }
        draw_list->AddRectFilled(orient_pos, orient_pos + ImVec2(sv_orient_size, sv_orient_size), ImColor(style.Colors[ImGuiCol_FrameBgActive]), style.FrameRounding);
    }
    else
    {
        highlighted = ImGui::IsItemHovered();
        draw_list->AddRectFilled(orient_pos, orient_pos + ImVec2(sv_orient_size, sv_orient_size), ImColor(highlighted ? style.Colors[ImGuiCol_FrameBgHovered]: style.Colors[ImGuiCol_FrameBg]), style.FrameRounding);
    }


    float normDir = m_ShowDir.Length();
    bool drawDir = m_IsDir || (normDir > FLT_EPSILON);

    ImVec2 inner_pos = orient_pos;
    float inner_size = w;
    if (drawDir)
    {
        inner_size = sv_orient_size;
    }
    else
    {
        inner_pos.x += sv_orient_size * .25f * .5f;
        inner_pos.y += sv_orient_size * .25f * .5f;
        inner_size *= .75f;
    }

    ImQuat quat;
    int i, j, k, l, m;

    // normalize quaternion
    float qn = (float)sqrt(Qt.w*Qt.w + Qt.x*Qt.x + Qt.y*Qt.y + Qt.z*Qt.z);
    if (qn > FLT_EPSILON)
    {
        quat.x = (float)Qt.x / qn;
        quat.y = (float)Qt.y / qn;
        quat.z = (float)Qt.z / qn;
        quat.w = (float)Qt.w / qn;
    }
    else
    {
        quat.x = quat.y = quat.z = 0.0f;
        quat.w = 1.0f;
    }

    ImColor alpha(1.0f, 1.0f, 1.0f, highlighted ? 1.0f : 0.75f);

    // check if frame is right-handed
    ImVec3 px = AxisTransform.Transform(ImVec3(1.0f, 0.0f, 0.0f));
    ImVec3 py = AxisTransform.Transform(ImVec3(0.0f, 1.0f, 0.0f));
    ImVec3 pz = AxisTransform.Transform(ImVec3(0.0f, 0.0f, 1.0f));

    ImVec3 ez = px.Cross(py);

    // Use the handedness of the frame matrix to determine cull direction (?)
    bool frameRightHanded = ez.Dot(ez) >= 0;
    float cullDir = frameRightHanded ? 1.0f : -1.0f;

    // Drawing an arrow
    if (drawDir)
    {
        ImVec3 dir = m_ShowDir;
        if (normDir < FLT_EPSILON)
        {
            normDir = 1;
            dir.x = 1;
        }
        ImVec3 kVec = dir;

        ImVec3 rotDirAxis = { 0, -kVec.z, kVec.y };
        if (rotDirAxis.Dot(rotDirAxis) < FLT_EPSILON*FLT_EPSILON)
        {
            rotDirAxis.x = rotDirAxis.y = 0;
            rotDirAxis.z = 1;
        }
        float rotDirAngle = acos(kVec.x / normDir);
        ImQuat rotDirQuat;
        QuatFromAxisAngle(rotDirQuat, rotDirAxis, rotDirAngle);

        kVec = ImVec3(1.0f, 0.0f, 0.0f);
        kVec = rotDirQuat.Rotate(kVec);
        kVec = quat.Rotate(kVec);
        for (k = 0; k < 4; ++k) // 4 parts of the arrow
        {
            // draw order
            ImVec3 arrowDir = AxisTransform.Transform(kVec);
            j = (arrowDir.z > 0) ? 3 - k : k;

            assert(s_ArrowTriProj[j].size() == (s_ArrowTri[j].size()) && s_ArrowColLight[j].size() == s_ArrowTri[j].size() && s_ArrowNorm[j].size() == s_ArrowTri[j].size());

            const int ntri = (int)s_ArrowTri[j].size();
            for (i = 0; i < ntri; ++i)
            {
                ImVec3 coord = s_ArrowTri[j][i];
                ImVec3 norm = s_ArrowNorm[j][i];

                if (coord.x > 0)
                    coord.x = 2.5f * coord.x - 2.0f;
                else
                    coord.x += 0.2f;
                coord.y *= 1.5f;
                coord.z *= 1.5f;

                coord = rotDirQuat.Rotate(coord);
                coord = quat.Rotate(coord);
                coord = AxisTransform.Transform(coord);

                norm = rotDirQuat.Rotate(norm);
                norm = quat.Rotate(norm);
                norm = AxisTransform.Transform(norm);

                s_ArrowTriProj[j][i] = ImVec2(QuatPX(coord.x, w, h), QuatPY(coord.y, w, h));
                ImU32 col = (m_DirColor | 0xff000000) & alpha;
                s_ArrowColLight[j][i] = ColorBlend(0xff000000, col, fabsf(ImClamp(norm.z, -1.0f, 1.0f)));
            }
            DrawTriangles(draw_list, inner_pos, s_ArrowTriProj[j], s_ArrowColLight[j], ntri, cullDir);
        }
    }
    else
    {
        // draw arrows & sphere
        const float SPH_RADIUS = 0.75f;
        for (m = 0; m < 2; ++m)  // m=0: back, m=1: front
        {
            for (l = 0; l < 3; ++l)  // draw 3 arrows
            {
                ImVec3 kVec(1, 0, 0);
                if (l == 1)
                {
                    kVec = kVec.RotZ();
                }
                else if (l == 2)
                {
                    kVec = kVec.RotY();
                }
                kVec = quat.Rotate(kVec);
                for (k = 0; k < 4; ++k) // 4 parts of the arrow
                {
                    // draw order
                    ImVec3 arrowCoord = AxisTransform.Transform(kVec);
                    j = (arrowCoord.z > 0) ? 3 - k : k;

                    bool cone = true;
                    if ((m == 0 && arrowCoord.z > 0) || (m == 1 && arrowCoord.z <= 0))
                    {
                        if (j == ImOrient::ARROW_CONE || j == ImOrient::ARROW_CONE_CAP) // do not draw cone
                            continue;
                        else
                            cone = false;
                    }
                    assert(ImOrient::s_ArrowTriProj[j].size() == (ImOrient::s_ArrowTri[j].size()) && ImOrient::s_ArrowColLight[j].size() == ImOrient::s_ArrowTri[j].size() && ImOrient::s_ArrowNorm[j].size() == ImOrient::s_ArrowTri[j].size());
                    const int ntri = (int)ImOrient::s_ArrowTri[j].size();
                    for (i = 0; i < ntri; ++i)
                    {
                        ImVec3 coord = s_ArrowTri[j][i];
                        if (cone && coord.x <= 0)
                            coord.x = SPH_RADIUS;
                        else if (!cone && coord.x > 0)
                            coord.x = -SPH_RADIUS;
                        ImVec3 norm = s_ArrowNorm[j][i];
                        if (l == 1)
                        {
                            coord = coord.RotZ();
                            norm = norm.RotZ();
                        }
                        else if (l == 2)
                        {
                            coord = coord.RotY();
                            norm = norm.RotY();
                        }
                        coord = quat.Rotate(coord);
                        coord = AxisTransform.Transform(coord);
                        norm = quat.Rotate(norm);
                        norm = AxisTransform.Transform(norm);
                        s_ArrowTriProj[j][i] = ImVec2(QuatPX(coord.x, inner_size, inner_size), QuatPY(coord.y, inner_size, inner_size));
                        float fade = (m == 0 && coord.z < 0) ? ImClamp(2.0f*coord.z*coord.z, 0.0f, 1.0f) : 0;
                        float alphaFade = 1.0f;
                        alphaFade = alpha.Value.w;
                        alphaFade *= (1.0f - fade);
                        ImColor alphaFadeCol(1.0f, 1.0f, 1.0f, alphaFade);
                        ImU32 col = (l == 0) ? 0xffff0000 : ((l == 1) ? 0xff00ff00 : 0xff0000ff);
                        s_ArrowColLight[j][i] = ColorBlend(0xff000000, col, fabsf(ImClamp(norm.z, -1.0f, 1.0f))) & ImU32(alphaFadeCol);
                    }
                    DrawTriangles(draw_list, inner_pos, s_ArrowTriProj[j], s_ArrowColLight[j], ntri, cullDir);
                }
            }

            if (m == 0)
            {
                const int ntri = (int)ImOrient::s_SphTri.size();
                for (i = 0; i < ntri; ++i)   // draw sphere
                {
                    ImVec3 coord = s_SphTri[i].Mult(SPH_RADIUS);
                    coord = quat.Rotate(coord);
                    coord = AxisTransform.Transform(coord);
                    s_SphTriProj[i] = ImVec2(QuatPX(coord.x, inner_size, inner_size), QuatPY(coord.y, inner_size, inner_size));
                    s_SphColLight[i] = ColorBlend(0xff000000, s_SphCol[i], fabsf(ImClamp(coord.z / SPH_RADIUS, -1.0f, 1.0f)))& ImU32(alpha);
                }

                DrawTriangles(draw_list, inner_pos, s_SphTriProj, s_SphColLight, ntri, cullDir);
            }
        }

        // draw x
        draw_list->AddLine(orient_pos + ImVec2(w - 12, h - 36), orient_pos + ImVec2(w - 12 + 5, h - 36 + 5), 0xff0000c0);
        draw_list->AddLine(orient_pos + ImVec2(w - 12 + 5, h - 36), orient_pos + ImVec2(w - 12, h - 36 + 5), 0xff0000c0);
        // draw y
        draw_list->AddLine(orient_pos + ImVec2(w - 12, h - 25), orient_pos + ImVec2(w - 12 + 3, h - 25 + 4), 0xff00c000);
        draw_list->AddLine(orient_pos + ImVec2(w - 12 + 5, h - 25), orient_pos + ImVec2(w - 12, h - 25 + 7), 0xff00c000);
        // draw z
        draw_list->AddLine(orient_pos + ImVec2(w - 12, h - 12), orient_pos + ImVec2(w - 12 + 5, h - 12), 0xffc00000);
        draw_list->AddLine(orient_pos + ImVec2(w - 12, h - 12 + 5), orient_pos + ImVec2(w - 12 + 5, h - 12 + 5), 0xffc00000);
        draw_list->AddLine(orient_pos + ImVec2(w - 12, h - 12 + 5), orient_pos + ImVec2(w - 12 + 5, h - 12), 0xffc00000);
    }

    ImGui::EndGroup();
    ImGui::PopID();

    return value_changed;
}


void ImOrient::DrawTriangles(ImDrawList* draw_list, const ImVec2& offset, const ImVector<ImVec2>& triProj, const ImVector<ImU32>& colLight, int numVertices, float cullDir)
{
    const ImVec2 uv = GImGui->FontTexUvWhitePixel;
    assert(numVertices % 3 == 0);
    draw_list->PrimReserve(numVertices, numVertices); // num vert/indices 
    for (int ii = 0; ii < numVertices / 3; ii++)
    {
        ImVec2 v1 = offset + triProj[ii * 3];
        ImVec2 v2 = offset + triProj[ii * 3 + 1];
        ImVec2 v3 = offset + triProj[ii * 3 + 2];

        // 2D cross product to do culling
        ImVec2 d1 = ImVec2Subtract(v2, v1);
        ImVec2 d2 = ImVec2Subtract(v3, v1);
        float c = ImVec2Cross(d1, d2) * cullDir;
        if (c > 0.0f)
        {
            v2 = v1;
            v3 = v1;
        }

        draw_list->PrimWriteIdx(ImDrawIdx(draw_list->_VtxCurrentIdx));
        draw_list->PrimWriteIdx(ImDrawIdx(draw_list->_VtxCurrentIdx + 1));
        draw_list->PrimWriteIdx(ImDrawIdx(draw_list->_VtxCurrentIdx + 2));
        draw_list->PrimWriteVtx(v1, uv, colLight[ii * 3]);
        draw_list->PrimWriteVtx(v2, uv, colLight[ii * 3 + 1]);
        draw_list->PrimWriteVtx(v3, uv, colLight[ii * 3 + 2]);
    }
}

void ImOrient::CreateSphere()
{
    const int SUBDIV = 7;
    s_SphTri.clear();
    s_SphCol.clear();

    const float A[8 * 3] = { 1,0,0, 0,0,-1, -1,0,0, 0,0,1,   0,0,1,  1,0,0,  0,0,-1, -1,0,0 };
    const float B[8 * 3] = { 0,1,0, 0,1,0,  0,1,0,  0,1,0,   0,-1,0, 0,-1,0, 0,-1,0, 0,-1,0 };
    const float C[8 * 3] = { 0,0,1, 1,0,0,  0,0,-1, -1,0,0,  1,0,0,  0,0,-1, -1,0,0, 0,0,1 };
    const ImU32 COL_A[8] = { 0xffffffff, 0xffffff40, 0xff40ff40, 0xff40ffff,  0xffff40ff, 0xffff4040, 0xff404040, 0xff4040ff };
    const ImU32 COL_B[8] = { 0xffffffff, 0xffffff40, 0xff40ff40, 0xff40ffff,  0xffff40ff, 0xffff4040, 0xff404040, 0xff4040ff };
    const ImU32 COL_C[8] = { 0xffffffff, 0xffffff40, 0xff40ff40, 0xff40ffff,  0xffff40ff, 0xffff4040, 0xff404040, 0xff4040ff };

    int i, j, k, l;
    float xa, ya, za, xb, yb, zb, xc, yc, zc, x, y, z, norm, u[3], v[3];
    ImU32 col;
    for (i = 0; i < 8; ++i)
    {
        xa = A[3 * i + 0]; ya = A[3 * i + 1]; za = A[3 * i + 2];
        xb = B[3 * i + 0]; yb = B[3 * i + 1]; zb = B[3 * i + 2];
        xc = C[3 * i + 0]; yc = C[3 * i + 1]; zc = C[3 * i + 2];
        for (j = 0; j <= SUBDIV; ++j)
            for (k = 0; k <= 2 * (SUBDIV - j); ++k)
            {
                if (k % 2 == 0)
                {
                    u[0] = ((float)j) / (SUBDIV + 1);
                    v[0] = ((float)(k / 2)) / (SUBDIV + 1);
                    u[1] = ((float)(j + 1)) / (SUBDIV + 1);
                    v[1] = ((float)(k / 2)) / (SUBDIV + 1);
                    u[2] = ((float)j) / (SUBDIV + 1);
                    v[2] = ((float)(k / 2 + 1)) / (SUBDIV + 1);
                }
                else
                {
                    u[0] = ((float)j) / (SUBDIV + 1);
                    v[0] = ((float)(k / 2 + 1)) / (SUBDIV + 1);
                    u[1] = ((float)(j + 1)) / (SUBDIV + 1);
                    v[1] = ((float)(k / 2)) / (SUBDIV + 1);
                    u[2] = ((float)(j + 1)) / (SUBDIV + 1);
                    v[2] = ((float)(k / 2 + 1)) / (SUBDIV + 1);
                }

                for (l = 0; l < 3; ++l)
                {
                    x = (1.0f - u[l] - v[l])*xa + u[l] * xb + v[l] * xc;
                    y = (1.0f - u[l] - v[l])*ya + u[l] * yb + v[l] * yc;
                    z = (1.0f - u[l] - v[l])*za + u[l] * zb + v[l] * zc;
                    norm = sqrtf(x*x + y*y + z*z);
                    x /= norm; y /= norm; z /= norm;
                    s_SphTri.push_back(ImVec3(x, y, z));
                    if (u[l] + v[l] > FLT_EPSILON)
                        col = ColorBlend(COL_A[i], ColorBlend(COL_B[i], COL_C[i], v[l] / (u[l] + v[l])), u[l] + v[l]);
                    else
                        col = COL_A[i];
                    s_SphCol.push_back(col);
                }
            }
    }
    s_SphTriProj.clear();
    s_SphTriProj.resize(s_SphTri.size());
    s_SphColLight.clear();
    s_SphColLight.resize(s_SphCol.size());
}

void ImOrient::CreateArrow()
{
    const int SUBDIV = 15;
    const float CYL_RADIUS = 0.08f;
    const float CONE_RADIUS = 0.16f;
    const float CONE_LENGTH = 0.25f;
    const float ARROW_BGN = -1.1f;
    const float ARROW_END = 1.15f;
    int i;
     
    for (i = 0; i < 4; ++i)
    {
        s_ArrowTri[i].clear();
        s_ArrowNorm[i].clear();
    }

    float x0, x1, y0, y1, z0, z1, a0, a1, nx, nn;
    for (i = 0; i < SUBDIV; ++i)
    {
        a0 = 2.0f*float(M_PI)*(float(i)) / SUBDIV;
        a1 = 2.0f*float(M_PI)*(float(i + 1)) / SUBDIV;
        x0 = ARROW_BGN;
        x1 = ARROW_END - CONE_LENGTH;
        y0 = cosf(a0);
        z0 = sinf(a0);
        y1 = cosf(a1);
        z1 = sinf(a1);
        s_ArrowTri[ARROW_CYL].push_back(ImVec3(x1, CYL_RADIUS*y0, CYL_RADIUS*z0));
        s_ArrowTri[ARROW_CYL].push_back(ImVec3(x0, CYL_RADIUS*y0, CYL_RADIUS*z0));
        s_ArrowTri[ARROW_CYL].push_back(ImVec3(x0, CYL_RADIUS*y1, CYL_RADIUS*z1));
        s_ArrowTri[ARROW_CYL].push_back(ImVec3(x1, CYL_RADIUS*y0, CYL_RADIUS*z0));
        s_ArrowTri[ARROW_CYL].push_back(ImVec3(x0, CYL_RADIUS*y1, CYL_RADIUS*z1));
        s_ArrowTri[ARROW_CYL].push_back(ImVec3(x1, CYL_RADIUS*y1, CYL_RADIUS*z1));
        s_ArrowNorm[ARROW_CYL].push_back(ImVec3(0, y0, z0));
        s_ArrowNorm[ARROW_CYL].push_back(ImVec3(0, y0, z0));
        s_ArrowNorm[ARROW_CYL].push_back(ImVec3(0, y1, z1));
        s_ArrowNorm[ARROW_CYL].push_back(ImVec3(0, y0, z0));
        s_ArrowNorm[ARROW_CYL].push_back(ImVec3(0, y1, z1));
        s_ArrowNorm[ARROW_CYL].push_back(ImVec3(0, y1, z1));
        s_ArrowTri[ARROW_CYL_CAP].push_back(ImVec3(x0, 0, 0));
        s_ArrowTri[ARROW_CYL_CAP].push_back(ImVec3(x0, CYL_RADIUS*y1, CYL_RADIUS*z1));
        s_ArrowTri[ARROW_CYL_CAP].push_back(ImVec3(x0, CYL_RADIUS*y0, CYL_RADIUS*z0));
        s_ArrowNorm[ARROW_CYL_CAP].push_back(ImVec3(-1, 0, 0));
        s_ArrowNorm[ARROW_CYL_CAP].push_back(ImVec3(-1, 0, 0));
        s_ArrowNorm[ARROW_CYL_CAP].push_back(ImVec3(-1, 0, 0));
        x0 = ARROW_END - CONE_LENGTH;
        x1 = ARROW_END;
        nx = CONE_RADIUS / (x1 - x0);
        nn = 1.0f / sqrtf(nx*nx + 1);
        s_ArrowTri[ARROW_CONE].push_back(ImVec3(x1, 0, 0));
        s_ArrowTri[ARROW_CONE].push_back(ImVec3(x0, CONE_RADIUS*y0, CONE_RADIUS*z0));
        s_ArrowTri[ARROW_CONE].push_back(ImVec3(x0, CONE_RADIUS*y1, CONE_RADIUS*z1));
        s_ArrowTri[ARROW_CONE].push_back(ImVec3(x1, 0, 0));
        s_ArrowTri[ARROW_CONE].push_back(ImVec3(x0, CONE_RADIUS*y1, CONE_RADIUS*z1));
        s_ArrowTri[ARROW_CONE].push_back(ImVec3(x1, 0, 0));
        s_ArrowNorm[ARROW_CONE].push_back(ImVec3(nn*nx, nn*y0, nn*z0));
        s_ArrowNorm[ARROW_CONE].push_back(ImVec3(nn*nx, nn*y0, nn*z0));
        s_ArrowNorm[ARROW_CONE].push_back(ImVec3(nn*nx, nn*y1, nn*z1));
        s_ArrowNorm[ARROW_CONE].push_back(ImVec3(nn*nx, nn*y0, nn*z0));
        s_ArrowNorm[ARROW_CONE].push_back(ImVec3(nn*nx, nn*y1, nn*z1));
        s_ArrowNorm[ARROW_CONE].push_back(ImVec3(nn*nx, nn*y1, nn*z1));
        s_ArrowTri[ARROW_CONE_CAP].push_back(ImVec3(x0, 0, 0));
        s_ArrowTri[ARROW_CONE_CAP].push_back(ImVec3(x0, CONE_RADIUS*y1, CONE_RADIUS*z1));
        s_ArrowTri[ARROW_CONE_CAP].push_back(ImVec3(x0, CONE_RADIUS*y0, CONE_RADIUS*z0));
        s_ArrowNorm[ARROW_CONE_CAP].push_back(ImVec3(-1, 0, 0));
        s_ArrowNorm[ARROW_CONE_CAP].push_back(ImVec3(-1, 0, 0));
        s_ArrowNorm[ARROW_CONE_CAP].push_back(ImVec3(-1, 0, 0));
    }

    for (i = 0; i < 4; ++i)
    {
        s_ArrowTriProj[i].clear();
        s_ArrowTriProj[i].resize(s_ArrowTri[i].size());
        s_ArrowColLight[i].clear();
        s_ArrowColLight[i].resize(s_ArrowTri[i].size());
    }
}



void ImOrient::ConvertToAxisAngle()
{
    if (fabs(Qt.w) > (1.0 + FLT_EPSILON))
    {
        //Axis.x = Axis.y = Axis.z = 0; // no, keep the previous value
        Angle = 0;
    }
    else
    {
        float a;
        if (Qt.w >= 1.0f)
            a = 0.0f; // and keep V
        else if (Qt.w <= -1.0f)
            a = float(M_PI); // and keep V
        else if (fabs(Qt.x*Qt.x + Qt.y*Qt.y + Qt.z*Qt.z + Qt.w*Qt.w) < (FLT_EPSILON * FLT_EPSILON))
            a = 0.0f;
        else
        {
            a = acos(Qt.w);
            if (a*Angle < 0) // Preserve the sign of Angle
                a = -a;
            float f = 1.0f / sin(a);
            Axis.x = Qt.x * f;
            Axis.y = Qt.y * f;
            Axis.z = Qt.z * f;
        }
        Angle = 2.0f*a;
    }

    Angle = ImRadToDeg(Angle);

    if (fabs(Angle) < FLT_EPSILON && fabs(Axis.x*Axis.x + Axis.y*Axis.y + Axis.z*Axis.z) < FLT_EPSILON * FLT_EPSILON)
        Axis.x = FLT_MIN;    // all components cannot be null
}

void ImOrient::ConvertFromAxisAngle()
{
    float n = Axis.x*Axis.x + Axis.y*Axis.y + Axis.z*Axis.z;
    if (fabs(n) > (FLT_EPSILON * FLT_EPSILON))
    {
        float f = 0.5f*ImDegToRad(Angle);
        Qt.w = cos(f);
        f = sin(f);

        Qt.x = Axis.x * f;
        Qt.y = Axis.y * f;
        Qt.z = Axis.z * f;
    }
    else
    {
        Qt.w = 1.0;
        Qt.x = Qt.y = Qt.z = 0.0;
    }
}

/*
void ImGui_Orient::CopyToVar()
{
    if( m_StructProxy!=NULL )
    {
        if( m_StructProxy->m_StructSetCallback!=NULL )
        {
            if( m_IsFloat )
            {
                if( m_IsDir )
                {
                    float d[] = {1, 0, 0};
                    ApplyQuat(d+0, d+1, d+2, 1, 0, 0, (float)Qt.x, (float)Qt.y, (float)Qt.z, (float)Qt.w);
                    float l = (float)sqrt(Dir.x*Dir.x + Dir.y*Dir.y + Dir.z*Dir.z);
                    d[0] *= l; d[1] *= l; d[2] *= l;
                    Dir.x = d[0]; Dir.y = d[1]; Dir.z = d[2]; // update also Dir.x,Dir.y,Dir.z
                    m_StructProxy->m_StructSetCallback(d, m_StructProxy->m_StructClientData);
                }
                else
                {
                    float q[] = { (float)Qt.x, (float)Qt.y, (float)Qt.z, (float)Qt.w };
                    m_StructProxy->m_StructSetCallback(q, m_StructProxy->m_StructClientData);
                }
            }
       }
        else if( m_StructProxy->m_StructData!=NULL )
        {
            if( m_IsFloat )
            {
                if( m_IsDir )
                {
                    float *d = static_cast<float *>(m_StructProxy->m_StructData);
                    ApplyQuat(d+0, d+1, d+2, 1, 0, 0, (float)Qt.x, (float)Qt.y, (float)Qt.z, (float)Qt.w);
                    float l = (float)sqrt(Dir.x*Dir.x + Dir.y*Dir.y + Dir.z*Dir.z);
                    d[0] *= l; d[1] *= l; d[2] *= l;
                    Dir.x = d[0]; Dir.y = d[1]; Dir.z = d[2]; // update also Dir.x,Dir.y,Dir.z
                }
                else
                {
                    float *q = static_cast<float *>(m_StructProxy->m_StructData);
                    q[0] = (float)Qt.x; q[1] = (float)Qt.y; q[2] = (float)Qt.z; q[3] = (float)Qt.w;
                }
            }
            else
            {
                if( m_IsDir )
                {
                    float *dd = static_cast<float *>(m_StructProxy->m_StructData);
                    float d[] = {1, 0, 0};
                    ApplyQuat(d+0, d+1, d+2, 1, 0, 0, (float)Qt.x, (float)Qt.y, (float)Qt.z, (float)Qt.w);
                    float l = sqrt(Dir.x*Dir.x + Dir.y*Dir.y + Dir.z*Dir.z);
                    dd[0] = l*d[0]; dd[1] = l*d[1]; dd[2] = l*d[2];
                    Dir.x = dd[0]; Dir.y = dd[1]; Dir.z = dd[2]; // update also Dir.x,Dir.y,Dir.z
                }
                else
                {
                    float *q = static_cast<float *>(m_StructProxy->m_StructData);
                    q[0] = Qt.x; q[1] = Qt.y; q[2] = Qt.z; q[3] = Qt.w;
                }
            }
        }
    }
}
*/

ImU32 ImOrient::ColorBlend(ImU32 _Color1, ImU32 _Color2, float sigma)
{
    ImColor color1(_Color1);
    ImColor color2(_Color2);
    float invSigma = 1.0f - sigma;

    color1 = ImColor((color1.Value.x * invSigma) + (color2.Value.x * sigma),
        (color1.Value.y * invSigma) + (color2.Value.y * sigma),
        (color1.Value.z * invSigma) + (color2.Value.z * sigma),
        (color1.Value.w * invSigma) + (color2.Value.w * sigma));

    return color1;
}


void ImOrient::QuatFromAxisAngle(ImQuat& out, const ImVec3& axis, float angle)
{
    float n = axis.x * axis.x + axis.y * axis.y + axis.z * axis.z;
    if (fabs(n) > FLT_EPSILON)
    {
        float f = 0.5f*angle;
        out.w = cos(f);
        f = sin(f) / sqrt(n);
        out.x = axis.x * f;
        out.y = axis.y * f;
        out.z = axis.z * f;
    }
    else
    {
        out.w = 1.0;
        out.x = out.y = out.z = 0.0;
    }
}

void ImOrient::QuatFromDir(ImQuat& out, const ImVec3& dir)
{
    // compute a quaternion that rotates (1,0,0) to (dx,dy,dz)
    float dn = sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
    if (dn < FLT_EPSILON * FLT_EPSILON)
    {
        out.x = out.y = out.z = 0;
        out.w = 1;
    }
    else
    {
        ImVec3 rotAxis = { 0, -dir.z, dir.y };
        if (rotAxis.x * rotAxis.x + rotAxis.y * rotAxis.y + rotAxis.z * rotAxis.z < (FLT_EPSILON * FLT_EPSILON))
        {
            rotAxis.x = rotAxis.y = 0;
            rotAxis.z = 1;
        }
        float rotAngle = acos(dir.x / dn);
        ImQuat rotQuat;
        QuatFromAxisAngle(rotQuat, rotAxis, rotAngle);
        out.x = rotQuat.x;
        out.y = rotQuat.y;
        out.z = rotQuat.z;
        out.w = rotQuat.w;
    }
}
