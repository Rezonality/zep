////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Fbx file encoder
// 

#ifndef VKU_fbx_encoder_INCLUDED
#define VKU_fbx_encoder_INCLUDED

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <exception>
#include <cstring>
#include <iostream>

#include <glm/glm.hpp>
#include <gilgamesh/mesh.hpp>
#include <gilgamesh/scene.hpp>

// see https://code.blender.org/2013/08/fbx-binary-file-format-specification/
// and https://banexdevblog.wordpress.com/2014/06/23/a-quick-tutorial-about-the-fbx-ascii-format/

namespace gilgamesh {
  template<class ValueType, class IdxType>
  void make_index(std::vector<ValueType> &values_out, std::vector<IdxType> &idx_out, const std::vector<ValueType> &values_in) {
    struct evec_t {
      ValueType v;
      size_t original_idx;
    };

    std::vector<evec_t> evec(values_in.size());
    for (size_t i = 0; i != values_in.size(); ++i) {
      evec[i] = evec_t{ values_in[i], i };
    }

    std::sort(
      evec.begin(), evec.end(),
      [](evec_t &a, evec_t &b) { return memcmp(&a.v, &b.v, sizeof(a.v)) < 0; }
    );

    values_out.resize(0);
    idx_out.resize(evec.size());
    size_t index = 0;
    for (size_t i = 0; i != evec.size();) {
      evec_t &e = evec[i];
      size_t new_idx = values_out.size();
      values_out.push_back(e.v);
      idx_out[e.original_idx] = (IdxType)new_idx;
      ++i;
      while (i != evec.size() && !memcmp(&evec[i].v, &e.v, sizeof(e.v))) {
        idx_out[evec[i].original_idx] = (IdxType)new_idx;
        ++i;
      }
    }
  }

  class fbx_encoder {
  public:
    fbx_encoder() {
    }

    void saveMesh(gilgamesh::mesh &mesh, const std::string &filename) {
      auto bytes = saveMesh(mesh);
      if (filename == "-") {
        std::cout.write((char*)bytes.data(), bytes.size());
      } else {
        auto fout = std::ofstream(filename, std::ios_base::binary);
        fout.write((char*)bytes.data(), bytes.size());
      }
    }

    void saveScene(const gilgamesh::scene &scene, const std::string &filename) {
      auto bytes = saveScene(scene);
      std::ofstream(filename, std::ios_base::binary).write((char*)bytes.data(), bytes.size());
    }

    std::vector<uint8_t> saveMesh(gilgamesh::mesh &mesh) {
      gilgamesh::scene scene;
      scene.addMesh(&mesh);
      scene.addNode(glm::mat4(), 0, 0);
      return std::move(saveScene(scene));
    }

    std::vector<uint8_t> saveScene(const gilgamesh::scene &scene) {
      //const std::vector<const mesh*> &meshes, const std::vector<glm::mat4> &transforms, const std::vector<int> &parent_transforms, const std::vector<int> &mesh_indices) {
      bytes_.resize(0);

      int version = 0x1ce8;

      static const uint8_t fbx_header[] = {
        0x4b, 0x61, 0x79, 0x64, 0x61, 0x72, 0x61, 0x20, 0x46, 0x42, 0x58, 0x20,
        0x42, 0x69, 0x6e, 0x61, 0x72, 0x79, 0x20, 0x20, 0x00, 0x1a, 0x00, 
      };
      bytes_.reserve(0x10000);

      bytes_.assign(fbx_header, fbx_header + sizeof(fbx_header));
      u4(version);

      writeFBXHeaderExtension();

      begin("FileId");
        R({0x28,0xb3,0x2a,0xeb,0xb6,0x24,0xcc,0xc2,0xbf,0xc8,0xb0,0x2a,0xa9,0x2b,0xfc,0xf1,});
      end("FileId");

      begin("CreationTime");
        S("1970-01-01 10:00:00:000");
      end("CreationTime");

      begin("Creator");
        S("Meshutils 0.1");
      end("Creator");

      writeGlobalSettings();
      writeDocuments();

      begin("References");
      end("References");

      writeDefinitions();

      begin("Objects");
        auto &meshes = scene.meshes();
        auto &transforms = scene.transforms();
        for (size_t i = 0; i != meshes.size(); ++i) {
          writeGeometry(*meshes[i], i);
        }

        for (size_t i = 0; i != transforms.size(); ++i) {
          writeModel(transforms[i], i);
        }

        // todo: support materials
        //writeMaterial(0);
      end("Objects");

      size_t model_index = 0;
      size_t mesh_index = 0;
      size_t material_index = 0;
      begin("Connections");
        // root node
        begin("C");
          S("OO");
          L(0x20000000 + model_index);
          L(0);
        end("C");

        begin("C");
          S("OO");
          L(0x10000000 + mesh_index);
          L(0x20000000 + model_index);
        end("C");

        /*begin("C");
          S("OO");
          L(0x30000000 + material_index);
          L(0x20000000 + model_index);
        end("C");*/
      end("Connections");

      // todo: (maybe) support animation.
      begin("Takes");
        begin("Current");
          S("");
        end("Current");
      end("Takes");
      nullnode();

      // like the header, this footer seems to server no function other than to infuriate codec writers.
      static const uint8_t foot_id[] = { 0xfa,0xbc,0xab,0x09,0xd0,0xc8,0xd4,0x66,0xb1,0x76,0xfb,0x83,0x1c,0xf7,0x26,0x7e,0x00,0x00,0x00,0x00 };
      for (size_t i = 0; i != sizeof(foot_id); ++i) bytes_.push_back(foot_id[i]);

      //int pad = ((bytes_.size() + 15) & ~15) - bytes_.size();
      //if (pad == 0) pad = 16;
      while (bytes_.size() & 0x0f) bytes_.push_back(0x00);

      u4(version);
      for (int i = 0; i != 120; ++i) bytes_.push_back(0x00);

      // another seemingly pointless binary string
      static const uint8_t unknown_id[] = { 0xf8,0x5a,0x8c,0x6a,0xde,0xf5,0xd9,0x7e,0xec,0xe9,0x0c,0xe3,0x75,0x8f,0x29,0x0b };
      for (size_t i = 0; i != sizeof(unknown_id); ++i) bytes_.push_back(unknown_id[i]);

      return std::move(bytes_);
    }
  public:

