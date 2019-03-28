////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// gilgamesh: distance field class
//
// This field represents the minimum distance from a set of points
// 

#ifndef GILGAMESH_DISTANCE_FIELD_INCLUDED
#define GILGAMESH_DISTANCE_FIELD_INCLUDED

#include <glm/glm.hpp>
#include "utils.hpp"

#if 0
namespace gilgamesh {

  class distance_field {
    // square of closest distance to points
    std::vector<float> distance_squared_;
    int xdim_;
    int ydim_;
    int zdim_;
    float grid_spacing_;
  public:
    distance_field() {
    }

    // distance field from a set of points up to max_radius
    // This is calculated in parallel using an axis sweep
    distance_field(int xdim, int ydim, int zdim, float grid_spacing, glm::vec3 min, const std::vector<glm::vec3> &points, const std::vector<float> &radii, float max_radius) :
      xdim_(xdim), ydim_(ydim), zdim_(zdim), grid_spacing_(grid_spacing)
    {
      std::vector<glm::vec4> zsorter(points.size());
      for (size_t i = 0; i != points.size(); ++i) {
        zsorter[i].x = points[i].x;
        zsorter[i].y = points[i].y;
        zsorter[i].z = points[i].z;
        zsorter[i].w = radii[i] * radii[i];
      }

      distance_squared_.resize((xdim+1)*(ydim+1)*(zdim+1));

      auto idx = [xdim, ydim](int x, int y, int z) {
        return ((z * (ydim+1)) + y) * (xdim+1) + x;
      };

      auto cmpz = [](const glm::vec4 &a, const glm::vec4 &b) { return a.z < b.z; };
      auto cmpy = [](const glm::vec4 &a, const glm::vec4 &b) { return a.y < b.y; };

      // initially sort the points by z
      std::sort(zsorter.begin(), zsorter.end(), cmpz);

      // process in parallel by z value first
      float outside_value = -(max_radius * max_radius);
      par_for(0, zdim+1, [&](int z) {
        std::vector<glm::vec4> ysorter;

        // search only a band of z values in zpos +/- max_radius
        float zpos = z * grid_spacing + min.z;
        auto p = std::lower_bound( zsorter.begin(), zsorter.end(), glm::vec4(0, 0, (glm::vec3)zpos - (max_radius + grid_spacing)), cmpz);
        auto q = std::upper_bound( zsorter.begin(), zsorter.end(), glm::vec4(0, 0, (glm::vec3)zpos + (max_radius + grid_spacing)), cmpz);

        for (int y = 0; y != ydim+1; ++y) {
          float ypos = y * grid_spacing + min.y;

          // filter by y position
          ysorter.clear();
          for (auto r = p; r != q; ++r) {
            if (std::abs(r->y - ypos) <= max_radius + grid_spacing) {
              ysorter.push_back(*r);
            }
          }

          for (int x = 0; x != xdim+1; ++x) {
            float value = 1e37f;
            glm::vec4 xyz(x * grid_spacing + min.x, ypos, zpos);

            // find the closest point to xyz.
            for (auto &r : ysorter) {
              glm::vec3 pos(r.x, r.y, r.z);
              float d2 = glm::dot(xyz - r, xyz - r) - r.w;
              value = std::min(value, d2);
            }
            if (value == 1e37f) {
              value = outside_value;
            }
            distance_squared_[idx(x, y, z)] = value;
          }
        }
      });

    }

    float distance_squared(int x, int y, int z) const {
      return distance_squared_[((z * (ydim_+1)) + y) * (xdim_+1) + x];
    }
  };
}
#endif

#endif
