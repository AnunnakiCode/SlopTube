// Settings: playback in the background and in picture-in-picture, autoplay, upload
// notifications and the search history.

#include "app/App.hpp"
#include "app/DataFiles.hpp"
#include "app/ui/Strings.hpp"
#include "app/ui/Theme.hpp"
#include "core/BuildInfo.hpp"

#include <filesystem>

namespace sloptube::app {
namespace {

// Groups that moved out of the main list have a page of their own: the screen's argument names
// it, and an empty argument is the main list.
constexpr std::string_view kAppearancePage = "appearance";
constexpr std::string_view kLanguagePage = "language";

} // namespace

void App::openSettings() {
    push(Screen{.route = Route::Settings, .title = std::string(tr(Text::Settings))});
}

void App::applySettings() {
    m_settings.save(m_platform.dataDirectory() / kSettingsFile);
    // The job is (re)scheduled every launch: the system drops it when the app is updated.
    m_platform.setUploadNotifications(m_settings.uploadNotifications);
}

void App::drawSettings(Frame& frame, Screen& screen, ImVec2 min, ImVec2 max) {
    const Canvas& c = frame.canvas;
    const Canvas medium = c.with(Weight::Medium);
    const float width = max.x - min.x;
    const float bar = c.dp(theme::kTopBarHeight);
    const float row = c.dp(kSettingRowHeight);
    const ImVec2 contentMin{min.x, min.y + bar};
    const bool appearancePage = screen.argument == kAppearancePage;
    const bool languagePage = screen.argument == kLanguagePage;

    struct Toggle {
        Text label;
        Text detail;
        bool Settings::* value;
    };

    // The first three belong to the player, the last one to notifications.
    constexpr std::array<Toggle, 4> kToggles{{
        {Text::BackgroundPlay, Text::BackgroundPlayDetail, &Settings::backgroundPlay},
        {Text::PictureInPicture, Text::PictureInPictureDetail, &Settings::pictureInPicture},
        {Text::Autoplay, Text::AutoplayDetail, &Settings::autoplay},
        {Text::UploadNotifications, Text::UploadNotificationsDetail,
         &Settings::uploadNotifications},
    }};
    constexpr std::array<Text, kInterfaceSizes> kSizeLabels{Text::SizeNormal, Text::SizeLarge,
                                                            Text::SizeExtraLarge};
    constexpr std::array<Text, kLanguageChoices> kLanguageLabels{
        Text::LanguagePhone, Text::LanguageEnglish, Text::LanguageFrench, Text::LanguageSpanish};
    const float gutter = c.dp(theme::kGutter);
    const float captionHeight = c.lineHeight(theme::kCaptionSize);
    // A section is a gap, its title and a little room under it.
    const float sectionHeight = gutter * 1.5f + captionHeight;
    const float sections = build::kChecksForUpdates ? 4.0f : 3.0f;
    const float chipBlock = row + c.dp(theme::kChipHeight) + gutter;
    const float updateRows = build::kChecksForUpdates ? 2 : 0;
    // Two rows open the Appearance and Languages pages.
    const float mainContent = row * 2 + sectionHeight * sections +
                              row * (static_cast<float>(kToggles.size() + 1) + updateRows) +
                              gutter * 3 + captionHeight;
    const float content = appearancePage || languagePage ? chipBlock + gutter * 2 : mainContent;
    screen.feed->scroller.update(frame.touch, contentMin, max, content, frame.seconds, c.dp(1));
    frame.animating |= screen.feed->scroller.moving();

    c.list()->PushClipRect(contentMin, max, true);
    float y = contentMin.y - screen.feed->scroller.offset();
    float x = min.x + gutter;

    const auto section = [&](Text title) {
        y += gutter;
        medium.text({min.x + gutter, y}, theme::kCaptionSize, theme::kTextSecondary, tr(title));
        y += captionHeight + gutter * 0.5f;
    };
    const auto toggleRow = [&](const Toggle& toggle) {
        bool& value = m_settings.*toggle.value;
        if (switchRow(frame, tr(toggle.label), tr(toggle.detail), {min.x, y}, width, value) &&
            !screen.feed->scroller.moving()) {
            value = !value;
            later([this] { applySettings(); });
        }
        y += row;
    };
    // A row with just a large title that opens one of the pages of Settings.
    const auto pageRow = [&](Text label, std::string_view page) {
        const ImVec2 rowMin{min.x, y};
        const ImVec2 rowMax{max.x, y + row};
        drawPressed(frame, rowMin, rowMax);
        medium.text({min.x + gutter, y + (row - c.lineHeight(theme::kTitleSize)) * 0.5f},
                    theme::kTitleSize, theme::kTextPrimary, tr(label));
        if (!screen.feed->scroller.moving() && frame.touch.tapped(rowMin, rowMax)) {
            later([this, label, page] {
                push(Screen{.route = Route::Settings,
                            .argument = std::string(page),
                            .title = std::string(tr(label))});
            });
        }
        y += row;
    };

    if (appearancePage) {
        // Text and button size: the setting that decides whether the rest can be read.
        (void)switchRow(frame, tr(Text::InterfaceSize), tr(Text::InterfaceSizeDetail), {min.x, y},
                        width, std::nullopt);
        y += row;
        for (int size = 0; size < kInterfaceSizes; ++size) {
            const bool selected = m_settings.interfaceSize == size;
            if (chipButton(frame, tr(kSizeLabels[static_cast<std::size_t>(size)]), {x, y},
                           selected, x) &&
                !selected && !screen.feed->scroller.moving()) {
                m_settings.interfaceSize = size;
                later([this] {
                    applySettings();
                    applyScale();
                });
            }
        }
        y += c.dp(theme::kChipHeight) + gutter;
    } else if (languagePage) {
        // Language of the interface: follow the phone, or choose one.
        (void)switchRow(frame, tr(Text::LanguageSetting), tr(Text::LanguageSettingDetail),
                        {min.x, y}, width, std::nullopt);
        y += row;
        for (int choice = 0; choice < kLanguageChoices; ++choice) {
            const bool selected = m_settings.language == choice;
            if (chipButton(frame, tr(kLanguageLabels[static_cast<std::size_t>(choice)]), {x, y},
                           selected, x) &&
                !selected && !screen.feed->scroller.moving()) {
                m_settings.language = choice;
                later([this] {
                    applySettings();
                    setLanguage(
                        languageForChoice(m_settings.language, m_platform.locale().language));
                });
            }
        }
        y += c.dp(theme::kChipHeight) + gutter;
    } else {
        pageRow(Text::SectionAppearance, kAppearancePage);
        pageRow(Text::SectionLanguages, kLanguagePage);

        section(Text::SectionPlayer);
        for (std::size_t i = 0; i < 3; ++i) {
            toggleRow(kToggles[i]);
        }

        section(Text::SectionNotifications);
        toggleRow(kToggles[3]);

        // Updates of the app, for builds that can install them (release builds).
        if (build::kChecksForUpdates) {
            section(Text::SectionUpdates);
            if (switchRow(frame, tr(Text::CheckUpdatesAutomatically), tr(Text::CheckUpdatesDetail),
                          {min.x, y}, width, m_settings.checkUpdates) &&
                !screen.feed->scroller.moving()) {
                m_settings.checkUpdates = !m_settings.checkUpdates;
                later([this] { applySettings(); });
            }
            y += row;
            const std::string current =
                std::vformat(tr(Text::CurrentVersion), std::make_format_args(build::kVersion));
            if (switchRow(frame,
                          tr(m_update.checking ? Text::CheckingForUpdates : Text::CheckForUpdates),
                          current, {min.x, y}, width, std::nullopt) &&
                !screen.feed->scroller.moving()) {
                later([this] { checkForUpdate(true); });
            }
            y += row;
        }

        // The search history, kept on the phone only.
        section(Text::SectionPrivacy);
        const bool empty = m_searchHistory.empty();
        if (switchRow(frame, tr(Text::ClearSearchHistory),
                      tr(empty ? Text::SearchHistoryEmpty : Text::ClearSearchHistoryDetail),
                      {min.x, y}, width, std::nullopt) &&
            !empty) {
            m_searchHistory.clear();
            std::error_code error;
            std::filesystem::remove(m_platform.dataDirectory() / kSearchHistoryFile, error);
            showToast(tr(Text::SearchHistoryCleared));
        }
        y += row + gutter * 2;

        const std::string version =
            std::string(build::kAppName) + ' ' + std::string(build::kVersion);
        c.line({min.x + gutter, y}, width - gutter * 2, theme::kCaptionSize, theme::kTextSecondary,
               version, TextAlign::Center);
    }
    c.list()->PopClipRect();

    // The title is drawn from the language in use, so changing it here updates it at once.
    Text titleText = Text::Settings;
    if (appearancePage) {
        titleText = Text::SectionAppearance;
    } else if (languagePage) {
        titleText = Text::SectionLanguages;
    }
    drawTitleBar(frame, std::string(tr(titleText)), {min.x, 0}, width);
}

} // namespace sloptube::app
