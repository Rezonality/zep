///////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// High performance Protien Data Bank and CIF file format reader
// 
// PDB files are Fortran-style text files containing positions of atoms in molecules.
// CIF files are similar but with variable field sizes.

#ifndef MESHUTILS_pdb_decoder_INCLUDED
#define MESHUTILS_pdb_decoder_INCLUDED

#include <iostream>
#include <cstdint>
#include <string>
#include <cstring>
#include <vector>
#include <cmath>

#include <glm/glm.hpp>


// https://en.wikipedia.org/wiki/Protein_Data_Bank_(file_format)
// https://www.cgl.ucsf.edu/chimera/docs/UsersGuide/tutorials/framepdbintro.html
namespace gilgamesh {
  class pdb_decoder {
  public:
    // This data structure is design to be as compact as possible.
    // please do not add std::string!
    class atom {
      friend pdb_decoder;

      glm::vec3 pos_;
      int serial_;
      int resSeq_;
      float x_;
      float y_;
      float z_;
      float occupancy_;
      float tempFactor_;
      std::array<char, 4> atomName_;
      std::array<char, 4> resName_;
      std::array<char, 2> element_;
      std::array<char, 2> charge_;
      char chainID_;
      char iCode_;
      bool is_hetatom_;
      char altLoc_;

    public:
      atom() {
      }

      atom(const uint8_t *p, const uint8_t *eol, bool is_hetatom) : is_hetatom_(is_hetatom) {
        serial_ = atoi(p - 1 + 7, p + 11);
        read(atomName_, p - 1 + 13, p + 16);
        altLoc_ = (char)p[-1+17];
        read(resName_, p - 1 + 18, p + 20);
        chainID_ = (char)p[-1+22];
        resSeq_ = atoi(p - 1 + 23, p + 26);
        iCode_ = (char)p[-1+27];
        pos_.x = atof(p - 1 + 31, p + 38);
        pos_.y = atof(p - 1 + 39, p + 46);
        pos_.z = atof(p - 1 + 47, p + 54);
        occupancy_ = atof(p - 1 + 55, p + 60);
        tempFactor_ = atof(p - 1 + 61, p + 66);
        read(element_, p - 1 + 77, p + 78);
        read(charge_, p - 1 + 79, p + 80);
      }

      // http://www.wwpdb.org/documentation/file-format-content/format33/sect9.html#ATOM
      //  7 - 11        Integer       serial       Atom  serial number.
      // 13 - 16        Atom          name         Atom name.
      // 17             Character     altLoc       Alternate location indicator.
      // 18 - 20        Residue name  resName      Residue name.
      // 22             Character     chainID      Chain identifier.
      // 23 - 26        Integer       resSeq       Residue sequence number.
      // 27             AChar         iCode        Code for insertion of residues.
      // 31 - 38        Real(8.3)     x            Orthogonal coordinates for X in Angstroms.
      // 39 - 46        Real(8.3)     y            Orthogonal coordinates for Y in Angstroms.
      // 47 - 54        Real(8.3)     z            Orthogonal coordinates for Z in Angstroms.
      // 55 - 60        Real(6.2)     occupancy    Occupancy.
      // 61 - 66        Real(6.2)     tempFactor   Temperature  factor.
      // 77 - 78        LString(2)    element      Element symbol, right-justified.
      // 79 - 80        LString(2)    charge       Charge  on the atom.
      int serial() const { return serial_; }
      std::string atomName() const { return std::string(atomName_.begin(), atomName_.end()); }
      char altLoc() const { return altLoc_; }
      std::string resName() const { return std::string(resName_.begin(), resName_.end()); }

      char chainID() const { return chainID_; }
      int resSeq() const { return resSeq_; }
      char iCode() const { return iCode_; }
      float x() const { return pos_.x; }
      float y() const { return pos_.y; }
      float z() const { return pos_.z; }
      glm::vec3 pos() const { return pos_; }
      float occupancy() const { return occupancy_; }
      float tempFactor() const { return tempFactor_; }
      std::string element() const { return std::string(element_.begin(), element_.end()); }
      std::string charge() const { return std::string(charge_.begin(), charge_.end()); }

