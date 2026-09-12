#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace framecompare::config
{
class IniDocument
{
public:
    static IniDocument parse(std::string_view text);

    std::optional<std::string> get(std::string_view section,
                                   std::string_view key) const;
    void set(std::string section, std::string key, std::string value);
    std::string serialize() const;

private:
    using Values = std::map<std::string, std::string>;
    std::map<std::string, Values> sections_;
};
}
