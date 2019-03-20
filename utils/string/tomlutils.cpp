#include "tomlutils.h"
#include "utils/string/stringutils.h"

namespace Mgfx
{

void sanitize_for_toml(std::string& text)
{
    // Ensure a trailing '\n' for the text; since the cpptoml compiler is a bit picky
    text = string_right_trim(text, "\0");
    text += "\n";
}

int extract_toml_error_line(std::string err)
{
    auto errs = string_split(err, " ");
    auto itr = errs.begin();

    bool next = false;
    while (itr != errs.end())
    {
        if (*itr == "line")
        {
            next = true;
        }
        else if (next)
        {
            try
            {
                // Get line, indexed from 0
                int line = std::stoi(*itr);
                line--;
                return line;
            }
            catch (...)
            {
                return -1;
            }
        }
        itr++;
    }
    return -1;
}
} // Mgfx