      bool resNameIs(const char *name) const { return compare(resName_, name); }
      bool atomNameIs(const char *name) const { return compare(atomName_, name); }
      bool elementIs(const char *name) const { return compare(element_, name); }
      bool chargeIs(const char *name) const { return compare(charge_, name); }
      bool isHydrogen() const { return compare(element_, "H"); }

      bool is_hetatom() const { return is_hetatom_; }

      glm::vec4 colorByFunction() const {
        if (
          (atomNameIs("NZ") && resNameIs("LYS")) ||
          (atomNameIs("NH1") && resNameIs("ARG")) ||
          (atomNameIs("NH2") && resNameIs("ARG")) ||
          (atomNameIs("ND1") && resNameIs("HIS")) ||
          (atomNameIs("NE2") && resNameIs("HIS"))
        ) {
          // Positive: blue
          return glm::vec4(0, 0, 1, 1);
        } else if (
          (atomNameIs("OE1") && resNameIs("GLU")) ||
          (atomNameIs("OE2") && resNameIs("GLU")) ||
          (atomNameIs("OD1") && resNameIs("ASP")) ||
          (atomNameIs("OD2") && resNameIs("ASP"))
        ) {
          // Negative: red
          return glm::vec4(1, 0, 0, 1);
        } else {
          return glm::vec4(1, 1, 1, 1);
        }
      }

      glm::vec4 colorByElement() const {
        // https://en.wikipedia.org/wiki/CPK_coloring

        struct data_t { char name[4]; uint32_t color; };

        static const data_t cpk[] = {
          "H", 0xffffff, "C", 0x222222, "N", 0x2233ff, "O", 0xff2200, "S", 0xdddd00,
        };
        static const data_t jmol[] = {
          "H", 0xffffff, "C", 0x909090, "N", 0x3050F8, "O", 0xFF0D0D, "F", 0x90E050,
          "NA", 0xAB5CF2, "MG", 0x8AFF00, "AL", 0xBFA6A6, "SI", 0xF0C8A0,
          "P", 0xFF8000, "S", 0xFFFF30, "CL", 0x1FF01F, "AR", 0x80D1E3,
          "K", 0x8F40D4, "CA", 0x3DFF00,
        };

        char e0 = element_[0];
        char e1 = element_[1];
        uint32_t color = 0xdd77ff;
        for (const data_t &d : jmol) {
          if (e0 == d.name[0] && e1 == d.name[1]) {
            color = d.color;
            break;
          }
        }
        return glm::vec4((color >> 16) * (1.0f/255), ((color >> 8)&0xff) * (1.0f/255), (color & 0xff) * (1.0f/255), 1.0f);
      }

      float vanDerVaalsRadius() const {
        // https://en.wikipedia.org/wiki/Atomic_radii_of_the_elements_(data_page)

        struct data_t { char name[4]; short vdv; };

        static const data_t data[] = {
          "H", 120, "C", 170, "N", 155, "O", 152, "S", 180, "HE", 140, "LI", 182, "BE", 153, "B", 192, 
          "F", 147, "NE", 154, "NA", 227, "MG", 173, "AL", 184, "SI", 210, "P", 180,
          "CL", 175, "AR", 188, "K", 275, "CA", 231, "SC", 211, "NI", 163, "CU", 140, "ZN", 139,
          "GA", 187, "GE", 211, "AS", 185, "SE", 190, "BR", 185, "KR", 202, "RB", 303, "SR", 249, 
          "PD", 163, "AG", 172, "CD", 158, "IN", 193, "SN", 217, "SB", 206, "TE", 206, "I", 198, 
          "XE", 216, "CS", 343, "BA", 268, "PT", 175, "AU", 166, "HG", 155, "TL", 196, "PB", 202,
          "BI", 207, "PO", 197, "AT", 202, "RN", 220, "FR", 348, "RA", 283, "U", 186,
        };

        char e0 = element_[0];
        char e1 = element_[1];
        for (const data_t &d : data) {
          if (e0 == d.name[0] && e1 == d.name[1]) {
            return d.vdv * 0.01f;
          }
        }
        return 1.2f;
      }
    };


    pdb_decoder() {
    }

