#pragma once

// This is a quick and simple sort of vertices by distance from a plane.
// It is a quick way to find points near a given list of other points, by narrowing your search to similar distances
// At the moment 
class VertexSpacialSort
{
public:
    struct SortedVertex
    {
        uint32_t index;
        float distance;
        bool operator < (const SortedVertex& v) const { return distance < v.distance; }
    };
    using Iterator = std::vector<SortedVertex>::iterator;

    // A list of vertices to search
    VertexSpacialSort(const std::vector<glm::vec3>& vertices);

    // Find a range of vertices close to this position
    bool Find(const glm::vec3& position,
        std::pair<Iterator, Iterator>& pair);

protected:
    glm::vec3 m_rootPlane = glm::normalize(glm::vec3(.5, .1, .8));

    std::vector<SortedVertex> m_sortedVertices;
};