    void writeFBXHeaderExtension() {
      begin("FBXHeaderExtension");
        begin("FBXHeaderVersion");
          I(1003);
        end("FBXHeaderVersion");
        begin("FBXVersion");
          I(7400);
        end("FBXVersion");
        begin("EncryptionType");
          I(0);
        end("EncryptionType");
        begin("CreationTimeStamp");
          begin("Version");
            I(1000);
          end("Version");
          begin("Year");
            I(2016);
          end("Year");
          begin("Month");
            I(7);
          end("Month");
          begin("Day");
            I(25);
          end("Day");
          begin("Hour");
            I(19);
          end("Hour");
          begin("Minute");
            I(4);
          end("Minute");
          begin("Second");
            I(5);
          end("Second");
          begin("Millisecond");
            I(805);
          end("Millisecond");
        end("CreationTimeStamp");
        begin("Creator");
          S("Blender (stable FBX IO) - 2.76 (sub 0) - 3.6.2");
        end("Creator");
        begin("SceneInfo");
          S("GlobalInfo\x00\x01SceneInfo", 21);
          S("UserData");
          begin("Type");
            S("UserData");
          end("Type");
          begin("Version");
            I(100);
          end("Version");
          begin("MetaData");
            begin("Version");
              I(100);
            end("Version");
            begin("Title");
              S("");
            end("Title");
            begin("Subject");
              S("");
            end("Subject");
            begin("Author");
              S("");
            end("Author");
            begin("Keywords");
              S("");
            end("Keywords");
            begin("Revision");
              S("");
            end("Revision");
            begin("Comment");
              S("");
            end("Comment");
          end("MetaData");
          begin("Properties70");
            begin("P");
              S("DocumentUrl");
              S("KString");
              S("Url");
              S("");
              S("/foobar.fbx");
            end("P");
            begin("P");
              S("SrcDocumentUrl");
              S("KString");
              S("Url");
              S("");
              S("/foobar.fbx");
            end("P");
            begin("P");
              S("Original");
              S("Compound");
              S("");
              S("");
            end("P");
            begin("P");
              S("Original|ApplicationVendor");
              S("KString");
              S("");
              S("");
              S("Blender Foundation");
            end("P");
            begin("P");
              S("Original|ApplicationName");
              S("KString");
              S("");
              S("");
              S("Blender (stable FBX IO)");
            end("P");
            begin("P");
              S("Original|ApplicationVersion");
              S("KString");
              S("");
              S("");
              S("2.76 (sub 0)");
            end("P");
            begin("P");
              S("Original|DateTime_GMT");
              S("DateTime");
              S("");
              S("");
              S("01/01/1970 00:00:00.000");
            end("P");
            begin("P");
              S("Original|FileName");
              S("KString");
              S("");
              S("");
              S("/foobar.fbx");
            end("P");
            begin("P");
              S("LastSaved");
              S("Compound");
              S("");
              S("");
            end("P");
            begin("P");
              S("LastSaved|ApplicationVendor");
              S("KString");
              S("");
              S("");
              S("Blender Foundation");
            end("P");
            begin("P");
              S("LastSaved|ApplicationName");
              S("KString");
              S("");
              S("");
              S("Blender (stable FBX IO)");
            end("P");
            begin("P");
              S("LastSaved|ApplicationVersion");
              S("KString");
              S("");
              S("");
              S("2.76 (sub 0)");
            end("P");
            begin("P");
              S("LastSaved|DateTime_GMT");
              S("DateTime");
              S("");
              S("");
              S("01/01/1970 00:00:00.000");
            end("P");
          end("Properties70");
        end("SceneInfo");
      end("FBXHeaderExtension");
    }

