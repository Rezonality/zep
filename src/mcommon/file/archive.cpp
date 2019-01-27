#include "mcommon/logger.h"

#include "mcommon/file/archive.h"

#include "mcommon/string/murmur_hash.h"
#include "mcommon/string/stringutils.h"

#include "mcommon/animation/timer.h"

#include <iomanip>
#include <cassert>
#include <algorithm>
#include <cassert>

namespace Zep
{

// Convert a number to a string, removing trailing 0s to make it as compact as possible
template <class Type>
std::string T(Type f)
{
    auto str = std::to_string(f);
    if (str.find_first_of('.') != std::string::npos)
    {
        RTrim(str, "0");
        if (str[str.size() - 1] == '.')
        {
            str = str.substr(0, str.length() - 1);
        }
    }
    return str;
}

const size_t* archive_find(Archive& ar, ArchiveSection* pSection, StringId key)
{
    auto itrKeys = &ar.keys[pSection->keyBase];
    while (itrKeys->first.id != 0)
    {
        if (itrKeys->first == key)
        {
            return &ar.values[itrKeys->second];
        }
        itrKeys++;
    }
    return nullptr;
}

#ifdef USE_GLM
std::string Archive_ToString(const glm::quat& v)
{
    return T(v.x) + ", " + T(v.y) + ", " + T(v.z) + ", " + T(v.w);
}

std::string Archive_ToString(const glm::vec2& v)
{
    return T(v.x) + ", " + T(v.y);
}
std::string Archive_ToString(const glm::ivec2& v)
{
    return T(v.x) + ", " + T(v.y);
}
std::string Archive_ToString(const glm::uvec2& v)
{
    return T(v.x) + ", " + T(v.y);
}

std::string Archive_ToString(const glm::vec3& v)
{
    return T(v.x) + ", " + T(v.y) + ", " + T(v.z);
}
std::string Archive_ToString(const glm::ivec3& v)
{
    return T(v.x) + ", " + T(v.y) + ", " + T(v.z);
}
std::string Archive_ToString(const glm::uvec3& v)
{
    return T(v.x) + ", " + T(v.y) + ", " + T(v.z);
}

std::string Archive_ToString(const glm::vec4& v)
{
    return T(v.x) + ", " + T(v.y) + ", " + T(v.z) + ", " + T(v.w);
}
std::string Archive_ToString(const glm::ivec4& v)
{
    return T(v.x) + ", " + T(v.y) + ", " + T(v.z) + ", " + T(v.w);
}
std::string Archive_ToString(const glm::uvec4& v)
{
    return T(v.x) + ", " + T(v.y) + ", " + T(v.z) + ", " + T(v.w);
}
#endif

void archive_comment(Archive& ar, const char* pszComment)
{
    if (pszComment)
    {
        ar.str << std::endl
               << "#" << std::string(pszComment) << std::endl;
    }
}

std::shared_ptr<ArchiveSection> archive_add_section(Archive& ar, ArchiveSection* pParentSection, const std::string& name)
{
    auto pNewSection = std::make_shared<ArchiveSection>();
    pNewSection->name = name;

    if (pParentSection)
    {
        pParentSection->children.push_back(pNewSection);
    }

    if (pParentSection && !pParentSection->fullName.ToString().empty())
    {
        pNewSection->fullName = pParentSection->fullName.ToString() + "/" + name;
    }
    else
    {
        pNewSection->fullName = name;
    }

    pNewSection->pParent = pParentSection;

    ar.sectionLookup[pNewSection->fullName] = pNewSection.get();

    return pNewSection;
}

ArchiveSection* archive_current_section(Archive& ar)
{
    if (ar.root == nullptr)
    {
        ar.root = archive_add_section(ar, nullptr, "");
        return ar.root.get();
    }

    if (ar.currentSection == nullptr)
    {
        ar.currentSection = ar.root.get();
    }

    return ar.currentSection;
}

void archive_sections(Archive& ar, ArchiveSection* section, std::function<void(ArchiveSection*)> fn)
{
    auto parent = ar.currentSection;
    for (auto& s : section->children)
    {
        ar.currentSection = s.get();
        fn(s.get());
    }
    ar.currentSection = parent;
}

std::vector<ArchiveSection*> Archive_FindSections(ArchiveSection* section, const char* pszName)
{
    std::vector<ArchiveSection*> sections;
    for (auto& s : section->children)
    {
        if (string_equals(s->name, pszName))
        {
            sections.push_back(s.get());
        }
    }
    return sections;
}

ArchiveSection* archive_find_section(ArchiveSection* section, const char* pszName)
{
    for (auto& s : section->children)
    {
        if (string_equals(s->name, pszName))
        {
            return s.get();
        }
    }
    return nullptr;
}

ArchiveSection* Archive_SetSection(Archive& ar, const std::string& strSection)
{
    // Ensure there is a root
    if (ar.root == nullptr)
    {
        ar.root = archive_add_section(ar, nullptr, "");
    }

    // Create a new section as a child
    auto addSection = [&](ArchiveSection* pSection, const std::string& name) {
        auto pFound = archive_find_section(pSection, name.c_str());
        if (pFound)
        {
            return pFound;
        }

        return archive_add_section(ar, pSection, name).get();
    };

    // Split the sections and add
    auto current = ar.root.get();
    std::vector<std::string> split;
    string_split(strSection, "/", split);
    for (auto& s : split)
    {
        current = addSection(current, s);
    }

    ar.currentSection = current;
    return current;
}

void Archive_AddPair(Archive& ar, StringId key, const std::string& pszValue)
{
    if (ar.sectionChanged && ar.saving)
    {
        ar.sectionChanged = false;
        ar.str << std::endl;
        ar.str << ":/" << archive_current_section(ar)->fullName.ToString();
        ar.str << std::endl;
    }
    ar.str << key;

    auto keySize = key.ToString().length();
    assert(keySize < 1024);

    for (size_t i = keySize; i < std::max(keySize, size_t(30)); i++)
    {
        ar.str << " ";
    }
    ar.str << pszValue;
    ar.str << std::endl;
}

bool archive_begin_section(Archive& ar, const std::string& name)
{
    auto pCurrent = archive_current_section(ar);
    if (ar.saving)
    {
        // Add a new section
        auto pNewSection = archive_add_section(ar, pCurrent, name);
        ar.currentSection = pNewSection.get();
        ar.currentSectionIndex++;
        ar.sectionChanged = true;
        return true;
    }
    else
    {
        // @Document; not sure what this is doing
        if (ar.currentSections.empty())
        {
            ar.currentSections = Archive_FindSections(archive_current_section(ar), name.c_str());
            ar.currentSectionIndex = 0;
        }

        if (ar.currentSectionIndex < ar.currentSections.size())
        {
            ar.currentSection = ar.currentSections[ar.currentSectionIndex];
            return true;
        }

        // Finished looping through the cases
        ar.currentSections.clear();
        ar.currentSectionIndex = 0;
        return false;
    }
}

void archive_end_section(Archive& ar)
{
    ar.currentSectionIndex++;
    if (ar.currentSection->pParent != nullptr)
    {
        ar.currentSection = ar.currentSection->pParent;
    }
    ar.sectionChanged = true;
}

void archive_value(Archive& ar, StringId key, std::string& val)
{
    if (ar.saving)
    {
        Archive_AddPair(ar, key, val);
    }
    else
    {
        auto pVals = archive_find(ar, archive_current_section(ar), key);
        if (pVals && *pVals++ != 0)
        {
            val = std::string(&ar.strText[*pVals]);
        }
    }
}

void archive_value(Archive& ar, StringId key, fs::path& val)
{
    if (ar.saving)
    {
        Archive_AddPair(ar, key, val.string());
    }
    else
    {
        auto pVals = archive_find(ar, archive_current_section(ar), key);
        if (pVals && *pVals++ != 0)
        {
            val = std::string(&ar.strText[*pVals]);
        }
    }
}
void archive_value(Archive& ar, StringId key, float& val)
{
    if (ar.saving)
    {
        Archive_AddPair(ar, key, T(val));
    }
    else
    {
        auto pVals = archive_find(ar, archive_current_section(ar), key);
        if (pVals && *pVals++ != 0)
        {
            val = std::stof(&ar.strText[*pVals]);
        }
    }
}

void archive_value(Archive& ar, StringId key, double& val)
{
    if (ar.saving)
    {
        Archive_AddPair(ar, key, T(val));
    }
    else
    {
        auto pVals = archive_find(ar, archive_current_section(ar), key);
        if (pVals && *pVals++ != 0)
        {
            val = std::stod(&ar.strText[*pVals]);
        }
    }
}

void archive_value(Archive& ar, StringId key, uint32_t& val)
{
    if (ar.saving)
    {
        Archive_AddPair(ar, key, T(val));
    }
    else
    {
        auto pVals = archive_find(ar, archive_current_section(ar), key);
        if (pVals && *pVals++ != 0)
        {
            val = std::stoul(&ar.strText[*pVals]);
        }
    }
}

void archive_value(Archive& ar, StringId key, uint64_t& val)
{
    if (ar.saving)
    {
        Archive_AddPair(ar, key, T(val));
    }
    else
    {
        auto pVals = archive_find(ar, archive_current_section(ar), key);
        if (pVals && *pVals++ == 1)
        {
            val = std::stoull(&ar.strText[*pVals]);
        }
    }
}

#ifdef USE_GLM
void archive_value(Archive& ar, StringId key, glm::uvec2& val)
{
    if (ar.saving)
    {
        Archive_AddPair(ar, key, Archive_ToString(val));
    }
    else
    {
        auto pVals = archive_find(ar, archive_current_section(ar), key);
        if (pVals)
        {
            for (int i = 0; i < *pVals; i++)
            {
                val[i] = std::stoul(&ar.strText[pVals[i + 1]]);
            }
        }
    }
}
void archive_value(Archive& ar, StringId key, glm::vec2& val)
{
    if (ar.saving)
    {
        Archive_AddPair(ar, key, Archive_ToString(val));
    }
    else
    {
        auto pVals = archive_find(ar, archive_current_section(ar), key);
        if (pVals)
        {
            for (int i = 0; i < *pVals; i++)
            {
                val[i] = std::stof(&ar.strText[pVals[i + 1]]);
            }
        }
    }
}
void archive_value(Archive& ar, StringId key, glm::ivec2& val)
{
    if (ar.saving)
    {
        Archive_AddPair(ar, key, Archive_ToString(val));
    }
    else
    {
        auto pVals = archive_find(ar, archive_current_section(ar), key);
        if (pVals)
        {
            for (int i = 0; i < *pVals; i++)
            {
                val[i] = std::stoi(&ar.strText[pVals[i + 1]]);
            }
        }
    }
}

void archive_value(Archive& ar, StringId key, glm::uvec3& val)
{
    if (ar.saving)
    {
        Archive_AddPair(ar, key, Archive_ToString(val));
    }
    else
    {
        auto pVals = archive_find(ar, archive_current_section(ar), key);
        if (pVals)
        {
            for (int i = 0; i < *pVals; i++)
            {
                val[i] = std::stoul(&ar.strText[pVals[i + 1]]);
            }
        }
    }
}
void archive_value(Archive& ar, StringId key, glm::ivec3& val)
{
    if (ar.saving)
    {
        Archive_AddPair(ar, key, Archive_ToString(val));
    }
    else
    {
        auto pVals = archive_find(ar, archive_current_section(ar), key);
        if (pVals)
        {
            for (int i = 0; i < *pVals; i++)
            {
                val[i] = std::stoi(&ar.strText[pVals[i + 1]]);
            }
        }
    }
}
void archive_value(Archive& ar, StringId key, glm::vec3& val)
{
    if (ar.saving)
    {
        Archive_AddPair(ar, key, Archive_ToString(val));
    }
    else
    {
        auto pVals = archive_find(ar, archive_current_section(ar), key);
        if (pVals)
        {
            for (int i = 0; i < *pVals; i++)
            {
                val[i] = std::stof(&ar.strText[pVals[i + 1]]);
            }
        }
    }
}

void archive_value(Archive& ar, StringId key, glm::vec4& val)
{
    if (ar.saving)
    {
        Archive_AddPair(ar, key, Archive_ToString(val));
    }
    else
    {
        auto pVals = archive_find(ar, archive_current_section(ar), key);
        if (pVals)
        {
            for (int i = 0; i < *pVals; i++)
            {
                val[i] = std::stof(&ar.strText[pVals[i + 1]]);
            }
        }
    }
}
void archive_value(Archive& ar, StringId key, glm::quat& val)
{
    if (ar.saving)
    {
        Archive_AddPair(ar, key, Archive_ToString(val));
    }
    else
    {
        auto pVals = archive_find(ar, archive_current_section(ar), key);
        if (pVals)
        {
            for (int i = 0; i < *pVals; i++)
            {
                val[i] = std::stof(&ar.strText[pVals[i + 1]]);
            }
        }
    }
}
#endif
void archive_value(Archive& ar, StringId key, bool& val)
{
    if (ar.saving)
    {
        Archive_AddPair(ar, key, val ? "1" : "0");
    }
    else
    {
        auto pVals = archive_find(ar, archive_current_section(ar), key);
        if (pVals && *pVals++ != 0)
        {
            val = (ar.strText[*pVals] == '1' ? true : false);
        }
    }
}

// Parse into a lookup table.
// @Document how this works
// Section->
//     Keys (offset)
//         Count
//             Key
// Values (Big Array; offsets)
void archive_parse_values(Archive& ar)
{
    TIME_SCOPE(archive_parse_values);
    ar.saving = false;

    // Parse out the key and values
    auto textSize = ar.strText.size();

    // Find the end of the first line
    size_t startLine = 0;
    size_t endLine = string_first_of(ar.strText.c_str(), 0, textSize, "\n\r");

    ArchiveSection* pSection = nullptr;
    while (endLine < ar.strText.size())
    {
        size_t index = 0;
        bool first = true;

        // Find things that aren't space or comma
        string_split_each(&ar.strText[0], startLine, endLine, " ,", [&](size_t start, size_t end) {
            if (ar.strText[start] == '#')
            {
                // Ignore
                // break out
                return false;
            }
            else if (ar.strText[start] == ':')
            {
                // Record section and quit parsing this line
                assert(end - start > 1);
                ar.keys.push_back(std::make_pair(0, 0));
                pSection = Archive_SetSection(ar, std::string(&ar.strText[start + 1], end - start - 1));
                pSection->keyBase = ar.keys.size();
                return false;
            }
            else
            {
                // Pollute the text with a 0, so we don't have to copy strings!
                ar.strText[end] = 0;
                if (first)
                {
                    // Store a parameter count in a big central value-index array
                    ar.values.push_back(0);
                    index = ar.values.size() - 1;

                    // TODO: This is a performance issue for debug loading
                    // Store the index of the parameters (count and (count)offsets)
                    StringId key(ar.strText.substr(start, int(end - start)));
                    ar.keys.push_back(std::make_pair(key, index));
                    first = false;
                }
                else
                {
                    // Add offset to the heap
                    ar.values.push_back(start);
                    ar.values[index]++;
                }
            }
            // Keep going until tokens found
            return true;
        });

        // Skip line end
        while ((endLine < textSize) && (ar.strText[endLine] == '\n' || ar.strText[endLine] == '\r' || ar.strText[endLine] == 0))
        {
            endLine++;
        }
        if (endLine >= textSize)
            break;

        // Find end of next line
        startLine = endLine;
        endLine = string_first_of(ar.strText.c_str(), startLine, textSize, "\n\r");
    }
    ar.keys.push_back(std::make_pair(0, 0));
    ar.currentSection = ar.root.get();
}

std::string archive_to_file_text(Archive& ar)
{
    std::ostringstream str;
    std::function<void(ArchiveSection*)> fnDump;
    fnDump = [&](ArchiveSection* section) {
        if (section == nullptr)
            return;

        // Draw section if there are any keys
        if (ar.keys[section->keyBase].first.id != 0)
        {
            str << ":/" + section->fullName.ToString() << std::endl;

            auto itrKeys = &ar.keys[section->keyBase];
            while (itrKeys->first.id != 0)
            {
                str << itrKeys->first;

                auto pVals = &ar.values[itrKeys->second];
                auto numVals = *pVals++;
                for (int i = 0; i < numVals; i++)
                {
                    if (i == 0)
                    {
                        for (size_t j = itrKeys->first.ToString().size(); j < 30; j++)
                        {
                            str << " ";
                        }
                    }
                    else
                        str << ", ";

                    str << (const char*)&ar.strText[*pVals++];
                }
                str << std::endl;
                itrKeys++;
            }
            str << std::endl;
        }

        for (auto& child : section->children)
        {
            fnDump(child.get());
        }
    };

    fnDump(ar.root.get());

    /*
    str << "Sections: " << std::endl;
    for (auto s : ar.sectionLookup)
    {
        str << s.first.ToString() << std::endl;
    }
    */

    return str.str();
}

std::shared_ptr<Archive> archive_load(const fs::path& path)
{
    TIME_SCOPE(archive_load);
    auto spArchive = std::make_shared<Archive>();
    spArchive->strText = file_read(path);
    spArchive->path = path;
    archive_parse_values(*spArchive);
    return spArchive;
}

void archive_update_binding(Archive& ar, Binding& binding)
{
    auto itr = ar.sectionLookup.find(binding.section);
    if (itr == ar.sectionLookup.end())
    {
        LOG(INFO) << "Binding section not found: " << binding.section.ToString();
        return;
    }
    ar.currentSection = itr->second;
    switch (binding.type)
    {
        default:
            LOG(INFO) << "No binding type";
            break;
        case BindingType::Float:
            archive_value(ar, binding.name, *(float*)binding.pData);
            break;
        case BindingType::Bool:
            archive_value(ar, binding.name, *(bool*)binding.pData);
            break;
        case BindingType::UInt32:
            archive_value(ar, binding.name, *(uint32_t*)binding.pData);
            break;
    }
}

void archive_bind(Archive& ar, StringId section, StringId key, float& val)
{
    ar.bindings.push_back(Binding{section, key, BindingType::Float, &val});
    archive_update_binding(ar, ar.bindings[ar.bindings.size() - 1]);
}

void archive_bind(Archive& ar, StringId section, StringId key, bool& val)
{
    ar.bindings.push_back(Binding{section, key, BindingType::Bool, &val});
    archive_update_binding(ar, ar.bindings[ar.bindings.size() - 1]);
}

void archive_bind(Archive& ar, StringId section, StringId key, uint32_t& val)
{
    ar.bindings.push_back(Binding{section, key, BindingType::UInt32, &val});
    archive_update_binding(ar, ar.bindings[ar.bindings.size() - 1]);
}

void archive_reload(Archive& ar)
{
    ar.currentSections.clear();
    ar.currentSection = nullptr;
    ar.values.clear();
    ar.keys.clear();
    ar.root.reset();
    ar.entityRemap.clear();
    ar.sectionLookup.clear();

    // Read the text again and parse it
    ar.strText = file_read(ar.path);
    archive_parse_values(ar);

    // Update the bindings
    for (auto& bind : ar.bindings)
    {
        archive_update_binding(ar, bind);
    }
}

} // namespace Zep
