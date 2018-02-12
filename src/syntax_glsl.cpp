#include "syntax_glsl.h"
#include "utils/stringutils.h"

namespace
{
}

namespace Zep
{

ZepSyntaxGlsl::ZepSyntaxGlsl(ZepBuffer& buffer)
    : TParent(buffer)
{
    keywords.insert({ "float", "vec2", "vec3", "vec4", "int", "uint", "mat2", "mat3", "mat4", "mat" });
    keywords.insert({ "uniform", "layout", "location", "void", "out", "in" });
    keywords.insert({ "#version", "core" });
    keywords.insert({ "sampler1D", "sampler2D", "sampler3D" });
    keywords.insert({ "pow", "sin", "cos", "mul", "abs", "floor", "ceil" });
    keywords.insert({ "gl_position" });
}

ZepSyntaxGlsl::~ZepSyntaxGlsl()
{
}

} // Zep
