#include "off/ui/project_localization.hpp"

#include <array>
#include <stdexcept>
#include <utility>

namespace off::ui::l10n {
namespace {

bool valid_utf8(std::string_view value) noexcept {
  for (std::size_t i = 0; i < value.size();) {
    const auto lead = static_cast<unsigned char>(value[i]);
    if (lead <= 0x7fU) {
      ++i;
      continue;
    }
    unsigned continuation_count = 0;
    std::uint32_t code_point = 0;
    if (lead >= 0xc2U && lead <= 0xdfU) {
      continuation_count = 1;
      code_point = lead & 0x1fU;
    } else if (lead >= 0xe0U && lead <= 0xefU) {
      continuation_count = 2;
      code_point = lead & 0x0fU;
    } else if (lead >= 0xf0U && lead <= 0xf4U) {
      continuation_count = 3;
      code_point = lead & 0x07U;
    } else {
      return false;
    }
    if (i + continuation_count >= value.size())
      return false;
    for (unsigned j = 1; j <= continuation_count; ++j) {
      const auto byte = static_cast<unsigned char>(value[i + j]);
      if ((byte & 0xc0U) != 0x80U)
        return false;
      code_point = (code_point << 6U) | (byte & 0x3fU);
    }
    const auto minimum = continuation_count == 1   ? 0x80U
                         : continuation_count == 2 ? 0x800U
                                                   : 0x10000U;
    if (code_point < minimum || code_point > 0x10ffffU ||
        (code_point >= 0xd800U && code_point <= 0xdfffU))
      return false;
    i += continuation_count + 1;
  }
  return true;
}

std::optional<Locale> locale_from_tag(std::string_view tag) noexcept {
  const auto language_end = tag.find_first_of("-_.");
  const auto language = tag.substr(0, language_end);
  if (language.size() == 2 && (language[0] == 'e' || language[0] == 'E') &&
      (language[1] == 'n' || language[1] == 'N'))
    return Locale::english;
  if (language.size() == 2 && (language[0] == 's' || language[0] == 'S') &&
      (language[1] == 'v' || language[1] == 'V'))
    return Locale::swedish;
  constexpr std::array<std::pair<std::string_view, Locale>, 18> aliases{{
      {"da", Locale::danish},       {"nb", Locale::norwegian_bokmal},
      {"no", Locale::norwegian_bokmal}, {"fi", Locale::finnish},
      {"de", Locale::german},       {"fr", Locale::french},
      {"es", Locale::spanish},      {"it", Locale::italian},
      {"pt", Locale::portuguese_brazil}, {"pl", Locale::polish},
      {"cs", Locale::czech},        {"hu", Locale::hungarian},
      {"ro", Locale::romanian},     {"tr", Locale::turkish},
      {"ru", Locale::russian},      {"uk", Locale::ukrainian},
      {"ja", Locale::japanese},     {"ko", Locale::korean},
  }};
  for (const auto &[alias, locale] : aliases) {
    if (language.size() != alias.size())
      continue;
    bool matches = true;
    for (std::size_t i = 0; i < alias.size(); ++i) {
      const auto character = language[i];
      const auto lower = character >= 'A' && character <= 'Z'
                             ? static_cast<char>(character - 'A' + 'a')
                             : character;
      if (lower != alias[i]) {
        matches = false;
        break;
      }
    }
    if (matches)
      return locale;
  }
  if (language.size() == 2 && (language[0] == 'z' || language[0] == 'Z') &&
      (language[1] == 'h' || language[1] == 'H')) {
    // Do not silently substitute Simplified Chinese for a locale that
    // explicitly requests a Traditional script.
    const auto has_case_insensitive = [&](std::string_view needle) {
      if (tag.size() < needle.size())
        return false;
      for (std::size_t start = 0; start + needle.size() <= tag.size(); ++start) {
        bool matches = true;
        for (std::size_t i = 0; i < needle.size(); ++i) {
          const auto character = tag[start + i];
          const auto lower = character >= 'A' && character <= 'Z'
                                 ? static_cast<char>(character - 'A' + 'a')
                                 : character;
          if (lower != needle[i]) {
            matches = false;
            break;
          }
        }
        if (matches)
          return true;
      }
      return false;
    };
    if (has_case_insensitive("hant") || has_case_insensitive("tw") ||
        has_case_insensitive("hk"))
      return std::nullopt;
    return Locale::simplified_chinese;
  }
  return std::nullopt;
}

std::optional<Locale> select_locale(std::string_view explicit_locale,
                                    std::string_view platform_locale) noexcept {
  if (const auto locale = locale_from_tag(explicit_locale))
    return locale;
  if (const auto locale = locale_from_tag(platform_locale))
    return locale;
  return Locale::english;
}

constexpr std::array<std::array<std::string_view, 17>, locale_count> localized_prose{{
    {{"GRAPHICS SETTINGS", "Profile", "Window mode", "Resolution", "Present mode", "Render scale", "Upscaler", "Shadows", "Apply", "Back", "Defaults", "Keep", "Revert", "Applying settings...", "Restoring settings...", "Keep these display settings?", "Reverting in {seconds} seconds"}},
    {{"GRAFIKINSTÄLLNINGAR", "Profil", "Fönsterläge", "Upplösning", "Presentationsläge", "Renderingsskala", "Uppskalning", "Skuggor", "Tillämpa", "Tillbaka", "Standardvärden", "Behåll", "Återställ", "Tillämpar inställningar...", "Återställer inställningar...", "Behålla dessa bildskärmsinställningar?", "Återställer om {seconds} sekunder"}},
    {{"GRAFIKINDSTILLINGER", "Profil", "Vinduestilstand", "Opløsning", "Visningstilstand", "Renderingsskala", "Opskalering", "Skygger", "Anvend", "Tilbage", "Standarder", "Behold", "Gendan", "Anvender indstillinger...", "Gendanner indstillinger...", "Behold disse skærmindstillinger?", "Gendanner om {seconds} sekunder"}},
    {{"GRAFIKKINNSTILLINGER", "Profil", "Vindusmodus", "Oppløsning", "Visningsmodus", "Renderingsskala", "Oppskalering", "Skygger", "Bruk", "Tilbake", "Standarder", "Behold", "Tilbakestill", "Bruker innstillinger...", "Gjenoppretter innstillinger...", "Beholde disse skjerminnstillingene?", "Gjenoppretter om {seconds} sekunder"}},
    {{"GRAFIIKKA-ASETUKSET", "Profiili", "Ikkunatila", "Tarkkuus", "Esitystila", "Renderöintiasteikko", "Skaalaus", "Varjot", "Käytä", "Takaisin", "Oletukset", "Pidä", "Palauta", "Otetaan asetuksia käyttöön...", "Palautetaan asetuksia...", "Säilytetäänkö nämä näyttöasetukset?", "Palautetaan {seconds} sekunnissa"}},
    {{"GRAFIKEINSTELLUNGEN", "Profil", "Fenstermodus", "Auflösung", "Präsentationsmodus", "Render-Skalierung", "Hochskalierung", "Schatten", "Anwenden", "Zurück", "Standardwerte", "Behalten", "Zurücksetzen", "Einstellungen werden angewendet...", "Einstellungen werden wiederhergestellt...", "Diese Anzeigeeinstellungen behalten?", "Wird in {seconds} Sekunden zurückgesetzt"}},
    {{"PARAMÈTRES GRAPHIQUES", "Profil", "Mode fenêtre", "Résolution", "Mode d'affichage", "Échelle de rendu", "Mise à l'échelle", "Ombres", "Appliquer", "Retour", "Valeurs par défaut", "Conserver", "Rétablir", "Application des paramètres...", "Restauration des paramètres...", "Conserver ces paramètres d'affichage ?", "Rétablissement dans {seconds} secondes"}},
    {{"AJUSTES GRÁFICOS", "Perfil", "Modo de ventana", "Resolución", "Modo de presentación", "Escala de renderizado", "Reescalado", "Sombras", "Aplicar", "Atrás", "Predeterminados", "Conservar", "Revertir", "Aplicando ajustes...", "Restaurando ajustes...", "¿Conservar estos ajustes de pantalla?", "Se revertirá en {seconds} segundos"}},
    {{"IMPOSTAZIONI GRAFICHE", "Profilo", "Modalità finestra", "Risoluzione", "Modalità presentazione", "Scala rendering", "Upscaling", "Ombre", "Applica", "Indietro", "Predefiniti", "Mantieni", "Ripristina", "Applicazione impostazioni...", "Ripristino impostazioni...", "Mantenere queste impostazioni schermo?", "Ripristino tra {seconds} secondi"}},
    {{"CONFIGURAÇÕES GRÁFICAS", "Perfil", "Modo de janela", "Resolução", "Modo de apresentação", "Escala de renderização", "Ampliação", "Sombras", "Aplicar", "Voltar", "Padrões", "Manter", "Reverter", "Aplicando configurações...", "Restaurando configurações...", "Manter estas configurações de vídeo?", "Reverter em {seconds} segundos"}},
    {{"USTAWIENIA GRAFIKI", "Profil", "Tryb okna", "Rozdzielczość", "Tryb prezentacji", "Skala renderowania", "Skalowanie", "Cienie", "Zastosuj", "Wstecz", "Domyślne", "Zachowaj", "Przywróć", "Stosowanie ustawień...", "Przywracanie ustawień...", "Zachować te ustawienia obrazu?", "Przywracanie za {seconds} s"}},
    {{"NASTAVENÍ GRAFIKY", "Profil", "Režim okna", "Rozlišení", "Režim prezentace", "Měřítko vykreslení", "Převzorkování", "Stíny", "Použít", "Zpět", "Výchozí", "Ponechat", "Vrátit", "Používání nastavení...", "Obnovování nastavení...", "Ponechat toto nastavení zobrazení?", "Vrácení za {seconds} sekund"}},
    {{"GRAFIKAI BEÁLLÍTÁSOK", "Profil", "Ablakmód", "Felbontás", "Megjelenítési mód", "Renderelési skála", "Felskálázás", "Árnyékok", "Alkalmaz", "Vissza", "Alapértékek", "Megtart", "Visszaállít", "Beállítások alkalmazása...", "Beállítások visszaállítása...", "Megtartja ezeket a kijelzőbeállításokat?", "Visszaállítás {seconds} másodperc múlva"}},
    {{"SETĂRI GRAFICE", "Profil", "Mod fereastră", "Rezoluție", "Mod prezentare", "Scară de randare", "Mărire", "Umbre", "Aplică", "Înapoi", "Implicite", "Păstrează", "Revino", "Se aplică setările...", "Se restaurează setările...", "Păstrați aceste setări de afișare?", "Revenire în {seconds} secunde"}},
    {{"GRAFİK AYARLARI", "Profil", "Pencere modu", "Çözünürlük", "Sunum modu", "İşleme ölçeği", "Yükseltme", "Gölgeler", "Uygula", "Geri", "Varsayılanlar", "Koru", "Geri al", "Ayarlar uygulanıyor...", "Ayarlar geri yükleniyor...", "Bu görüntü ayarları korunsun mu?", "{seconds} saniye içinde geri alınacak"}},
    {{"НАСТРОЙКИ ГРАФИКИ", "Профиль", "Режим окна", "Разрешение", "Режим вывода", "Масштаб рендеринга", "Масштабирование", "Тени", "Применить", "Назад", "По умолчанию", "Сохранить", "Отменить", "Применение настроек...", "Восстановление настроек...", "Сохранить эти настройки экрана?", "Возврат через {seconds} с"}},
    {{"НАЛАШТУВАННЯ ГРАФІКИ", "Профіль", "Режим вікна", "Роздільність", "Режим показу", "Масштаб рендерингу", "Масштабування", "Тіні", "Застосувати", "Назад", "Типові", "Зберегти", "Скасувати", "Застосування налаштувань...", "Відновлення налаштувань...", "Зберегти ці налаштування екрана?", "Повернення через {seconds} с"}},
    {{"グラフィック設定", "プロフィール", "ウィンドウモード", "解像度", "表示モード", "描画スケール", "アップスケーラー", "影", "適用", "戻る", "初期設定", "保持", "元に戻す", "設定を適用中...", "設定を復元中...", "この表示設定を維持しますか？", "{seconds} 秒後に元に戻します"}},
    {{"그래픽 설정", "프로필", "창 모드", "해상도", "표시 모드", "렌더링 배율", "업스케일러", "그림자", "적용", "뒤로", "기본값", "유지", "되돌리기", "설정 적용 중...", "설정 복원 중...", "이 화면 설정을 유지할까요?", "{seconds}초 후 되돌립니다"}},
    {{"图形设置", "配置", "窗口模式", "分辨率", "显示模式", "渲染比例", "超分辨率", "阴影", "应用", "返回", "默认值", "保留", "还原", "正在应用设置...", "正在恢复设置...", "保留这些显示设置？", "将在 {seconds} 秒后还原"}},
}};

// Profile names remain product names. All remaining F10 labels are authored
// per locale too, including the display-mode value that the draw path shows.
constexpr std::array<std::array<std::string_view, 14>, locale_count> localized_labels{{
    {{"Original", "Modern", "Modern+", "Windowed", "Borderless desktop", "VSync", "Mailbox", "Immediate", "Native", "Temporal", "DLSS", "Reference", "High", "Ultra"}},
    {{"Original", "Modern", "Modern+", "Fönster", "Kantlöst skrivbord", "VSync", "Mailbox", "Omedelbar", "Inbyggd", "Temporal", "DLSS", "Referens", "Hög", "Ultra"}},
    {{"Original", "Modern", "Modern+", "Vindue", "Kantløst skrivebord", "VSync", "Postkasse", "Straks", "Indbygget", "Tidsbaseret", "DLSS", "Reference", "Høj", "Ultra"}},
    {{"Original", "Modern", "Modern+", "Vindu", "Kantløst skrivebord", "VSync", "Postkasse", "Umiddelbar", "Innebygd", "Tidsbasert", "DLSS", "Referanse", "Høy", "Ultra"}},
    {{"Original", "Modern", "Modern+", "Ikkuna", "Reunaton työpöytä", "VSync", "Postilaatikko", "Välitön", "Natiivi", "Ajallinen", "DLSS", "Viite", "Korkea", "Ultra"}},
    {{"Original", "Modern", "Modern+", "Fenster", "Rahmenloser Desktop", "VSync", "Mailbox", "Sofort", "Nativ", "Temporal", "DLSS", "Referenz", "Hoch", "Ultra"}},
    {{"Original", "Modern", "Modern+", "Fenêtré", "Bureau sans bordure", "VSync", "Boîte aux lettres", "Immédiat", "Natif", "Temporel", "DLSS", "Référence", "Élevé", "Ultra"}},
    {{"Original", "Modern", "Modern+", "Ventana", "Escritorio sin bordes", "VSync", "Buzón", "Inmediato", "Nativo", "Temporal", "DLSS", "Referencia", "Alto", "Ultra"}},
    {{"Original", "Modern", "Modern+", "Finestra", "Desktop senza bordi", "VSync", "Cassetta postale", "Immediato", "Nativo", "Temporale", "DLSS", "Riferimento", "Alto", "Ultra"}},
    {{"Original", "Modern", "Modern+", "Janela", "Área de trabalho sem bordas", "VSync", "Caixa de correio", "Imediato", "Nativo", "Temporal", "DLSS", "Referência", "Alto", "Ultra"}},
    {{"Oryginalny", "Nowoczesny", "Nowoczesny+", "Okno", "Pulpit bez obramowania", "VSync", "Skrzynka", "Natychmiast", "Natywny", "Czasowy", "DLSS", "Referencyjny", "Wysokie", "Ultra"}},
    {{"Původní", "Moderní", "Moderní+", "Okno", "Plocha bez okrajů", "VSync", "Poštovní schránka", "Ihned", "Nativní", "Dočasný", "DLSS", "Referenční", "Vysoké", "Ultra"}},
    {{"Eredeti", "Modern", "Modern+", "Ablakos", "Keret nélküli asztal", "VSync", "Postafiók", "Azonnali", "Natív", "Időbeli", "DLSS", "Referencia", "Magas", "Ultra"}},
    {{"Original", "Modern", "Modern+", "Fereastră", "Desktop fără margini", "VSync", "Cutie poștală", "Imediat", "Nativ", "Temporal", "DLSS", "Referință", "Ridicat", "Ultra"}},
    {{"Özgün", "Modern", "Modern+", "Pencereli", "Kenarlıksız masaüstü", "VSync", "Posta kutusu", "Anında", "Yerel", "Zamansal", "DLSS", "Başvuru", "Yüksek", "Ultra"}},
    {{"Оригинал", "Современный", "Современный+", "В окне", "Полноэкранный без рамки", "VSync", "Почтовый ящик", "Сразу", "Нативный", "Временной", "DLSS", "Эталон", "Высокое", "Ультра"}},
    {{"Оригінал", "Сучасний", "Сучасний+", "У вікні", "Безрамковий робочий стіл", "VSync", "Поштова скринька", "Негайно", "Нативний", "Часовий", "DLSS", "Еталон", "Високий", "Ультра"}},
    {{"オリジナル", "モダン", "モダン+", "ウィンドウ", "ボーダーレスデスクトップ", "VSync", "メールボックス", "即時", "ネイティブ", "時間的", "DLSS", "基準", "高", "ウルトラ"}},
    {{"오리지널", "모던", "모던+", "창", "테두리 없는 데스크톱", "VSync", "메일박스", "즉시", "네이티브", "시간적", "DLSS", "참조", "높음", "울트라"}},
    {{"原版", "现代", "现代+", "窗口", "无边框桌面", "VSync", "邮箱", "立即", "原生", "时序", "DLSS", "参考", "高", "极高"}},
}};

constexpr std::array<CatalogEntry, message_id_count * locale_count>
make_f10_entries() {
  std::array<CatalogEntry, message_id_count * locale_count> entries{};
  for (std::size_t locale = 0; locale < locale_count; ++locale)
    for (std::size_t id = 0; id < message_id_count; ++id) {
      const auto text = id < localized_prose[locale].size()
                            ? localized_prose[locale][id]
                            : localized_labels[locale][id - localized_prose[locale].size()];
      entries[locale * message_id_count + id] = {
          static_cast<Locale>(locale), static_cast<MessageId>(id), text};
    }
  return entries;
}

constexpr auto f10_entries = make_f10_entries();

} // namespace

CatalogBuildResult
ProjectCatalog::build(std::span<const CatalogEntry> entries) {
  ProjectCatalog catalog;
  std::array<std::array<bool, message_id_count>, locale_count> seen{};
  for (const auto &entry : entries) {
    const auto locale = static_cast<std::size_t>(entry.locale);
    const auto id = static_cast<std::size_t>(entry.id);
    if (locale >= locale_count)
      return {.catalog = std::nullopt, .error = CatalogError::invalid_locale};
    if (id >= message_id_count)
      return {.catalog = std::nullopt,
              .error = CatalogError::invalid_message_id};
    if (entry.text.empty())
      return {.catalog = std::nullopt, .error = CatalogError::empty_message};
    if (!valid_utf8(entry.text))
      return {.catalog = std::nullopt, .error = CatalogError::invalid_utf8};
    if (seen[locale][id])
      return {.catalog = std::nullopt,
              .error = CatalogError::duplicate_message};
    seen[locale][id] = true;
    catalog.messages_[locale][id] = entry.text;
  }
  for (const auto &by_locale : seen)
    for (const auto present : by_locale)
      if (!present)
        return {.catalog = std::nullopt,
                .error = CatalogError::missing_message};
  return {.catalog = std::move(catalog), .error = std::nullopt};
}

std::optional<std::string_view>
ProjectCatalog::resolve(MessageId id, std::string_view explicit_locale,
                        std::string_view platform_locale) const noexcept {
  const auto index = static_cast<std::size_t>(id);
  if (index >= message_id_count)
    return std::nullopt;
  const auto locale = *select_locale(explicit_locale, platform_locale);
  return messages_[static_cast<std::size_t>(locale)][index];
}

std::optional<std::string_view>
ProjectCatalog::format_seconds(MessageId id, unsigned seconds,
                               std::string_view explicit_locale,
                               std::string_view platform_locale) const {
  const auto pattern = resolve(id, explicit_locale, platform_locale);
  if (!pattern || id != MessageId::reverting_in_seconds)
    return std::nullopt;
  const auto marker = pattern->find("{seconds}");
  if (marker == std::string_view::npos)
    return std::nullopt;
  thread_local std::string formatted;
  formatted = std::string{pattern->substr(0, marker)} +
              std::to_string(seconds) +
              std::string{pattern->substr(marker + 9)};
  return formatted;
}

const ProjectCatalog &f10_catalog() {
  static const ProjectCatalog catalog = [] {
    const auto result = ProjectCatalog::build(f10_entries);
    if (!result.catalog)
      throw std::logic_error("built-in F10 catalog is invalid");
    return *result.catalog;
  }();
  return catalog;
}

} // namespace off::ui::l10n
