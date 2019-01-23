#include "glm/glm/gtc/quaternion.hpp"

#include "math/mathutils.h"
#include "math/randomutils.h"

static std::random_device rd;
static std::mt19937 mt(rd());

double RandRange(double min, double max)
{
    std::uniform_real_distribution<double> dist(min, std::nextafter(max, DBL_MAX));
    return dist(mt);
}

float RandRange(float min, float max)
{
    std::uniform_real_distribution<float> dist(min, std::nextafter(max, FLT_MAX));
    return dist(mt);
}

uint32_t RandRange(uint32_t min, uint32_t max)
{
    std::uniform_int_distribution<uint32_t> dist(min, max);
    return dist(mt);
}

int32_t RandRange(int32_t min, int32_t max)
{
    std::uniform_int_distribution<int32_t> dist (min, max);
    return dist(mt);
}

/** Build a unit quaternion representing the rotation
 * from u to v. The input vectors need not be normalised. */
glm::quat QuatFromVectors(glm::vec3 u, glm::vec3 v)
{
    float norm_u_norm_v = sqrt(dot(u, u) * dot(v, v));
    float real_part = norm_u_norm_v + dot(u, v);
    glm::vec3 w;

    if (real_part < 1.e-6f * norm_u_norm_v)
    {
        /* If u and v are exactly opposite, rotate 180 degrees
         * around an arbitrary orthogonal axis. Axis normalisation
         * can happen later, when we normalise the quaternion. */
        real_part = 0.0f;
        w = std::abs(u.x) > std::abs(u.z) ? glm::vec3(-u.y, u.x, 0.f)
            : glm::vec3(0.f, -u.z, u.y);
    }
    else
    {
        /* Otherwise, build quaternion the standard way. */
        w = cross(u, v);
    }

    return glm::normalize(glm::quat(real_part, w.x, w.y, w.z));
}

float SmoothStep(float val)
{
    return val * val * (3.0f - 2.0f * val);
}

glm::vec4 RectClip(const glm::vec4& rect, const glm::vec4& clip)
{
    glm::vec4 ret = rect;
    if (ret.x < clip.x)
    {
        ret.x = clip.x;
    }
    if (ret.y < clip.y)
    {
        ret.y = clip.y;
    }
    if ((ret.x + ret.z) > (clip.x + clip.z))
    {
        ret.z = clip.z + clip.x - ret.x;
    }
    if ((ret.y + ret.w) > (clip.y + clip.w))
    {
        ret.w = clip.w + clip.y - ret.y;
    }

    return ret;
}

float Luminance(const glm::vec4& color)
{
    return Luminance(glm::vec3(color.x, color.y, color.z) * color.w);
}

float Luminance(const glm::vec3& color)
{
    // Perceived.
    return (0.299f * color.x + 0.587f * color.y + 0.114f * color.z);
}

glm::vec4 Desaturate(const glm::vec4& col)
{
    float r = col.x;
    float g = col.y;
    float b = col.z;

    r += .5f;
    g += .5f;
    b += .5f;

    auto sq = glm::sqrt((r * r) + (g * g) + (b * b));
    if (sq != 0.0f)
        sq = 1.0f / sq;

    r = r * sq;
    g = g * sq;
    b = b * sq;

    return glm::vec4(r, g, b, col.w);
}

glm::vec4 Saturate(const glm::vec4& col)
{
    float r = col.x;
    float g = col.y;
    float b = col.z;

    r -= .25f;
    g -= .25f;
    b -= .25f;

    if (r < 0.0f)
        r = 0.0f;
    if (g < 0.0f)
        g = 0.0f;
    if (b < 0.0f)
        b = 0.0f;
    return glm::vec4(r, g, b, col.w);
}

void GetBounds(const glm::vec3* coords, uint32_t count, glm::vec3& min, glm::vec3& max)
{
    if (count == 0 || coords == nullptr)
    {
        min = glm::vec3(0.0f);
        max = glm::vec3(0.0f);
        return;
    }

    min = glm::vec3(std::numeric_limits<float>::max());
    max = glm::vec3(-std::numeric_limits<float>::max());
    for (uint32_t i = 0; i < count; i++)
    {
        min = glm::min(min, coords[i]);
        max = glm::max(max, coords[i]);
    }
}
