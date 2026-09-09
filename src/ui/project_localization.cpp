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

std::optional<Locale> select_locale(
    std::string_view explicit_locale,
    std::span<const std::string_view> platform_locales) noexcept {
  if (const auto locale = locale_from_tag(explicit_locale))
    return locale;
  for (const auto locale_tag : platform_locales)
    if (const auto locale = locale_from_tag(locale_tag))
      return locale;
  return Locale::english;
}

constexpr std::array<std::array<std::string_view, 29>, locale_count> localized_prose{{
    {{"GRAPHICS SETTINGS", "Profile", "Window mode", "Resolution", "Present mode", "Render scale", "Upscaler", "Shadows", "Apply", "Back", "Defaults", "Keep", "Revert", "Applying settings...", "Restoring settings...", "Keep these display settings?", "Reverting in {seconds} seconds", "Game data required", "OpenFreedomFighters needs game data from a legally purchased Freedom Fighters Steam installation.", "The selected game-data folder cannot be found.", "Freedom.Exe is missing from the selected game-data folder.", "Freedom.Exe is not a supported Freedom Fighters version.", "Required Freedom Fighters game data is missing.", "The selected game data could not be read.", "Game-data verification did not succeed.", "Relaunch with: --data PATH", "Technical details", "Verifying game data...", "Preparing startup..."}},
    {{"GRAFIKINSTÄLLNINGAR", "Profil", "Fönsterläge", "Upplösning", "Presentationsläge", "Renderingsskala", "Uppskalning", "Skuggor", "Tillämpa", "Tillbaka", "Standardvärden", "Behåll", "Återställ", "Tillämpar inställningar...", "Återställer inställningar...", "Behålla dessa bildskärmsinställningar?", "Återställer om {seconds} sekunder", "Speldata krävs", "OpenFreedomFighters behöver speldata från en lagligt köpt Steam-installation av Freedom Fighters.", "Den valda mappen med speldata kan inte hittas.", "Freedom.Exe saknas i den valda speldata-mappen.", "Freedom.Exe är inte en version av Freedom Fighters som stöds.", "Nödvändig speldata för Freedom Fighters saknas.", "Den valda speldata kunde inte läsas.", "Verifieringen av speldata lyckades inte.", "Starta om med: --data SÖKVÄG", "Tekniska detaljer", "Verifierar speldata...", "Förbereder uppstart..."}},
    {{"GRAFIKINDSTILLINGER", "Profil", "Vinduestilstand", "Opløsning", "Visningstilstand", "Renderingsskala", "Opskalering", "Skygger", "Anvend", "Tilbage", "Standarder", "Behold", "Gendan", "Anvender indstillinger...", "Gendanner indstillinger...", "Behold disse skærmindstillinger?", "Gendanner om {seconds} sekunder", "Spildata kræves", "OpenFreedomFighters kræver spildata fra en lovligt købt Steam-installation af Freedom Fighters.", "Den valgte mappe med spildata kan ikke findes.", "Freedom.Exe mangler i den valgte mappe med spildata.", "Freedom.Exe er ikke en understøttet version af Freedom Fighters.", "Påkrævede Freedom Fighters-spildata mangler.", "De valgte spildata kunne ikke læses.", "Kontrollen af spildata lykkedes ikke.", "Genstart med: --data STI", "Tekniske detaljer", "Kontrollerer spildata...", "Forbereder opstart..."}},
    {{"GRAFIKKINNSTILLINGER", "Profil", "Vindusmodus", "Oppløsning", "Visningsmodus", "Renderingsskala", "Oppskalering", "Skygger", "Bruk", "Tilbake", "Standarder", "Behold", "Tilbakestill", "Bruker innstillinger...", "Gjenoppretter innstillinger...", "Beholde disse skjerminnstillingene?", "Gjenoppretter om {seconds} sekunder", "Spilldata kreves", "OpenFreedomFighters krever spilldata fra en lovlig kjøpt Steam-installasjon av Freedom Fighters.", "Den valgte spilldatamappen ble ikke funnet.", "Freedom.Exe mangler i den valgte spilldatamappen.", "Freedom.Exe er ikke en støttet Freedom Fighters-versjon.", "Nødvendige Freedom Fighters-spilldata mangler.", "De valgte spilldataene kunne ikke leses.", "Kontroll av spilldata lyktes ikke.", "Start på nytt med: --data STI", "Tekniske detaljer", "Kontrollerer spilldata...", "Forbereder oppstart..."}},
    {{"GRAFIIKKA-ASETUKSET", "Profiili", "Ikkunatila", "Tarkkuus", "Esitystila", "Renderöintiasteikko", "Skaalaus", "Varjot", "Käytä", "Takaisin", "Oletukset", "Pidä", "Palauta", "Otetaan asetuksia käyttöön...", "Palautetaan asetuksia...", "Säilytetäänkö nämä näyttöasetukset?", "Palautetaan {seconds} sekunnissa", "Pelitiedot vaaditaan", "OpenFreedomFighters tarvitsee pelitiedot laillisesti ostetusta Freedom Fighters Steam -asennuksesta.", "Valittua pelitietokansiota ei löydy.", "Freedom.Exe puuttuu valitusta pelitietokansiosta.", "Freedom.Exe ei ole tuettu Freedom Fighters -versio.", "Vaadittuja Freedom Fighters -pelitietoja puuttuu.", "Valittuja pelitietoja ei voitu lukea.", "Pelitietojen tarkistus ei onnistunut.", "Käynnistä uudelleen: --data POLKU", "Tekniset tiedot", "Tarkistetaan pelitietoja...", "Valmistellaan käynnistystä..."}},
    {{"GRAFIKEINSTELLUNGEN", "Profil", "Fenstermodus", "Auflösung", "Präsentationsmodus", "Render-Skalierung", "Hochskalierung", "Schatten", "Anwenden", "Zurück", "Standardwerte", "Behalten", "Zurücksetzen", "Einstellungen werden angewendet...", "Einstellungen werden wiederhergestellt...", "Diese Anzeigeeinstellungen behalten?", "Wird in {seconds} Sekunden zurückgesetzt", "Spieldaten erforderlich", "OpenFreedomFighters benötigt Spieldaten aus einer legal erworbenen Steam-Installation von Freedom Fighters.", "Der ausgewählte Spieldatenordner wurde nicht gefunden.", "Freedom.Exe fehlt im ausgewählten Spieldatenordner.", "Freedom.Exe ist keine unterstützte Freedom Fighters-Version.", "Erforderliche Freedom Fighters-Spieldaten fehlen.", "Die ausgewählten Spieldaten konnten nicht gelesen werden.", "Die Prüfung der Spieldaten war nicht erfolgreich.", "Neu starten mit: --data PFAD", "Technische Details", "Spieldaten werden überprüft...", "Start wird vorbereitet..."}},
    {{"PARAMÈTRES GRAPHIQUES", "Profil", "Mode fenêtre", "Résolution", "Mode d'affichage", "Échelle de rendu", "Mise à l'échelle", "Ombres", "Appliquer", "Retour", "Valeurs par défaut", "Conserver", "Rétablir", "Application des paramètres...", "Restauration des paramètres...", "Conserver ces paramètres d'affichage ?", "Rétablissement dans {seconds} secondes", "Données du jeu requises", "OpenFreedomFighters nécessite les données d'une installation Steam légalement achetée de Freedom Fighters.", "Le dossier de données sélectionné est introuvable.", "Freedom.Exe manque dans le dossier de données sélectionné.", "Freedom.Exe n'est pas une version prise en charge de Freedom Fighters.", "Des données Freedom Fighters requises sont manquantes.", "Les données sélectionnées n'ont pas pu être lues.", "La vérification des données a échoué.", "Relancez avec : --data CHEMIN", "Détails techniques", "Vérification des données du jeu...", "Préparation du démarrage..."}},
    {{"AJUSTES GRÁFICOS", "Perfil", "Modo de ventana", "Resolución", "Modo de presentación", "Escala de renderizado", "Reescalado", "Sombras", "Aplicar", "Atrás", "Predeterminados", "Conservar", "Revertir", "Aplicando ajustes...", "Restaurando ajustes...", "¿Conservar estos ajustes de pantalla?", "Se revertirá en {seconds} segundos", "Se requieren datos del juego", "OpenFreedomFighters necesita datos de una instalación de Steam de Freedom Fighters comprada legalmente.", "No se encuentra la carpeta de datos seleccionada.", "Falta Freedom.Exe en la carpeta de datos seleccionada.", "Freedom.Exe no es una versión compatible de Freedom Fighters.", "Faltan datos necesarios de Freedom Fighters.", "No se pudieron leer los datos seleccionados.", "La verificación de datos no se completó.", "Reinicia con: --data RUTA", "Detalles técnicos", "Verificando los datos del juego...", "Preparando el inicio..."}},
    {{"IMPOSTAZIONI GRAFICHE", "Profilo", "Modalità finestra", "Risoluzione", "Modalità presentazione", "Scala rendering", "Upscaling", "Ombre", "Applica", "Indietro", "Predefiniti", "Mantieni", "Ripristina", "Applicazione impostazioni...", "Ripristino impostazioni...", "Mantenere queste impostazioni schermo?", "Ripristino tra {seconds} secondi", "Dati di gioco richiesti", "OpenFreedomFighters richiede dati da un'installazione Steam di Freedom Fighters acquistata legalmente.", "La cartella dei dati selezionata non è stata trovata.", "Freedom.Exe manca nella cartella dei dati selezionata.", "Freedom.Exe non è una versione supportata di Freedom Fighters.", "Mancano dati necessari di Freedom Fighters.", "I dati selezionati non possono essere letti.", "La verifica dei dati non è riuscita.", "Riavvia con: --data PERCORSO", "Dettagli tecnici", "Verifica dei dati di gioco...", "Preparazione dell'avvio..."}},
    {{"CONFIGURAÇÕES GRÁFICAS", "Perfil", "Modo de janela", "Resolução", "Modo de apresentação", "Escala de renderização", "Ampliação", "Sombras", "Aplicar", "Voltar", "Padrões", "Manter", "Reverter", "Aplicando configurações...", "Restaurando configurações...", "Manter estas configurações de vídeo?", "Reverter em {seconds} segundos", "Dados do jogo necessários", "OpenFreedomFighters precisa de dados de uma instalação Steam do Freedom Fighters adquirida legalmente.", "A pasta de dados selecionada não foi encontrada.", "Freedom.Exe não está na pasta de dados selecionada.", "Freedom.Exe não é uma versão compatível do Freedom Fighters.", "Faltam dados necessários do Freedom Fighters.", "Não foi possível ler os dados selecionados.", "A verificação dos dados não foi concluída.", "Reinicie com: --data CAMINHO", "Detalhes técnicos", "Verificando dados do jogo...", "Preparando a inicialização..."}},
    {{"USTAWIENIA GRAFIKI", "Profil", "Tryb okna", "Rozdzielczość", "Tryb prezentacji", "Skala renderowania", "Skalowanie", "Cienie", "Zastosuj", "Wstecz", "Domyślne", "Zachowaj", "Przywróć", "Stosowanie ustawień...", "Przywracanie ustawień...", "Zachować te ustawienia obrazu?", "Przywracanie za {seconds} s", "Wymagane dane gry", "OpenFreedomFighters wymaga danych z legalnie zakupionej instalacji Steam Freedom Fighters.", "Nie znaleziono wybranego folderu danych gry.", "W wybranym folderze danych gry brakuje Freedom.Exe.", "Freedom.Exe nie jest obsługiwaną wersją Freedom Fighters.", "Brakuje wymaganych danych Freedom Fighters.", "Nie można odczytać wybranych danych gry.", "Weryfikacja danych gry nie powiodła się.", "Uruchom ponownie z: --data ŚCIEŻKA", "Szczegóły techniczne", "Weryfikowanie danych gry...", "Przygotowywanie uruchomienia..."}},
    {{"NASTAVENÍ GRAFIKY", "Profil", "Režim okna", "Rozlišení", "Režim prezentace", "Měřítko vykreslení", "Převzorkování", "Stíny", "Použít", "Zpět", "Výchozí", "Ponechat", "Vrátit", "Používání nastavení...", "Obnovování nastavení...", "Ponechat toto nastavení zobrazení?", "Vrácení za {seconds} sekund", "Vyžadována herní data", "OpenFreedomFighters vyžaduje data z legálně zakoupené instalace Freedom Fighters ve službě Steam.", "Vybranou složku s herními daty nelze najít.", "Ve vybrané složce s herními daty chybí Freedom.Exe.", "Freedom.Exe není podporovaná verze Freedom Fighters.", "Chybí požadovaná data Freedom Fighters.", "Vybraná herní data nelze přečíst.", "Ověření herních dat nebylo úspěšné.", "Spusťte znovu s: --data CESTA", "Technické podrobnosti", "Ověřování herních dat...", "Příprava spuštění..."}},
    {{"GRAFIKAI BEÁLLÍTÁSOK", "Profil", "Ablakmód", "Felbontás", "Megjelenítési mód", "Renderelési skála", "Felskálázás", "Árnyékok", "Alkalmaz", "Vissza", "Alapértékek", "Megtart", "Visszaállít", "Beállítások alkalmazása...", "Beállítások visszaállítása...", "Megtartja ezeket a kijelzőbeállításokat?", "Visszaállítás {seconds} másodperc múlva", "Játékadatok szükségesek", "Az OpenFreedomFighters egy jogszerűen megvásárolt Freedom Fighters Steam-telepítés játékadatait igényli.", "A kijelölt játékadatmappa nem található.", "A Freedom.Exe hiányzik a kijelölt játékadatmappából.", "A Freedom.Exe nem támogatott Freedom Fighters-verzió.", "Hiányoznak a szükséges Freedom Fighters-játékadatok.", "A kijelölt játékadatok nem olvashatók.", "A játékadatok ellenőrzése sikertelen volt.", "Indítsa újra ezzel: --data ÚTVONAL", "Műszaki részletek", "Játékadatok ellenőrzése...", "Indítás előkészítése..."}},
    {{"SETĂRI GRAFICE", "Profil", "Mod fereastră", "Rezoluție", "Mod prezentare", "Scară de randare", "Mărire", "Umbre", "Aplică", "Înapoi", "Implicite", "Păstrează", "Revino", "Se aplică setările...", "Se restaurează setările...", "Păstrați aceste setări de afișare?", "Revenire în {seconds} secunde", "Sunt necesare datele jocului", "OpenFreedomFighters necesită date dintr-o instalare Steam Freedom Fighters cumpărată legal.", "Dosarul de date selectat nu a fost găsit.", "Freedom.Exe lipsește din dosarul de date selectat.", "Freedom.Exe nu este o versiune Freedom Fighters acceptată.", "Lipsesc date Freedom Fighters necesare.", "Datele selectate nu au putut fi citite.", "Verificarea datelor jocului nu a reușit.", "Reporniți cu: --data CALE", "Detalii tehnice", "Se verifică datele jocului...", "Se pregătește pornirea..."}},
    {{"GRAFİK AYARLARI", "Profil", "Pencere modu", "Çözünürlük", "Sunum modu", "İşleme ölçeği", "Yükseltme", "Gölgeler", "Uygula", "Geri", "Varsayılanlar", "Koru", "Geri al", "Ayarlar uygulanıyor...", "Ayarlar geri yükleniyor...", "Bu görüntü ayarları korunsun mu?", "{seconds} saniye içinde geri alınacak", "Oyun verileri gerekli", "OpenFreedomFighters, yasal olarak satın alınmış bir Freedom Fighters Steam kurulumundan oyun verileri gerektirir.", "Seçilen oyun verisi klasörü bulunamadı.", "Seçilen oyun verisi klasöründe Freedom.Exe yok.", "Freedom.Exe desteklenen bir Freedom Fighters sürümü değil.", "Gerekli Freedom Fighters oyun verileri eksik.", "Seçilen oyun verileri okunamadı.", "Oyun verisi doğrulaması başarılı olmadı.", "Şununla yeniden başlatın: --data YOL", "Teknik ayrıntılar", "Oyun verileri doğrulanıyor...", "Başlatma hazırlanıyor..."}},
    {{"НАСТРОЙКИ ГРАФИКИ", "Профиль", "Режим окна", "Разрешение", "Режим вывода", "Масштаб рендеринга", "Масштабирование", "Тени", "Применить", "Назад", "По умолчанию", "Сохранить", "Отменить", "Применение настроек...", "Восстановление настроек...", "Сохранить эти настройки экрана?", "Возврат через {seconds} с", "Требуются данные игры", "OpenFreedomFighters требует данные из легально приобретённой Steam-установки Freedom Fighters.", "Выбранная папка с данными игры не найдена.", "В выбранной папке с данными игры отсутствует Freedom.Exe.", "Freedom.Exe не является поддерживаемой версией Freedom Fighters.", "Отсутствуют необходимые данные Freedom Fighters.", "Не удалось прочитать выбранные данные игры.", "Проверка данных игры не завершилась успешно.", "Перезапустите с: --data ПУТЬ", "Технические сведения", "Проверка данных игры...", "Подготовка запуска..."}},
    {{"НАЛАШТУВАННЯ ГРАФІКИ", "Профіль", "Режим вікна", "Роздільність", "Режим показу", "Масштаб рендерингу", "Масштабування", "Тіні", "Застосувати", "Назад", "Типові", "Зберегти", "Скасувати", "Застосування налаштувань...", "Відновлення налаштувань...", "Зберегти ці налаштування екрана?", "Повернення через {seconds} с", "Потрібні дані гри", "OpenFreedomFighters потребує даних із законно придбаної Steam-інсталяції Freedom Fighters.", "Вибрану папку з даними гри не знайдено.", "У вибраній папці з даними гри немає Freedom.Exe.", "Freedom.Exe не є підтримуваною версією Freedom Fighters.", "Відсутні потрібні дані Freedom Fighters.", "Не вдалося прочитати вибрані дані гри.", "Перевірка даних гри не вдалася.", "Перезапустіть із: --data ШЛЯХ", "Технічні відомості", "Перевірка даних гри...", "Підготовка запуску..."}},
    {{"グラフィック設定", "プロフィール", "ウィンドウモード", "解像度", "表示モード", "描画スケール", "アップスケーラー", "影", "適用", "戻る", "初期設定", "保持", "元に戻す", "設定を適用中...", "設定を復元中...", "この表示設定を維持しますか？", "{seconds} 秒後に元に戻します", "ゲームデータが必要です", "OpenFreedomFighters には、正規に購入した Freedom Fighters の Steam インストールからのゲームデータが必要です。", "選択されたゲームデータフォルダーが見つかりません。", "選択されたゲームデータフォルダーに Freedom.Exe がありません。", "Freedom.Exe はサポートされている Freedom Fighters のバージョンではありません。", "必要な Freedom Fighters のゲームデータが不足しています。", "選択されたゲームデータを読み取れませんでした。", "ゲームデータの検証に失敗しました。", "次で再起動: --data パス", "技術情報", "ゲームデータを検証中...", "起動を準備中..."}},
    {{"그래픽 설정", "프로필", "창 모드", "해상도", "표시 모드", "렌더링 배율", "업스케일러", "그림자", "적용", "뒤로", "기본값", "유지", "되돌리기", "설정 적용 중...", "설정 복원 중...", "이 화면 설정을 유지할까요?", "{seconds}초 후 되돌립니다", "게임 데이터가 필요합니다", "OpenFreedomFighters에는 합법적으로 구매한 Freedom Fighters Steam 설치본의 게임 데이터가 필요합니다.", "선택한 게임 데이터 폴더를 찾을 수 없습니다.", "선택한 게임 데이터 폴더에 Freedom.Exe가 없습니다.", "Freedom.Exe는 지원되는 Freedom Fighters 버전이 아닙니다.", "필수 Freedom Fighters 게임 데이터가 없습니다.", "선택한 게임 데이터를 읽을 수 없습니다.", "게임 데이터 확인에 실패했습니다.", "다음으로 다시 시작: --data 경로", "기술 정보", "게임 데이터를 확인하는 중...", "시작 준비 중..."}},
    {{"图形设置", "配置", "窗口模式", "分辨率", "显示模式", "渲染比例", "超分辨率", "阴影", "应用", "返回", "默认值", "保留", "还原", "正在应用设置...", "正在恢复设置...", "保留这些显示设置？", "将在 {seconds} 秒后还原", "需要游戏数据", "OpenFreedomFighters 需要来自合法购买的 Freedom Fighters Steam 安装的游戏数据。", "找不到所选游戏数据文件夹。", "所选游戏数据文件夹中缺少 Freedom.Exe。", "Freedom.Exe 不是受支持的 Freedom Fighters 版本。", "缺少必需的 Freedom Fighters 游戏数据。", "无法读取所选游戏数据。", "游戏数据验证未成功。", "使用以下方式重新启动：--data 路径", "技术详情", "正在验证游戏数据...", "正在准备启动..."}},
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
  const std::array<std::string_view, 1> platform_locales{{platform_locale}};
  return resolve(id, explicit_locale, platform_locales);
}

std::optional<std::string_view>
ProjectCatalog::resolve(MessageId id, std::string_view explicit_locale,
                        std::span<const std::string_view> platform_locales) const noexcept {
  const auto index = static_cast<std::size_t>(id);
  if (index >= message_id_count)
    return std::nullopt;
  const auto locale = *select_locale(explicit_locale, platform_locales);
  return messages_[static_cast<std::size_t>(locale)][index];
}

std::optional<std::string_view>
ProjectCatalog::format_seconds(MessageId id, unsigned seconds,
                               std::string_view explicit_locale,
                               std::string_view platform_locale) const {
  const std::array<std::string_view, 1> platform_locales{{platform_locale}};
  return format_seconds(id, seconds, explicit_locale, platform_locales);
}

std::optional<std::string_view>
ProjectCatalog::format_seconds(
    MessageId id, unsigned seconds, std::string_view explicit_locale,
    std::span<const std::string_view> platform_locales) const {
  const auto pattern = resolve(id, explicit_locale, platform_locales);
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
