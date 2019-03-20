#include "app/app.h"

#include "indexer.h"
#include "vertex_spacial_sort.h"
#include <bitset>
#include <glm/gtc/epsilon.hpp>

void indexVBO(
    std::vector<glm::vec3> & in_vertices,
    std::vector<glm::vec2> & in_uvs,
    std::vector<glm::vec3> & in_normals,
    std::vector<uint32_t> & out_indices,
    std::vector<glm::vec3> & out_vertices,
    std::vector<glm::vec2> & out_uvs,
    std::vector<glm::vec3> & out_normals
)
{
    VertexSpacialSort sorter(in_vertices);
    std::vector<int32_t> indices(in_vertices.size(), -1);

    // For each input vertex
    for (uint32_t i = 0; i < in_vertices.size(); i++)
    {
        if (indices[i] == -1)
        {
            out_vertices.push_back(in_vertices[i]);
            out_uvs.push_back(in_uvs[i]);
            out_normals.push_back(in_normals[i]);
            indices[i] = uint32_t(out_vertices.size() - 1);
            out_indices.push_back(indices[i]);

            std::pair<VertexSpacialSort::Iterator, VertexSpacialSort::Iterator> searchResult;
            if (sorter.Find(in_vertices[i], searchResult))
            {
                for (auto& itr = searchResult.first; itr != searchResult.second; itr++)
                {
                    if (indices[itr->index] == -1)
                    {
                        if (glm::all(glm::epsilonEqual(in_vertices[i], in_vertices[itr->index], glm::epsilon<float>())) &&
                            glm::all(glm::epsilonEqual(in_normals[i], in_normals[itr->index], glm::epsilon<float>())) &&
                            glm::all(glm::epsilonEqual(in_uvs[i], in_uvs[itr->index], glm::epsilon<float>())))
                        {
                            indices[itr->index] = indices[i];
                        }
                    }
                }
            }
        }
        else
        {
            out_indices.push_back(indices[i]);
        }
    }
}