    pdb_decoder(const uint8_t *begin, const uint8_t *end) {
      glm::mat4 biomt;

      if (begin + 5 <= end && !memcmp(begin, "HEADE", 5)) {
        // PDB format
        for (const uint8_t *p = begin; p != end; ) {
          const uint8_t *eol = p;
          while (eol != end && *eol != '\n') ++eol;
          const uint8_t *next_p = eol != end ? eol + 1 : end;
          while (eol != p && (*eol == '\r' || *eol == '\n')) --eol;
          if (p != eol) {
            switch (*p) {
              case 'A': {
                if (p + 5 < eol && !memcmp(p, "ATOM  ", 6)) {
                  atoms_.emplace_back(p, eol, false);
                }
              } break;
              case 'H': {
                if (p + 5 < eol && !memcmp(p, "HETATM", 6)) {
                  atoms_.emplace_back(p, eol, true);
                }
              } break;
              case 'C': {
                if (p + 5 < eol && !memcmp(p, "CONECT", 6)) {
                  // COLUMNS       DATA  TYPE      FIELD        DEFINITION
                  // -------------------------------------------------------------------------
                  //  1 -  6        Record name    "CONECT"
                  //  7 - 11       Integer        serial       Atom  serial number
                  //  12 - 16        Integer        serial       Serial number of bonded atom
                  //  17 - 21        Integer        serial       Serial  number of bonded atom
                  //  22 - 26        Integer        serial       Serial number of bonded atom
                  //  27 - 31        Integer        serial       Serial number of bonded atom
                  int a0 = atoi(p + 1 + 7, p + 11);
                  int a1 = atoi(p + 1 + 12, p + 16);
                  int a2 = atoi(p + 1 + 17, p + 21);
                  int a3 = atoi(p + 1 + 22, p + 26);
                  int a4 = atoi(p + 1 + 27, p + 31);
                  if (a0 && a1) connections_.emplace_back(a0, a1);
                  if (a0 && a2) connections_.emplace_back(a0, a2);
                  if (a0 && a3) connections_.emplace_back(a0, a3);
                  if (a0 && a4) connections_.emplace_back(a0, a4);
                }
              } break;
              case 'R': {
                if (p + 18 < eol && !memcmp(p, "REMARK 350   BIOMT", 18)) {
                  int row, inst;
                  float x, y, z, w;
                  sscanf((char*)p + 18, "%d %d %f %f %f %f", &row, &inst, &x, &y, &z, &w);
                  //printf("%d %d %f %f %f %f\n", row, inst, x, y, z, w);
                  if (row >= 1 && row <= 3) {
                    biomt[0][row-1] = x;
                    biomt[1][row-1] = y;
                    biomt[2][row-1] = z;
                    biomt[3][row-1] = w;
                  }
                  if (row == 3) {
                    //printf("%s\n", glm::to_string(biomt).c_str());
                    instanceMatrices_.push_back(biomt);
                  }
                }
              } break;
            }
          }
          p = next_p;
        }
      } else {
        // CIF format (very liberal parser!)
        for (const uint8_t *p = begin; p != end; ++p) {
          // Skip whitespace.
          for(;;) {
            while (p != end && *p <= ' ') {
              ++p;
            }
            if (p != end && *p == '#') {
              // Comment.
              while (p != end && *p != '\r' && *p != '\n') {
                ++p;
              }
            } else {
              break;
            }
          }

          // Read token
          if (p == end) break;

          if (*p == ';' && p != begin && (p[-1] == '\n' || p[-1] == '\r')) {
            auto b = ++p;
            for (;;) {
              if (p == end) break;
              if (*p == ';' && (p[-1] == '\n' || p[-1] == '\r')) break;
              ++p;
            }
            cif_value(b, p);
            p += p != end;
          } if (*p == '\'' || *p == '"') {
            auto delim = *p++;
            auto b = p;
            while (p != end && *p != delim) {
              ++p;
            }
            cif_value(b, p);
            p += p != end;
          } else {
            auto b = p;
            while (p != end && *p >= '!') {
              ++p;
            }
            switch (*b) {
              case 'd': case 'D': {
                if (p - b >= 5 && (b[1] | 0x20) == 'a' && (b[2] | 0x20) == 't' && (b[3] | 0x20) == 'a' && b[4] == '_') {
                  cif_endloop();
                  cif_data(b, p);
                } else {
                  cif_value(b, p);
                }
              } break;
              case 's': case 'S': {
                if (p - b >= 5 && (b[1] | 0x20) == 'a' && (b[2] | 0x20) == 'v' && (b[3] | 0x20) == 'e' && b[4] == '_') {
                  cif_endloop();
                  cif_save(b, p);
                } else if (p - b >= 5 && (b[1] | 0x20) == 't' && (b[2] | 0x20) == 'o' && (b[3] | 0x20) == 'p' && b[4] == '_') {
                  cif_endloop();
                  cif_stop(b, p);
                } else {
                  cif_value(b, p);
                }
              } break;
              case 'g': case 'G': {
                if (p - b == 7 && (b[1] | 0x20) == 'l' && (b[2] | 0x20) == 'o' && (b[3] | 0x20) == 'b' && (b[4] | 0x20) == 'a' && (b[5] | 0x20) == 'l' && b[6] == '_') {
                  cif_endloop();
                  cif_global();
                } else {
                  cif_value(b, p);
                }
              } break;
              case 'l': case 'L': {
                if (p - b == 5 && (b[1] | 0x20) == 'o' && (b[2] | 0x20) == 'o' && (b[3] | 0x20) == 'p' && b[4] == '_') {
                  cif_endloop();
                  cif_loop();
                } else {
                  cif_value(b, p);
                }
              } break;
              case '_': {
                cif_endloop();
                cif_tag(b, p);
              } break;
              default: {
                cif_value(b, p);
              } break;
            }
          }
        }
      }
    }

