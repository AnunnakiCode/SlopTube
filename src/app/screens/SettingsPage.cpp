// Settings: a list of categories, each opening a page of its own: appearance, languages,
// playback, notifications, updates and the search history.

#include "app/App.hpp"
#include "app/DataFiles.hpp"
#include "app/ui/Strings.hpp"
#include "app/ui/Theme.hpp"
#include "core/BuildInfo.hpp"

#include <filesystem>

namespace sloptube::app {
namespace {

// Every category has a page of its own: the screen's argument names it, and an empty argument
// is the main list.
constexpr std::string_view kAppearancePage = "appearance";
constexpr std::string_view kLanguagePage = "language";
constexpr std::string_view kPlayerPage = "player";
constexpr std::string_view kNotificationsPage = "notifications";
constexpr std::string_view kUpdatesPage = "updates";
constexpr std::string_view kPrivacyPage = "privacy";

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
    const std::string_view page = screen.argument;

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
    const float chipBlock = row + c.dp(theme::kChipHeight) + gutter;
    // The main list has one row for each page; updates only exist in release builds.
    const float pageRows = build::kChecksForUpdates ? 6.0f : 5.0f;
    float content = row * pageRows + gutter * 3 + captionHeight;
    if (page == kAppearancePage) {
    // The theme row, then the text and button size.
    content = row * 2 + chipBlock + gutter * 2;
    } else if (page == kLanguagePage) {
        content = chipBlock + gutter * 2;
    } else if (page == kPlayerPage) {
        content = row * 3 + gutter * 2;
    } else if (page == kNotificationsPage || page == kPrivacyPage) {
        content = row + gutter * 2;
    } else if (page == kUpdatesPage) {
        content = row * 2 + gutter * 2;
    }
    screen.feed->scroller.update(frame.touch, contentMin, max, content, frame.seconds, c.dp(1));
    frame.animating |= screen.feed->scroller.moving();

    c.list()->PushClipRect(contentMin, max, true);
    float y = contentMin.y - screen.feed->scroller.offset();
    float x = min.x + gutter;

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
    const auto pageRow = [&](Text label, std::string_view target) {
        const ImVec2 rowMin{min.x, y};
        const ImVec2 rowMax{max.x, y + row};
        drawPressed(frame, rowMin, rowMax);
        medium.text({min.x + gutter, y + (row - c.lineHeight(theme::kTitleSize)) * 0.5f},
                    theme::kTitleSize, theme::kTextPrimary, tr(label));
        if (!screen.feed->scroller.moving() && frame.touch.tapped(rowMin, rowMax)) {
            later([this, label, target] {
                push(Screen{.route = Route::Settings,
                            .argument = std::string(target),
                            .title = std::string(tr(label))});
            });
        }
        y += row;
    };
    

    if (page == kAppearancePage) {
    if (switchRow(frame, tr(Text::ThemeSetting), tr(Text::ThemeSettingDetail),
                  {min.x, y}, width, std::nullopt) &&
        !screen.feed->scroller.moving()) {
        m_appearancePending = m_settings.appearance;
        m_appearanceDialogOpen = true;
    }
    y += row;
    y += gutter;
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
    } else if (page == kLanguagePage) {
        // Open the language picker without changing the current language.
        if (switchRow(frame, tr(Text::LanguageSetting), tr(Text::LanguageSettingDetail),
                      {min.x, y}, width, std::nullopt) &&
            !screen.feed->scroller.moving()) {
            m_languagePending = m_settings.language;
            m_languageDialogOpen = true;
        }
        y += row;
    } else if (page == kPlayerPage) {
        for (std::size_t i = 0; i < 3; ++i) {
            toggleRow(kToggles[i]);
        }
    } else if (page == kNotificationsPage) {
        toggleRow(kToggles[3]);
    } else if (page == kUpdatesPage) {
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
    } else if (page == kPrivacyPage) {
        // The search history, kept on the phone only.
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
        y += row;
    } else {
        pageRow(Text::SectionAppearance, kAppearancePage);
        pageRow(Text::SectionLanguages, kLanguagePage);
        pageRow(Text::SectionPlayer, kPlayerPage);
        pageRow(Text::SectionNotifications, kNotificationsPage);
        if (build::kChecksForUpdates) {
            pageRow(Text::SectionUpdates, kUpdatesPage);
        }
                pageRow(Text::SectionPrivacy, kPrivacyPage);
        y += gutter * 2;

        if (switchRow(frame,
                      "Conectar cuenta de YouTube",
                      "Inicia sesión para conectar tu cuenta",
                      {min.x, y}, width, std::nullopt) &&
            !screen.feed->scroller.moving()) {
            m_platform.openYouTubeLogin();
        }
        y += row;

        const std::string version =
            std::string(build::kAppName) + ' ' + std::string(build::kVersion);
        c.line({min.x + gutter, y}, width - gutter * 2, theme::kCaptionSize, theme::kTextSecondary,
               version, TextAlign::Center);
    }
    c.list()->PopClipRect();

    // The title is drawn from the language in use, so changing it here updates it at once.
    Text titleText = Text::Settings;
    if (page == kAppearancePage) {
        titleText = Text::SectionAppearance;
    } else if (page == kLanguagePage) {
        titleText = Text::SectionLanguages;
    } else if (page == kPlayerPage) {
        titleText = Text::SectionPlayer;
    } else if (page == kNotificationsPage) {
        titleText = Text::SectionNotifications;
    } else if (page == kUpdatesPage) {
        titleText = Text::SectionUpdates;
    } else if (page == kPrivacyPage) {
        titleText = Text::SectionPrivacy;
    }
    drawTitleBar(frame, std::string(tr(titleText)), {min.x, 0}, width);
}

} // namespace sloptube::app
