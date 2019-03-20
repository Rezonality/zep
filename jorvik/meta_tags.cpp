#include "meta_tags.h"
#include "utils/container_utils.h"
#include "utils/string/stringutils.h"
#include "utils/logger.h"

#include <vector>
namespace Mgfx
{

std::shared_ptr<MetaTags> parse_meta_tags(const std::string& text)
{
    //LOG(DEBUG) << "parse_meta_tags";
    auto spTags = std::make_shared<MetaTags>();

    try
    {
        std::vector<std::string> lines;
        string_split_lines(text, lines);

        int32_t lineNumber = 0;
        for (auto& line : lines)
        {
            auto pos = line.find("//", 0);
            if (pos != std::string::npos)
            {
                std::vector<std::string> tokens;
                string_split(line.substr(pos + 2), " \t,", tokens);

                // Always lower case
                for (int tok = 0; tok < tokens.size(); tok++)
                {
                    tokens[tok] = string_tolower(tokens[tok]);
                }

                if (tokens.size() % 2 == 0)
                {
                    auto keyValuePairs = vector_convert_to_pairs(tokens);
                    for (auto& keyValue : keyValuePairs)
                    {
                        //LOG(DEBUG) << "Tag - " << keyValue.first << " = " << keyValue.second;
                        if (keyValue.first == "#shader_type")
                        {
                            spTags->shader_type.value = keyValue.second;
                            spTags->shader_type.line = lineNumber;
                        }
                        else if (keyValue.first == "#entry_point")
                        {
                            spTags->entry_point.value = keyValue.second;
                            spTags->entry_point.line = lineNumber;
                        }
                    }
                }
            }
            lineNumber++;
        }
    }
    catch (std::exception & ex)
    {
        LOG(DEBUG) << ex.what();
    }
    return spTags;
}

}; // namespace Mgfx