    /// Get the atoms in a set of chains.
    /// If use_hetatoms is true, include HETATM atoms.
    /// HETATM atoms are auxiliary atoms to proteins such as water or ions.
    /// If invert is true, skip HETATMs and return atoms *not* in the chains.
    std::vector<atom> atoms(const std::string &chains, bool invert=false, bool use_hetatoms = false) const {
      std::vector<atom> result;
      for (int idx = 0; idx != atoms_.size(); ++idx) {
        auto &p = atoms_[idx];
        char chainID = p.chainID();
        if (!invert) {
          if (!p.is_hetatom() || use_hetatoms) {
            // if the chain is in the set specified on the command line (eg. ACBD)
            if (chains.find(chainID) != std::string::npos) {
              result.push_back(p);
            }
          }
        } else {
          // if the chain is not in the set specified on the command line (eg. ACBD)
          if (!p.is_hetatom() && chains.find(chainID) == std::string::npos) {
            result.push_back(p);
          }
        }
      }
      return std::move(result);
    }

    /// Return the set of chains used in this PDB file (ie. "ABCD").
    /// If use_hetatoms is true, include HETATM "chains".
    std::string chains(bool use_hetatoms=false) const {
      bool used[256] = {};
      for (auto &p : atoms_) {
        if (!p.is_hetatom() || use_hetatoms) {
          used[p.chainID()] = true;
        }
      }
      std::string result;
      for (size_t i = 32; i != 127; ++i) {
        if (used[i]) result.push_back((char)i);
      }
      return std::move(result);
    }

