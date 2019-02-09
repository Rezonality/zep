#pragma once

#include <sstream>
#include <unordered_map>
#include <functional>
#include <memory>

#include "mcommon/string/stringutils.h"
#include "mcommon/file/path.h"

namespace Zep
{

enum class BindingType
{
    Float,
    Bool,
    UInt32
};

struct Binding
{
    StringId section;
    StringId name;
    BindingType type;
    void* pData;
};

struct ArchiveSection
{
    ArchiveSection* pParent = nullptr;
    StringId name;                                         // Subsection name    eg. materialcomponent.
    StringId fullName;                                     // Full section name: eg. foo/bar/me
    size_t keyBase = 0;                                    // Base of the keys in the value array
    std::vector<std::shared_ptr<ArchiveSection>> children; // Child sections
};

// See archive_dump for an explanation of the loaded data structure.
// It is essentially 2 lookup tables for keys and values, and tree for archive entries.
// The tables index into the loaded string, which is poked full of 0's to deliniate the data
struct Archive
{
    bool saving = false;
    uint64_t currentSectionIndex;

    // Path of this file
    ZepPath path;

    // Saving
    std::ostringstream str;
    bool sectionChanged = false;

    // Loading
    std::vector<ArchiveSection*> currentSections;
    std::string strText;
    ArchiveSection* currentSection = nullptr;
    std::vector<size_t> values;
    std::vector<std::pair<StringId, size_t>> keys; // string hash/value index
    std::shared_ptr<ArchiveSection> root;
    std::unordered_map<uint64_t, uint64_t> entityRemap;

    std::unordered_map<StringId, ArchiveSection*> sectionLookup;

    std::vector<Binding> bindings;
};

void archive_comment(Archive& ar, const char* pszComment);

bool archive_begin_section(Archive& ar, const std::string& name);
void archive_end_section(Archive& ar);

void archive_value(Archive& ar, StringId key, std::string& val);
void archive_value(Archive& ar, StringId key, ZepPath& val);
void archive_value(Archive& ar, StringId key, float& val);
void archive_value(Archive& ar, StringId key, double& val);
void archive_value(Archive& ar, StringId key, uint32_t& val);
void archive_value(Archive& ar, StringId key, uint64_t& val);
void archive_value(Archive& ar, StringId key, bool& val);
#ifdef USE_GLM
void archive_value(Archive& ar, StringId key, glm::uvec2& val);
void archive_value(Archive& ar, StringId key, glm::vec2& val);
void archive_value(Archive& ar, StringId key, glm::ivec2& val);
void archive_value(Archive& ar, StringId key, glm::uvec3& val);
void archive_value(Archive& ar, StringId key, glm::vec3& val);
void archive_value(Archive& ar, StringId key, glm::vec4& val);
void archive_value(Archive& ar, StringId key, glm::ivec3& val);
void archive_value(Archive& ar, StringId key, glm::quat& val);
#endif

void archive_parse_values(Archive& ar);
ArchiveSection* archive_current_section(Archive& ar);
void archive_sections(Archive& ar, ArchiveSection* pSection, std::function<void(ArchiveSection*)> fn);
ArchiveSection* archive_find_section(ArchiveSection* pSection, const char* name);
std::string archive_to_file_text(Archive& ar);

std::shared_ptr<Archive> archive_load(const ZepPath& path, const std::string& text);
void archive_reload(Archive& ar, const std::string& text);

void archive_bind(Archive& archive, StringId section, StringId key, float& val);
void archive_bind(Archive& archive, StringId section, StringId key, bool& val);
void archive_bind(Archive& archive, StringId section, StringId key, uint32_t& val);

} // namespace Zep