    void writeGlobalSettings() {
      begin("GlobalSettings");
        begin("Version");
          I(1000);
        end("Version");
        begin("Properties70");
          begin("P");
            S("UpAxis");
            S("int");
            S("Integer");
            S("");
            I(1);
          end("P");
          begin("P");
            S("UpAxisSign");
            S("int");
            S("Integer");
            S("");
            I(1);
          end("P");
          begin("P");
            S("FrontAxis");
            S("int");
            S("Integer");
            S("");
            I(2);
          end("P");
          begin("P");
            S("FrontAxisSign");
            S("int");
            S("Integer");
            S("");
            I(1);
          end("P");
          begin("P");
            S("CoordAxis");
            S("int");
            S("Integer");
            S("");
            I(0);
          end("P");
          begin("P");
            S("CoordAxisSign");
            S("int");
            S("Integer");
            S("");
            I(1);
          end("P");
          begin("P");
            S("OriginalUpAxis");
            S("int");
            S("Integer");
            S("");
            I(-1);
          end("P");
          begin("P");
            S("OriginalUpAxisSign");
            S("int");
            S("Integer");
            S("");
            I(1);
          end("P");
          begin("P");
            S("UnitScaleFactor");
            S("double");
            S("Number");
            S("");
            D(  1.000000);
          end("P");
          begin("P");
            S("OriginalUnitScaleFactor");
            S("double");
            S("Number");
            S("");
            D(  1.000000);
          end("P");
          begin("P");
            S("AmbientColor");
            S("ColorRGB");
            S("Color");
            S("");
            D(  0.000000);
            D(  0.000000);
            D(  0.000000);
          end("P");
          begin("P");
            S("DefaultCamera");
            S("KString");
            S("");
            S("");
            S("Producer Perspective");
          end("P");
          begin("P");
            S("TimeMode");
            S("enum");
            S("");
            S("");
            I(11);
          end("P");
          begin("P");
            S("TimeSpanStart");
            S("KTime");
            S("Time");
            S("");
            L(0);
          end("P");
          begin("P");
            S("TimeSpanStop");
            S("KTime");
            S("Time");
            S("");
            L(46186158000);
          end("P");
          begin("P");
            S("CustomFrameRate");
            S("double");
            S("Number");
            S("");
            D( 24.000000);
          end("P");
        end("Properties70");
      end("GlobalSettings");
    }

    void writeDocuments() {
      begin("Documents");
        begin("Count");
          I(1);
        end("Count");
        begin("Document");
          L(860982320);
          S("Scene");
          S("Scene");
          begin("Properties70");
            begin("P");
              S("SourceObject");
              S("object");
              S("");
              S("");
            end("P");
            begin("P");
              S("ActiveAnimStackName");
              S("KString");
              S("");
              S("");
              S("");
            end("P");
          end("Properties70");
          begin("RootNode");
            L(0);
          end("RootNode");
        end("Document");
      end("Documents");
    }

