////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Zipfile reader class
//


#include <vector>
#include <stdexcept>
#include <cstring>
#include <andyzip/deflate_decoder.hpp>

// Simple zipfile reader. Allows extraction of files in a mapped zipfile.
class zipfile_reader {
public:
  zipfile_reader(const uint8_t *begin, const uint8_t *end) : begin_(begin), end_(end) {
    // https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT
    // end of central dir signature    4 bytes  (0x06054b50)
    // number of this disk             2 bytes
    // number of the disk with the
    // start of the central directory  2 bytes
    // total number of entries in the
    // central directory on this disk  2 bytes
    // total number of entries in
    // the central directory           2 bytes
    // size of the central directory   4 bytes
    // offset of start of central
    // directory with respect to
    // the starting disk number        4 bytes
    // .ZIP file comment length        2 bytes
    // .ZIP file comment       (variable size)

    const uint8_t *p = end_ - 22;
    central_dir_begin_ = nullptr;
    central_dir_end_ = nullptr;
    for (; p >= begin_; --p) {
      if (*p == 'P' && u4(p) == 0x06054b50) break;
    }
    if (p < begin_ || p - u4(p + 12) < begin_) {
      throw std::runtime_error("cannot find central directory");
    }
    central_dir_begin_ = p - u4(p + 12);
    central_dir_end_ = p;
  }

  // Get a list of filenames.
  std::vector<std::string> filenames() const {
    // central file header signature   4 bytes  (0x02014b50)
    // version made by                 2 bytes (+4)
    // version needed to extract       2 bytes (+6)
    // general purpose bit flag        2 bytes (+8)
    // compression method              2 bytes (+10)
    // last mod file time              2 bytes (+12)
    // last mod file date              2 bytes (+14)
    // crc-32                          4 bytes (+16)
    // compressed size                 4 bytes (+20)
    // uncompressed size               4 bytes (+24)
    // file name length                2 bytes (+28)
    // extra field length              2 bytes (+30)
    // file comment length             2 bytes (+32)
    // disk number start               2 bytes (+34)
    // internal file attributes        2 bytes (+36)
    // external file attributes        4 bytes (+38)
    // relative offset of local header 4 bytes (+42)
    //                                         (+46)

    // file name (variable size)
    // extra field (variable size)
    // file comment (variable size)

    std::vector<std::string> names;
    for (const uint8_t *p = central_dir_begin_; p < central_dir_end_; ++p) {
      if (u4(p) != 0x02014b50) {
        throw std::runtime_error("bad directory entry");
      }
      uint16_t filename_len = u2(p + 28);
      uint16_t extra_len = u2(p + 30);
      uint16_t comment_len = u2(p + 32);
      names.emplace_back((const char*)p + 46, (const char*)p + 46 + filename_len);
      p += 46 + filename_len + extra_len + comment_len;
    }
    return names;
  }

  // Get a list of directory entries.
  // todo: make a class for a directory entry that wraps the pointer.
  std::vector<const uint8_t *> dir_entries() const {
    // central file header signature   4 bytes  (0x02014b50)
    // version made by                 2 bytes (+4)
    // version needed to extract       2 bytes (+6)
    // general purpose bit flag        2 bytes (+8)
    // compression method              2 bytes (+10)
    // last mod file time              2 bytes (+12)
    // last mod file date              2 bytes (+14)
    // crc-32                          4 bytes (+16)
    // compressed size                 4 bytes (+20)
    // uncompressed size               4 bytes (+24)
    // file name length                2 bytes (+28)
    // extra field length              2 bytes (+30)
    // file comment length             2 bytes (+32)
    // disk number start               2 bytes (+34)
    // internal file attributes        2 bytes (+36)
    // external file attributes        4 bytes (+38)
    // relative offset of local header 4 bytes (+42)
    //                                         (+46)

    // file name (variable size)
    // extra field (variable size)
    // file comment (variable size)

    std::vector<const uint8_t *> result;
    for (const uint8_t *p = central_dir_begin_; p < central_dir_end_; ++p) {
      if (u4(p) != 0x02014b50) {
        throw std::runtime_error("bad directory entry");
      }
      uint16_t filename_len = u2(p + 28);
      uint16_t extra_len = u2(p + 30);
      uint16_t comment_len = u2(p + 32);
      result.emplace_back(begin_ + u4(p + 42));
      p += 46 + filename_len + extra_len + comment_len;
    }
    return result;
  }

  // Read a file by filename.
  std::vector<uint8_t> read(const std::string &filename) const {
    const uint8_t *p = get_dir_entry(filename);
    if (!p || u4(p + 0) != 0x04034b50) {
      throw std::runtime_error("file not found");
    }

    return read_entry(p);
  }

  // Read a file by directory entry.
  std::vector<uint8_t> read_entry(const uint8_t *p) const {
    // https://en.wikipedia.org/wiki/Zip_(file_format)
    //  0 4 Local file header signature = 0x04034b50 (read as a little-endian number)
    uint32_t sig = u4(p + 0);
    //  4 2 Version needed to extract (minimum)
    uint16_t version = u2(p + 4);
    //  6 2 General purpose bit flag
    uint16_t flags = u2(p + 6);
    //  8 2 Compression method
    uint16_t method = u2(p + 8);
    // 10 2 File last modification time
    uint16_t time = u2(p + 10);
    // 12 2 File last modification date
    uint16_t date = u2(p + 12);
    // 14 4 CRC-32
    uint32_t crc = u4(p + 14);
    // 18 4 Compressed size
    uint32_t csize = u4(p + 18);
    // 22 4 Uncompressed size
    uint32_t usize = u4(p + 22);
    // 26 2 File name length (n)
    uint16_t namelen = u2(p + 26);
    // 28 2 Extra field length (m)
    uint16_t extlen = u2(p + 28);

    const uint8_t *b = p + 30 + namelen + extlen;
    const uint8_t *e = b + csize;

    std::vector<uint8_t> result(usize);
    if (method == 8) {
      if (!dec_.decode(result.data(), result.data() + result.size(), b, e)) {
        result.resize(0);
        throw std::runtime_error("deflate decode failure");
      }
    } else if (method == 0) {
      memcpy(result.data(), b, usize);
    } else {
      result.resize(0);
      throw std::runtime_error("unsupported compression method");
    }
    return result;
  }

  // Convert a filename to a directory entry.
  const uint8_t *get_dir_entry(const std::string &filename) const {
    uint8_t c0 = filename[0];
    uint16_t len = (uint16_t)filename.size();
    for (const uint8_t *p = central_dir_begin_; p < central_dir_end_; ++p) {
      if (u2(p + 28) == len) {
        if (!memcmp(filename.data(), p + 46, u2(p + 28))) {
          return begin_ + u4(p + 42);
        }
      }
      uint16_t filename_len = u2(p + 28);
      uint16_t extra_len = u2(p + 30);
      uint16_t comment_len = u2(p + 32);
      p += 46 + filename_len + extra_len + comment_len;
    }
    return nullptr;
  }
private:
  static inline unsigned u4(const uint8_t *p) {
    return (p[3] << 24) | (p[2] << 16) | (p[1] << 8) | (p[0] << 0);
  }

  static inline unsigned u2(const uint8_t *p) {
    return (p[1] << 8) | (p[0] << 0);
  }

  const uint8_t *begin_;
  const uint8_t *end_;
  const uint8_t *central_dir_begin_;
  const uint8_t *central_dir_end_;
  andyzip::deflate_decoder dec_;
};
