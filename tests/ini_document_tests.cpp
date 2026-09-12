#include "config/ini_document.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace
{
void require(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}
}

int main()
{
    const auto document = framecompare::config::IniDocument::parse(
        "\xEF\xBB\xBF; comment\n[General]\nEnabled=1\n"
        "Name=Frame=Compare\n\n[HUD.0]\nTextOn=HDR ON\n");

    require(document.get("General", "Enabled").value_or("") == "1",
            "parser should read a section value after a UTF-8 BOM");
    require(document.get("General", "Name").value_or("") ==
                "Frame=Compare",
            "parser should split only the first equals sign");
    require(document.get("HUD.0", "TextOn").value_or("") == "HDR ON",
            "parser should read indexed HUD sections");

    auto edited = document;
    edited.set("Divider", "Position", "0.5");
    edited.set("UI", "Language", "zh-CN");
    const std::string serialized = edited.serialize();
    require(serialized.rfind("\xEF\xBB\xBF", 0) == 0,
            "serialized INI should use a UTF-8 BOM");

    const auto round_trip =
        framecompare::config::IniDocument::parse(serialized);
    require(round_trip.get("Divider", "Position").value_or("") == "0.5",
            "serialized values should survive a parse round trip");
    require(round_trip.get("UI", "Language").value_or("") == "zh-CN",
            "interface language should survive an INI round trip");
    std::cout << "ini_document_tests: PASS\n";
}