    void writeDefinitions() {
      begin("Definitions");
        begin("Version");
          I(100);
        end("Version");
        begin("Count");
          I(4);
        end("Count");
        begin("ObjectType");
          S("GlobalSettings");
          begin("Count");
            I(1);
          end("Count");
        end("ObjectType");
        begin("ObjectType");
          S("Geometry");
          begin("Count");
            I(1);
          end("Count");
          begin("PropertyTemplate");
            S("FbxMesh");
            begin("Properties70");
              begin("P");
                S("Color");
                S("ColorRGB");
                S("Color");
                S("");
                D(  0.800000);
                D(  0.800000);
                D(  0.800000);
              end("P");
              begin("P");
                S("BBoxMin");
                S("Vector3D");
                S("Vector");
                S("");
                D(  0.000000);
                D(  0.000000);
                D(  0.000000);
              end("P");
              begin("P");
                S("BBoxMax");
                S("Vector3D");
                S("Vector");
                S("");
                D(  0.000000);
                D(  0.000000);
                D(  0.000000);
              end("P");
              begin("P");
                S("Primary Visibility");
                S("bool");
                S("");
                S("");
                I(1);
              end("P");
              begin("P");
                S("Casts Shadows");
                S("bool");
                S("");
                S("");
                I(1);
              end("P");
              begin("P");
                S("Receive Shadows");
                S("bool");
                S("");
                S("");
                I(1);
              end("P");
            end("Properties70");
          end("PropertyTemplate");
        end("ObjectType");
        begin("ObjectType");
          S("Model");
          begin("Count");
            I(1);
          end("Count");
          begin("PropertyTemplate");
            S("FbxNode");
            begin("Properties70");
              begin("P");
                S("QuaternionInterpolate");
                S("enum");
                S("");
                S("");
                I(0);
              end("P");
              begin("P");
                S("RotationOffset");
                S("Vector3D");
                S("Vector");
                S("");
                D(  0.000000);
                D(  0.000000);
                D(  0.000000);
              end("P");
              begin("P");
                S("RotationPivot");
                S("Vector3D");
                S("Vector");
                S("");
                D(  0.000000);
                D(  0.000000);
                D(  0.000000);
              end("P");
              begin("P");
                S("ScalingOffset");
                S("Vector3D");
                S("Vector");
                S("");
                D(  0.000000);
                D(  0.000000);
                D(  0.000000);
              end("P");
              begin("P");
                S("ScalingPivot");
                S("Vector3D");
                S("Vector");
                S("");
                D(  0.000000);
                D(  0.000000);
                D(  0.000000);
              end("P");
              begin("P");
                S("TranslationActive");
                S("bool");
                S("");
                S("");
                I(0);
              end("P");
              begin("P");
                S("TranslationMin");
                S("Vector3D");
                S("Vector");
                S("");
                D(  0.000000);
                D(  0.000000);
                D(  0.000000);
              end("P");
              begin("P");
                S("TranslationMax");
                S("Vector3D");
                S("Vector");
                S("");
                D(  0.000000);
                D(  0.000000);
                D(  0.000000);
              end("P");
              begin("P");
                S("TranslationMinX");
                S("bool");
                S("");
                S("");
                I(0);
              end("P");
              begin("P");
                S("TranslationMinY");
                S("bool");
                S("");
                S("");
                I(0);
              end("P");
              begin("P");
                S("TranslationMinZ");
                S("bool");
                S("");
                S("");
                I(0);
              end("P");
              begin("P");
                S("TranslationMaxX");
                S("bool");
                S("");
                S("");
                I(0);
              end("P");
              begin("P");
                S("TranslationMaxY");
                S("bool");
                S("");
                S("");
                I(0);
              end("P");
              begin("P");
                S("TranslationMaxZ");
                S("bool");
                S("");
                S("");
                I(0);
              end("P");
              begin("P");
                S("RotationOrder");
                S("enum");
                S("");
                S("");
                I(0);
              end("P");
              begin("P");
                S("RotationSpaceForLimitOnly");
                S("bool");
                S("");
                S("");
                I(0);
              end("P");
              begin("P");
                S("RotationStiffnessX");
                S("double");
                S("Number");
                S("");
                D(  0.000000);
              end("P");
              begin("P");
                S("RotationStiffnessY");
                S("double");
                S("Number");
                S("");
                D(  0.000000);
              end("P");
              begin("P");
                S("RotationStiffnessZ");
                S("double");
                S("Number");
                S("");
                D(  0.000000);
              end("P");
              begin("P");
                S("AxisLen");
                S("double");
                S("Number");
                S("");
                D( 10.000000);
              end("P");
              begin("P");
                S("PreRotation");
                S("Vector3D");
                S("Vector");
                S("");
                D(  0.000000);
                D(  0.000000);
                D(  0.000000);
              end("P");
              begin("P");
                S("PostRotation");
                S("Vector3D");
                S("Vector");
                S("");
                D(  0.000000);
                D(  0.000000);
                D(  0.000000);
              end("P");
              begin("P");
                S("RotationActive");
                S("bool");
                S("");
                S("");
                I(0);
              end("P");
              begin("P");
                S("RotationMin");
                S("Vector3D");
                S("Vector");
                S("");
                D(  0.000000);
                D(  0.000000);
                D(  0.000000);
              end("P");
              begin("P");
                S("RotationMax");
                S("Vector3D");
                S("Vector");
                S("");
                D(  0.000000);
                D(  0.000000);
                D(  0.000000);
              end("P");
              begin("P");
                S("RotationMinX");
                S("bool");
                S("");
                S("");
                I(0);
              end("P");
              begin("P");
                S("RotationMinY");
                S("bool");
                S("");
                S("");
                I(0);
              end("P");
              begin("P");
                S("RotationMinZ");
                S("bool");
                S("");
                S("");
                I(0);
              end("P");
              begin("P");
                S("RotationMaxX");
                S("bool");
                S("");
                S("");
                I(0);
              end("P");
              begin("P");
                S("RotationMaxY");
                S("bool");
                S("");
                S("");
                I(0);
              end("P");
              begin("P");
                S("RotationMaxZ");
                S("bool");
                S("");
                S("");
                I(0);
              end("P");
              begin("P");
                S("InheritType");
                S("enum");
                S("");
                S("");
                I(0);
              end("P");
              begin("P");
                S("ScalingActive");
                S("bool");
                S("");
                S("");
                I(0);
              end("P");
              begin("P");
                S("ScalingMin");
                S("Vector3D");
                S("Vector");
                S("");
                D(  0.000000);
                D(  0.000000);
                D(  0.000000);
              end("P");
              begin("P");
                S("ScalingMax");
                S("Vector3D");
                S("Vector");
                S("");
                D(  1.000000);
                D(  1.000000);
                D(  1.000000);
              end("P");
              begin("P");
                S("ScalingMinX");
                S("bool");
                S("");
                S("");
                I(0);
              end("P");
              begin("P");
                S("ScalingMinY");
                S("bool");
                S("");
                S("");
                I(0);
              end("P");
              begin("P");
                S("ScalingMinZ");
                S("bool");
                S("");
                S("");
                I(0);
              end("P");
              begin("P");
                S("ScalingMaxX");
                S("bool");
                S("");
                S("");
                I(0);
              end("P");
              begin("P");
                S("ScalingMaxY");
                S("bool");
                S("");
                S("");
                I(0);
              end("P");
              begin("P");
                S("ScalingMaxZ");
                S("bool");
                S("");
                S("");
                I(0);
              end("P");
              begin("P");
                S("GeometricTranslation");
                S("Vector3D");
                S("Vector");
                S("");
                D(  0.000000);
                D(  0.000000);
                D(  0.000000);
              end("P");
              begin("P");
                S("GeometricRotation");
                S("Vector3D");
                S("Vector");
                S("");
                D(  0.000000);
                D(  0.000000);
                D(  0.000000);
              end("P");
              begin("P");
                S("GeometricScaling");
                S("Vector3D");
                S("Vector");
                S("");
                D(  1.000000);
                D(  1.000000);
                D(  1.000000);
              end("P");
              begin("P");
                S("MinDampRangeX");
                S("double");
                S("Number");
                S("");
                D(  0.000000);
              end("P");
              begin("P");
                S("MinDampRangeY");
                S("double");
                S("Number");
                S("");
                D(  0.000000);
              end("P");
              begin("P");
                S("MinDampRangeZ");
                S("double");
                S("Number");
                S("");
                D(  0.000000);
              end("P");
              begin("P");
                S("MaxDampRangeX");
                S("double");
                S("Number");
                S("");
                D(  0.000000);
              end("P");
              begin("P");
                S("MaxDampRangeY");
                S("double");
                S("Number");
                S("");
                D(  0.000000);
              end("P");
              begin("P");
                S("MaxDampRangeZ");
                S("double");
                S("Number");
                S("");
                D(  0.000000);
              end("P");
              begin("P");
                S("MinDampStrengthX");
                S("double");
                S("Number");
                S("");
                D(  0.000000);
              end("P");
              begin("P");
                S("MinDampStrengthY");
                S("double");
                S("Number");
                S("");
                D(  0.000000);
              end("P");
              begin("P");
                S("MinDampStrengthZ");
                S("double");
                S("Number");
                S("");
                D(  0.000000);
              end("P");
              begin("P");
                S("MaxDampStrengthX");
                S("double");
                S("Number");
                S("");
                D(  0.000000);
              end("P");
              begin("P");
                S("MaxDampStrengthY");
                S("double");
                S("Number");
                S("");
                D(  0.000000);
              end("P");
              begin("P");
                S("MaxDampStrengthZ");
                S("double");
                S("Number");
                S("");
                D(  0.000000);
              end("P");
              begin("P");
                S("PreferedAngleX");
                S("double");
                S("Number");
                S("");
                D(  0.000000);
              end("P");
              begin("P");
                S("PreferedAngleY");
                S("double");
                S("Number");
                S("");
                D(  0.000000);
              end("P");
              begin("P");
                S("PreferedAngleZ");
                S("double");
                S("Number");
                S("");
                D(  0.000000);
              end("P");
              begin("P");
                S("LookAtProperty");
                S("object");
                S("");
                S("");
              end("P");
              begin("P");
                S("UpVectorProperty");
                S("object");
                S("");
                S("");
              end("P");
              begin("P");
                S("Show");
                S("bool");
                S("");
                S("");
                I(1);
              end("P");
              begin("P");
                S("NegativePercentShapeSupport");
                S("bool");
                S("");
                S("");
                I(1);
              end("P");
              begin("P");
                S("DefaultAttributeIndex");
                S("int");
                S("Integer");
                S("");
                I(-1);
              end("P");
              begin("P");
                S("Freeze");
                S("bool");
                S("");
                S("");
                I(0);
              end("P");
              begin("P");
                S("LODBox");
                S("bool");
                S("");
                S("");
                I(0);
              end("P");
              begin("P");
                S("Lcl Translation");
                S("Lcl Translation");
                S("");
                S("A");
                D(  0.000000);
                D(  0.000000);
                D(  0.000000);
              end("P");
              begin("P");
                S("Lcl Rotation");
                S("Lcl Rotation");
                S("");
                S("A");
                D(  0.000000);
                D(  0.000000);
                D(  0.000000);
              end("P");
              begin("P");
                S("Lcl Scaling");
                S("Lcl Scaling");
                S("");
                S("A");
                D(  1.000000);
                D(  1.000000);
                D(  1.000000);
              end("P");
              begin("P");
                S("Visibility");
                S("Visibility");
                S("");
                S("A");
                D(  1.000000);
              end("P");
              begin("P");
                S("Visibility Inheritance");
                S("Visibility Inheritance");
                S("");
                S("");
                I(1);
              end("P");
            end("Properties70");
          end("PropertyTemplate");
        end("ObjectType");
        /*begin("ObjectType");
          S("Material");
          begin("Count");
            I(1);
          end("Count");
          begin("PropertyTemplate");
            S("FbxSurfacePhong");
            begin("Properties70");
              begin("P");
                S("ShadingModel");
                S("KString");
                S("");
                S("");
                S("Phong");
              end("P");
              begin("P");
                S("MultiLayer");
                S("bool");
                S("");
                S("");
                I(0);
              end("P");
              begin("P");
                S("EmissiveColor");
                S("Color");
                S("");
                S("A");
                D(  0.000000);
                D(  0.000000);
                D(  0.000000);
              end("P");
              begin("P");
                S("EmissiveFactor");
                S("Number");
                S("");
                S("A");
                D(  1.000000);
              end("P");
              begin("P");
                S("AmbientColor");
                S("Color");
                S("");
                S("A");
                D(  0.200000);
                D(  0.200000);
                D(  0.200000);
              end("P");
              begin("P");
                S("AmbientFactor");
                S("Number");
                S("");
                S("A");
                D(  1.000000);
              end("P");
              begin("P");
                S("DiffuseColor");
                S("Color");
                S("");
                S("A");
                D(  0.800000);
                D(  0.800000);
                D(  0.800000);
              end("P");
              begin("P");
                S("DiffuseFactor");
                S("Number");
                S("");
                S("A");
                D(  1.000000);
              end("P");
              begin("P");
                S("TransparentColor");
                S("Color");
                S("");
                S("A");
                D(  0.000000);
                D(  0.000000);
                D(  0.000000);
              end("P");
              begin("P");
                S("TransparencyFactor");
                S("Number");
                S("");
                S("A");
                D(  0.000000);
              end("P");
              begin("P");
                S("Opacity");
                S("Number");
                S("");
                S("A");
                D(  1.000000);
              end("P");
              begin("P");
                S("NormalMap");
                S("Vector3D");
                S("Vector");
                S("");
                D(  0.000000);
                D(  0.000000);
                D(  0.000000);
              end("P");
              begin("P");
                S("Bump");
                S("Vector3D");
                S("Vector");
                S("");
                D(  0.000000);
                D(  0.000000);
                D(  0.000000);
              end("P");
              begin("P");
                S("BumpFactor");
                S("double");
                S("Number");
                S("");
                D(  1.000000);
              end("P");
              begin("P");
                S("DisplacementColor");
                S("ColorRGB");
                S("Color");
                S("");
                D(  0.000000);
                D(  0.000000);
                D(  0.000000);
              end("P");
              begin("P");
                S("DisplacementFactor");
                S("double");
                S("Number");
                S("");
                D(  1.000000);
              end("P");
              begin("P");
                S("VectorDisplacementColor");
                S("ColorRGB");
                S("Color");
                S("");
                D(  0.000000);
                D(  0.000000);
                D(  0.000000);
              end("P");
              begin("P");
                S("VectorDisplacementFactor");
                S("double");
                S("Number");
                S("");
                D(  1.000000);
              end("P");
              begin("P");
                S("SpecularColor");
                S("Color");
                S("");
                S("A");
                D(  0.200000);
                D(  0.200000);
                D(  0.200000);
              end("P");
              begin("P");
                S("SpecularFactor");
                S("Number");
                S("");
                S("A");
                D(  1.000000);
              end("P");
              begin("P");
                S("Shininess");
                S("Number");
                S("");
                S("A");
                D( 20.000000);
              end("P");
              begin("P");
                S("ShininessExponent");
                S("Number");
                S("");
                S("A");
                D( 20.000000);
              end("P");
              begin("P");
                S("ReflectionColor");
                S("Color");
                S("");
                S("A");
                D(  0.000000);
                D(  0.000000);
                D(  0.000000);
              end("P");
              begin("P");
                S("ReflectionFactor");
                S("Number");
                S("");
                S("A");
                D(  1.000000);
              end("P");
            end("Properties70");
          end("PropertyTemplate");
        end("ObjectType");*/
      end("Definitions");
    }