    /// Use knowledge of the chemistry to add connections to a list.
    int addImplicitConnections(const std::vector<atom> &atoms, std::vector<std::pair<int, int> > &out, size_t bidx, size_t eidx, int prevC, bool is_ca) const {
      static const char table[][5] = {
        "ASP",
          "CB", "CG",
          "CG", "OD1",
          "CG", "OD2",
          "!",
        "ALA",
          "!",
        "CYS",
          "CB", "SG",
          "!",
        "GLU",
          "CB", "CG",
          "CG", "CD",
          "CD", "OE1",
          "CD", "OE2",
          "!",
        "PHE",
          "CB", "CG",
          "CG", "CD1",
          "CG", "CD2",
          "CD1", "CE1",
          "CD2", "CE2",
          "CE1", "CZ",
          "CE2", "CZ",
          "!",
        "GLY",
          "!",
        "HIS",
          "CB", "CG",
          "CG", "ND1",
          "CG", "CD2",
          "ND1", "CE1",
          "CD2", "NE2",
          "CE1", "NE2",
          "!",
        "ILE",
          "CB", "CG1",
          "CB", "CG2",
          "CG1", "CD1",
          "!",
        "LYS",
          "CB", "CG",
          "CG", "CD",
          "CD", "CE",
          "CE", "NZ",
          "!",
        "LEU",
          "CB", "CG",
          "CG", "CD1",
          "CG", "CD2",
          "!",
        "MET",
          "CB", "CG",
          "CG", "SD",
          "SD", "CE",
          "!",
        "ASN",
          "CB", "CG",
          "CG", "OD1",
          "CG", "ND2",
          "!",
        "PRO",
          "CB", "CG",
          "CG", "CD",
          "!",
        "GLN",
          "CB", "CG",
          "CG", "CD",
          "CD", "OE1",
          "CD", "NE2",
          "!",
        "ARG",
          "CB", "CG",
          "CG", "CD",
          "CD", "NE",
          "NE", "CZ",
          "CZ", "NH1",
          "CZ", "NH2",
          "!",
        "SER",
          "CB", "OG",
          "!",
        "THR",
          "CB", "OG1",
          "CB", "CG2",
          "!",
        "VAL",
          "CB", "CG1",
          "CB", "CG2",
          "!",
        "TRP",
          "CB", "CG",
          "CG", "CD1",
          "CG", "CD2",
          "CD1", "NE1",
          "CD2", "CE3",
          "NE1", "CE2",
          "CE2", "CZ2",
          "CE3", "CZ3",
          "CZ2", "CH2",
          "CZ3", "CH2",
          "!",
        "TYR",
          "CB", "CG",
          "CG", "CD1",
          "CG", "CD2",
          "CD1", "CE1",
          "CD2", "CE2",
          "CE1", "CZ",
          "CE2", "CZ",
          "CZ", "OH",
          "!",
        "A",
          "C1'", "N9",
          "N9", "C8",
          "C8", "N7",
          "N7", "C5",
          "C5", "C6",
          "C6", "N6",
          "C6", "N1",
          "N1", "C2",
          "C2", "N3",
          "N3", "C4",
          "C4", "N9",
          "C4", "C5",
          "!",
        "C",
          "C1'", "N1",
          "N1", "C2",
          "C2", "O2",
          "C2", "N3",
          "N3", "C4",
          "C4", "N4",
          "C4", "C5",
          "C5", "C6",
          "C6", "N1",
          "!",
        "G",
          "C1'", "N9",
          "C1'", "N9",
          "N9", "C8",
          "C8", "N7",
          "N7", "C5",
          "C5", "C6",
          "C6", "O6",
          "C6", "N1",
          "N1", "C2",
          "C2", "N3",
          "N3", "C4",
          "C4", "N9",
          "C4", "C5",
          "!",
        "U",
          "C1'", "N1",
          "N1", "C2",
          "C2", "O2",
          "C2", "N3",
          "N3", "C4",
          "C4", "O4",
          "C4", "C5",
          "C5", "C6",
          "C6", "N1",
          "!",
        ""
      };

      if (atoms[bidx].resNameIs("A") || atoms[bidx].resNameIs("C") || atoms[bidx].resNameIs("G") || atoms[bidx].resNameIs("U")) {
        int P_idx = findAtom(atoms, bidx, eidx, "P");
        int OP1_idx = findAtom(atoms, bidx, eidx, "OP1");
        int OP2_idx = findAtom(atoms, bidx, eidx, "OP2");
        int O5d_idx = findAtom(atoms, bidx, eidx, "O5'");
        int C5d_idx = findAtom(atoms, bidx, eidx, "C5'");
        int C4d_idx = findAtom(atoms, bidx, eidx, "C4'");
        int O4d_idx = findAtom(atoms, bidx, eidx, "O4'");
        int C3d_idx = findAtom(atoms, bidx, eidx, "C3'");
        int O3d_idx = findAtom(atoms, bidx, eidx, "O3'");
        int C2d_idx = findAtom(atoms, bidx, eidx, "C2'");
        int O2d_idx = findAtom(atoms, bidx, eidx, "O2'");
        int C1d_idx = findAtom(atoms, bidx, eidx, "C1'");

        if (O5d_idx != -1 && C5d_idx != -1 && C4d_idx != -1 && O4d_idx != -1 && C3d_idx != -1 && O3d_idx != -1 && C2d_idx != -1 && O2d_idx != -1 && C1d_idx != -1) {
          if (P_idx != -1) {
            if (prevC != -1) out.emplace_back(prevC, P_idx);
            if (OP1_idx != -1) out.emplace_back(P_idx, OP1_idx);
            if (OP2_idx != -1) out.emplace_back(P_idx, OP2_idx);
            out.emplace_back(P_idx, O5d_idx);
          }

          out.emplace_back(C5d_idx, C4d_idx);
          out.emplace_back(C4d_idx, C3d_idx);
          out.emplace_back(C3d_idx, C2d_idx);
          out.emplace_back(C2d_idx, C1d_idx);

          out.emplace_back(C5d_idx, O5d_idx);
          out.emplace_back(C4d_idx, O4d_idx);
          out.emplace_back(C3d_idx, O3d_idx);
          out.emplace_back(C2d_idx, O2d_idx);
          out.emplace_back(C1d_idx, O4d_idx);
          prevC = O3d_idx;
        } else {
          printf("bad base %d %d %d %d %d %d %d %d %d %d %d\n", OP1_idx, OP2_idx, O5d_idx, C5d_idx, C4d_idx, O4d_idx, C3d_idx, O3d_idx, C2d_idx, O2d_idx, C1d_idx);
          return -1;
        }
      } else {
        int N_idx = findAtom(atoms, bidx, eidx, "N");
        int C_idx = findAtom(atoms, bidx, eidx, "C");
        int O_idx = findAtom(atoms, bidx, eidx, "O");
        int CA_idx = findAtom(atoms, bidx, eidx, "CA");
        int CB_idx = findAtom(atoms, bidx, eidx, "CB");

        //printf("find %s N%d C%d O%d CA%d CB%d\n", atoms[bidx].resName().c_str(), N_idx, C_idx, O_idx, CA_idx, CB_idx);

        if (N_idx == -1 || C_idx == -1 || CA_idx == -1) {
          printf("addImplicitConnections: bad %s N%d C%d O%d CA%d CB%d\n", atoms[bidx].resName().c_str(), N_idx, C_idx, O_idx, CA_idx, CB_idx);
          return -1;
        }

        if (is_ca) {
          if (prevC != -1) {
            out.emplace_back(prevC, CA_idx);
          }
          prevC = CA_idx;
        } else {
          if (prevC != -1) {
            out.emplace_back(prevC, N_idx);
          }

          out.emplace_back(N_idx, CA_idx);

          out.emplace_back(CA_idx, C_idx);

          if (O_idx != -1) out.emplace_back(C_idx, O_idx);

          prevC = C_idx;
        }

        //return C_idx;

        // All except GLY
        if (CB_idx != -1) {
          out.emplace_back(CA_idx, CB_idx);
        }
      }

      //printf("!%s\n", atoms[bidx].resName().c_str());
      bool found = false;
      for (size_t i = 0; table[i][0];) {
        //printf("%d -%s\n", (int)i, table[i]);
        if (atoms[bidx].resNameIs(table[i])) {
          const char *res_name = table[i];
          ++i;
          while (table[i][0] != '!') {
            //printf("  %s %s\n", table[i], table[i+1]);
            int from = findAtom(atoms, bidx, eidx, table[i]);
            int to = findAtom(atoms, bidx, eidx, table[i+1]);
            i += 2;
            //printf("  %d..%d\n", from, to);
            if (from != -1 && to != -1) {
              out.emplace_back(from, to);
            } else {
              //int resSeq = atoms[bidx].resSeq();
              //printf("Unexpected chemistry in %s/%d: %s %d   %s %d\n", res_name, resSeq, table[i-2], from, table[i-1], to);
            }
          }
          found = true;
          break;
        } else {
          ++i;
          while (table[i][0] != '!') {
            //printf("skip  %s %s\n", table[i], table[i+1]);
            i += 2;
          }
          i++;
        }
      }
      if (!found) {
        printf("not found\n");
      }

      return prevC;
    }
    // return the index of the next resiude
    size_t nextResidue(const std::vector<atom> &atoms, size_t bidx) const {
      int resSeq = atoms[bidx].resSeq();
      char iCode = atoms[bidx].iCode();
      //printf("%d rs=%d ic=%c\n", atoms[bidx].serial(), resSeq, iCode);
      size_t eidx = bidx + 1;
      for (; eidx != atoms.size(); ++eidx) {
        if (atoms[eidx].resSeq() != resSeq || atoms[eidx].iCode() != iCode) {
          break;
        }
      }
      return eidx;
    }

