#include <gtest/gtest.h>
#include "mcommon/math/mathutils.h"

#ifdef USE_GLM
TEST(MathUtils, RectEmpty)
{
    ASSERT_TRUE(IsRectEmpty(glm::vec4(0)));
}

TEST(MathUtils, RectContains)
{
    ASSERT_TRUE(RectContains(glm::vec4(1, 2, 50, 60), glm::vec2(30, 30)));
    ASSERT_FALSE(RectContains(glm::vec4(1, 2, 50, 60), glm::vec2(80, 30)));
}

TEST(MathUtils, Clamp)
{
    ASSERT_TRUE(Clamp(9, 4, 8) == 8);
    ASSERT_TRUE(Clamp(6, 4, 8) == 6);
    ASSERT_TRUE(Clamp(2, 4, 8) == 4);
}

TEST(MathUtils, RectClip)
{
    ASSERT_TRUE(RectClip(glm::vec4(3, 3, 5, 5), glm::vec4(4, 4, 2, 2)) == glm::vec4(4,4, 2, 2));
}

TEST(MathUtils, Bounds)
{
    std::vector<glm::vec3> bounds;

    bounds.push_back(glm::vec3(.3, -.5, .6));
    bounds.push_back(glm::vec3(.9, -.8, .6));

    glm::vec3 min, max;
    GetBounds(&bounds[0], 2, min, max);
    ASSERT_FLOAT_EQ(min.x, .3f);
    ASSERT_FLOAT_EQ(min.y, -.8f);
    ASSERT_FLOAT_EQ(min.z, .6f);

    ASSERT_FLOAT_EQ(max.x, .9f);
    ASSERT_FLOAT_EQ(max.y, -.5f);
    ASSERT_FLOAT_EQ(max.z, .6f);

}
#endif