    void writeGeometry(const mesh &mesh, size_t index) {
      std::vector<uint32_t> indices = mesh.indices32();
      std::vector<glm::vec3> pos = mesh.pos();
      std::vector<glm::vec3> normal = mesh.normal();
      std::vector<glm::vec4> color = mesh.color();

      std::vector<glm::vec3> epos;
      std::vector<glm::vec4> ecolor;
      std::vector<glm::uint32_t> ipos;
      std::vector<glm::uint32_t> icolor;

      make_index(epos, ipos, pos);
      make_index(ecolor, icolor, color);

      glm::vec4 white(1, 1, 1, 1);
      bool has_color = ecolor.size() != 1 || ecolor[0] != white;

      begin("Geometry");
        L(index + 0x10000000);
        S("Cube\x00\x01Geometry", 14);
        S("Mesh");
        begin("Properties70");
        end("Properties70");
        begin("GeometryVersion");
          I(124);
        end("GeometryVersion");
        begin("Vertices");
          std::vector<double> dpos;
          dpos.reserve(epos.size() * 3);
          for (auto &p : epos) {
            dpos.push_back((double)p.x);
            dpos.push_back((double)p.y);
            dpos.push_back((double)p.z);
          }
          d(dpos.data(), dpos.size());
        end("Vertices");
        begin("PolygonVertexIndex");
          std::vector<uint32_t> pvi;
          for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            pvi.push_back(ipos[indices[i+0]]);
            pvi.push_back(ipos[indices[i+1]]);
            pvi.push_back(~ipos[indices[i+2]]);
          }
          i(pvi.data(), pvi.size());
        end("PolygonVertexIndex");
        /*begin("Edges");
          std::vector<uint32_t> edges;
          for (size_t i = 0; i < indices.size(); i += 3) {
            edges.push_back(i);
          }
          i(edges.data(), edges.size());
        end("Edges");*/
        begin("LayerElementNormal");
          I(0);
          begin("Version");
            I(101);
          end("Version");
          begin("Name");
            S("");
          end("Name");
          begin("MappingInformationType");
            S("ByPolygonVertex");
          end("MappingInformationType");
          begin("ReferenceInformationType");
            S("Direct");
          end("ReferenceInformationType");
          begin("Normals");
            std::vector<double> dnormal;
            for (size_t i = 0; i < indices.size(); ++i) {
              auto &p = normal[indices[i]];
              dnormal.push_back((double)p.x);
              dnormal.push_back((double)p.y);
              dnormal.push_back((double)p.z);
            }
            d(dnormal.data(), dnormal.size());
          end("Normals");
        end("LayerElementNormal");
        /*begin("LayerElementMaterial");
          I(0);
          begin("Version");
            I(101);
          end("Version");
          begin("Name");
            S("");
          end("Name");
          begin("MappingInformationType");
            S("AllSame");
          end("MappingInformationType");
          begin("ReferenceInformationType");
            S("IndexToDirect");
          end("ReferenceInformationType");
          begin("Materials");
            uint32_t mat[] = { 0 };
            i(mat, 1);
          end("Materials");
        end("LayerElementMaterial");*/
        if (has_color) {
          begin("LayerElementColor");
            I(0);
            begin("Version");
              I(101);
            end("Version");
            begin("Name");
              S("Colour");
            end("Name");
            begin("MappingInformationType");
              S("ByPolygonVertex");
            end("MappingInformationType");
            begin("ReferenceInformationType");
              S("IndexToDirect");
            end("ReferenceInformationType");
            begin("Colors");
              std::vector<double> dcolor;
              dcolor.reserve(ecolor.size() * 4);
              for (auto &p : ecolor) {
                dcolor.push_back((double)p.x);
                dcolor.push_back((double)p.y);
                dcolor.push_back((double)p.z);
                dcolor.push_back((double)p.w);
              }
              d(dcolor.data(), dcolor.size());
            end("Colors");
            begin("ColorIndex");
              std::vector<uint32_t> iicolor(indices.size());
              for (size_t i = 0; i < indices.size(); ++i) {
                iicolor[i] = icolor[indices[i]];
              }
              i(iicolor.data(), iicolor.size());
            end("ColorIndex");
          end("LayerElementColor");
        }
        begin("Layer");
          I(0);
          begin("Version");
            I(100);
          end("Version");
          begin("LayerElement");
            begin("Type");
              S("LayerElementNormal");
            end("Type");
            begin("TypedIndex");
              I(0);
            end("TypedIndex");
          end("LayerElement");
          if (has_color) {
            begin("LayerElement");
              begin("Type");
                S("LayerElementColor");
              end("Type");
              begin("TypedIndex");
                I(0);
              end("TypedIndex");
            end("LayerElement");
          }
          /*begin("LayerElement");
            begin("Type");
              S("LayerElementMaterial");
            end("Type");
            begin("TypedIndex");
              I(0);
            end("TypedIndex");
          end("LayerElement");*/
        end("Layer");
      end("Geometry");
    }