    int findAtom(const std::vector<atom> &atoms, size_t bidx, size_t eidx, const char *name) const {
      for (size_t i = bidx; i != eidx; ++i) {
        if (atoms[i].atomNameIs(name)) {
          return (int)i;
        }
      }
      return -1;
    }

    // Some PDB files contain instance information for the molecules. 
    const std::vector<glm::mat4> &instanceMatrices() const { return instanceMatrices_; }

  private:
    struct res {
      uint8_t *p;
      bool ok = false;
    };

    static constexpr bool debug_cif = true;

    enum Tag {
      _atom_site_group_PDB,
      _atom_site_id,
      _atom_site_type_symbol,
      _atom_site_label_atom_id,
      _atom_site_label_alt_id,
      _atom_site_label_comp_id,
      _atom_site_label_asym_id,
      _atom_site_label_entity_id,
      _atom_site_label_seq_id,
      _atom_site_pdbx_PDB_ins_code,
      _atom_site_Cartn_x,
      _atom_site_Cartn_y,
      _atom_site_Cartn_z,
      _atom_site_occupancy,
      _atom_site_B_iso_or_equiv,
      _atom_site_Cartn_x_esd,
      _atom_site_Cartn_y_esd,
      _atom_site_Cartn_z_esd,
      _atom_site_occupancy_esd,
      _atom_site_B_iso_or_equiv_esd,
      _atom_site_pdbx_formal_charge,
      _atom_site_auth_seq_id,
      _atom_site_auth_comp_id,
      _atom_site_auth_asym_id,
      _atom_site_auth_atom_id,
      _atom_site_pdbx_PDB_model_num,
      _unknown_tag,
    };

