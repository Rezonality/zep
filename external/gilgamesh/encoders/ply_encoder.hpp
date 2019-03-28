////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// gilgamesh: Stanford PLY encoder class
// 

#ifndef MESHUTILS_PLY_ENCODER_INCLUDED
#define MESHUTILS_PLY_ENCODER_INCLUDED

#include <fstream>

namespace gilgamesh {

class ply_encoder {
public:
  ply_encoder() {
  }

  template <class MeshTraits>
  void saveMesh(const basic_mesh<MeshTraits> &mesh, const std::string &filename, bool ascii=true, const char *features="pnuc") {
    if (filename == "-") {
      encode(mesh, std::cout, ascii, features);
    } else {
      std::ofstream fout(filename, ascii ? std::ios_base::openmode() : std::ios_base::binary);
      encode(mesh, fout, ascii, features);
    }
  }

  template <class MeshTraits, class Writer>
  void encode(const basic_mesh<MeshTraits> &mesh, Writer &writer, bool ascii=true, const char *features="pnuc") {
    auto wr = [&writer](const char *stuff) {
      writer.write(stuff, strlen(stuff));
    };

    char tmp[256];
    wr("ply\n");
    if (ascii) {
      wr("format ascii 1.0\n");
    } else {
      wr("format binary_little_endian 1.0\n");
    }
    wr("comment Created by https://github.com/andy-thomason/gilgamesh\n");
    snprintf(tmp, sizeof(tmp), "element vertex %d\n", (int)mesh.vertices().size());
    wr(tmp);

    bool pos_enabled = strchr(features, 'p') != nullptr;
    bool normal_enabled = strchr(features, 'n') != nullptr;
    bool uv_enabled = strchr(features, 'u') != nullptr;
    bool color_enabled = strchr(features, 'c') != nullptr;

    if (pos_enabled) {
      wr("property float x\n");
      wr("property float y\n");
      wr("property float z\n");
    }

    if (normal_enabled) {
      wr("property float nx\n");
      wr("property float ny\n");
      wr("property float nz\n");
    }

    if (uv_enabled) {
      wr("property float u\n");
      wr("property float v\n");
    }

    if (color_enabled) {
      wr("property uchar red\n");
      wr("property uchar green\n");
      wr("property uchar blue\n");
    }

    snprintf(tmp, sizeof(tmp), "element face %d\n", (int)mesh.indices().size()/3);
    wr(tmp);
    wr("property list uchar uint vertex_indices\n");
    wr("end_header\n");

    auto touchar = [](float x) { return std::max(std::min(int(x*256), 255), 0); };

    for (auto &v : mesh.vertices()) {
      char *p = tmp;
      char *e = tmp + sizeof(tmp);

      auto wf32 = [&p, e](float v) {
        union { uint32_t u; float f; } u;
        u.f = v;
        if (p < e-3) { *p++ = (char)u.u; *p++ = (char)(u.u >> 8); *p++ = (char)(u.u >> 16); *p++ = (char)(u.u >> 24); }
      };
      auto wu8 = [&p, e](int v) {
        if (p != e) *p++ = (char)v;
      };
        
      if (pos_enabled) {
        glm::vec3 pos = v.pos();
        if (ascii) {
          p += snprintf(p, e - p, "%f %f %f ", pos.x, pos.y, pos.z);
        } else {
          wf32(pos.x); wf32(pos.y); wf32(pos.z);
        }
      }
      if (normal_enabled) {
        glm::vec3 normal = v.normal();
        if (ascii) {
          p += snprintf(p, e - p, "%f %f %f ", normal.x, normal.y, normal.z);
        } else {
          wf32(normal.x); wf32(normal.y); wf32(normal.z);
        }
      }
      if (uv_enabled) {
        glm::vec2 uv = v.uv();
        if (ascii) {
          p += snprintf(p, e - p, "%f %f ", uv.x, uv.y);
        } else {
          wf32(uv.x); wf32(uv.y);
        }
      }
      if (color_enabled) {
        glm::vec4 color = v.color();
        int r = touchar(color.x);
        int g = touchar(color.y);
        int b = touchar(color.z);
        if (ascii) {
          p += snprintf(p, e - p, "%3d %3d %3d ", r, g, b);
        } else {
          wu8(r); wu8(g); wu8(b);
        }
      }

      if (ascii && p != e) *p++ = '\n';

      writer.write(tmp, size_t(p - tmp));
    }

    auto &indices = mesh.indices();
    for (size_t i = 0; i < indices.size(); i += 3) {
      char *p = tmp;
      char *e = tmp + sizeof(tmp);

      auto wu32 = [&p, e](uint32_t v) {
        *p++ = (char)v; *p++ = (char)(v >> 8); *p++ = (char)(v >> 16); *p++ = (char)(v >> 24);
      };
      if (ascii) {
        p += snprintf(tmp, e - p, "3 %d %d %d\n", indices[i], indices[i+1], indices[i+2]);
      } else {
        *p++ = 3; wu32(indices[i]); wu32(indices[i+1]); wu32(indices[i+2]);
      }
      writer.write(tmp, size_t(p - tmp));
    }
  }
private:
  
};

}

#endif