    void writeModel(const glm::mat4 &mat, size_t index) {
      // transpose of rotation matrix
      glm::vec3 xrow(mat[0].x, mat[1].x, mat[2].x);
      glm::vec3 yrow(mat[0].y, mat[1].y, mat[2].y);
      glm::vec3 zrow(mat[0].z, mat[1].z, mat[2].z);

      // derive scale from transpose
      glm::vec3 scale(
        glm::length(xrow),
        glm::length(yrow),
        glm::length(zrow)
      );

      // remove scale
      xrow /= scale.x;
      yrow /= scale.y;
      zrow /= scale.z;

      // translation
      glm::vec3 translate = xrow * mat[3].x + yrow * mat[3].y + zrow * mat[3].z;

      // http://www.staff.city.ac.uk/~sbbh653/publications/euler.pdf
      // http://www.geometrictools.com/Documentation/EulerAngles.pdf
      float rx = 0, ry = 0, rz = 0;
      if (xrow.z >= 0.99999f) {
        ry = 90;
        rx = std::atan2(yrow.x, yrow.y) * 57.29577951308232f;
        rz = 0;
      } else if (xrow.z <= -0.99999f) {
        ry = -90;
        rx = -std::atan2(yrow.x, yrow.y) * 57.29577951308232f;
        rz = 0;
      } else {
        ry = std::asin(xrow.z) * 57.29577951308232f;
        rx = std::atan2(-yrow.z, zrow.z) * 57.29577951308232f;
        rz = std::atan2(-xrow.y, xrow.x) * 57.29577951308232f;
      }

      begin("Model");
        //L(511284486);
        L(0x20000000 + index);
        S("Cube\x00\x01Model", 11);
        S("Mesh");
        begin("Version");
          I(232);
        end("Version");
        begin("Properties70");
          begin("P");
            S("Lcl Rotation");
            S("Lcl Rotation");
            S("");
            S("A");
            D(rx);
            D(ry);
            D(rz);
          end("P");
          begin("P");
            S("Lcl Scaling");
            S("Lcl Scaling");
            S("");
            S("A");
            D(scale.x * 100);
            D(scale.y * 100);
            D(scale.z * 100);
          end("P");
          begin("P");
            S("Lcl Translation");
            S("Lcl Translation");
            S("");
            S("A");
            D(translate.x);
            D(translate.y);
            D(translate.z);
          end("P");
          begin("P");
            S("DefaultAttributeIndex");
            S("int");
            S("Integer");
            S("");
            I(0);
          end("P");
          begin("P");
            S("InheritType");
            S("enum");
            S("");
            S("");
            I(1);
          end("P");
        end("Properties70");
        begin("MultiLayer");
          I(0);
        end("MultiLayer");
        begin("MultiTake");
          I(0);
        end("MultiTake");
        begin("Shading");
          C(true);
        end("Shading");
        begin("Culling");
          S("CullingOff");
        end("Culling");
      end("Model");
    }