    const char *tagName(Tag tag) {
      static const char *names[] = {
        "_atom_site.group_PDB",
        "_atom_site.id",
        "_atom_site.type_symbol",
        "_atom_site.label_atom_id",
        "_atom_site.label_alt_id",
        "_atom_site.label_comp_id",
        "_atom_site.label_asym_id",
        "_atom_site.label_entity_id",
        "_atom_site.label_seq_id",
        "_atom_site.pdbx_PDB_ins_code",
        "_atom_site.Cartn_x",
        "_atom_site.Cartn_y",
        "_atom_site.Cartn_z",
        "_atom_site.occupancy",
        "_atom_site.B_iso_or_equiv",
        "_atom_site.Cartn_x_esd",
        "_atom_site.Cartn_y_esd",
        "_atom_site.Cartn_z_esd",
        "_atom_site.occupancy_esd",
        "_atom_site.B_iso_or_equiv_esd",
        "_atom_site.pdbx_formal_charge",
        "_atom_site.auth_seq_id",
        "_atom_site.auth_comp_id",
        "_atom_site.auth_asym_id",
        "_atom_site.auth_atom_id",
        "_atom_site.pdbx_PDB_model_num",
        "_unknown_tag",
      };
      return names[(int)tag];
    }

    std::vector<Tag> loop_tags_;
    size_t tag_idx_ = 0;

    enum State {
      state_dataitem,
      state_looptags,
      state_loopvalues,
    };
 
    State state_ = state_dataitem;

    void cif_loop() {
      state_ = state_looptags;
      tag_idx_ = 0;
      loop_tags_.clear();
    }

    void cif_endloop() {
      if (state_ == state_loopvalues) {
        state_ = state_dataitem;
      }
    }

    void cif_data(const uint8_t *b, const uint8_t *e) {
      std::cout << std::string(b, e) << "D\n";
    }

