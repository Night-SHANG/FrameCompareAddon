#include "ui/i18n/localization.hpp"

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
    using namespace framecompare::ui::i18n;

    require(language() == UiLanguage::zh_cn,
            "the default interface language should be Simplified Chinese");
    require(parse_language("zh-CN") == UiLanguage::zh_cn,
            "zh-CN should parse as Simplified Chinese");
    require(parse_language("en") == UiLanguage::en,
            "en should parse as English");
    require(parse_language("broken") == UiLanguage::zh_cn,
            "unknown language values should safely fall back to Chinese");
    require(std::string_view(language_code(UiLanguage::zh_cn)) == "zh-CN",
            "Chinese should serialize with its stable code");
    require(std::string_view(language_code(UiLanguage::en)) == "en",
            "English should serialize with its stable code");

    for (std::size_t index = 0;
         index < static_cast<std::size_t>(TextId::count); ++index)
    {
        const auto id = static_cast<TextId>(index);
        require(text(id, UiLanguage::zh_cn)[0] != '\0',
                "every Chinese translation must be present");
        require(text(id, UiLanguage::en)[0] != '\0',
                "every English translation must be present");
    }

    set_language(UiLanguage::en);
    require(std::string_view(text(TextId::tab_compare)) == "Compare",
            "runtime language changes should select the English table");
    set_language(UiLanguage::zh_cn);
    require(label(TextId::tab_compare, "tab-compare") ==
                "对比##tab-compare",
            "localized labels should retain stable ImGui identifiers");
    require(std::string_view(text(TextId::dlss5_before)) ==
                "DLSS5 处理前画面作为 Before",
            "the DLSS5 checkbox needs a precise Chinese label");
    require(std::string_view(text(TextId::dlss5_before,
                                  UiLanguage::en)) ==
                "Use pre-DLSS5 image for Before",
            "the DLSS5 checkbox needs an English translation");

    std::cout << "localization_tests: PASS\n";
}