    void writeMaterial(size_t index) {
      begin("Material");
        L(0x30000000 + index);
        S("Material\x00\x01Material", 18);
        S("");
        begin("Version");
          I(102);
        end("Version");
        begin("ShadingModel");
          S("Phong");
        end("ShadingModel");
        begin("MultiLayer");
          I(0);
        end("MultiLayer");
        begin("Properties70");
          begin("P");
            S("EmissiveColor");
            S("Color");
            S("");
            S("A");
            D(  0.800000);
            D(  0.800000);
            D(  0.800000);
          end("P");
          begin("P");
            S("EmissiveFactor");
            S("Number");
            S("");
            S("A");
            D(  0.000000);
          end("P");
          begin("P");
            S("AmbientColor");
            S("Color");
            S("");
            S("A");
            D(  0.000000);
            D(  0.000000);
            D(  0.000000);
          end("P");
          begin("P");
            S("DiffuseColor");
            S("Color");
            S("");
            S("A");
            D(  0.800000);
            D(  0.800000);
            D(  0.800000);
          end("P");
          begin("P");
            S("DiffuseFactor");
            S("Number");
            S("");
            S("A");
            D(  0.800000);
          end("P");
          begin("P");
            S("TransparentColor");
            S("Color");
            S("");
            S("A");
            D(  1.000000);
            D(  1.000000);
            D(  1.000000);
          end("P");
          begin("P");
            S("SpecularColor");
            S("Color");
            S("");
            S("A");
            D(  1.000000);
            D(  1.000000);
            D(  1.000000);
          end("P");
          begin("P");
            S("SpecularFactor");
            S("Number");
            S("");
            S("A");
            D(  0.250000);
          end("P");
          begin("P");
            S("Shininess");
            S("Number");
            S("");
            S("A");
            D(  9.607843);
          end("P");
          begin("P");
            S("ShininessExponent");
            S("Number");
            S("");
            S("A");
            D(  9.607843);
          end("P");
          begin("P");
            S("ReflectionColor");
            S("Color");
            S("");
            S("A");
            D(  1.000000);
            D(  1.000000);
            D(  1.000000);
          end("P");
          begin("P");
            S("ReflectionFactor");
            S("Number");
            S("");
            S("A");
            D(  0.000000);
          end("P");
        end("Properties70");
      end("Material");
    }