    void cif_value(const uint8_t *b, const uint8_t *e) {
      if (state_ == state_looptags) {
        state_ = state_loopvalues;
      }

      static int times = 0;

      if (state_ == state_loopvalues) {
        Tag tag = loop_tags_[tag_idx_];
        if (tag == _atom_site_group_PDB) {
          atoms_.push_back(atom{});
        }
        atom &a = atoms_.back();
        int len = int(e - b);
        switch (tag) {
          case _atom_site_group_PDB: a.is_hetatom_ = *b == 'H'; break;
          case _atom_site_id: a.serial_ = atoi(b, e); break;
          case _atom_site_type_symbol: read(a.element_, b, e); break;
          case _atom_site_label_atom_id: read(a.atomName_, b, e); break;
          case _atom_site_label_alt_id: a.altLoc_ = *b; break;
          case _atom_site_label_comp_id: read(a.resName_, b, e); break;
          case _atom_site_label_asym_id: a.chainID_ = *b; break;
          case _atom_site_label_entity_id: break;
          case _atom_site_label_seq_id: a.resSeq_ = atoi(b, e); break;
          case _atom_site_pdbx_PDB_ins_code: a.iCode_ = (char)*b; break;
          case _atom_site_Cartn_x: a.pos_.x = atof(b, e); break;
          case _atom_site_Cartn_y: a.pos_.y = atof(b, e); break;
          case _atom_site_Cartn_z: a.pos_.z = atof(b, e); break;
          case _atom_site_occupancy: a.occupancy_ = atof(b, e); break;
          case _atom_site_B_iso_or_equiv: break;
          case _atom_site_Cartn_x_esd: break;
          case _atom_site_Cartn_y_esd: break;
          case _atom_site_Cartn_z_esd: break;
          case _atom_site_occupancy_esd: break;
          case _atom_site_B_iso_or_equiv_esd: break;
          case _atom_site_pdbx_formal_charge: break;
          case _atom_site_auth_seq_id: break;
          case _atom_site_auth_comp_id: break;
          case _atom_site_auth_asym_id: break;
          case _atom_site_auth_atom_id: break;
          case _atom_site_pdbx_PDB_model_num: break;
          case _unknown_tag: break;
        }
        //std::cout << std::string(b, e) << " " << tag << " V\n";
        if (++tag_idx_ >= loop_tags_.size()) {
          tag_idx_ = 0;
        }
      }
    }

    void cif_save(const uint8_t *b, const uint8_t *e) {
    }

    void cif_stop(const uint8_t *b, const uint8_t *e) {
    }

    void cif_global() {
    }

    Tag getTag(const uint8_t *b, const uint8_t *e) {
      if (e - b < 11 || memcmp(b, "_atom_site.", 11)) return _unknown_tag;
      for (Tag tag = _atom_site_group_PDB; tag != _atom_site_pdbx_PDB_model_num; tag = Tag(int(tag)+1)) {
        const char *tn = tagName(tag);
        size_t len = strlen(tn);
        if (e - b == len && !memcmp(b, tn, len)) {
          return tag;
        }
      }
      return _unknown_tag;
    }

    void cif_tag(const uint8_t *b, const uint8_t *e) {
      uint64_t h = 0;
      Tag tag = getTag(b, e);
      loop_tags_.push_back(tag);
    }

    template<class Array> 
    static void read(Array &a, const uint8_t *b, const uint8_t *e) {
      auto d = a.begin();
      while (b != e && *b == ' ') ++b;
      while (b != e && d != a.end() && *b != ' ') *d++ = *b++;
      while (d != a.end()) *d++ = 0;
    }

    template <class Array>
    static bool compare(const Array &a, const char *str) {
      auto s = a.begin();
      while (*str && s != a.end()) if (*str++ != *s++) return false;
      if (*str) return false;
      return s == a.end() || *s == 0;
    }

    static int atoi(const uint8_t *b, const uint8_t *e) {
      while (b != e && *b == ' ') ++b;
      int s = b != e && *b == '-' ? -1 : 1;
      b += s == -1;
      int n = 0;
      while (b != e && *b >= '0' && *b <= '9') n = n * 10 + *b++ - '0';
      return s * n;
    }

    static float atof(const uint8_t *b, const uint8_t *e) {
      while (b != e && *b == ' ') ++b;
      float n = 0;
      float s = 1.0f;
      if (b != e && *b == '-') { s = -s; b++; }
      while (b != e && *b >= '0' && *b <= '9') n = n * 10 + *b++ - '0';
      if (b != e && *b == '.') {
        ++b;
        float frac = 0, p10 = 1;
        while (b != e && *b >= '0' && *b <= '9') p10 *= 10, frac = frac * 10 + *b++ - '0';
        n += frac / p10;
      }
      if (b != e && (*b == 'e'||*b == 'E')) {
        ++b;
        int es = 1;
        if (b != e && *b == '-') { es = -es; b++; }
        int exp = 0;
        while (b != e && *b >= '0' && *b <= '9') exp = exp * 10 + *b++ - '0';
        return s * n * std::pow(10.0f, exp * es);
      } else {
        return s * n;
      }
    }

    std::vector<atom> atoms_;
    std::vector<glm::mat4> instanceMatrices_;
    std::vector<std::pair<int, int> > connections_;
  };
}

#endif
