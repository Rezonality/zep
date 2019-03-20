#include "app/app.h"
#include "vertex_spacial_sort.h"
#include <glm/gtc/ulp.hpp>

VertexSpacialSort::VertexSpacialSort(const std::vector<glm::vec3>& vertices)
{
    // Make a sorted list of vertex indices, by distance from a random plane
    m_sortedVertices.resize(vertices.size());
    for (uint32_t index = 0; index < uint32_t(vertices.size()); index++)
    {
        float distance = glm::dot(vertices[index], m_rootPlane);
        m_sortedVertices[index] = SortedVertex{ index, distance };
    }
    std::sort(m_sortedVertices.begin(), m_sortedVertices.end());
}

bool VertexSpacialSort::Find(const glm::vec3& position, std::pair<Iterator, Iterator>& ret)
{
    float distance = glm::dot(position, m_rootPlane);
    struct Comp
    {
        bool operator() ( const SortedVertex& s, float d )
        {
            return s.distance < d;
        }
     
        bool operator() ( float d, const SortedVertex& s )
        {
            return d < s.distance;
        }
    };

    // Search for the range of positions found at this distance.  We are pretty much looking for matching ones in this function
    ret = std::equal_range(m_sortedVertices.begin(), m_sortedVertices.end(), distance, Comp());

    if (ret.first != m_sortedVertices.end() &&
        ret.second != m_sortedVertices.end())
    {
        return true;
    }
    return false;
}