    struct node {
      size_t offset;
      uint32_t num_properties;
      size_t property_list_len;
      size_t property_list_start;
      const char *name;
    };

    std::vector<uint8_t> bytes_;
    std::vector<node> nodes;
    bool just_ended = false;

    void u1(int value) {
      bytes_.push_back((uint8_t)value);
    }

    void u2(int value) {
      u1(value);
      u1(value>>8);
    }

    void u4(int value) {
      u2(value);
      u2(value>>16);
    }

    void u8(uint64_t value) {
      u4((int)value);
      u4((int)(value>>32));
    }

    void begin(const char *name) {
      just_ended = false;
      //printf("begin %x %s\n", (unsigned)bytes_.size(), name);

      node n = {};
      n.offset = bytes_.size();
      n.name = name;

      u4(0);
      u4(0);
      u4(0);
      u1((int)strlen(name));
      while (*name) u1(*name++);

      n.property_list_start = bytes_.size();
      nodes.push_back(n);
    }

    void nullnode() {
      for (int i = 0; i != 13; ++i) bytes_.push_back(0);
    }

    void end(const char *name) {
      node &n = nodes.back();
      if (just_ended || n.property_list_start == bytes_.size()) {
        nullnode();
      }
      uint32_t offset = (uint32_t)n.offset;
      uint32_t end_offset = (uint32_t)bytes_.size();
      bytes_[offset+0] = (uint8_t)(end_offset >> 0);
      bytes_[offset+1] = (uint8_t)(end_offset >> 8);
      bytes_[offset+2] = (uint8_t)(end_offset >> 16);
      bytes_[offset+3] = (uint8_t)(end_offset >> 24);
      bytes_[offset+4] = (uint8_t)(n.num_properties >> 0);
      bytes_[offset+5] = (uint8_t)(n.num_properties >> 8);
      bytes_[offset+6] = (uint8_t)(n.num_properties >> 16);
      bytes_[offset+7] = (uint8_t)(n.num_properties >> 24);
      bytes_[offset+8] = (uint8_t)(n.property_list_len >> 0);
      bytes_[offset+9] = (uint8_t)(n.property_list_len >> 8);
      bytes_[offset+10] = (uint8_t)(n.property_list_len >> 16);
      bytes_[offset+11] = (uint8_t)(n.property_list_len >> 24);

      if (strcmp(name, n.name)) {
        throw std::runtime_error("non-matching end");
      }
      nodes.pop_back();
      just_ended = true;

    }

    void prop(char code) {
      node &node = nodes.back();
      node.num_properties++;
      u1(code);
    }

    void propend() {
      node &node = nodes.back();
      node.property_list_len = bytes_.size() - node.property_list_start;
    }

    void Y(int value) {
      prop('Y');
      u2(value);
      propend();
    }

    void C(int value) {
      prop('C');
      u1(value);
      propend();
    }

    void I(int value) {
      prop('I');
      u4(value);
      propend();
    }

    void F(double value) {
      prop('F');
      union { float f; int v; } u;
      u.f = (float)value;
      u4(u.v);
      propend();
    }

    void D(double value) {
      prop('D');
      union { double f; uint64_t v; } u;
      u.f = value;
      u8(u.v);
      propend();
    }

    void L(uint64_t value) {
      prop('L');
      u8(value);
      propend();
    }

    void f(const float *value, size_t size) {
      prop('f');
      u4((int)size);
      u4(0);
      u4((int)size * 4);
      union { float f; int v; } u;
      while (size--) {
        u.f = *value++;
        u4(u.v);
      }
      propend();
    }

    void d(const double *value, size_t size) {
      prop('d');
      u4((int)size);
      u4(0);
      u4((int)size * 8);
      union { double f; uint64_t v; } u;
      while (size--) {
        u.f = *value++;
        u8(u.v);
      }
      propend();
    }

    void l(const uint64_t *value, size_t size) {
      prop('l');
      u4((int)size);
      u4(0);
      u4((int)size * 8);
      while (size--) {
        u8(*value++);
      }
      propend();
    }

    void i(const uint32_t *value, size_t size) {
      prop('i');
      u4((int)size);
      u4(0);
      u4((int)size * 4);
      while (size--) {
        u4(*value++);
      }
      propend();
    }

    void b(const int *value, size_t size) {
      prop('b');
      u4((int)size);
      u4(0);
      u4((int)size * 4);
      while (size--) {
        u4(*value++);
      }
      propend();
    }

    void S(const char *value) {
      size_t size = strlen(value);
      S(value, size);
      propend();
    }

    void S(const char *value, size_t size) {
      prop('S');
      u4((int)size);
      while (size--) {
        u1(*value++);
      }
      propend();
    }

    void R(const std::initializer_list<uint8_t> &value) {
      prop('R');
      u4((int)value.size());
      for (auto c : value) {
        u1(c);
      }
      propend();
    }
  };

}

#endif
