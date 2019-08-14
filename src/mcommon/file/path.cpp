#include "zep/mcommon/file/path.h"

namespace Zep
{

// http://stackoverflow.com/a/29221546/18942
ZepPath path_get_relative(const ZepPath& from, const ZepPath& to)
{
    // Start at the root path and while they are the same then do nothing then when they first
    // diverge take the remainder of the two path and replace the entire from path with ".."
    // segments.
    ZepPath::const_iterator fromIter = from.begin();
    ZepPath::const_iterator toIter = to.begin();

    // Loop through both
    while (fromIter != from.end() && toIter != to.end() && (*toIter) == (*fromIter))
    {
        ++toIter;
        ++fromIter;
    }

    ZepPath finalPath;
    while (fromIter != from.end())
    {
        finalPath = finalPath / "..";
        ++fromIter;
    }

    while (toIter != to.end())
    {
        finalPath = finalPath / *toIter;
        ++toIter;
    }

    return finalPath;
}

} // Zep
