#include "config/ini_document.hpp"

#include <cctype>
#include <sstream>

namespace framecompare::config
{
namespace
{
std::string trim(std::string value)
{
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.front())) != 0)
        value.erase(value.begin());
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.back())) != 0)
        value.pop_back();
    return value;
}
}

IniDocument IniDocument::parse(std::string_view text)
{
    IniDocument result;
    std::string input(text);
    if (input.rfind("\xEF\xBB\xBF", 0) == 0)
        input.erase(0, 3);

    std::istringstream stream(input);
    std::string section;
    std::string line;
    while (std::getline(stream, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        line = trim(std::move(line));
        if (line.empty() || line.front() == ';' || line.front() == '#')
            continue;
        if (line.front() == '[' && line.back() == ']')
        {
            section = trim(line.substr(1, line.size() - 2));
            continue;
        }

        const std::size_t equals = line.find('=');
        if (equals == std::string::npos)
            continue;
        result.set(section, trim(line.substr(0, equals)),
                   trim(line.substr(equals + 1)));
    }
    return result;
}

std::optional<std::string> IniDocument::get(
    std::string_view section, std::string_view key) const
{
    const auto section_it = sections_.find(std::string(section));
    if (section_it == sections_.end())
        return std::nullopt;
    const auto value_it = section_it->second.find(std::string(key));
    if (value_it == section_it->second.end())
        return std::nullopt;
    return value_it->second;
}

void IniDocument::set(std::string section, std::string key,
                      std::string value)
{
    sections_[std::move(section)][std::move(key)] = std::move(value);
}

std::string IniDocument::serialize() const
{
    std::ostringstream output;
    output << "\xEF\xBB\xBF"
           << "; FrameCompare v2 portable configuration (UTF-8)\n";
    for (const auto &[section, values] : sections_)
    {
        if (!section.empty())
            output << '\n' << '[' << section << "]\n";
        for (const auto &[key, value] : values)
            output << key << '=' << value << '\n';
    }
    return output.str();
}
}
