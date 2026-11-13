/**
 * This is open-source software licensed under the terms of the MIT License.
 *
 * Copyright (c) 2026 Petr Červinka - FortSoft <cervinka@fortsoft.eu>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 **
 * Last modified for version 1.0.0.0
 */

#include "Localization.h"
#include <array>
#include <initializer_list>

/// Associates an XML field or existing application key with its named translation identifier.
struct LocalizedTextKey {
    std::string_view key;
    TextId id;
};

/// Builds one language at compile time and rejects missing, duplicate, empty, or invalid text entries.
static consteval LocalizedLanguage DefineLanguage(AppLanguage language,
        std::initializer_list<LocalizedText> entries) {
    if (entries.size() != TXT_COUNT) {
        throw "Every language must define every TextId exactly once.";
    }
    LocalizedLanguage result = { language, {} };
    for (const LocalizedText& entry : entries) {
        int id = static_cast<int>(entry.id);
        if (id < 0 || id >= TXT_COUNT) {
            throw "Invalid translation TextId.";
        }
        if (entry.text == nullptr || entry.text[0] == L'\0') {
            throw "A translation cannot be null or empty.";
        }
        if (result.texts[id] != nullptr) {
            throw "Duplicate translation TextId.";
        }
        result.texts[id] = entry.text;
    }
    return result;
}

/// Indexes complete languages by AppLanguage at compile time, independently of their source order.
/// Rejects missing, duplicate, or invalid language identifiers before any executable can be produced.
static consteval std::array<LocalizedLanguage, LANG_COUNT> DefineLocalization(
        std::initializer_list<LocalizedLanguage> languages) {
    if (languages.size() != LANG_COUNT) {
        throw "Every AppLanguage must have a complete translation table.";
    }
    std::array<LocalizedLanguage, LANG_COUNT> result = {};
    std::array<bool, LANG_COUNT> included = {};
    for (const LocalizedLanguage& language : languages) {
        int id = static_cast<int>(language.id);
        if (id < 0 || id >= LANG_COUNT) {
            throw "Invalid localization AppLanguage.";
        }
        if (included[id]) {
            throw "Duplicate localization AppLanguage.";
        }
        for (const wchar_t* text : language.texts) {
            if (text == nullptr || text[0] == L'\0') {
                throw "Every language must contain a nonempty translation for every TextId.";
            }
        }
        result[id] = language;
        included[id] = true;
    }
    return result;
}

constexpr std::initializer_list<LocalizedLanguage> languageDefinitions{
    DefineLanguage(LANG_CS, std::initializer_list<LocalizedText>{
        LocalizedText{ TXT_CURL_LOAD_OPERATION, L"načítání libcurl.dll" },
        LocalizedText{ TXT_FILE_OPEN_FAILED, L"Nelze otevřít %s. Zkontrolujte cestu a oprávnění." },
        LocalizedText{ TXT_INVALID_WINDOWS_DLL, L"Vybraný soubor není platná knihovna DLL systému Windows." },
        LocalizedText{ TXT_LIBRARY_DECOMPRESSION_FAILED, L"Dekomprese knihovny nebo kontrola CRC selhala." },
        LocalizedText{ TXT_LIBRARY_EXTRACTION_FAILED,
            L"Vloženou knihovnu nelze zapsat do AppData, profilu uživatele, Program Files ani dočasné složky."
        },
        LocalizedText{ TXT_LIBRARY_REQUIRES_X86,
            L"Prohlížeč Win32 vyžaduje 32bitovou %s. Vyberte knihovnu x86/i386."
        },
        LocalizedText{ TXT_LIBRARY_RESOURCE_INVALID, L"Prostředek vložené knihovny je neplatný." },
        LocalizedText{ TXT_BMP_EXPORT_FAILED,
            L"Obrázek BMP nelze uložit. Zkontrolujte cíl, volnou paměť a místo na disku."
        },
        LocalizedText{ TXT_BMP_FORMAT, L"Windows Bitmap BMP (*.bmp)" },
        LocalizedText{ TXT_GIF_EXPORT_FAILED,
            L"Export GIF selhal. Přesný výstup GIF vyžaduje nejvýše 256 barev."
        },
        LocalizedText{ TXT_GIF_FORMAT, L"CompuServe GIF 89a (*.gif)" },
        LocalizedText{ TXT_IMAGE_FORMAT_UNSUPPORTED, L"Vybraný formát obrázku není podporován." },
        LocalizedText{ TXT_JPEG_EXPORT_FAILED,
            L"Export JPEG selhal. Je nutný funkční kodér JPEG systému GDI+."
        },
        LocalizedText{ TXT_JPEG_FORMAT, L"JPEG File Interchange Format (*.jpg)" },
        LocalizedText{ TXT_PNG_EXPORT_FAILED,
            L"Obrázek PNG nelze uložit. Zkontrolujte cíl, volnou paměť a místo na disku."
        },
        LocalizedText{ TXT_PNG_FORMAT, L"Portable Network Graphics PNG (*.png)" },
        LocalizedText{ TXT_REPORT_IMAGE_CREATE_FAILED, L"Zobrazený obsah nelze převést na obrázek." },
        LocalizedText{ TXT_HTML_FORMAT, L"HyperText Markup Language 4.01 Strict (*.html)" },
        LocalizedText{ TXT_XHTML_FORMAT, L"Extensible HyperText Markup Language 1.1 (*.xhtml)" },
        LocalizedText{ TXT_ODT_FORMAT, L"OpenDocument Text (*.odt)" },
        LocalizedText{ TXT_DOCX_FORMAT, L"Office Open XML WordprocessingML Document (*.docx)" },
        LocalizedText{ TXT_RTF_FORMAT, L"Rich Text Format (*.rtf)" },
        LocalizedText{ TXT_TEXT_ENCODING_FAILED,
            L"Text nelze v tomto kódování uložit bez ztráty znaků. Zvolte jiné kódování textu."
        },
        LocalizedText{ TXT_TEXT_EXPORT_FAILED,
            L"Text nelze uložit. Zkontrolujte cíl a volné místo na disku."
        },
        LocalizedText{ TXT_TEXT_FORMAT, L"Textový dokument ANSI (*.txt)" },
        LocalizedText{ TXT_TEXT_UTF16_BE_FORMAT, L"Textový dokument UTF-16 BE BOM (*.txt)" },
        LocalizedText{ TXT_TEXT_UTF16_LE_FORMAT, L"Textový dokument UTF-16 LE BOM (*.txt)" },
        LocalizedText{ TXT_TEXT_UTF8_BOM_FORMAT, L"Textový dokument UTF-8 BOM (*.txt)" },
        LocalizedText{ TXT_TEXT_UTF8_FORMAT, L"Textový dokument UTF-8 (*.txt)" },
        LocalizedText{ TXT_TIFF_EXPORT_FAILED,
            L"Obrázek TIFF nelze uložit. Zkontrolujte cíl, volnou paměť a místo na disku."
        },
        LocalizedText{ TXT_TIFF_FORMAT, L"Tagged Image File Format TIFF (*.tif)" },
        LocalizedText{ TXT_WEBP_EXPORT_FAILED,
            L"Obrázek WebP nelze uložit. Zkontrolujte cíl, volnou paměť a místo na disku."
        },
        LocalizedText{ TXT_WEBP_FORMAT, L"Google WebP (*.webp)" },
        LocalizedText{ TXT_XML_FORMAT, L"Extensible Markup Language (*.xml)" },
        LocalizedText{ TXT_ZFO_FORMAT, L"Soubor ZFO (*.zfo)" },
        LocalizedText{ TXT_ABOUT, L"O programu" },
        LocalizedText{ TXT_ABOUT_COPY_INFORMATION_FAILED, L"Informace nelze zkopírovat do schránky." },
        LocalizedText{ TXT_ABOUT_COPY_URL_FAILED, L"Adresu URL nelze zkopírovat do schránky." },
        LocalizedText{ TXT_ABOUT_DESCRIPTION, L"Nativní prohlížeč datových zpráv a doručenek ISDS." },
        LocalizedText{ TXT_ABOUT_LICENSE_UNAVAILABLE, L"Prostředek s licencí není k dispozici." },
        LocalizedText{ TXT_ABOUT_PLATFORM_LABEL, L"Cílová platforma:" },
        LocalizedText{ TXT_ABOUT_VERSION_LABEL, L"Verze:" },
        LocalizedText{ TXT_ABOUT_VISIT_TOOLTIP, L"Navštívit web %ls" },
        LocalizedText{ TXT_ABOUT_WEBSITE_FAILED, L"Web nelze otevřít ve výchozím prohlížeči." },
        LocalizedText{ TXT_ABOUT_WEBSITE_LABEL, L"Web:" },
        LocalizedText{ TXT_APPLICATION_HELP,
            L"SOUBORY A KARTY\r\n"
            L"Zvolte Soubor > Otevřít (Ctrl+O) nebo Otevřít složku, přetáhněte ZFO do okna či na ikonu "
            L"aplikace nebo předejte názvy souborů na příkazovém řádku. Každá zpráva má vlastní kartu. "
            L"Ctrl+Tab a Ctrl+Shift+Tab přepínají karty. Ctrl+W nebo prostřední tlačítko kartu zavře. Soubor "
            L"> Zavřít vše zavře všechny karty. Nabídka Soubor obsahuje osm posledních souborů. Soubor > "
            L"Uložit kopii jako uloží původní ZFO beze změn včetně obálky a příloh. Aktuální karta zůstane u "
            L"původního souboru. Alt+F4 ukončí aplikaci.\r\n\r\n"
            L"ČTENÍ ZPRÁV\r\n"
            L"Přehled zobrazuje všechna dostupná metadata, včetně prázdných polí a rozšíření. Kliknutí na "
            L"název přílohy ji otevře v přidružené aplikaci; sousední odkaz Uložit uloží její kopii. V "
            L"přehledu funguje standardní výběr textu a Ctrl+C. Pravé tlačítko nad textem, kartou nebo "
            L"stavovým řádkem otevře příslušnou kontextovou nabídku. Každá karta rozbaluje otevírané přílohy "
            L"pod původními názvy do vlastního krátkého adresáře v Local AppData\\FortSoft\\ZfoViewer. Při "
            L"zavření karty se její dočasné soubory rekurzivně mažou. Zamčené soubory se přeskočí a úklid se "
            L"zopakuje při spuštění, otevírání příloh, zavírání karet a ukončení.\r\n\r\n"
            L"ZDROJOVÉ XML A EXPORT\r\n"
            L"Zobrazení > Barevné zvýraznění přepíná zvýraznění aktivní zprávy nebo XML zdroje. Volba je "
            L"propojená s Předvolbami a zapamatuje se. Zobrazení > Zobrazit zdroj XML (Ctrl+U) otevře "
            L"odsazené XML v nové kartě. Na zdrojové kartě je tento příkaz neaktivní. Opakovaný příkaz "
            L"vybere existující zdrojovou kartu. Soubor > Exportovat (Ctrl+E) nabízí ANSI, UTF-8, UTF-8 s "
            L"BOM, UTF-16 BE, UTF-16 LE, ODT, DOCX, RTF, HTML 4.01 Strict, XHTML 1.1, XML, BMP, GIF, JPEG, "
            L"PNG, TIFF a WebP. ODT a DOCX nevyžadují instalaci Office. Export XML používá stejné odsazení "
            L"jako zdrojová karta.\r\n\r\n"
            L"HLEDÁNÍ\r\n"
            L"Úpravy > Najít (Ctrl+F) hledá ve zprávě, zdrojovém XML, nápovědě nebo licenci v okně O "
            L"programu. F3 najde další výskyt, Shift+F3 předchozí. Volby zahrnují rozlišování velikosti "
            L"písmen, hledání zpět, hranice řádků a regulární výrazy ECMAScript s víceřádkovým "
            L"hledáním.\r\n\r\n"
            L"TISK\r\n"
            L"Soubor > Vzhled stránky mění okraje a nastavení tiskárny. Soubor > Tisk (Ctrl+P) tiskne text "
            L"aktivní zprávy, zdrojového XML, nápovědy nebo okna O programu. Okno O programu tiskne licenci. "
            L"Windows 11 používá moderní systémové tiskové rozhraní, starší systémy klasický tiskový "
            L"dialog.\r\n\r\n"
            L"PŘEDVOLBY\r\n"
            L"První dva řádky nastavují písmo zprávy a XML zdroje. Výchozí obnoví systémové písmo zprávy a "
            L"Consolas pro zdroj. Písmo rozhraní, nápovědy ani licence se nemění. Obě zvýraznění jsou "
            L"výchozí zapnutá a přenášejí se i do exportů. U zprávy mění pouze barvu nadpisů sekcí; XML "
            L"používá styl Firefoxu včetně tučných názvů značek a atributů. Možnosti > Předvolby (Ctrl+G) "
            L"obsahují češtinu, slovenštinu a americkou angličtinu. První spuštění vybere jazyk rozhraní "
            L"Windows, pro ostatní jazyky americkou angličtinu. Ručně vybraný jazyk se zapamatuje. Další "
            L"volby jsou délka názvu karty (výchozí 80), délka názvu v záhlaví (výchozí 40), zavírání "
            L"dvojklikem a povolení více instancí. Plynulé posouvání je ve výchozím stavu vypnuté; běžné "
            L"kolečko myši posune zprávu či zdrojové XML ihned podle systémové volby řádků nebo stránek. "
            L"Vymazat odstraní poslední soubory ihned. OK a Použít ukládají změny, Zrušit zahodí změny od "
            L"posledního Použít. Výchozí je jediná instance.\r\n\r\n"
            L"PODPISY\r\n"
            L"Prohlížeč místně kontroluje kryptografický podpis obálky CMS. Neověřuje důvěryhodnost "
            L"certifikátů, jejich odvolání, časová razítka ani dlouhodobou platnost podpisu. F1 otevře "
            L"nápovědu."
        },
        LocalizedText{ TXT_HELP, L"Nápověda" },
        LocalizedText{ TXT_OPEN_IN_BROWSER, L"&Otevřít ve výchozím prohlížeči" },
        LocalizedText{ TXT_ABOUT_TITLE, L"O aplikaci %s" },
        LocalizedText{ TXT_CLOSE, L"&Zavřít" },
        LocalizedText{ TXT_COPY_INFORMATION, L"Kopírovat informace" },
        LocalizedText{ TXT_COPY_URL, L"Kopírovat odkaz" },
        LocalizedText{ TXT_FIND_BACKWARD, L"Hledat zpět" },
        LocalizedText{ TXT_FIND_ENDS_WITH, L"Konec řádku" },
        LocalizedText{ TXT_FIND_FAILED, L"Hledání se nepodařilo dokončit." },
        LocalizedText{ TXT_FIND_MATCH_CASE, L"Rozlišovat velikost písmen" },
        LocalizedText{ TXT_FIND_NEXT, L"Najít další" },
        LocalizedText{ TXT_FIND_NOT_FOUND, L"Nebyla nalezena žádná shoda." },
        LocalizedText{ TXT_FIND_REGEX, L"Regulární výraz" },
        LocalizedText{ TXT_FIND_REGEX_ERROR, L"Regulární výraz: %s" },
        LocalizedText{ TXT_FIND_STARTS_WITH, L"Začátek řádku" },
        LocalizedText{ TXT_FIND_TITLE, L"ZFO Viewer — Hledání" },
        LocalizedText{ TXT_FIND_TOPMOST, L"Vždy navrchu" },
        LocalizedText{ TXT_FIND_WHAT, L"Hledat:" },
        LocalizedText{ TXT_HELP_TITLE, L"ZFO Viewer — Nápověda" },
        LocalizedText{ TXT_PAGE_SETUP_FAILED, L"Nelze otevřít dialog Vzhled stránky." },
        LocalizedText{ TXT_PLATFORM, L"Platforma:" },
        LocalizedText{ TXT_PRINT_FAILED, L"Dokument se nepodařilo vytisknout." },
        LocalizedText{ TXT_COPY_CONTEXT, L"&Kopírovat" },
        LocalizedText{ TXT_CLOSE_TAB_CONTEXT, L"Zavřít &kartu" },
        LocalizedText{ TXT_SHOW_IN_FOLDER, L"Zo&brazit ve složce" },
        LocalizedText{ TXT_COLOR_HIGHLIGHTING, L"&Barevné zvýraznění" },
        LocalizedText{ TXT_XML_SOURCE, L"Zobrazit &zdroj XML\tCtrl+U" },
        LocalizedText{ TXT_SOURCE_PREFIX, L"Zdroj XML: " },
        LocalizedText{ TXT_OK, L"OK" },
        LocalizedText{ TXT_CANCEL, L"Zrušit" },
        LocalizedText{ TXT_APPLY, L"P&oužít" },
        LocalizedText{ TXT_CLOSE_ON_DOUBLE_CLICK, L"Zavírat karty &dvojklikem" },
        LocalizedText{ TXT_TITLE_NAME_LENGTH_INVALID, L"Délka názvu v záhlaví musí být mezi 8 a 255." },
        LocalizedText{ TXT_TITLE_NAME_LENGTH, L"Délka názvu záhla&ví:" },
        LocalizedText{ TXT_TAB_NAME_LENGTH_INVALID, L"Délka názvu karty musí být mezi 8 a 255." },
        LocalizedText{ TXT_CLEAR_RECENT_FILES, L"Vyma&zat" },
        LocalizedText{ TXT_RECENT_FILES, L"Poslední soubory:" },
        LocalizedText{ TXT_TAB_NAME_LENGTH, L"Délka názvu &karty:" },
        LocalizedText{ TXT_COPY_FULL_PATH, L"Kopírovat název souboru včetně &cesty" },
        LocalizedText{ TXT_COPY_FILE_NAME, L"Kopírovat ná&zev souboru" },
        LocalizedText{ TXT_CLOSE_ALL, L"Zavřít &vše" },
        LocalizedText{ TXT_OPEN_FOLDER_TITLE, L"Otevřít složku" },
        LocalizedText{ TXT_OPEN_FOLDER, L"Otevřít s&ložku…" },
        LocalizedText{ TXT_APPLICATION_TITLE, L"Prohlížeč datových zpráv" },
        LocalizedText{ TXT_LANGUAGE_NAME, L"Čeština" },
        LocalizedText{ TXT_VIEW, L"&Zobrazení" },
        LocalizedText{ TXT_PREFERENCES, L"&Předvolby…\tCtrl+G" },
        LocalizedText{ TXT_PREFERENCES_TITLE, L"Předvolby" },
        LocalizedText{ TXT_MULTIPLE_INSTANCES, L"Povo&lit více instancí" },
        LocalizedText{ TXT_SMOOTH_SCROLLING, L"Plynulé po&souvání" },
        LocalizedText{ TXT_MESSAGE_FONT, L"Písmo zprá&vy:" },
        LocalizedText{ TXT_SOURCE_FONT, L"Písmo &XML zdroje:" },
        LocalizedText{ TXT_FONT_DEFAULT, L"Výc&hozí" },
        LocalizedText{ TXT_SYSTEM_DEFAULT_FONT_LABEL, L"Výchozí systémové" },
        LocalizedText{ TXT_FONT_REGULAR, L"Normální" },
        LocalizedText{ TXT_FONT_BOLD, L"Tučné" },
        LocalizedText{ TXT_FONT_ITALIC, L"Kurzíva" },
        LocalizedText{ TXT_FONT_BOLD_ITALIC, L"Tučné kurzíva" },
        LocalizedText{ TXT_FONT_FAILED, L"Písmo se nepodařilo nastavit." },
        LocalizedText{ TXT_HIGHLIGHT_MESSAGE, L"Barevné zvýraznění &zprávy" },
        LocalizedText{ TXT_HIGHLIGHT_SOURCE, L"Barevné zvýraznění XML z&droje" },
        LocalizedText{ TXT_LOWERCASE_EXTENSIONS, L"Malá písmena v &příponě" },
        LocalizedText{ TXT_SETTINGS_FAILED, L"Předvolby nelze uložit." },
        LocalizedText{ TXT_RICH_EDIT_FAILED, L"Systémový RichEdit se nepodařilo načíst." },
        LocalizedText{ TXT_SENDER_SECTION, L"Odesílatel" },
        LocalizedText{ TXT_RECIPIENT_SECTION, L"Adresát" },
        LocalizedText{ TXT_DELIVERY_SECTION, L"Doručení" },
        LocalizedText{ TXT_LEGAL_SECTION, L"Zmocnění" },
        LocalizedText{ TXT_SIGNATURE_SECTION, L"Podpis obálky" },
        LocalizedText{ TXT_SAVE_LINK, L"Uložit..." },
        LocalizedText{ TXT_BYTES, L"bajtů" },
        LocalizedText{ TXT_GENERAL_SECTION, L"Obecné" },
        LocalizedText{ TXT_SENDER_PERSON_SECTION, L"Odesílající osoba" },
        LocalizedText{ TXT_ADDITIONAL_SECTION, L"Doplňující údaje" },
        LocalizedText{ TXT_STATUS_SECTION, L"Stav zprávy" },
        LocalizedText{ TXT_EVENTS_SECTION, L"Události" },
        LocalizedText{ TXT_NOT_PROVIDED, L"Neuvedeno" },
        LocalizedText{ TXT_DM_ATTACHMENT_SIZE, L"Velikost příloh podle ISDS" },
        LocalizedText{ TXT_DM_TYPE, L"Typ zprávy" },
        LocalizedText{ TXT_DM_SENDER_TYPE, L"Typ datové schránky" },
        LocalizedText{ TXT_DM_RECIPIENT_TYPE, L"Typ datové schránky" },
        LocalizedText{ TXT_DM_SENDER_ORG_UNIT_NUM, L"Číslo organizační jednotky" },
        LocalizedText{ TXT_DM_RECIPIENT_ORG_UNIT_NUM, L"Číslo organizační jednotky" },
        LocalizedText{ TXT_DM_VODZ, L"Velkoobjemová datová zpráva" },
        LocalizedText{ TXT_ATTS_NUM, L"Uvedený počet příloh" },
        LocalizedText{ TXT_DM_HASH, L"Otisk zprávy" },
        LocalizedText{ TXT_ALGORITHM, L"Algoritmus" },
        LocalizedText{ TXT_DM_Q_TIMESTAMP, L"Kvalifikované časové razítko" },
        LocalizedText{ TXT_DM_EVENT_TIME, L"Čas události" },
        LocalizedText{ TXT_DM_EVENT_DESCR, L"Popis události" },
        LocalizedText{ TXT_DM_FILE_DESCR, L"Název souboru uložený v ZFO" },
        LocalizedText{ TXT_DM_MIME_TYPE, L"MIME type" },
        LocalizedText{ TXT_MIME_UNKNOWN, L"Neznámý formát" },
        LocalizedText{ TXT_MIME_BINARY, L"Binární data" },
        LocalizedText{ TXT_MIME_ENCRYPTED, L"Šifrovaný dokument" },
        LocalizedText{ TXT_MIME_PDF, L"Portable Document Format" },
        LocalizedText{ TXT_MIME_TEXT, L"Prostý text" },
        LocalizedText{ TXT_MIME_CSV, L"Comma-Separated Values" },
        LocalizedText{ TXT_MIME_RTF, L"Rich Text Format" },
        LocalizedText{ TXT_MIME_HTML, L"HyperText Markup Language" },
        LocalizedText{ TXT_MIME_XHTML, L"Extensible HyperText Markup Language" },
        LocalizedText{ TXT_MIME_XML, L"Extensible Markup Language" },
        LocalizedText{ TXT_MIME_JSON, L"JavaScript Object Notation" },
        LocalizedText{ TXT_MIME_DOC, L"Microsoft Word Document" },
        LocalizedText{ TXT_MIME_DOCX, L"Office Open XML WordprocessingML Document" },
        LocalizedText{ TXT_MIME_XLS, L"Microsoft Excel Workbook" },
        LocalizedText{ TXT_MIME_XLSX, L"Office Open XML SpreadsheetML Workbook" },
        LocalizedText{ TXT_MIME_PPT, L"Microsoft PowerPoint Presentation" },
        LocalizedText{ TXT_MIME_PPTX, L"Office Open XML PresentationML Presentation" },
        LocalizedText{ TXT_MIME_ODT, L"OpenDocument Text" },
        LocalizedText{ TXT_MIME_ODS, L"OpenDocument Spreadsheet" },
        LocalizedText{ TXT_MIME_ODP, L"OpenDocument Presentation" },
        LocalizedText{ TXT_MIME_PNG, L"Portable Network Graphics" },
        LocalizedText{ TXT_MIME_GIF, L"CompuServe Graphics Interchange Format" },
        LocalizedText{ TXT_MIME_JPEG, L"Joint Photographic Experts Group" },
        LocalizedText{ TXT_MIME_TIFF, L"Tagged Image File Format" },
        LocalizedText{ TXT_MIME_HEIC, L"High Efficiency Image Container" },
        LocalizedText{ TXT_MIME_HEIC_SEQUENCE, L"High Efficiency Image Container Sequence" },
        LocalizedText{ TXT_MIME_HEIF, L"High Efficiency Image File Format" },
        LocalizedText{ TXT_MIME_HEIF_SEQUENCE, L"High Efficiency Image File Format Sequence" },
        LocalizedText{ TXT_MIME_BMP, L"Windows Bitmap" },
        LocalizedText{ TXT_MIME_WEBP, L"WebP" },
        LocalizedText{ TXT_MIME_SVG, L"Scalable Vector Graphics" },
        LocalizedText{ TXT_MIME_WAV, L"Waveform Audio File Format" },
        LocalizedText{ TXT_MIME_MPEG_AUDIO, L"MPEG Audio" },
        LocalizedText{ TXT_MIME_MP4_AUDIO, L"MPEG-4 Audio" },
        LocalizedText{ TXT_MIME_MPEG_VIDEO, L"MPEG Video" },
        LocalizedText{ TXT_MIME_MP4_VIDEO, L"MPEG-4 Video" },
        LocalizedText{ TXT_MIME_DWG, L"AutoCAD Drawing" },
        LocalizedText{ TXT_MIME_EDIFACT,
            L"Electronic Data Interchange for Administration, Commerce and Transport"
        },
        LocalizedText{ TXT_MIME_EDI_X12, L"ANSI ASC X12 Electronic Data Interchange" },
        LocalizedText{ TXT_MIME_EDI_CONSENT, L"Electronic Data Interchange Consent" },
        LocalizedText{ TXT_MIME_ISDOC, L"Information System Document" },
        LocalizedText{ TXT_MIME_ISDOCX, L"Information System Document Archive" },
        LocalizedText{ TXT_MIME_FO, L"Software602 Form Filler XML" },
        LocalizedText{ TXT_MIME_ZFO, L"Software602 Form Filler ZFO" },
        LocalizedText{ TXT_MIME_ZIP, L"ZIP Archive" },
        LocalizedText{ TXT_MIME_ASIC_E, L"Associated Signature Container Extended" },
        LocalizedText{ TXT_MIME_ASIC_S, L"Associated Signature Container Simple" },
        LocalizedText{ TXT_MIME_PKCS7, L"PKCS #7 Cryptographic Message Syntax" },
        LocalizedText{ TXT_MIME_PKCS7_SIGNATURE, L"PKCS #7 Signature" },
        LocalizedText{ TXT_MIME_PKCS7_CERTIFICATES, L"PKCS #7 Certificates" },
        LocalizedText{ TXT_MIME_X509, L"X.509 Certificate" },
        LocalizedText{ TXT_MIME_TIMESTAMP, L"Odpověď časového razítka" },
        LocalizedText{ TXT_DM_FILE_META_TYPE, L"Druh přílohy" },
        LocalizedText{ TXT_DM_FILE_GUID, L"Identifikátor souboru" },
        LocalizedText{ TXT_DM_UP_FILE_GUID, L"Identifikátor nadřazeného souboru" },
        LocalizedText{ TXT_DM_FILE_SIZE, L"Velikost souboru" },
        LocalizedText{ TXT_USER_TYPE, L"Typ uživatele" },
        LocalizedText{ TXT_DM_USER_TYPE, L"Typ uživatele" },
        LocalizedText{ TXT_USER_NAME, L"Uživatelské jméno" },
        LocalizedText{ TXT_DM_USER_NAME, L"Uživatelské jméno" },
        LocalizedText{ TXT_PN_FIRST_NAME, L"Jméno" },
        LocalizedText{ TXT_PN_MIDDLE_NAME, L"Další jméno" },
        LocalizedText{ TXT_PN_LAST_NAME, L"Příjmení" },
        LocalizedText{ TXT_PN_LAST_NAME_AT_BIRTH, L"Rodné příjmení" },
        LocalizedText{ TXT_FIRM_NAME, L"Název organizace" },
        LocalizedText{ TXT_BI_DATE, L"Datum narození" },
        LocalizedText{ TXT_BI_CITY, L"Místo narození" },
        LocalizedText{ TXT_BI_COUNTY, L"Okres narození" },
        LocalizedText{ TXT_BI_STATE, L"Stát narození" },
        LocalizedText{ TXT_AD_CITY, L"Obec" },
        LocalizedText{ TXT_AD_STREET, L"Ulice" },
        LocalizedText{ TXT_AD_NUMBER_IN_STREET, L"Číslo orientační" },
        LocalizedText{ TXT_AD_NUMBER_IN_MUNICIPALITY, L"Číslo popisné" },
        LocalizedText{ TXT_AD_ZIP_CODE, L"PSČ" },
        LocalizedText{ TXT_AD_STATE, L"Stát" },
        LocalizedText{ TXT_IC, L"IČO" },
        LocalizedText{ TXT_DM_STATUS_CODE, L"Stavový kód odpovědi" },
        LocalizedText{ TXT_DM_STATUS_MESSAGE, L"Popis stavu odpovědi" },
        LocalizedText{ TXT_FILE_ROLE_MAIN, L"Hlavní příloha" },
        LocalizedText{ TXT_FILE_ROLE_ENCLOSURE, L"Příloha" },
        LocalizedText{ TXT_FILE_ROLE_SIGNATURE, L"Podpis" },
        LocalizedText{ TXT_BOX_TYPE_0, L"Systémová schránka" },
        LocalizedText{ TXT_BOX_TYPE_10, L"Orgán veřejné moci" },
        LocalizedText{ TXT_BOX_TYPE_20, L"Právnická osoba" },
        LocalizedText{ TXT_BOX_TYPE_30, L"Podnikající fyzická osoba" },
        LocalizedText{ TXT_BOX_TYPE_40, L"Fyzická osoba" },
        LocalizedText{ TXT_MESSAGE_TYPE_V, L"Veřejná zpráva; jedna ze stran je orgán veřejné moci" },
        LocalizedText{ TXT_MESSAGE_TYPE_K, L"Poštovní datová zpráva" },
        LocalizedText{ TXT_MESSAGE_TYPE_I, L"Poštovní datová zpráva umožňující předplacenou odpověď" },
        LocalizedText{ TXT_MESSAGE_TYPE_O, L"Odpověď hrazená odesílatelem původní zprávy" },
        LocalizedText{ TXT_MESSAGE_TYPE_E, L"Poštovní datová zpráva hrazená z kreditu" },
        LocalizedText{ TXT_MESSAGE_TYPE_A, L"Dotovaná zpráva s předplacenou odpovědí" },
        LocalizedText{ TXT_MESSAGE_TYPE_B, L"Dotovaná zpráva; předplacená odpověď využita" },
        LocalizedText{ TXT_MESSAGE_TYPE_C, L"Dotovaná zpráva; možnost odpovědi vypršela" },
        LocalizedText{ TXT_MESSAGE_TYPE_G, L"Poštovní datová zpráva hrazená sponzorem" },
        LocalizedText{ TXT_MESSAGE_TYPE_X, L"Iniciační zpráva; možnost odpovědi vypršela" },
        LocalizedText{ TXT_MESSAGE_TYPE_Y, L"Iniciační zpráva; předplacená odpověď využita" },
        LocalizedText{ TXT_STATE_1, L"Zpráva byla podána do ISDS" },
        LocalizedText{ TXT_STATE_2, L"Zpráva včetně příloh byla opatřena časovým razítkem" },
        LocalizedText{ TXT_STATE_3, L"Antivirová kontrola selhala; nakažená příloha byla odstraněna" },
        LocalizedText{ TXT_STATE_4, L"Zpráva byla dodána do schránky příjemce" },
        LocalizedText{ TXT_STATE_5, L"Uplynula lhůta pro doručení fikcí" },
        LocalizedText{ TXT_STATE_6, L"Oprávněná osoba se přihlásila; zpráva byla doručena" },
        LocalizedText{ TXT_STATE_7, L"Zpráva byla přečtena" },
        LocalizedText{ TXT_STATE_8, L"Nedoručitelná zpráva; schránka příjemce byla znepřístupněna" },
        LocalizedText{ TXT_STATE_9, L"Obsah zprávy byl smazán; obálka byla archivována" },
        LocalizedText{ TXT_STATE_10, L"Zpráva je v datovém trezoru" },
        LocalizedText{ TXT_USER_TYPE_PRIMARY_USER, L"Oprávněná osoba" },
        LocalizedText{ TXT_USER_TYPE_ENTRUSTED_USER, L"Pověřená osoba" },
        LocalizedText{ TXT_USER_TYPE_ADMINISTRATOR, L"Administrátor" },
        LocalizedText{ TXT_USER_TYPE_OFFICIAL, L"Úřední osoba" },
        LocalizedText{ TXT_INSTANCE_FAILED, L"Nepodařilo se získat zámek spuštění aplikace." },
        LocalizedText{ TXT_FORWARD_FAILED, L"Běžící aplikace nepřijala soubory." },
        LocalizedText{ TXT_OPTIONS, L"&Možnosti" },
        LocalizedText{ TXT_EDIT, L"&Úpravy" },
        LocalizedText{ TXT_SELECT_ALL, L"Vybrat &vše\tCtrl+A" },
        LocalizedText{ TXT_FIND_MENU, L"&Najít…\tCtrl+F" },
        LocalizedText{ TXT_FIND_NEXT_MENU, L"Najít &další\tF3" },
        LocalizedText{ TXT_FIND_PREVIOUS_MENU, L"Najít &předchozí\tShift+F3" },
        LocalizedText{ TXT_PRINT, L"&Tisk…\tCtrl+P" },
        LocalizedText{ TXT_PAGE_SETUP, L"Vz&hled stránky…" },
        LocalizedText{ TXT_REGEX_INVALID, L"Neplatný regulární výraz." },
        LocalizedText{ TXT_REGEX_ESCAPE, L"Neplatná řídicí sekvence." },
        LocalizedText{ TXT_REGEX_BRACKET, L"Chybějící nebo nadbytečná hranatá závorka." },
        LocalizedText{ TXT_REGEX_PARENTHESIS, L"Chybějící nebo nadbytečná kulatá závorka." },
        LocalizedText{ TXT_REGEX_REPEAT, L"Neplatný zápis opakování." },
        LocalizedText{ TXT_REGEX_RANGE, L"Neplatný rozsah znaků." },
        LocalizedText{ TXT_REGEX_COMPLEXITY, L"Výraz je pro vyhodnocení příliš složitý." },
        LocalizedText{ TXT_LANGUAGE_LABEL, L"&Jazyk:" },
        LocalizedText{ TXT_FILE, L"&Soubor" },
        LocalizedText{ TXT_OPEN, L"&Otevřít...\tCtrl+O" },
        LocalizedText{ TXT_EXPORT, L"&Exportovat…\tCtrl+E" },
        LocalizedText{ TXT_CLOSE_FILE, L"&Zavřít\tCtrl+W" },
        LocalizedText{ TXT_SAVE_COPY_AS, L"&Uložit kopii jako…" },
        LocalizedText{ TXT_SAVE_COPY_AS_TITLE, L"Uložit kopii jako" },
        LocalizedText{ TXT_EXIT, L"&Konec\tAlt+F4" },
        LocalizedText{ TXT_ATTACHMENT, L"&Příloha" },
        LocalizedText{ TXT_OPEN_ATTACHMENT, L"&Otevřít přílohu\tEnter" },
        LocalizedText{ TXT_SAVE_ATTACHMENT, L"&Uložit přílohu...\tCtrl+S" },
        LocalizedText{ TXT_COPY, L"&Kopírovat\tCtrl+C" },
        LocalizedText{ TXT_LANGUAGE, L"&Jazyk" },
        LocalizedText{ TXT_HELP_MENU, L"&Nápověda" },
        LocalizedText{ TXT_HELP_TEXT, L"&Nápověda\tF1" },
        LocalizedText{ TXT_ABOUT_TEXT, L"&O programu" },
        LocalizedText{ TXT_ABOUT_WINDOW_TITLE, L"O aplikaci ZFO Viewer" },
        LocalizedText{ TXT_MESSAGE, L"Zpráva" },
        LocalizedText{ TXT_ATTACHMENTS, L"Přílohy" },
        LocalizedText{ TXT_XML, L"Zdroj XML" },
        LocalizedText{ TXT_FIELD, L"Údaj" },
        LocalizedText{ TXT_VALUE, L"Hodnota" },
        LocalizedText{ TXT_NAME, L"Název souboru" },
        LocalizedText{ TXT_SIZE, L"Velikost (bajty)" },
        LocalizedText{ TXT_TYPE, L"Typ obsahu" },
        LocalizedText{ TXT_READY, L"Připraven" },
        LocalizedText{ TXT_NO_ATTACHMENTS, L"Zpráva neobsahuje vložené přílohy." },
        LocalizedText{ TXT_YES, L"Ano" },
        LocalizedText{ TXT_NO, L"Ne" },
        LocalizedText{ TXT_DM_ID, L"ID zprávy" },
        LocalizedText{ TXT_DM_ANNOTATION, L"Předmět" },
        LocalizedText{ TXT_DM_SENDER, L"Jméno" },
        LocalizedText{ TXT_DB_ID_SENDER, L"ID schránky" },
        LocalizedText{ TXT_DM_SENDER_ADDRESS, L"Adresa" },
        LocalizedText{ TXT_DM_SENDER_ORG_UNIT, L"Organizační jednotka" },
        LocalizedText{ TXT_DM_RECIPIENT, L"Jméno" },
        LocalizedText{ TXT_DB_ID_RECIPIENT, L"ID schránky" },
        LocalizedText{ TXT_DM_RECIPIENT_ADDRESS, L"Adresa" },
        LocalizedText{ TXT_DM_RECIPIENT_ORG_UNIT, L"Organizační jednotka" },
        LocalizedText{ TXT_DM_DELIVERY_TIME, L"Čas dodání" },
        LocalizedText{ TXT_DM_ACCEPTANCE_TIME, L"Čas doručení" },
        LocalizedText{ TXT_CENTRAL_EUROPE_TIME, L"SEČ" },
        LocalizedText{ TXT_CENTRAL_EUROPE_SUMMER_TIME, L"SELČ" },
        LocalizedText{ TXT_DM_MESSAGE_STATUS, L"Stav zprávy" },
        LocalizedText{ TXT_DM_SENDER_REF_NUMBER, L"Naše číslo jednací" },
        LocalizedText{ TXT_DM_SENDER_IDENT, L"Naše spisová značka" },
        LocalizedText{ TXT_DM_RECIPIENT_REF_NUMBER, L"Vaše číslo jednací" },
        LocalizedText{ TXT_DM_RECIPIENT_IDENT, L"Vaše spisová značka" },
        LocalizedText{ TXT_DM_TO_HANDS, L"K rukám" },
        LocalizedText{ TXT_DM_PERSONAL_DELIVERY, L"Do vlastních rukou" },
        LocalizedText{ TXT_DM_ALLOW_SUBST_DELIVERY, L"Zakázáno doručení fikcí" },
        LocalizedText{ TXT_DM_LEGAL_TITLE, L"Zmocnění" },
        LocalizedText{ TXT_LEGAL_TITLE_PARAGRAPH, L"odstavec" },
        LocalizedText{ TXT_LEGAL_TITLE_LETTER, L"písmeno" },
        LocalizedText{ TXT_DM_LEGAL_TITLE_LAW, L"Zmocnění — zákon" },
        LocalizedText{ TXT_DM_LEGAL_TITLE_YEAR, L"Zmocnění — rok" },
        LocalizedText{ TXT_DM_LEGAL_TITLE_SECT, L"Zmocnění — paragraf" },
        LocalizedText{ TXT_DM_LEGAL_TITLE_PAR, L"Zmocnění — odstavec" },
        LocalizedText{ TXT_DM_LEGAL_TITLE_POINT, L"Zmocnění — písmeno" },
        LocalizedText{ TXT_SIGNATURE, L"Podpis obálky" },
        LocalizedText{ TXT_SIGNATURE_VALID, L"Kryptografický podpis je v pořádku." },
        LocalizedText{ TXT_SIGNATURE_INVALID, L"Kryptografický podpis se nepodařilo ověřit." },
        LocalizedText{ TXT_SIGNER, L"Podepsal" },
        LocalizedText{ TXT_TRUST, L"Důvěryhodnost podpisu" },
        LocalizedText{ TXT_TRUST_INFO, L"Řetězec certifikátů, odvolání a časová razítka nejsou ověřovány." },
        LocalizedText{ TXT_EVENT, L"Událost doručení" },
        LocalizedText{ TXT_INVALID_ZFO, L"Soubor není čitelná zpráva ZFO ve formátu CMS/PKCS#7." },
        LocalizedText{ TXT_INVALID_XML, L"Soubor ZFO obsahuje neplatné nebo nepodporované XML ISDS." },
        LocalizedText{ TXT_DECODE_FAILED, L"Obálku ZFO se nepodařilo dekódovat." },
        LocalizedText{ TXT_EXTERNAL_ATTACHMENT,
            L"Příloha nemá vložený obsah. Externí přílohy nejsou podporovány."
        },
        LocalizedText{ TXT_INVALID_BASE64, L"Příloha obsahuje neplatná data Base64." },
        LocalizedText{ TXT_SAVE_FAILED,
            L"Soubor nelze uložit. Ověřte cestu, oprávnění a volné místo na disku."
        },
        LocalizedText{ TXT_OPEN_FAILED, L"Přílohu nelze otevřít v přidružené aplikaci." },
        LocalizedText{ TXT_OPEN_FOLDER_FAILED, L"Zdrojovou složku nelze otevřít." },
        LocalizedText{ TXT_UNEXPECTED_ERROR, L"Operaci se nepodařilo dokončit." },
        LocalizedText{ TXT_ONE_FILE, L"Přetáhněte vždy jeden soubor ZFO." },
    }),
    DefineLanguage(LANG_SK, std::initializer_list<LocalizedText>{
        LocalizedText{ TXT_CURL_LOAD_OPERATION, L"načítavaní libcurl.dll" },
        LocalizedText{ TXT_FILE_OPEN_FAILED, L"Nemožno otvoriť %s. Skontrolujte cestu a oprávnenia." },
        LocalizedText{ TXT_INVALID_WINDOWS_DLL,
            L"Vybraný súbor nie je platná knižnica DLL systému Windows."
        },
        LocalizedText{ TXT_LIBRARY_DECOMPRESSION_FAILED,
            L"Dekompresia knižnice alebo kontrola CRC zlyhala."
        },
        LocalizedText{ TXT_LIBRARY_EXTRACTION_FAILED,
            L"Vloženú knižnicu nemožno zapísať do AppData, profilu používateľa, Program Files ani dočasného "
            L"priečinka."
        },
        LocalizedText{ TXT_LIBRARY_REQUIRES_X86,
            L"Prehliadač Win32 vyžaduje 32-bitovú %s. Vyberte knižnicu x86/i386."
        },
        LocalizedText{ TXT_LIBRARY_RESOURCE_INVALID, L"Prostriedok vloženej knižnice je neplatný." },
        LocalizedText{ TXT_BMP_EXPORT_FAILED,
            L"Obrázok BMP nemožno uložiť. Skontrolujte cieľ, voľnú pamäť a miesto na disku."
        },
        LocalizedText{ TXT_BMP_FORMAT, L"Windows Bitmap BMP (*.bmp)" },
        LocalizedText{ TXT_GIF_EXPORT_FAILED,
            L"Export GIF zlyhal. Presný výstup GIF vyžaduje najviac 256 farieb."
        },
        LocalizedText{ TXT_GIF_FORMAT, L"CompuServe GIF 89a (*.gif)" },
        LocalizedText{ TXT_IMAGE_FORMAT_UNSUPPORTED, L"Vybraný formát obrázka nie je podporovaný." },
        LocalizedText{ TXT_JPEG_EXPORT_FAILED,
            L"Export JPEG zlyhal. Vyžaduje sa funkčný kodér JPEG systému GDI+."
        },
        LocalizedText{ TXT_JPEG_FORMAT, L"JPEG File Interchange Format (*.jpg)" },
        LocalizedText{ TXT_PNG_EXPORT_FAILED,
            L"Obrázok PNG nemožno uložiť. Skontrolujte cieľ, voľnú pamäť a miesto na disku."
        },
        LocalizedText{ TXT_PNG_FORMAT, L"Portable Network Graphics PNG (*.png)" },
        LocalizedText{ TXT_REPORT_IMAGE_CREATE_FAILED, L"Zobrazený obsah nemožno previesť na obrázok." },
        LocalizedText{ TXT_HTML_FORMAT, L"HyperText Markup Language 4.01 Strict (*.html)" },
        LocalizedText{ TXT_XHTML_FORMAT, L"Extensible HyperText Markup Language 1.1 (*.xhtml)" },
        LocalizedText{ TXT_ODT_FORMAT, L"OpenDocument Text (*.odt)" },
        LocalizedText{ TXT_DOCX_FORMAT, L"Office Open XML WordprocessingML Document (*.docx)" },
        LocalizedText{ TXT_RTF_FORMAT, L"Rich Text Format (*.rtf)" },
        LocalizedText{ TXT_TEXT_ENCODING_FAILED,
            L"Text nemožno v tomto kódovaní uložiť bez straty znakov. Vyberte iné kódovanie textu."
        },
        LocalizedText{ TXT_TEXT_EXPORT_FAILED,
            L"Text nemožno uložiť. Skontrolujte cieľ a voľné miesto na disku."
        },
        LocalizedText{ TXT_TEXT_FORMAT, L"Textový dokument ANSI (*.txt)" },
        LocalizedText{ TXT_TEXT_UTF16_BE_FORMAT, L"Textový dokument UTF-16 BE BOM (*.txt)" },
        LocalizedText{ TXT_TEXT_UTF16_LE_FORMAT, L"Textový dokument UTF-16 LE BOM (*.txt)" },
        LocalizedText{ TXT_TEXT_UTF8_BOM_FORMAT, L"Textový dokument UTF-8 BOM (*.txt)" },
        LocalizedText{ TXT_TEXT_UTF8_FORMAT, L"Textový dokument UTF-8 (*.txt)" },
        LocalizedText{ TXT_TIFF_EXPORT_FAILED,
            L"Obrázok TIFF nemožno uložiť. Skontrolujte cieľ, voľnú pamäť a miesto na disku."
        },
        LocalizedText{ TXT_TIFF_FORMAT, L"Tagged Image File Format TIFF (*.tif)" },
        LocalizedText{ TXT_WEBP_EXPORT_FAILED,
            L"Obrázok WebP nemožno uložiť. Skontrolujte cieľ, voľnú pamäť a miesto na disku."
        },
        LocalizedText{ TXT_WEBP_FORMAT, L"Google WebP (*.webp)" },
        LocalizedText{ TXT_XML_FORMAT, L"Extensible Markup Language (*.xml)" },
        LocalizedText{ TXT_ZFO_FORMAT, L"Súbor ZFO (*.zfo)" },
        LocalizedText{ TXT_ABOUT, L"O programe" },
        LocalizedText{ TXT_ABOUT_COPY_INFORMATION_FAILED, L"Informácie nemožno skopírovať do schránky." },
        LocalizedText{ TXT_ABOUT_COPY_URL_FAILED, L"Adresu URL nemožno skopírovať do schránky." },
        LocalizedText{ TXT_ABOUT_DESCRIPTION, L"Natívny prehliadač dátových správ a doručeniek ISDS." },
        LocalizedText{ TXT_ABOUT_LICENSE_UNAVAILABLE, L"Prostriedok s licenciou nie je k dispozícii." },
        LocalizedText{ TXT_ABOUT_PLATFORM_LABEL, L"Cieľová platforma:" },
        LocalizedText{ TXT_ABOUT_VERSION_LABEL, L"Verzia:" },
        LocalizedText{ TXT_ABOUT_VISIT_TOOLTIP, L"Navštíviť web %ls" },
        LocalizedText{ TXT_ABOUT_WEBSITE_FAILED, L"Web nemožno otvoriť v predvolenom prehliadači." },
        LocalizedText{ TXT_ABOUT_WEBSITE_LABEL, L"Web:" },
        LocalizedText{ TXT_APPLICATION_HELP,
            L"SÚBORY A KARTY\r\n"
            L"Zvoľte Súbor > Otvoriť (Ctrl+O) alebo Otvoriť priečinok, presuňte ZFO do okna či na ikonu "
            L"aplikácie alebo zadajte názvy súborov na príkazovom riadku. Každá správa má vlastnú kartu. "
            L"Ctrl+Tab a Ctrl+Shift+Tab prepínajú karty. Ctrl+W alebo prostredné tlačidlo kartu zatvorí. "
            L"Súbor > Zavrieť všetko zatvorí všetky karty. Ponuka Súbor obsahuje osem posledných súborov. "
            L"Súbor > Uložiť kópiu ako uloží pôvodné ZFO bez zmien vrátane obálky a príloh. Aktuálna karta "
            L"zostane pri pôvodnom súbore. Alt+F4 ukončí aplikáciu.\r\n\r\n"
            L"ČÍTANIE SPRÁV\r\n"
            L"Prehľad zobrazuje všetky dostupné metadáta vrátane prázdnych polí a rozšírení. Kliknutie na "
            L"názov prílohy ju otvorí v priradenej aplikácii; susedný odkaz Uložiť uloží jej kópiu. V "
            L"prehľade funguje štandardný výber textu a Ctrl+C. Pravé tlačidlo nad textom, kartou alebo "
            L"stavovým riadkom otvorí príslušnú kontextovú ponuku. Každá karta rozbaľuje otvárané prílohy "
            L"pod pôvodnými názvami do vlastného krátkeho priečinka v Local AppData\\FortSoft\\ZfoViewer. "
            L"Pri zatvorení karty sa jej dočasné súbory rekurzívne mažú. Zamknuté súbory sa preskočia a "
            L"čistenie sa zopakuje pri spustení, otváraní príloh, zatváraní kariet a ukončení.\r\n\r\n"
            L"ZDROJOVÉ XML A EXPORT\r\n"
            L"Zobrazenie > Farebné zvýraznenie prepína zvýraznenie aktívnej správy alebo XML zdroja. Voľba "
            L"je prepojená s Predvoľbami a zapamätá sa. Zobrazenie > Zobraziť zdroj XML (Ctrl+U) otvorí "
            L"odsadené XML v novej karte. Na zdrojovej karte je tento príkaz neaktívny. Opakovaný príkaz "
            L"vyberie existujúcu zdrojovú kartu. Súbor > Exportovať (Ctrl+E) ponúka ANSI, UTF-8, UTF-8 s "
            L"BOM, UTF-16 BE, UTF-16 LE, ODT, DOCX, RTF, HTML 4.01 Strict, XHTML 1.1, XML, BMP, GIF, JPEG, "
            L"PNG, TIFF a WebP. ODT a DOCX nevyžadujú inštaláciu Office. Export XML používa rovnaké "
            L"odsadenie ako zdrojová karta.\r\n\r\n"
            L"HĽADANIE\r\n"
            L"Úpravy > Hľadať (Ctrl+F) hľadá v správe, zdrojovom XML, pomocníkovi alebo licencii v okne O "
            L"programe. F3 nájde ďalší výskyt, Shift+F3 predchádzajúci. Voľby zahŕňajú rozlišovanie veľkosti "
            L"písmen, hľadanie späť, hranice riadkov a regulárne výrazy ECMAScript s viacriadkovým "
            L"hľadaním.\r\n\r\n"
            L"TLAČ\r\n"
            L"Súbor > Vzhľad strany mení okraje a nastavenia tlačiarne. Súbor > Tlačiť (Ctrl+P) tlačí text "
            L"aktívnej správy, zdrojového XML, pomocníka alebo okna O programe. Okno O programe tlačí "
            L"licenciu. Windows 11 používa moderné systémové tlačové rozhranie, staršie systémy klasický "
            L"tlačový dialóg.\r\n\r\n"
            L"PREDVOĽBY\r\n"
            L"Prvé dva riadky nastavujú písmo správy a XML zdroja. Predvolené obnoví systémové písmo správy "
            L"a Consolas pre zdroj. Písmo rozhrania, pomocníka ani licencie sa nemení. Obe zvýraznenia sú "
            L"predvolene zapnuté a prenášajú sa aj do exportov. Pri správe menia iba farbu nadpisov sekcií; "
            L"XML používa štýl Firefoxu vrátane tučných názvov značiek a atribútov. Možnosti > Predvoľby "
            L"(Ctrl+G) obsahujú češtinu, slovenčinu a americkú angličtinu. Prvé spustenie vyberie jazyk "
            L"rozhrania Windows, pre ostatné jazyky americkú angličtinu. Ručne vybraný jazyk sa zapamätá. "
            L"Ďalšie voľby sú dĺžka názvu karty (predvolená 80), dĺžka názvu v záhlaví (predvolená 40), "
            L"zatváranie dvojklikom a povolenie viacerých inštancií. Plynulé posúvanie je predvolene "
            L"vypnuté; bežné koliesko myši posunie správu či zdrojové XML ihneď podľa systémovej voľby "
            L"riadkov alebo strán. Vymazať odstráni posledné súbory ihneď. OK a Použiť ukladajú zmeny, "
            L"Zrušiť zahodí zmeny od posledného Použiť. Predvolená je jediná inštancia.\r\n\r\n"
            L"PODPISY\r\n"
            L"Prehliadač miestne kontroluje kryptografický podpis obálky CMS. Neoveruje dôveryhodnosť "
            L"certifikátov, ich odvolanie, časové pečiatky ani dlhodobú platnosť podpisu. F1 otvorí "
            L"pomocníka."
        },
        LocalizedText{ TXT_HELP, L"Pomoc" },
        LocalizedText{ TXT_OPEN_IN_BROWSER, L"&Otvoriť v predvolenom prehliadači" },
        LocalizedText{ TXT_ABOUT_TITLE, L"O aplikácii %s" },
        LocalizedText{ TXT_CLOSE, L"&Zavrieť" },
        LocalizedText{ TXT_COPY_INFORMATION, L"Kopírovať informácie" },
        LocalizedText{ TXT_COPY_URL, L"Kopírovať odkaz" },
        LocalizedText{ TXT_FIND_BACKWARD, L"Hľadať späť" },
        LocalizedText{ TXT_FIND_ENDS_WITH, L"Koniec riadka" },
        LocalizedText{ TXT_FIND_FAILED, L"Hľadanie sa nepodarilo dokončiť." },
        LocalizedText{ TXT_FIND_MATCH_CASE, L"Rozlišovať veľkosť písmen" },
        LocalizedText{ TXT_FIND_NEXT, L"Nájsť ďalší" },
        LocalizedText{ TXT_FIND_NOT_FOUND, L"Nebola nájdená žiadna zhoda." },
        LocalizedText{ TXT_FIND_REGEX, L"Regulárny výraz" },
        LocalizedText{ TXT_FIND_REGEX_ERROR, L"Regulárny výraz: %s" },
        LocalizedText{ TXT_FIND_STARTS_WITH, L"Začiatok riadka" },
        LocalizedText{ TXT_FIND_TITLE, L"ZFO Viewer — Hľadanie" },
        LocalizedText{ TXT_FIND_TOPMOST, L"Vždy navrchu" },
        LocalizedText{ TXT_FIND_WHAT, L"Hľadať:" },
        LocalizedText{ TXT_HELP_TITLE, L"ZFO Viewer — Pomocník" },
        LocalizedText{ TXT_PAGE_SETUP_FAILED, L"Vzhľad strany sa nepodarilo otvoriť." },
        LocalizedText{ TXT_PLATFORM, L"Platforma:" },
        LocalizedText{ TXT_PRINT_FAILED, L"Dokument sa nepodarilo vytlačiť." },
        LocalizedText{ TXT_COPY_CONTEXT, L"&Kopírovať" },
        LocalizedText{ TXT_CLOSE_TAB_CONTEXT, L"Zavrieť &kartu" },
        LocalizedText{ TXT_SHOW_IN_FOLDER, L"Zo&braziť v priečinku" },
        LocalizedText{ TXT_COLOR_HIGHLIGHTING, L"&Farebné zvýraznenie" },
        LocalizedText{ TXT_XML_SOURCE, L"Zobraziť &zdroj XML\tCtrl+U" },
        LocalizedText{ TXT_SOURCE_PREFIX, L"Zdroj XML: " },
        LocalizedText{ TXT_OK, L"OK" },
        LocalizedText{ TXT_CANCEL, L"Zrušiť" },
        LocalizedText{ TXT_APPLY, L"P&oužiť" },
        LocalizedText{ TXT_CLOSE_ON_DOUBLE_CLICK, L"Z&atvárať karty dvojklikom" },
        LocalizedText{ TXT_TITLE_NAME_LENGTH_INVALID, L"Dĺžka názvu v záhlaví musí byť medzi 8 a 255." },
        LocalizedText{ TXT_TITLE_NAME_LENGTH, L"Dĺžka &názvu záhlavia:" },
        LocalizedText{ TXT_TAB_NAME_LENGTH_INVALID, L"Dĺžka názvu karty musí byť medzi 8 a 255." },
        LocalizedText{ TXT_CLEAR_RECENT_FILES, L"Vyma&zať" },
        LocalizedText{ TXT_RECENT_FILES, L"Posledné súbory:" },
        LocalizedText{ TXT_TAB_NAME_LENGTH, L"Dĺžka názvu &karty:" },
        LocalizedText{ TXT_COPY_FULL_PATH, L"Kopírovať úplnú &cestu" },
        LocalizedText{ TXT_COPY_FILE_NAME, L"Kopírovať ná&zov súboru" },
        LocalizedText{ TXT_CLOSE_ALL, L"Zavrieť &všetko" },
        LocalizedText{ TXT_OPEN_FOLDER_TITLE, L"Otvoriť priečinok" },
        LocalizedText{ TXT_OPEN_FOLDER, L"Otvoriť p&riečinok…" },
        LocalizedText{ TXT_APPLICATION_TITLE, L"Prehliadač dátových správ" },
        LocalizedText{ TXT_LANGUAGE_NAME, L"Slovenčina" },
        LocalizedText{ TXT_VIEW, L"&Zobrazenie" },
        LocalizedText{ TXT_PREFERENCES, L"&Predvoľby…\tCtrl+G" },
        LocalizedText{ TXT_PREFERENCES_TITLE, L"Predvoľby" },
        LocalizedText{ TXT_MULTIPLE_INSTANCES, L"Povo&liť viac inštancií" },
        LocalizedText{ TXT_SMOOTH_SCROLLING, L"Plynulé po&súvanie" },
        LocalizedText{ TXT_MESSAGE_FONT, L"Písmo sprá&vy:" },
        LocalizedText{ TXT_SOURCE_FONT, L"Písmo &XML zdroja:" },
        LocalizedText{ TXT_FONT_DEFAULT, L"Pred&volené" },
        LocalizedText{ TXT_SYSTEM_DEFAULT_FONT_LABEL, L"Predvolené systémové" },
        LocalizedText{ TXT_FONT_REGULAR, L"Normálne" },
        LocalizedText{ TXT_FONT_BOLD, L"Tučné" },
        LocalizedText{ TXT_FONT_ITALIC, L"Kurzíva" },
        LocalizedText{ TXT_FONT_BOLD_ITALIC, L"Tučné kurzíva" },
        LocalizedText{ TXT_FONT_FAILED, L"Písmo sa nepodarilo nastaviť." },
        LocalizedText{ TXT_HIGHLIGHT_MESSAGE, L"Farebné zvýraznenie &správy" },
        LocalizedText{ TXT_HIGHLIGHT_SOURCE, L"Farebné zvýraznenie XML z&droja" },
        LocalizedText{ TXT_LOWERCASE_EXTENSIONS, L"Malé písmená v &prípone" },
        LocalizedText{ TXT_SETTINGS_FAILED, L"Predvoľby nemožno uložiť." },
        LocalizedText{ TXT_RICH_EDIT_FAILED, L"Systémový RichEdit sa nepodarilo načítať." },
        LocalizedText{ TXT_SENDER_SECTION, L"Odosielateľ" },
        LocalizedText{ TXT_RECIPIENT_SECTION, L"Adresát" },
        LocalizedText{ TXT_DELIVERY_SECTION, L"Doručenie" },
        LocalizedText{ TXT_LEGAL_SECTION, L"Splnomocnenie" },
        LocalizedText{ TXT_SIGNATURE_SECTION, L"Podpis obálky" },
        LocalizedText{ TXT_SAVE_LINK, L"Uložiť..." },
        LocalizedText{ TXT_BYTES, L"bajtov" },
        LocalizedText{ TXT_GENERAL_SECTION, L"Všeobecné" },
        LocalizedText{ TXT_SENDER_PERSON_SECTION, L"Odosielajúca osoba" },
        LocalizedText{ TXT_ADDITIONAL_SECTION, L"Doplňujúce údaje" },
        LocalizedText{ TXT_STATUS_SECTION, L"Stav správy" },
        LocalizedText{ TXT_EVENTS_SECTION, L"Udalosti" },
        LocalizedText{ TXT_NOT_PROVIDED, L"Neuvedené" },
        LocalizedText{ TXT_DM_ATTACHMENT_SIZE, L"Veľkosť príloh podľa ISDS" },
        LocalizedText{ TXT_DM_TYPE, L"Typ správy" },
        LocalizedText{ TXT_DM_SENDER_TYPE, L"Typ dátovej schránky" },
        LocalizedText{ TXT_DM_RECIPIENT_TYPE, L"Typ dátovej schránky" },
        LocalizedText{ TXT_DM_SENDER_ORG_UNIT_NUM, L"Číslo organizačnej jednotky" },
        LocalizedText{ TXT_DM_RECIPIENT_ORG_UNIT_NUM, L"Číslo organizačnej jednotky" },
        LocalizedText{ TXT_DM_VODZ, L"Veľkoobjemová dátová správa" },
        LocalizedText{ TXT_ATTS_NUM, L"Uvedený počet príloh" },
        LocalizedText{ TXT_DM_HASH, L"Odtlačok správy" },
        LocalizedText{ TXT_ALGORITHM, L"Algoritmus" },
        LocalizedText{ TXT_DM_Q_TIMESTAMP, L"Kvalifikovaná časová pečiatka" },
        LocalizedText{ TXT_DM_EVENT_TIME, L"Čas udalosti" },
        LocalizedText{ TXT_DM_EVENT_DESCR, L"Popis udalosti" },
        LocalizedText{ TXT_DM_FILE_DESCR, L"Názov súboru uložený v ZFO" },
        LocalizedText{ TXT_DM_MIME_TYPE, L"MIME type" },
        LocalizedText{ TXT_MIME_UNKNOWN, L"Neznámy formát" },
        LocalizedText{ TXT_MIME_BINARY, L"Binárne údaje" },
        LocalizedText{ TXT_MIME_ENCRYPTED, L"Šifrovaný dokument" },
        LocalizedText{ TXT_MIME_PDF, L"Portable Document Format" },
        LocalizedText{ TXT_MIME_TEXT, L"Obyčajný text" },
        LocalizedText{ TXT_MIME_CSV, L"Comma-Separated Values" },
        LocalizedText{ TXT_MIME_RTF, L"Rich Text Format" },
        LocalizedText{ TXT_MIME_HTML, L"HyperText Markup Language" },
        LocalizedText{ TXT_MIME_XHTML, L"Extensible HyperText Markup Language" },
        LocalizedText{ TXT_MIME_XML, L"Extensible Markup Language" },
        LocalizedText{ TXT_MIME_JSON, L"JavaScript Object Notation" },
        LocalizedText{ TXT_MIME_DOC, L"Microsoft Word Document" },
        LocalizedText{ TXT_MIME_DOCX, L"Office Open XML WordprocessingML Document" },
        LocalizedText{ TXT_MIME_XLS, L"Microsoft Excel Workbook" },
        LocalizedText{ TXT_MIME_XLSX, L"Office Open XML SpreadsheetML Workbook" },
        LocalizedText{ TXT_MIME_PPT, L"Microsoft PowerPoint Presentation" },
        LocalizedText{ TXT_MIME_PPTX, L"Office Open XML PresentationML Presentation" },
        LocalizedText{ TXT_MIME_ODT, L"OpenDocument Text" },
        LocalizedText{ TXT_MIME_ODS, L"OpenDocument Spreadsheet" },
        LocalizedText{ TXT_MIME_ODP, L"OpenDocument Presentation" },
        LocalizedText{ TXT_MIME_PNG, L"Portable Network Graphics" },
        LocalizedText{ TXT_MIME_GIF, L"CompuServe Graphics Interchange Format" },
        LocalizedText{ TXT_MIME_JPEG, L"Joint Photographic Experts Group" },
        LocalizedText{ TXT_MIME_TIFF, L"Tagged Image File Format" },
        LocalizedText{ TXT_MIME_HEIC, L"High Efficiency Image Container" },
        LocalizedText{ TXT_MIME_HEIC_SEQUENCE, L"High Efficiency Image Container Sequence" },
        LocalizedText{ TXT_MIME_HEIF, L"High Efficiency Image File Format" },
        LocalizedText{ TXT_MIME_HEIF_SEQUENCE, L"High Efficiency Image File Format Sequence" },
        LocalizedText{ TXT_MIME_BMP, L"Windows Bitmap" },
        LocalizedText{ TXT_MIME_WEBP, L"WebP" },
        LocalizedText{ TXT_MIME_SVG, L"Scalable Vector Graphics" },
        LocalizedText{ TXT_MIME_WAV, L"Waveform Audio File Format" },
        LocalizedText{ TXT_MIME_MPEG_AUDIO, L"MPEG Audio" },
        LocalizedText{ TXT_MIME_MP4_AUDIO, L"MPEG-4 Audio" },
        LocalizedText{ TXT_MIME_MPEG_VIDEO, L"MPEG Video" },
        LocalizedText{ TXT_MIME_MP4_VIDEO, L"MPEG-4 Video" },
        LocalizedText{ TXT_MIME_DWG, L"AutoCAD Drawing" },
        LocalizedText{ TXT_MIME_EDIFACT,
            L"Electronic Data Interchange for Administration, Commerce and Transport"
        },
        LocalizedText{ TXT_MIME_EDI_X12, L"ANSI ASC X12 Electronic Data Interchange" },
        LocalizedText{ TXT_MIME_EDI_CONSENT, L"Electronic Data Interchange Consent" },
        LocalizedText{ TXT_MIME_ISDOC, L"Information System Document" },
        LocalizedText{ TXT_MIME_ISDOCX, L"Information System Document Archive" },
        LocalizedText{ TXT_MIME_FO, L"Software602 Form Filler XML" },
        LocalizedText{ TXT_MIME_ZFO, L"Software602 Form Filler ZFO" },
        LocalizedText{ TXT_MIME_ZIP, L"ZIP Archive" },
        LocalizedText{ TXT_MIME_ASIC_E, L"Associated Signature Container Extended" },
        LocalizedText{ TXT_MIME_ASIC_S, L"Associated Signature Container Simple" },
        LocalizedText{ TXT_MIME_PKCS7, L"PKCS #7 Cryptographic Message Syntax" },
        LocalizedText{ TXT_MIME_PKCS7_SIGNATURE, L"PKCS #7 Signature" },
        LocalizedText{ TXT_MIME_PKCS7_CERTIFICATES, L"PKCS #7 Certificates" },
        LocalizedText{ TXT_MIME_X509, L"X.509 Certificate" },
        LocalizedText{ TXT_MIME_TIMESTAMP, L"Odpoveď časovej pečiatky" },
        LocalizedText{ TXT_DM_FILE_META_TYPE, L"Druh prílohy" },
        LocalizedText{ TXT_DM_FILE_GUID, L"Identifikátor súboru" },
        LocalizedText{ TXT_DM_UP_FILE_GUID, L"Identifikátor nadradeného súboru" },
        LocalizedText{ TXT_DM_FILE_SIZE, L"Veľkosť súboru" },
        LocalizedText{ TXT_USER_TYPE, L"Typ používateľa" },
        LocalizedText{ TXT_DM_USER_TYPE, L"Typ používateľa" },
        LocalizedText{ TXT_USER_NAME, L"Používateľské meno" },
        LocalizedText{ TXT_DM_USER_NAME, L"Používateľské meno" },
        LocalizedText{ TXT_PN_FIRST_NAME, L"Meno" },
        LocalizedText{ TXT_PN_MIDDLE_NAME, L"Ďalšie meno" },
        LocalizedText{ TXT_PN_LAST_NAME, L"Priezvisko" },
        LocalizedText{ TXT_PN_LAST_NAME_AT_BIRTH, L"Rodné priezvisko" },
        LocalizedText{ TXT_FIRM_NAME, L"Názov organizácie" },
        LocalizedText{ TXT_BI_DATE, L"Dátum narodenia" },
        LocalizedText{ TXT_BI_CITY, L"Miesto narodenia" },
        LocalizedText{ TXT_BI_COUNTY, L"Okres narodenia" },
        LocalizedText{ TXT_BI_STATE, L"Štát narodenia" },
        LocalizedText{ TXT_AD_CITY, L"Obec" },
        LocalizedText{ TXT_AD_STREET, L"Ulica" },
        LocalizedText{ TXT_AD_NUMBER_IN_STREET, L"Orientačné číslo" },
        LocalizedText{ TXT_AD_NUMBER_IN_MUNICIPALITY, L"Súpisné číslo" },
        LocalizedText{ TXT_AD_ZIP_CODE, L"PSČ" },
        LocalizedText{ TXT_AD_STATE, L"Štát" },
        LocalizedText{ TXT_IC, L"IČO" },
        LocalizedText{ TXT_DM_STATUS_CODE, L"Stavový kód odpovede" },
        LocalizedText{ TXT_DM_STATUS_MESSAGE, L"Popis stavu odpovede" },
        LocalizedText{ TXT_FILE_ROLE_MAIN, L"Hlavná príloha" },
        LocalizedText{ TXT_FILE_ROLE_ENCLOSURE, L"Príloha" },
        LocalizedText{ TXT_FILE_ROLE_SIGNATURE, L"Podpis" },
        LocalizedText{ TXT_BOX_TYPE_0, L"Systémová schránka" },
        LocalizedText{ TXT_BOX_TYPE_10, L"Orgán verejnej moci" },
        LocalizedText{ TXT_BOX_TYPE_20, L"Právnická osoba" },
        LocalizedText{ TXT_BOX_TYPE_30, L"Podnikajúca fyzická osoba" },
        LocalizedText{ TXT_BOX_TYPE_40, L"Fyzická osoba" },
        LocalizedText{ TXT_MESSAGE_TYPE_V, L"Verejná správa; jedna zo strán je orgán verejnej moci" },
        LocalizedText{ TXT_MESSAGE_TYPE_K, L"Poštová dátová správa" },
        LocalizedText{ TXT_MESSAGE_TYPE_I, L"Poštová dátová správa umožňujúca predplatenú odpoveď" },
        LocalizedText{ TXT_MESSAGE_TYPE_O, L"Odpoveď hradená odosielateľom pôvodnej správy" },
        LocalizedText{ TXT_MESSAGE_TYPE_E, L"Poštová dátová správa hradená z kreditu" },
        LocalizedText{ TXT_MESSAGE_TYPE_A, L"Dotovaná správa s predplatenou odpoveďou" },
        LocalizedText{ TXT_MESSAGE_TYPE_B, L"Dotovaná správa; predplatená odpoveď využitá" },
        LocalizedText{ TXT_MESSAGE_TYPE_C, L"Dotovaná správa; možnosť odpovede vypršala" },
        LocalizedText{ TXT_MESSAGE_TYPE_G, L"Poštová dátová správa hradená sponzorom" },
        LocalizedText{ TXT_MESSAGE_TYPE_X, L"Iniciačná správa; možnosť odpovede vypršala" },
        LocalizedText{ TXT_MESSAGE_TYPE_Y, L"Iniciačná správa; predplatená odpoveď využitá" },
        LocalizedText{ TXT_STATE_1, L"Správa bola podaná do ISDS" },
        LocalizedText{ TXT_STATE_2, L"Správa vrátane príloh bola opatrená časovou pečiatkou" },
        LocalizedText{ TXT_STATE_3, L"Antivírusová kontrola zlyhala; napadnutá príloha bola odstránená" },
        LocalizedText{ TXT_STATE_4, L"Správa bola dodaná do schránky príjemcu" },
        LocalizedText{ TXT_STATE_5, L"Uplynula lehota na doručenie fikciou" },
        LocalizedText{ TXT_STATE_6, L"Oprávnená osoba sa prihlásila; správa bola doručená" },
        LocalizedText{ TXT_STATE_7, L"Správa bola prečítaná" },
        LocalizedText{ TXT_STATE_8, L"Nedoručiteľná správa; schránka príjemcu bola zneprístupnená" },
        LocalizedText{ TXT_STATE_9, L"Obsah správy bol vymazaný; obálka bola archivovaná" },
        LocalizedText{ TXT_STATE_10, L"Správa je v dátovom trezore" },
        LocalizedText{ TXT_USER_TYPE_PRIMARY_USER, L"Oprávnená osoba" },
        LocalizedText{ TXT_USER_TYPE_ENTRUSTED_USER, L"Poverená osoba" },
        LocalizedText{ TXT_USER_TYPE_ADMINISTRATOR, L"Administrátor" },
        LocalizedText{ TXT_USER_TYPE_OFFICIAL, L"Úradná osoba" },
        LocalizedText{ TXT_INSTANCE_FAILED, L"Nepodarilo sa získať zámok spustenia aplikácie." },
        LocalizedText{ TXT_FORWARD_FAILED, L"Bežiaca aplikácia neprijala súbory." },
        LocalizedText{ TXT_OPTIONS, L"&Možnosti" },
        LocalizedText{ TXT_EDIT, L"&Úpravy" },
        LocalizedText{ TXT_SELECT_ALL, L"Vybrať &všetko\tCtrl+A" },
        LocalizedText{ TXT_FIND_MENU, L"&Hľadať…\tCtrl+F" },
        LocalizedText{ TXT_FIND_NEXT_MENU, L"Hľadať ď&alej\tF3" },
        LocalizedText{ TXT_FIND_PREVIOUS_MENU, L"Hľadať &predchádzajúce\tShift+F3" },
        LocalizedText{ TXT_PRINT, L"&Tlačiť…\tCtrl+P" },
        LocalizedText{ TXT_PAGE_SETUP, L"Vz&hľad strany…" },
        LocalizedText{ TXT_REGEX_INVALID, L"Neplatný regulárny výraz." },
        LocalizedText{ TXT_REGEX_ESCAPE, L"Neplatná riadiaca sekvencia." },
        LocalizedText{ TXT_REGEX_BRACKET, L"Chýbajúca alebo nadbytočná hranatá zátvorka." },
        LocalizedText{ TXT_REGEX_PARENTHESIS, L"Chýbajúca alebo nadbytočná okrúhla zátvorka." },
        LocalizedText{ TXT_REGEX_REPEAT, L"Neplatný zápis opakovania." },
        LocalizedText{ TXT_REGEX_RANGE, L"Neplatný rozsah znakov." },
        LocalizedText{ TXT_REGEX_COMPLEXITY, L"Výraz je na vyhodnotenie príliš zložitý." },
        LocalizedText{ TXT_LANGUAGE_LABEL, L"&Jazyk:" },
        LocalizedText{ TXT_FILE, L"&Súbor" },
        LocalizedText{ TXT_OPEN, L"&Otvoriť...\tCtrl+O" },
        LocalizedText{ TXT_EXPORT, L"&Exportovať…\tCtrl+E" },
        LocalizedText{ TXT_CLOSE_FILE, L"&Zavrieť\tCtrl+W" },
        LocalizedText{ TXT_SAVE_COPY_AS, L"Uložiť kó&piu ako…" },
        LocalizedText{ TXT_SAVE_COPY_AS_TITLE, L"Uložiť kópiu ako" },
        LocalizedText{ TXT_EXIT, L"&Koniec\tAlt+F4" },
        LocalizedText{ TXT_ATTACHMENT, L"&Príloha" },
        LocalizedText{ TXT_OPEN_ATTACHMENT, L"&Otvoriť prílohu\tEnter" },
        LocalizedText{ TXT_SAVE_ATTACHMENT, L"&Uložiť prílohu...\tCtrl+S" },
        LocalizedText{ TXT_COPY, L"&Kopírovať\tCtrl+C" },
        LocalizedText{ TXT_LANGUAGE, L"&Jazyk" },
        LocalizedText{ TXT_HELP_MENU, L"&Pomoc" },
        LocalizedText{ TXT_HELP_TEXT, L"&Pomoc\tF1" },
        LocalizedText{ TXT_ABOUT_TEXT, L"&O programe" },
        LocalizedText{ TXT_ABOUT_WINDOW_TITLE, L"O aplikácii ZFO Viewer" },
        LocalizedText{ TXT_MESSAGE, L"Správa" },
        LocalizedText{ TXT_ATTACHMENTS, L"Prílohy" },
        LocalizedText{ TXT_XML, L"Zdroj XML" },
        LocalizedText{ TXT_FIELD, L"Údaj" },
        LocalizedText{ TXT_VALUE, L"Hodnota" },
        LocalizedText{ TXT_NAME, L"Názov súboru" },
        LocalizedText{ TXT_SIZE, L"Veľkosť (bajty)" },
        LocalizedText{ TXT_TYPE, L"Typ obsahu" },
        LocalizedText{ TXT_READY, L"Pripravený" },
        LocalizedText{ TXT_NO_ATTACHMENTS, L"Správa neobsahuje vložené prílohy." },
        LocalizedText{ TXT_YES, L"Áno" },
        LocalizedText{ TXT_NO, L"Nie" },
        LocalizedText{ TXT_DM_ID, L"ID správy" },
        LocalizedText{ TXT_DM_ANNOTATION, L"Predmet" },
        LocalizedText{ TXT_DM_SENDER, L"Meno" },
        LocalizedText{ TXT_DB_ID_SENDER, L"ID schránky" },
        LocalizedText{ TXT_DM_SENDER_ADDRESS, L"Adresa" },
        LocalizedText{ TXT_DM_SENDER_ORG_UNIT, L"Organizačná jednotka" },
        LocalizedText{ TXT_DM_RECIPIENT, L"Meno" },
        LocalizedText{ TXT_DB_ID_RECIPIENT, L"ID schránky" },
        LocalizedText{ TXT_DM_RECIPIENT_ADDRESS, L"Adresa" },
        LocalizedText{ TXT_DM_RECIPIENT_ORG_UNIT, L"Organizačná jednotka" },
        LocalizedText{ TXT_DM_DELIVERY_TIME, L"Čas dodania" },
        LocalizedText{ TXT_DM_ACCEPTANCE_TIME, L"Čas doručenia" },
        LocalizedText{ TXT_CENTRAL_EUROPE_TIME, L"SEČ" },
        LocalizedText{ TXT_CENTRAL_EUROPE_SUMMER_TIME, L"LSEČ" },
        LocalizedText{ TXT_DM_MESSAGE_STATUS, L"Stav správy" },
        LocalizedText{ TXT_DM_SENDER_REF_NUMBER, L"Naše číslo konania" },
        LocalizedText{ TXT_DM_SENDER_IDENT, L"Naša spisová značka" },
        LocalizedText{ TXT_DM_RECIPIENT_REF_NUMBER, L"Vaše číslo konania" },
        LocalizedText{ TXT_DM_RECIPIENT_IDENT, L"Vaša spisová značka" },
        LocalizedText{ TXT_DM_TO_HANDS, L"Do rúk" },
        LocalizedText{ TXT_DM_PERSONAL_DELIVERY, L"Do vlastných rúk" },
        LocalizedText{ TXT_DM_ALLOW_SUBST_DELIVERY, L"Zakázané doručenie fikciou" },
        LocalizedText{ TXT_DM_LEGAL_TITLE, L"Splnomocnenie" },
        LocalizedText{ TXT_LEGAL_TITLE_PARAGRAPH, L"odsek" },
        LocalizedText{ TXT_LEGAL_TITLE_LETTER, L"písmeno" },
        LocalizedText{ TXT_DM_LEGAL_TITLE_LAW, L"Splnomocnenie — zákon" },
        LocalizedText{ TXT_DM_LEGAL_TITLE_YEAR, L"Splnomocnenie — rok" },
        LocalizedText{ TXT_DM_LEGAL_TITLE_SECT, L"Splnomocnenie — paragraf" },
        LocalizedText{ TXT_DM_LEGAL_TITLE_PAR, L"Splnomocnenie — odsek" },
        LocalizedText{ TXT_DM_LEGAL_TITLE_POINT, L"Splnomocnenie — písmeno" },
        LocalizedText{ TXT_SIGNATURE, L"Podpis obálky" },
        LocalizedText{ TXT_SIGNATURE_VALID, L"Kryptografický podpis je v poriadku." },
        LocalizedText{ TXT_SIGNATURE_INVALID, L"Kryptografický podpis sa nepodarilo overiť." },
        LocalizedText{ TXT_SIGNER, L"Podpísal" },
        LocalizedText{ TXT_TRUST, L"Dôveryhodnosť podpisu" },
        LocalizedText{ TXT_TRUST_INFO,
            L"Reťazec certifikátov, odvolanie a časové pečiatky nie sú overované."
        },
        LocalizedText{ TXT_EVENT, L"Udalosť doručenia" },
        LocalizedText{ TXT_INVALID_ZFO, L"Súbor nie je čitateľná správa ZFO vo formáte CMS/PKCS#7." },
        LocalizedText{ TXT_INVALID_XML, L"Súbor ZFO obsahuje neplatné alebo nepodporované XML ISDS." },
        LocalizedText{ TXT_DECODE_FAILED, L"Obálku ZFO sa nepodarilo dekódovať." },
        LocalizedText{ TXT_EXTERNAL_ATTACHMENT,
            L"Príloha nemá vložený obsah. Externé prílohy nie sú podporované."
        },
        LocalizedText{ TXT_INVALID_BASE64, L"Príloha obsahuje neplatné údaje Base64." },
        LocalizedText{ TXT_SAVE_FAILED,
            L"Súbor nemožno uložiť. Overte cestu, oprávnenia a voľné miesto na disku."
        },
        LocalizedText{ TXT_OPEN_FAILED, L"Prílohu nemožno otvoriť v priradenej aplikácii." },
        LocalizedText{ TXT_OPEN_FOLDER_FAILED, L"Zdrojový priečinok nemožno otvoriť." },
        LocalizedText{ TXT_UNEXPECTED_ERROR, L"Operáciu sa nepodarilo dokončiť." },
        LocalizedText{ TXT_ONE_FILE, L"Presuňte vždy jeden súbor ZFO." },
    }),
    DefineLanguage(LANG_EN_US, std::initializer_list<LocalizedText>{
        LocalizedText{ TXT_CURL_LOAD_OPERATION, L"loading libcurl.dll" },
        LocalizedText{ TXT_FILE_OPEN_FAILED, L"%s could not be opened. Check its path and permissions." },
        LocalizedText{ TXT_INVALID_WINDOWS_DLL, L"The selected file is not a valid Windows DLL." },
        LocalizedText{ TXT_LIBRARY_DECOMPRESSION_FAILED,
            L"The bundled library failed decompression or CRC verification."
        },
        LocalizedText{ TXT_LIBRARY_EXTRACTION_FAILED,
            L"The bundled library could not be written to AppData, the user profile, Program Files or the "
            L"temporary directory."
        },
        LocalizedText{ TXT_LIBRARY_REQUIRES_X86,
            L"The Win32 viewer requires a 32-bit %s. Select the x86/i386 library."
        },
        LocalizedText{ TXT_LIBRARY_RESOURCE_INVALID, L"The bundled library resource is invalid." },
        LocalizedText{ TXT_BMP_EXPORT_FAILED,
            L"The BMP image could not be saved. Check the destination and available memory and disk space."
        },
        LocalizedText{ TXT_BMP_FORMAT, L"Windows Bitmap BMP (*.bmp)" },
        LocalizedText{ TXT_GIF_EXPORT_FAILED,
            L"GIF export failed. Exact GIF output requires at most 256 colors."
        },
        LocalizedText{ TXT_GIF_FORMAT, L"CompuServe GIF 89a (*.gif)" },
        LocalizedText{ TXT_IMAGE_FORMAT_UNSUPPORTED, L"The selected image format is not supported." },
        LocalizedText{ TXT_JPEG_EXPORT_FAILED,
            L"JPEG export failed. A working GDI+ JPEG encoder is required."
        },
        LocalizedText{ TXT_JPEG_FORMAT, L"JPEG File Interchange Format (*.jpg)" },
        LocalizedText{ TXT_PNG_EXPORT_FAILED,
            L"The PNG image could not be saved. Check the destination and available memory and disk space."
        },
        LocalizedText{ TXT_PNG_FORMAT, L"Portable Network Graphics PNG (*.png)" },
        LocalizedText{ TXT_REPORT_IMAGE_CREATE_FAILED,
            L"The displayed content could not be converted to an image."
        },
        LocalizedText{ TXT_HTML_FORMAT, L"HyperText Markup Language 4.01 Strict (*.html)" },
        LocalizedText{ TXT_XHTML_FORMAT, L"Extensible HyperText Markup Language 1.1 (*.xhtml)" },
        LocalizedText{ TXT_ODT_FORMAT, L"OpenDocument Text (*.odt)" },
        LocalizedText{ TXT_DOCX_FORMAT, L"Office Open XML WordprocessingML Document (*.docx)" },
        LocalizedText{ TXT_RTF_FORMAT, L"Rich Text Format (*.rtf)" },
        LocalizedText{ TXT_TEXT_ENCODING_FAILED,
            L"The text cannot be saved in this encoding without losing characters. Choose another text "
            L"encoding."
        },
        LocalizedText{ TXT_TEXT_EXPORT_FAILED,
            L"The text could not be saved. Check the destination and available disk space."
        },
        LocalizedText{ TXT_TEXT_FORMAT, L"Text Document ANSI (*.txt)" },
        LocalizedText{ TXT_TEXT_UTF16_BE_FORMAT, L"Text Document UTF-16 BE BOM (*.txt)" },
        LocalizedText{ TXT_TEXT_UTF16_LE_FORMAT, L"Text Document UTF-16 LE BOM (*.txt)" },
        LocalizedText{ TXT_TEXT_UTF8_BOM_FORMAT, L"Text Document UTF-8 BOM (*.txt)" },
        LocalizedText{ TXT_TEXT_UTF8_FORMAT, L"Text Document UTF-8 (*.txt)" },
        LocalizedText{ TXT_TIFF_EXPORT_FAILED,
            L"The TIFF image could not be saved. Check the destination and available memory and disk space."
        },
        LocalizedText{ TXT_TIFF_FORMAT, L"Tagged Image File Format TIFF (*.tif)" },
        LocalizedText{ TXT_WEBP_EXPORT_FAILED,
            L"The WebP image could not be saved. Check the destination and available memory and disk space."
        },
        LocalizedText{ TXT_WEBP_FORMAT, L"Google WebP (*.webp)" },
        LocalizedText{ TXT_XML_FORMAT, L"Extensible Markup Language (*.xml)" },
        LocalizedText{ TXT_ZFO_FORMAT, L"ZFO File (*.zfo)" },
        LocalizedText{ TXT_ABOUT, L"About" },
        LocalizedText{ TXT_ABOUT_COPY_INFORMATION_FAILED,
            L"The information could not be copied to the clipboard."
        },
        LocalizedText{ TXT_ABOUT_COPY_URL_FAILED, L"The URL could not be copied to the clipboard." },
        LocalizedText{ TXT_ABOUT_DESCRIPTION,
            L"Native viewer for ISDS data messages and delivery receipts."
        },
        LocalizedText{ TXT_ABOUT_LICENSE_UNAVAILABLE, L"License resource unavailable." },
        LocalizedText{ TXT_ABOUT_PLATFORM_LABEL, L"Target platform:" },
        LocalizedText{ TXT_ABOUT_VERSION_LABEL, L"Version:" },
        LocalizedText{ TXT_ABOUT_VISIT_TOOLTIP, L"Visit %ls" },
        LocalizedText{ TXT_ABOUT_WEBSITE_FAILED, L"The website could not be opened in the default browser." },
        LocalizedText{ TXT_ABOUT_WEBSITE_LABEL, L"Website:" },
        LocalizedText{ TXT_APPLICATION_HELP,
            L"FILES AND TABS\r\n"
            L"Choose File > Open (Ctrl+O) or Open Folder, drop ZFO files into the window or onto the "
            L"application icon, or pass filenames on the command line. Each message has its own tab. "
            L"Ctrl+Tab and Ctrl+Shift+Tab switch tabs. Ctrl+W or a middle-click closes a tab. File > Close "
            L"All closes every tab. The File menu remembers eight recent files. File > Save a Copy As saves "
            L"the original ZFO unchanged, including its envelope and attachments, and keeps the current tab "
            L"open on the original file. Alt+F4 exits the application.\r\n\r\n"
            L"READING MESSAGES\r\n"
            L"The message view displays all available metadata, including empty fields and extensions. Click "
            L"an attachment name to open it in its associated application, or click Save beside it to save a "
            L"copy. Native text selection and Ctrl+C work in the message view. Right-click text, a tab, or "
            L"the status bar for the corresponding context menu. Each tab uses its own short directory under "
            L"Local AppData\\FortSoft\\ZfoViewer for opened attachments, preserving their filenames. Closing "
            L"the tab removes its temporary files recursively. Locked files are skipped; cleanup is retried "
            L"at startup, when opening attachments or closing tabs, and on exit.\r\n\r\n"
            L"XML SOURCE AND EXPORT\r\n"
            L"View > Color Highlighting toggles highlighting for the active message or XML source and shares "
            L"its saved preference with Preferences. View > View XML Source (Ctrl+U) opens formatted XML in "
            L"a new tab. This command is disabled on a source tab. Repeating the command selects the "
            L"existing source tab. File > Export (Ctrl+E) offers ANSI, UTF-8, UTF-8 with BOM, UTF-16 "
            L"big-endian, UTF-16 little-endian, ODT, DOCX, RTF, HTML 4.01 Strict, XHTML 1.1, XML, BMP, GIF, "
            L"JPEG, PNG, TIFF, and WebP. ODT and DOCX do not require Office. XML export uses the same "
            L"indentation as the source view.\r\n\r\n"
            L"FIND\r\n"
            L"Edit > Find (Ctrl+F) searches the message, XML source, Help, or the license in About. F3 finds "
            L"the next match; Shift+F3 finds the previous match. Options include case matching, backward "
            L"searching, line boundaries, and ECMAScript regular expressions with multiline matching.\r\n\r\n"
            L"PRINT\r\n"
            L"File > Page Setup changes margins and printer settings. File > Print (Ctrl+P) prints text from "
            L"the active message, XML source, Help, or About. About prints its license text. Windows 11 uses "
            L"the modern system print interface; older systems use the classic print dialog.\r\n\r\n"
            L"PREFERENCES\r\n"
            L"The first two rows choose the message and XML source fonts. Default restores the system "
            L"message font and Consolas for source. Interface, Help, and license fonts are unchanged. Both "
            L"highlighting options are enabled by default and apply to exports. Message highlighting only "
            L"colors section headings. XML uses Firefox's source styles with bold tag and attribute names. "
            L"Options > Preferences (Ctrl+G) contains Czech, Slovak, and US English. The first launch "
            L"follows the Windows UI language, with US English as the fallback. A manual language selection "
            L"is remembered. Other settings include the tab-name length (80 by default), the name length in "
            L"the window title (40 by default), closing tabs on double-click, and allowing multiple "
            L"instances. Smooth scrolling is off by default; ordinary mouse-wheel scrolling in messages and "
            L"XML source moves immediately by the system's line or page setting. The Clear button removes "
            L"recent files immediately. OK and Apply save changes; Cancel discards changes since the last "
            L"Apply. One application instance is the default.\r\n\r\n"
            L"SIGNATURES\r\n"
            L"The viewer checks the cryptographic signature of the CMS envelope offline. Certificate trust, "
            L"revocation, timestamps, and long-term signature validity are not verified. F1 opens Help."
        },
        LocalizedText{ TXT_HELP, L"Help" },
        LocalizedText{ TXT_OPEN_IN_BROWSER, L"&Open in default browser" },
        LocalizedText{ TXT_ABOUT_TITLE, L"About %s" },
        LocalizedText{ TXT_CLOSE, L"&Close" },
        LocalizedText{ TXT_COPY_INFORMATION, L"Copy information" },
        LocalizedText{ TXT_COPY_URL, L"Copy link" },
        LocalizedText{ TXT_FIND_BACKWARD, L"Search backward" },
        LocalizedText{ TXT_FIND_ENDS_WITH, L"End of line" },
        LocalizedText{ TXT_FIND_FAILED, L"The search could not be completed." },
        LocalizedText{ TXT_FIND_MATCH_CASE, L"Match case" },
        LocalizedText{ TXT_FIND_NEXT, L"Find Next" },
        LocalizedText{ TXT_FIND_NOT_FOUND, L"No match was found." },
        LocalizedText{ TXT_FIND_REGEX, L"Regular expression" },
        LocalizedText{ TXT_FIND_REGEX_ERROR, L"Regular expression: %s" },
        LocalizedText{ TXT_FIND_STARTS_WITH, L"Beginning of line" },
        LocalizedText{ TXT_FIND_TITLE, L"ZFO Viewer — Find" },
        LocalizedText{ TXT_FIND_TOPMOST, L"Always on top" },
        LocalizedText{ TXT_FIND_WHAT, L"Find:" },
        LocalizedText{ TXT_HELP_TITLE, L"ZFO Viewer — Help" },
        LocalizedText{ TXT_PAGE_SETUP_FAILED, L"The Page Setup dialog could not be opened." },
        LocalizedText{ TXT_PLATFORM, L"Platform:" },
        LocalizedText{ TXT_PRINT_FAILED, L"The document could not be printed." },
        LocalizedText{ TXT_COPY_CONTEXT, L"&Copy" },
        LocalizedText{ TXT_CLOSE_TAB_CONTEXT, L"Close &Tab" },
        LocalizedText{ TXT_SHOW_IN_FOLDER, L"&Show in Containing Folder" },
        LocalizedText{ TXT_COLOR_HIGHLIGHTING, L"Color &Highlighting" },
        LocalizedText{ TXT_XML_SOURCE, L"View &XML Source\tCtrl+U" },
        LocalizedText{ TXT_SOURCE_PREFIX, L"XML Source: " },
        LocalizedText{ TXT_OK, L"OK" },
        LocalizedText{ TXT_CANCEL, L"Cancel" },
        LocalizedText{ TXT_APPLY, L"&Apply" },
        LocalizedText{ TXT_CLOSE_ON_DOUBLE_CLICK, L"Close tabs on double-clic&k" },
        LocalizedText{ TXT_TITLE_NAME_LENGTH_INVALID, L"The title name length must be between 8 and 255." },
        LocalizedText{ TXT_TITLE_NAME_LENGTH, L"Titl&e length:" },
        LocalizedText{ TXT_TAB_NAME_LENGTH_INVALID, L"The tab name length must be between 8 and 255." },
        LocalizedText{ TXT_CLEAR_RECENT_FILES, L"&Clear" },
        LocalizedText{ TXT_RECENT_FILES, L"Recent files:" },
        LocalizedText{ TXT_TAB_NAME_LENGTH, L"Tab name len&gth:" },
        LocalizedText{ TXT_COPY_FULL_PATH, L"Copy Full Pat&h" },
        LocalizedText{ TXT_COPY_FILE_NAME, L"Copy File Na&me" },
        LocalizedText{ TXT_CLOSE_ALL, L"Close &All" },
        LocalizedText{ TXT_OPEN_FOLDER_TITLE, L"Open Folder" },
        LocalizedText{ TXT_OPEN_FOLDER, L"Open &Folder…" },
        LocalizedText{ TXT_APPLICATION_TITLE, L"ZFO Viewer" },
        LocalizedText{ TXT_LANGUAGE_NAME, L"English (United States)" },
        LocalizedText{ TXT_VIEW, L"&View" },
        LocalizedText{ TXT_PREFERENCES, L"&Preferences…\tCtrl+G" },
        LocalizedText{ TXT_PREFERENCES_TITLE, L"Preferences" },
        LocalizedText{ TXT_MULTIPLE_INSTANCES, L"Allow multiple &instances" },
        LocalizedText{ TXT_SMOOTH_SCROLLING, L"S&mooth scrolling" },
        LocalizedText{ TXT_MESSAGE_FONT, L"Message &font:" },
        LocalizedText{ TXT_SOURCE_FONT, L"&XML source font:" },
        LocalizedText{ TXT_FONT_DEFAULT, L"&Default" },
        LocalizedText{ TXT_SYSTEM_DEFAULT_FONT_LABEL, L"System default" },
        LocalizedText{ TXT_FONT_REGULAR, L"Regular" },
        LocalizedText{ TXT_FONT_BOLD, L"Bold" },
        LocalizedText{ TXT_FONT_ITALIC, L"Italic" },
        LocalizedText{ TXT_FONT_BOLD_ITALIC, L"Bold Italic" },
        LocalizedText{ TXT_FONT_FAILED, L"The font could not be set." },
        LocalizedText{ TXT_HIGHLIGHT_MESSAGE, L"Message color &highlighting" },
        LocalizedText{ TXT_HIGHLIGHT_SOURCE, L"XML source color high&lighting" },
        LocalizedText{ TXT_LOWERCASE_EXTENSIONS, L"Lowercase &extensions" },
        LocalizedText{ TXT_SETTINGS_FAILED, L"Preferences could not be saved." },
        LocalizedText{ TXT_RICH_EDIT_FAILED, L"The system RichEdit control could not be loaded." },
        LocalizedText{ TXT_SENDER_SECTION, L"Sender" },
        LocalizedText{ TXT_RECIPIENT_SECTION, L"Recipient" },
        LocalizedText{ TXT_DELIVERY_SECTION, L"Delivery" },
        LocalizedText{ TXT_LEGAL_SECTION, L"Legal authority" },
        LocalizedText{ TXT_SIGNATURE_SECTION, L"Envelope signature" },
        LocalizedText{ TXT_SAVE_LINK, L"Save..." },
        LocalizedText{ TXT_BYTES, L"bytes" },
        LocalizedText{ TXT_GENERAL_SECTION, L"General" },
        LocalizedText{ TXT_SENDER_PERSON_SECTION, L"Sending person" },
        LocalizedText{ TXT_ADDITIONAL_SECTION, L"Additional information" },
        LocalizedText{ TXT_STATUS_SECTION, L"Message status" },
        LocalizedText{ TXT_EVENTS_SECTION, L"Events" },
        LocalizedText{ TXT_NOT_PROVIDED, L"Not provided" },
        LocalizedText{ TXT_DM_ATTACHMENT_SIZE, L"Attachment size reported by ISDS" },
        LocalizedText{ TXT_DM_TYPE, L"Message type" },
        LocalizedText{ TXT_DM_SENDER_TYPE, L"Data box type" },
        LocalizedText{ TXT_DM_RECIPIENT_TYPE, L"Data box type" },
        LocalizedText{ TXT_DM_SENDER_ORG_UNIT_NUM, L"Organizational unit number" },
        LocalizedText{ TXT_DM_RECIPIENT_ORG_UNIT_NUM, L"Organizational unit number" },
        LocalizedText{ TXT_DM_VODZ, L"Large-volume data message" },
        LocalizedText{ TXT_ATTS_NUM, L"Reported attachment count" },
        LocalizedText{ TXT_DM_HASH, L"Message hash" },
        LocalizedText{ TXT_ALGORITHM, L"Algorithm" },
        LocalizedText{ TXT_DM_Q_TIMESTAMP, L"Qualified timestamp" },
        LocalizedText{ TXT_DM_EVENT_TIME, L"Event time" },
        LocalizedText{ TXT_DM_EVENT_DESCR, L"Event description" },
        LocalizedText{ TXT_DM_FILE_DESCR, L"File name stored in ZFO" },
        LocalizedText{ TXT_DM_MIME_TYPE, L"MIME type" },
        LocalizedText{ TXT_MIME_UNKNOWN, L"Unknown format" },
        LocalizedText{ TXT_MIME_BINARY, L"Binary data" },
        LocalizedText{ TXT_MIME_ENCRYPTED, L"Encrypted document" },
        LocalizedText{ TXT_MIME_PDF, L"Portable Document Format" },
        LocalizedText{ TXT_MIME_TEXT, L"Plain text" },
        LocalizedText{ TXT_MIME_CSV, L"Comma-Separated Values" },
        LocalizedText{ TXT_MIME_RTF, L"Rich Text Format" },
        LocalizedText{ TXT_MIME_HTML, L"HyperText Markup Language" },
        LocalizedText{ TXT_MIME_XHTML, L"Extensible HyperText Markup Language" },
        LocalizedText{ TXT_MIME_XML, L"Extensible Markup Language" },
        LocalizedText{ TXT_MIME_JSON, L"JavaScript Object Notation" },
        LocalizedText{ TXT_MIME_DOC, L"Microsoft Word Document" },
        LocalizedText{ TXT_MIME_DOCX, L"Office Open XML WordprocessingML Document" },
        LocalizedText{ TXT_MIME_XLS, L"Microsoft Excel Workbook" },
        LocalizedText{ TXT_MIME_XLSX, L"Office Open XML SpreadsheetML Workbook" },
        LocalizedText{ TXT_MIME_PPT, L"Microsoft PowerPoint Presentation" },
        LocalizedText{ TXT_MIME_PPTX, L"Office Open XML PresentationML Presentation" },
        LocalizedText{ TXT_MIME_ODT, L"OpenDocument Text" },
        LocalizedText{ TXT_MIME_ODS, L"OpenDocument Spreadsheet" },
        LocalizedText{ TXT_MIME_ODP, L"OpenDocument Presentation" },
        LocalizedText{ TXT_MIME_PNG, L"Portable Network Graphics" },
        LocalizedText{ TXT_MIME_GIF, L"CompuServe Graphics Interchange Format" },
        LocalizedText{ TXT_MIME_JPEG, L"Joint Photographic Experts Group" },
        LocalizedText{ TXT_MIME_TIFF, L"Tagged Image File Format" },
        LocalizedText{ TXT_MIME_HEIC, L"High Efficiency Image Container" },
        LocalizedText{ TXT_MIME_HEIC_SEQUENCE, L"High Efficiency Image Container Sequence" },
        LocalizedText{ TXT_MIME_HEIF, L"High Efficiency Image File Format" },
        LocalizedText{ TXT_MIME_HEIF_SEQUENCE, L"High Efficiency Image File Format Sequence" },
        LocalizedText{ TXT_MIME_BMP, L"Windows Bitmap" },
        LocalizedText{ TXT_MIME_WEBP, L"WebP" },
        LocalizedText{ TXT_MIME_SVG, L"Scalable Vector Graphics" },
        LocalizedText{ TXT_MIME_WAV, L"Waveform Audio File Format" },
        LocalizedText{ TXT_MIME_MPEG_AUDIO, L"MPEG Audio" },
        LocalizedText{ TXT_MIME_MP4_AUDIO, L"MPEG-4 Audio" },
        LocalizedText{ TXT_MIME_MPEG_VIDEO, L"MPEG Video" },
        LocalizedText{ TXT_MIME_MP4_VIDEO, L"MPEG-4 Video" },
        LocalizedText{ TXT_MIME_DWG, L"AutoCAD Drawing" },
        LocalizedText{ TXT_MIME_EDIFACT,
            L"Electronic Data Interchange for Administration, Commerce and Transport"
        },
        LocalizedText{ TXT_MIME_EDI_X12, L"ANSI ASC X12 Electronic Data Interchange" },
        LocalizedText{ TXT_MIME_EDI_CONSENT, L"Electronic Data Interchange Consent" },
        LocalizedText{ TXT_MIME_ISDOC, L"Information System Document" },
        LocalizedText{ TXT_MIME_ISDOCX, L"Information System Document Archive" },
        LocalizedText{ TXT_MIME_FO, L"Software602 Form Filler XML" },
        LocalizedText{ TXT_MIME_ZFO, L"Software602 Form Filler ZFO" },
        LocalizedText{ TXT_MIME_ZIP, L"ZIP Archive" },
        LocalizedText{ TXT_MIME_ASIC_E, L"Associated Signature Container Extended" },
        LocalizedText{ TXT_MIME_ASIC_S, L"Associated Signature Container Simple" },
        LocalizedText{ TXT_MIME_PKCS7, L"PKCS #7 Cryptographic Message Syntax" },
        LocalizedText{ TXT_MIME_PKCS7_SIGNATURE, L"PKCS #7 Signature" },
        LocalizedText{ TXT_MIME_PKCS7_CERTIFICATES, L"PKCS #7 Certificates" },
        LocalizedText{ TXT_MIME_X509, L"X.509 Certificate" },
        LocalizedText{ TXT_MIME_TIMESTAMP, L"Time-Stamp Response" },
        LocalizedText{ TXT_DM_FILE_META_TYPE, L"File role" },
        LocalizedText{ TXT_DM_FILE_GUID, L"File identifier" },
        LocalizedText{ TXT_DM_UP_FILE_GUID, L"Parent file identifier" },
        LocalizedText{ TXT_DM_FILE_SIZE, L"File size" },
        LocalizedText{ TXT_USER_TYPE, L"User type" },
        LocalizedText{ TXT_DM_USER_TYPE, L"User type" },
        LocalizedText{ TXT_USER_NAME, L"User name" },
        LocalizedText{ TXT_DM_USER_NAME, L"User name" },
        LocalizedText{ TXT_PN_FIRST_NAME, L"First name" },
        LocalizedText{ TXT_PN_MIDDLE_NAME, L"Middle name" },
        LocalizedText{ TXT_PN_LAST_NAME, L"Last name" },
        LocalizedText{ TXT_PN_LAST_NAME_AT_BIRTH, L"Last name at birth" },
        LocalizedText{ TXT_FIRM_NAME, L"Organization name" },
        LocalizedText{ TXT_BI_DATE, L"Date of birth" },
        LocalizedText{ TXT_BI_CITY, L"Place of birth" },
        LocalizedText{ TXT_BI_COUNTY, L"County of birth" },
        LocalizedText{ TXT_BI_STATE, L"Country of birth" },
        LocalizedText{ TXT_AD_CITY, L"City" },
        LocalizedText{ TXT_AD_STREET, L"Street" },
        LocalizedText{ TXT_AD_NUMBER_IN_STREET, L"Street number" },
        LocalizedText{ TXT_AD_NUMBER_IN_MUNICIPALITY, L"House number" },
        LocalizedText{ TXT_AD_ZIP_CODE, L"Postal code" },
        LocalizedText{ TXT_AD_STATE, L"Country" },
        LocalizedText{ TXT_IC, L"Organization ID" },
        LocalizedText{ TXT_DM_STATUS_CODE, L"Response status code" },
        LocalizedText{ TXT_DM_STATUS_MESSAGE, L"Response status message" },
        LocalizedText{ TXT_FILE_ROLE_MAIN, L"Main attachment" },
        LocalizedText{ TXT_FILE_ROLE_ENCLOSURE, L"Enclosure" },
        LocalizedText{ TXT_FILE_ROLE_SIGNATURE, L"Signature" },
        LocalizedText{ TXT_BOX_TYPE_0, L"System data box" },
        LocalizedText{ TXT_BOX_TYPE_10, L"Public authority" },
        LocalizedText{ TXT_BOX_TYPE_20, L"Legal entity" },
        LocalizedText{ TXT_BOX_TYPE_30, L"Self-employed individual" },
        LocalizedText{ TXT_BOX_TYPE_40, L"Individual" },
        LocalizedText{ TXT_MESSAGE_TYPE_V, L"Public message; a party is a public authority" },
        LocalizedText{ TXT_MESSAGE_TYPE_K, L"Postal data message" },
        LocalizedText{ TXT_MESSAGE_TYPE_I, L"Postal data message initiating a prepaid reply" },
        LocalizedText{ TXT_MESSAGE_TYPE_O, L"Reply paid by the initiating sender" },
        LocalizedText{ TXT_MESSAGE_TYPE_E, L"Postal data message paid from credit" },
        LocalizedText{ TXT_MESSAGE_TYPE_A, L"Subsidized message with a prepaid reply" },
        LocalizedText{ TXT_MESSAGE_TYPE_B, L"Subsidized message; prepaid reply used" },
        LocalizedText{ TXT_MESSAGE_TYPE_C, L"Subsidized message; prepaid reply expired" },
        LocalizedText{ TXT_MESSAGE_TYPE_G, L"Postal data message paid by a sponsor" },
        LocalizedText{ TXT_MESSAGE_TYPE_X, L"Initiating message; reply offer expired" },
        LocalizedText{ TXT_MESSAGE_TYPE_Y, L"Initiating message; prepaid reply used" },
        LocalizedText{ TXT_STATE_1, L"Submitted to ISDS" },
        LocalizedText{ TXT_STATE_2, L"Message and attachments timestamped" },
        LocalizedText{ TXT_STATE_3, L"Rejected by the antivirus check; infected attachment removed" },
        LocalizedText{ TXT_STATE_4, L"Delivered to the recipient data box" },
        LocalizedText{ TXT_STATE_5, L"Deemed delivery after the delivery period elapsed" },
        LocalizedText{ TXT_STATE_6, L"Delivered after an authorized reader signed in" },
        LocalizedText{ TXT_STATE_7, L"Read by the recipient" },
        LocalizedText{ TXT_STATE_8, L"Undeliverable; the recipient data box became inaccessible" },
        LocalizedText{ TXT_STATE_9, L"Message content deleted; envelope archived" },
        LocalizedText{ TXT_STATE_10, L"Stored in the data vault" },
        LocalizedText{ TXT_USER_TYPE_PRIMARY_USER, L"Authorized person" },
        LocalizedText{ TXT_USER_TYPE_ENTRUSTED_USER, L"Delegated user" },
        LocalizedText{ TXT_USER_TYPE_ADMINISTRATOR, L"Administrator" },
        LocalizedText{ TXT_USER_TYPE_OFFICIAL, L"Official" },
        LocalizedText{ TXT_INSTANCE_FAILED, L"The application startup lock could not be acquired." },
        LocalizedText{ TXT_FORWARD_FAILED, L"The running application did not accept the files." },
        LocalizedText{ TXT_OPTIONS, L"&Options" },
        LocalizedText{ TXT_EDIT, L"&Edit" },
        LocalizedText{ TXT_SELECT_ALL, L"Select &All\tCtrl+A" },
        LocalizedText{ TXT_FIND_MENU, L"&Find…\tCtrl+F" },
        LocalizedText{ TXT_FIND_NEXT_MENU, L"Find &Next\tF3" },
        LocalizedText{ TXT_FIND_PREVIOUS_MENU, L"Find &Previous\tShift+F3" },
        LocalizedText{ TXT_PRINT, L"&Print…\tCtrl+P" },
        LocalizedText{ TXT_PAGE_SETUP, L"Page Set&up…" },
        LocalizedText{ TXT_REGEX_INVALID, L"Invalid regular expression." },
        LocalizedText{ TXT_REGEX_ESCAPE, L"Invalid escape sequence." },
        LocalizedText{ TXT_REGEX_BRACKET, L"Missing or unexpected bracket." },
        LocalizedText{ TXT_REGEX_PARENTHESIS, L"Missing or unexpected parenthesis." },
        LocalizedText{ TXT_REGEX_REPEAT, L"Invalid repetition expression." },
        LocalizedText{ TXT_REGEX_RANGE, L"Invalid character range." },
        LocalizedText{ TXT_REGEX_COMPLEXITY, L"The expression is too complex to evaluate." },
        LocalizedText{ TXT_LANGUAGE_LABEL, L"&Language:" },
        LocalizedText{ TXT_FILE, L"&File" },
        LocalizedText{ TXT_OPEN, L"&Open...\tCtrl+O" },
        LocalizedText{ TXT_EXPORT, L"&Export…\tCtrl+E" },
        LocalizedText{ TXT_CLOSE_FILE, L"&Close\tCtrl+W" },
        LocalizedText{ TXT_SAVE_COPY_AS, L"Save a Cop&y As…" },
        LocalizedText{ TXT_SAVE_COPY_AS_TITLE, L"Save a Copy As" },
        LocalizedText{ TXT_EXIT, L"E&xit\tAlt+F4" },
        LocalizedText{ TXT_ATTACHMENT, L"&Attachment" },
        LocalizedText{ TXT_OPEN_ATTACHMENT, L"&Open attachment\tEnter" },
        LocalizedText{ TXT_SAVE_ATTACHMENT, L"&Save attachment...\tCtrl+S" },
        LocalizedText{ TXT_COPY, L"&Copy\tCtrl+C" },
        LocalizedText{ TXT_LANGUAGE, L"&Language" },
        LocalizedText{ TXT_HELP_MENU, L"&Help" },
        LocalizedText{ TXT_HELP_TEXT, L"&Help\tF1" },
        LocalizedText{ TXT_ABOUT_TEXT, L"&About" },
        LocalizedText{ TXT_ABOUT_WINDOW_TITLE, L"About ZFO Viewer" },
        LocalizedText{ TXT_MESSAGE, L"Message" },
        LocalizedText{ TXT_ATTACHMENTS, L"Attachments" },
        LocalizedText{ TXT_XML, L"XML Source" },
        LocalizedText{ TXT_FIELD, L"Field" },
        LocalizedText{ TXT_VALUE, L"Value" },
        LocalizedText{ TXT_NAME, L"File name" },
        LocalizedText{ TXT_SIZE, L"Size (bytes)" },
        LocalizedText{ TXT_TYPE, L"Content type" },
        LocalizedText{ TXT_READY, L"Ready" },
        LocalizedText{ TXT_NO_ATTACHMENTS, L"This message has no embedded attachments." },
        LocalizedText{ TXT_YES, L"Yes" },
        LocalizedText{ TXT_NO, L"No" },
        LocalizedText{ TXT_DM_ID, L"Message ID" },
        LocalizedText{ TXT_DM_ANNOTATION, L"Subject" },
        LocalizedText{ TXT_DM_SENDER, L"Name" },
        LocalizedText{ TXT_DB_ID_SENDER, L"Data-Box ID" },
        LocalizedText{ TXT_DM_SENDER_ADDRESS, L"Address" },
        LocalizedText{ TXT_DM_SENDER_ORG_UNIT, L"Organizational unit" },
        LocalizedText{ TXT_DM_RECIPIENT, L"Name" },
        LocalizedText{ TXT_DB_ID_RECIPIENT, L"Data-Box ID" },
        LocalizedText{ TXT_DM_RECIPIENT_ADDRESS, L"Address" },
        LocalizedText{ TXT_DM_RECIPIENT_ORG_UNIT, L"Organizational unit" },
        LocalizedText{ TXT_DM_DELIVERY_TIME, L"Delivery Time" },
        LocalizedText{ TXT_DM_ACCEPTANCE_TIME, L"Acceptance Time" },
        LocalizedText{ TXT_CENTRAL_EUROPE_TIME, L"CET" },
        LocalizedText{ TXT_CENTRAL_EUROPE_SUMMER_TIME, L"CEST" },
        LocalizedText{ TXT_DM_MESSAGE_STATUS, L"Message status" },
        LocalizedText{ TXT_DM_SENDER_REF_NUMBER, L"Our Reference Number" },
        LocalizedText{ TXT_DM_SENDER_IDENT, L"Our File Mark" },
        LocalizedText{ TXT_DM_RECIPIENT_REF_NUMBER, L"Your Reference Number" },
        LocalizedText{ TXT_DM_RECIPIENT_IDENT, L"Your File Mark" },
        LocalizedText{ TXT_DM_TO_HANDS, L"For the attention of" },
        LocalizedText{ TXT_DM_PERSONAL_DELIVERY, L"Personal delivery" },
        LocalizedText{ TXT_DM_ALLOW_SUBST_DELIVERY, L"Prohibited Acceptance through Fiction" },
        LocalizedText{ TXT_DM_LEGAL_TITLE, L"Mandate" },
        LocalizedText{ TXT_LEGAL_TITLE_PARAGRAPH, L"paragraph" },
        LocalizedText{ TXT_LEGAL_TITLE_LETTER, L"letter" },
        LocalizedText{ TXT_DM_LEGAL_TITLE_LAW, L"Legal authority — law" },
        LocalizedText{ TXT_DM_LEGAL_TITLE_YEAR, L"Legal authority — year" },
        LocalizedText{ TXT_DM_LEGAL_TITLE_SECT, L"Legal authority — section" },
        LocalizedText{ TXT_DM_LEGAL_TITLE_PAR, L"Legal authority — paragraph" },
        LocalizedText{ TXT_DM_LEGAL_TITLE_POINT, L"Legal authority — point" },
        LocalizedText{ TXT_SIGNATURE, L"Envelope signature" },
        LocalizedText{ TXT_SIGNATURE_VALID, L"Cryptographic signature verified." },
        LocalizedText{ TXT_SIGNATURE_INVALID, L"Cryptographic signature could not be verified." },
        LocalizedText{ TXT_SIGNER, L"Signed by" },
        LocalizedText{ TXT_TRUST, L"Signature trust" },
        LocalizedText{ TXT_TRUST_INFO, L"Certificate chains, revocation, and timestamps are not verified." },
        LocalizedText{ TXT_EVENT, L"Delivery event" },
        LocalizedText{ TXT_INVALID_ZFO, L"This file is not a readable ZFO message in CMS/PKCS#7 format." },
        LocalizedText{ TXT_INVALID_XML, L"The ZFO file contains invalid or unsupported ISDS XML." },
        LocalizedText{ TXT_DECODE_FAILED, L"The ZFO envelope could not be decoded." },
        LocalizedText{ TXT_EXTERNAL_ATTACHMENT,
            L"An attachment has no embedded content. External attachments are not supported."
        },
        LocalizedText{ TXT_INVALID_BASE64, L"An attachment contains invalid Base64 data." },
        LocalizedText{ TXT_SAVE_FAILED,
            L"The file could not be saved. Check the path, permissions, and available disk space."
        },
        LocalizedText{ TXT_OPEN_FAILED,
            L"The attachment could not be opened in its associated application."
        },
        LocalizedText{ TXT_OPEN_FOLDER_FAILED, L"The source folder cannot be opened." },
        LocalizedText{ TXT_UNEXPECTED_ERROR, L"The operation could not be completed." },
        LocalizedText{ TXT_ONE_FILE, L"Drop one ZFO file at a time." },
    }),
};

constexpr std::array<LocalizedLanguage, LANG_COUNT> localizedLanguages =
    DefineLocalization(languageDefinitions);

/// Maps message XML names and application keys to the checked localization catalog.
constexpr LocalizedTextKey localizedTextKeys[] = {
    LocalizedTextKey{ "libraryLIBRARY_RESOURCE_INVALID", TXT_LIBRARY_RESOURCE_INVALID },
    LocalizedTextKey{ "libraryLIBRARY_REQUIRES_X86", TXT_LIBRARY_REQUIRES_X86 },
    LocalizedTextKey{ "libraryLIBRARY_EXTRACTION_FAILED", TXT_LIBRARY_EXTRACTION_FAILED },
    LocalizedTextKey{ "libraryLIBRARY_DECOMPRESSION_FAILED", TXT_LIBRARY_DECOMPRESSION_FAILED },
    LocalizedTextKey{ "libraryINVALID_WINDOWS_DLL", TXT_INVALID_WINDOWS_DLL },
    LocalizedTextKey{ "libraryFILE_OPEN_FAILED", TXT_FILE_OPEN_FAILED },
    LocalizedTextKey{ "libraryCURL_LOAD_OPERATION", TXT_CURL_LOAD_OPERATION },
    LocalizedTextKey{ "exportXML_FORMAT", TXT_XML_FORMAT },
    LocalizedTextKey{ "exportWEBP_FORMAT", TXT_WEBP_FORMAT },
    LocalizedTextKey{ "exportWEBP_EXPORT_FAILED", TXT_WEBP_EXPORT_FAILED },
    LocalizedTextKey{ "exportTIFF_FORMAT", TXT_TIFF_FORMAT },
    LocalizedTextKey{ "exportTIFF_EXPORT_FAILED", TXT_TIFF_EXPORT_FAILED },
    LocalizedTextKey{ "exportTEXT_UTF8_FORMAT", TXT_TEXT_UTF8_FORMAT },
    LocalizedTextKey{ "exportTEXT_UTF8_BOM_FORMAT", TXT_TEXT_UTF8_BOM_FORMAT },
    LocalizedTextKey{ "exportTEXT_UTF16_LE_FORMAT", TXT_TEXT_UTF16_LE_FORMAT },
    LocalizedTextKey{ "exportTEXT_UTF16_BE_FORMAT", TXT_TEXT_UTF16_BE_FORMAT },
    LocalizedTextKey{ "exportTEXT_FORMAT", TXT_TEXT_FORMAT },
    LocalizedTextKey{ "exportTEXT_EXPORT_FAILED", TXT_TEXT_EXPORT_FAILED },
    LocalizedTextKey{ "exportTEXT_ENCODING_FAILED", TXT_TEXT_ENCODING_FAILED },
    LocalizedTextKey{ "exportHTML_FORMAT", TXT_HTML_FORMAT },
    LocalizedTextKey{ "exportXHTML_FORMAT", TXT_XHTML_FORMAT },
    LocalizedTextKey{ "exportODT_FORMAT", TXT_ODT_FORMAT },
    LocalizedTextKey{ "exportDOCX_FORMAT", TXT_DOCX_FORMAT },
    LocalizedTextKey{ "exportRTF_FORMAT", TXT_RTF_FORMAT },
    LocalizedTextKey{ "exportREPORT_IMAGE_CREATE_FAILED", TXT_REPORT_IMAGE_CREATE_FAILED },
    LocalizedTextKey{ "exportPNG_FORMAT", TXT_PNG_FORMAT },
    LocalizedTextKey{ "exportPNG_EXPORT_FAILED", TXT_PNG_EXPORT_FAILED },
    LocalizedTextKey{ "exportJPEG_FORMAT", TXT_JPEG_FORMAT },
    LocalizedTextKey{ "exportJPEG_EXPORT_FAILED", TXT_JPEG_EXPORT_FAILED },
    LocalizedTextKey{ "exportIMAGE_FORMAT_UNSUPPORTED", TXT_IMAGE_FORMAT_UNSUPPORTED },
    LocalizedTextKey{ "exportGIF_FORMAT", TXT_GIF_FORMAT },
    LocalizedTextKey{ "exportGIF_EXPORT_FAILED", TXT_GIF_EXPORT_FAILED },
    LocalizedTextKey{ "exportBMP_FORMAT", TXT_BMP_FORMAT },
    LocalizedTextKey{ "exportBMP_EXPORT_FAILED", TXT_BMP_EXPORT_FAILED },
    LocalizedTextKey{ "copyContext", TXT_COPY_CONTEXT },
    LocalizedTextKey{ "closeTabContext", TXT_CLOSE_TAB_CONTEXT },
    LocalizedTextKey{ "showInFolder", TXT_SHOW_IN_FOLDER },
    LocalizedTextKey{ "colorHighlighting", TXT_COLOR_HIGHLIGHTING },
    LocalizedTextKey{ "xmlSource", TXT_XML_SOURCE },
    LocalizedTextKey{ "sourcePrefix", TXT_SOURCE_PREFIX },
    LocalizedTextKey{ "ok", TXT_OK },
    LocalizedTextKey{ "cancel", TXT_CANCEL },
    LocalizedTextKey{ "apply", TXT_APPLY },
    LocalizedTextKey{ "closeOnDoubleClick", TXT_CLOSE_ON_DOUBLE_CLICK },
    LocalizedTextKey{ "titleNameLengthInvalid", TXT_TITLE_NAME_LENGTH_INVALID },
    LocalizedTextKey{ "titleNameLength", TXT_TITLE_NAME_LENGTH },
    LocalizedTextKey{ "tabNameLengthInvalid", TXT_TAB_NAME_LENGTH_INVALID },
    LocalizedTextKey{ "clearRecentFiles", TXT_CLEAR_RECENT_FILES },
    LocalizedTextKey{ "recentFiles", TXT_RECENT_FILES },
    LocalizedTextKey{ "tabNameLength", TXT_TAB_NAME_LENGTH },
    LocalizedTextKey{ "copyFullPath", TXT_COPY_FULL_PATH },
    LocalizedTextKey{ "copyFileName", TXT_COPY_FILE_NAME },
    LocalizedTextKey{ "closeAll", TXT_CLOSE_ALL },
    LocalizedTextKey{ "openFolderTitle", TXT_OPEN_FOLDER_TITLE },
    LocalizedTextKey{ "openFolder", TXT_OPEN_FOLDER },
    LocalizedTextKey{ "applicationTitle", TXT_APPLICATION_TITLE },
    LocalizedTextKey{ "helpCaption", TXT_HELP },
    LocalizedTextKey{ "aboutPlatformLabel", TXT_ABOUT_PLATFORM_LABEL },
    LocalizedTextKey{ "aboutCaption", TXT_ABOUT },
    LocalizedTextKey{ "languageName", TXT_LANGUAGE_NAME },
    LocalizedTextKey{ "view", TXT_VIEW },
    LocalizedTextKey{ "preferences", TXT_PREFERENCES },
    LocalizedTextKey{ "preferencesTitle", TXT_PREFERENCES_TITLE },
    LocalizedTextKey{ "multipleInstances", TXT_MULTIPLE_INSTANCES },
    LocalizedTextKey{ "smoothScrolling", TXT_SMOOTH_SCROLLING },
    LocalizedTextKey{ "messageFont", TXT_MESSAGE_FONT },
    LocalizedTextKey{ "sourceFont", TXT_SOURCE_FONT },
    LocalizedTextKey{ "fontDefault", TXT_FONT_DEFAULT },
    LocalizedTextKey{ "systemDefaultFontLabel", TXT_SYSTEM_DEFAULT_FONT_LABEL },
    LocalizedTextKey{ "fontRegular", TXT_FONT_REGULAR },
    LocalizedTextKey{ "fontBold", TXT_FONT_BOLD },
    LocalizedTextKey{ "fontItalic", TXT_FONT_ITALIC },
    LocalizedTextKey{ "fontBoldItalic", TXT_FONT_BOLD_ITALIC },
    LocalizedTextKey{ "fontFailed", TXT_FONT_FAILED },
    LocalizedTextKey{ "highlightMessage", TXT_HIGHLIGHT_MESSAGE },
    LocalizedTextKey{ "highlightSource", TXT_HIGHLIGHT_SOURCE },
    LocalizedTextKey{ "lowercaseExtensions", TXT_LOWERCASE_EXTENSIONS },
    LocalizedTextKey{ "settingsFailed", TXT_SETTINGS_FAILED },
    LocalizedTextKey{ "richEditFailed", TXT_RICH_EDIT_FAILED },
    LocalizedTextKey{ "senderSection", TXT_SENDER_SECTION },
    LocalizedTextKey{ "recipientSection", TXT_RECIPIENT_SECTION },
    LocalizedTextKey{ "deliverySection", TXT_DELIVERY_SECTION },
    LocalizedTextKey{ "legalSection", TXT_LEGAL_SECTION },
    LocalizedTextKey{ "signatureSection", TXT_SIGNATURE_SECTION },
    LocalizedTextKey{ "saveLink", TXT_SAVE_LINK },
    LocalizedTextKey{ "bytes", TXT_BYTES },
    LocalizedTextKey{ "generalSection", TXT_GENERAL_SECTION },
    LocalizedTextKey{ "senderPersonSection", TXT_SENDER_PERSON_SECTION },
    LocalizedTextKey{ "additionalSection", TXT_ADDITIONAL_SECTION },
    LocalizedTextKey{ "statusSection", TXT_STATUS_SECTION },
    LocalizedTextKey{ "eventsSection", TXT_EVENTS_SECTION },
    LocalizedTextKey{ "notProvided", TXT_NOT_PROVIDED },
    LocalizedTextKey{ "dmAttachmentSize", TXT_DM_ATTACHMENT_SIZE },
    LocalizedTextKey{ "dmType", TXT_DM_TYPE },
    LocalizedTextKey{ "dmSenderType", TXT_DM_SENDER_TYPE },
    LocalizedTextKey{ "dmRecipientType", TXT_DM_RECIPIENT_TYPE },
    LocalizedTextKey{ "dmSenderOrgUnitNum", TXT_DM_SENDER_ORG_UNIT_NUM },
    LocalizedTextKey{ "dmRecipientOrgUnitNum", TXT_DM_RECIPIENT_ORG_UNIT_NUM },
    LocalizedTextKey{ "dmVODZ", TXT_DM_VODZ },
    LocalizedTextKey{ "attsNum", TXT_ATTS_NUM },
    LocalizedTextKey{ "dmHash", TXT_DM_HASH },
    LocalizedTextKey{ "algorithm", TXT_ALGORITHM },
    LocalizedTextKey{ "dmQTimestamp", TXT_DM_Q_TIMESTAMP },
    LocalizedTextKey{ "dmEventTime", TXT_DM_EVENT_TIME },
    LocalizedTextKey{ "dmEventDescr", TXT_DM_EVENT_DESCR },
    LocalizedTextKey{ "dmFileDescr", TXT_DM_FILE_DESCR },
    LocalizedTextKey{ "dmMimeType", TXT_DM_MIME_TYPE },
    LocalizedTextKey{ "dmFileMetaType", TXT_DM_FILE_META_TYPE },
    LocalizedTextKey{ "dmFileGuid", TXT_DM_FILE_GUID },
    LocalizedTextKey{ "dmUpFileGuid", TXT_DM_UP_FILE_GUID },
    LocalizedTextKey{ "dmFileSize", TXT_DM_FILE_SIZE },
    LocalizedTextKey{ "userType", TXT_USER_TYPE },
    LocalizedTextKey{ "dmUserType", TXT_DM_USER_TYPE },
    LocalizedTextKey{ "userName", TXT_USER_NAME },
    LocalizedTextKey{ "dmUserName", TXT_DM_USER_NAME },
    LocalizedTextKey{ "pnFirstName", TXT_PN_FIRST_NAME },
    LocalizedTextKey{ "pnMiddleName", TXT_PN_MIDDLE_NAME },
    LocalizedTextKey{ "pnLastName", TXT_PN_LAST_NAME },
    LocalizedTextKey{ "pnLastNameAtBirth", TXT_PN_LAST_NAME_AT_BIRTH },
    LocalizedTextKey{ "firmName", TXT_FIRM_NAME },
    LocalizedTextKey{ "biDate", TXT_BI_DATE },
    LocalizedTextKey{ "biCity", TXT_BI_CITY },
    LocalizedTextKey{ "biCounty", TXT_BI_COUNTY },
    LocalizedTextKey{ "biState", TXT_BI_STATE },
    LocalizedTextKey{ "adCity", TXT_AD_CITY },
    LocalizedTextKey{ "adStreet", TXT_AD_STREET },
    LocalizedTextKey{ "adNumberInStreet", TXT_AD_NUMBER_IN_STREET },
    LocalizedTextKey{ "adNumberInMunicipality", TXT_AD_NUMBER_IN_MUNICIPALITY },
    LocalizedTextKey{ "adZipCode", TXT_AD_ZIP_CODE },
    LocalizedTextKey{ "adState", TXT_AD_STATE },
    LocalizedTextKey{ "ic", TXT_IC },
    LocalizedTextKey{ "dmStatusCode", TXT_DM_STATUS_CODE },
    LocalizedTextKey{ "dmStatusMessage", TXT_DM_STATUS_MESSAGE },
    LocalizedTextKey{ "fileRole_main", TXT_FILE_ROLE_MAIN },
    LocalizedTextKey{ "fileRole_enclosure", TXT_FILE_ROLE_ENCLOSURE },
    LocalizedTextKey{ "fileRole_signature", TXT_FILE_ROLE_SIGNATURE },
    LocalizedTextKey{ "boxType_0", TXT_BOX_TYPE_0 },
    LocalizedTextKey{ "boxType_10", TXT_BOX_TYPE_10 },
    LocalizedTextKey{ "boxType_20", TXT_BOX_TYPE_20 },
    LocalizedTextKey{ "boxType_30", TXT_BOX_TYPE_30 },
    LocalizedTextKey{ "boxType_40", TXT_BOX_TYPE_40 },
    LocalizedTextKey{ "messageType_V", TXT_MESSAGE_TYPE_V },
    LocalizedTextKey{ "messageType_K", TXT_MESSAGE_TYPE_K },
    LocalizedTextKey{ "messageType_I", TXT_MESSAGE_TYPE_I },
    LocalizedTextKey{ "messageType_O", TXT_MESSAGE_TYPE_O },
    LocalizedTextKey{ "messageType_E", TXT_MESSAGE_TYPE_E },
    LocalizedTextKey{ "messageType_A", TXT_MESSAGE_TYPE_A },
    LocalizedTextKey{ "messageType_B", TXT_MESSAGE_TYPE_B },
    LocalizedTextKey{ "messageType_C", TXT_MESSAGE_TYPE_C },
    LocalizedTextKey{ "messageType_G", TXT_MESSAGE_TYPE_G },
    LocalizedTextKey{ "messageType_X", TXT_MESSAGE_TYPE_X },
    LocalizedTextKey{ "messageType_Y", TXT_MESSAGE_TYPE_Y },
    LocalizedTextKey{ "state_1", TXT_STATE_1 },
    LocalizedTextKey{ "state_2", TXT_STATE_2 },
    LocalizedTextKey{ "state_3", TXT_STATE_3 },
    LocalizedTextKey{ "state_4", TXT_STATE_4 },
    LocalizedTextKey{ "state_5", TXT_STATE_5 },
    LocalizedTextKey{ "state_6", TXT_STATE_6 },
    LocalizedTextKey{ "state_7", TXT_STATE_7 },
    LocalizedTextKey{ "state_8", TXT_STATE_8 },
    LocalizedTextKey{ "state_9", TXT_STATE_9 },
    LocalizedTextKey{ "state_10", TXT_STATE_10 },
    LocalizedTextKey{ "userType_PRIMARY_USER", TXT_USER_TYPE_PRIMARY_USER },
    LocalizedTextKey{ "userType_ENTRUSTED_USER", TXT_USER_TYPE_ENTRUSTED_USER },
    LocalizedTextKey{ "userType_ADMINISTRATOR", TXT_USER_TYPE_ADMINISTRATOR },
    LocalizedTextKey{ "userType_OFFICIAL", TXT_USER_TYPE_OFFICIAL },
    LocalizedTextKey{ "instanceFailed", TXT_INSTANCE_FAILED },
    LocalizedTextKey{ "forwardFailed", TXT_FORWARD_FAILED },
    LocalizedTextKey{ "componentAboutTitle", TXT_ABOUT_TITLE },
    LocalizedTextKey{ "copyInformation", TXT_COPY_INFORMATION },
    LocalizedTextKey{ "copyInformationFailed", TXT_ABOUT_COPY_INFORMATION_FAILED },
    LocalizedTextKey{ "copyUrl", TXT_COPY_URL },
    LocalizedTextKey{ "copyUrlFailed", TXT_ABOUT_COPY_URL_FAILED },
    LocalizedTextKey{ "findBackward", TXT_FIND_BACKWARD },
    LocalizedTextKey{ "findEndsWith", TXT_FIND_ENDS_WITH },
    LocalizedTextKey{ "findFailed", TXT_FIND_FAILED },
    LocalizedTextKey{ "findMatchCase", TXT_FIND_MATCH_CASE },
    LocalizedTextKey{ "findNext", TXT_FIND_NEXT },
    LocalizedTextKey{ "findNotFound", TXT_FIND_NOT_FOUND },
    LocalizedTextKey{ "findRegex", TXT_FIND_REGEX },
    LocalizedTextKey{ "findRegexError", TXT_FIND_REGEX_ERROR },
    LocalizedTextKey{ "findStartsWith", TXT_FIND_STARTS_WITH },
    LocalizedTextKey{ "findTitle", TXT_FIND_TITLE },
    LocalizedTextKey{ "findTopmost", TXT_FIND_TOPMOST },
    LocalizedTextKey{ "findWhat", TXT_FIND_WHAT },
    LocalizedTextKey{ "licenseUnavailable", TXT_ABOUT_LICENSE_UNAVAILABLE },
    LocalizedTextKey{ "openWebsite", TXT_OPEN_IN_BROWSER },
    LocalizedTextKey{ "pageSetupFailed", TXT_PAGE_SETUP_FAILED },
    LocalizedTextKey{ "printFailed", TXT_PRINT_FAILED },
    LocalizedTextKey{ "websiteOpenFailed", TXT_ABOUT_WEBSITE_FAILED },
    LocalizedTextKey{ "websiteTooltip", TXT_ABOUT_VISIT_TOOLTIP },
    LocalizedTextKey{ "options", TXT_OPTIONS },
    LocalizedTextKey{ "edit", TXT_EDIT },
    LocalizedTextKey{ "selectAll", TXT_SELECT_ALL },
    LocalizedTextKey{ "findMenu", TXT_FIND_MENU },
    LocalizedTextKey{ "findNextMenu", TXT_FIND_NEXT_MENU },
    LocalizedTextKey{ "findPreviousMenu", TXT_FIND_PREVIOUS_MENU },
    LocalizedTextKey{ "print", TXT_PRINT },
    LocalizedTextKey{ "pageSetup", TXT_PAGE_SETUP },
    LocalizedTextKey{ "regexInvalid", TXT_REGEX_INVALID },
    LocalizedTextKey{ "regexEscape", TXT_REGEX_ESCAPE },
    LocalizedTextKey{ "regexBracket", TXT_REGEX_BRACKET },
    LocalizedTextKey{ "regexParenthesis", TXT_REGEX_PARENTHESIS },
    LocalizedTextKey{ "regexRepeat", TXT_REGEX_REPEAT },
    LocalizedTextKey{ "regexRange", TXT_REGEX_RANGE },
    LocalizedTextKey{ "regexComplexity", TXT_REGEX_COMPLEXITY },
    LocalizedTextKey{ "languageLabel", TXT_LANGUAGE_LABEL },
    LocalizedTextKey{ "file", TXT_FILE },
    LocalizedTextKey{ "open", TXT_OPEN },
    LocalizedTextKey{ "export", TXT_EXPORT },
    LocalizedTextKey{ "closeFile", TXT_CLOSE_FILE },
    LocalizedTextKey{ "saveCopyAs", TXT_SAVE_COPY_AS },
    LocalizedTextKey{ "saveCopyAsTitle", TXT_SAVE_COPY_AS_TITLE },
    LocalizedTextKey{ "exit", TXT_EXIT },
    LocalizedTextKey{ "attachment", TXT_ATTACHMENT },
    LocalizedTextKey{ "openAttachment", TXT_OPEN_ATTACHMENT },
    LocalizedTextKey{ "saveAttachment", TXT_SAVE_ATTACHMENT },
    LocalizedTextKey{ "copy", TXT_COPY },
    LocalizedTextKey{ "language", TXT_LANGUAGE },
    LocalizedTextKey{ "helpMenu", TXT_HELP_MENU },
    LocalizedTextKey{ "help", TXT_HELP_TEXT },
    LocalizedTextKey{ "about", TXT_ABOUT_TEXT },
    LocalizedTextKey{ "aboutTitle", TXT_ABOUT_WINDOW_TITLE },
    LocalizedTextKey{ "helpTitle", TXT_HELP_TITLE },
    LocalizedTextKey{ "close", TXT_CLOSE },
    LocalizedTextKey{ "website", TXT_ABOUT_WEBSITE_LABEL },
    LocalizedTextKey{ "description", TXT_ABOUT_DESCRIPTION },
    LocalizedTextKey{ "version", TXT_ABOUT_VERSION_LABEL },
    LocalizedTextKey{ "platform", TXT_PLATFORM },
    LocalizedTextKey{ "message", TXT_MESSAGE },
    LocalizedTextKey{ "attachments", TXT_ATTACHMENTS },
    LocalizedTextKey{ "xml", TXT_XML },
    LocalizedTextKey{ "field", TXT_FIELD },
    LocalizedTextKey{ "value", TXT_VALUE },
    LocalizedTextKey{ "name", TXT_NAME },
    LocalizedTextKey{ "size", TXT_SIZE },
    LocalizedTextKey{ "type", TXT_TYPE },
    LocalizedTextKey{ "ready", TXT_READY },
    LocalizedTextKey{ "noAttachments", TXT_NO_ATTACHMENTS },
    LocalizedTextKey{ "yes", TXT_YES },
    LocalizedTextKey{ "no", TXT_NO },
    LocalizedTextKey{ "dmID", TXT_DM_ID },
    LocalizedTextKey{ "dmAnnotation", TXT_DM_ANNOTATION },
    LocalizedTextKey{ "dmSender", TXT_DM_SENDER },
    LocalizedTextKey{ "dbIDSender", TXT_DB_ID_SENDER },
    LocalizedTextKey{ "dmSenderAddress", TXT_DM_SENDER_ADDRESS },
    LocalizedTextKey{ "dmSenderOrgUnit", TXT_DM_SENDER_ORG_UNIT },
    LocalizedTextKey{ "dmRecipient", TXT_DM_RECIPIENT },
    LocalizedTextKey{ "dbIDRecipient", TXT_DB_ID_RECIPIENT },
    LocalizedTextKey{ "dmRecipientAddress", TXT_DM_RECIPIENT_ADDRESS },
    LocalizedTextKey{ "dmRecipientOrgUnit", TXT_DM_RECIPIENT_ORG_UNIT },
    LocalizedTextKey{ "dmDeliveryTime", TXT_DM_DELIVERY_TIME },
    LocalizedTextKey{ "dmAcceptanceTime", TXT_DM_ACCEPTANCE_TIME },
    LocalizedTextKey{ "dmMessageStatus", TXT_DM_MESSAGE_STATUS },
    LocalizedTextKey{ "dmSenderRefNumber", TXT_DM_SENDER_REF_NUMBER },
    LocalizedTextKey{ "dmSenderIdent", TXT_DM_SENDER_IDENT },
    LocalizedTextKey{ "dmRecipientRefNumber", TXT_DM_RECIPIENT_REF_NUMBER },
    LocalizedTextKey{ "dmRecipientIdent", TXT_DM_RECIPIENT_IDENT },
    LocalizedTextKey{ "dmToHands", TXT_DM_TO_HANDS },
    LocalizedTextKey{ "dmPersonalDelivery", TXT_DM_PERSONAL_DELIVERY },
    LocalizedTextKey{ "dmAllowSubstDelivery", TXT_DM_ALLOW_SUBST_DELIVERY },
    LocalizedTextKey{ "dmLegalTitle", TXT_DM_LEGAL_TITLE },
    LocalizedTextKey{ "dmLegalTitleLaw", TXT_DM_LEGAL_TITLE_LAW },
    LocalizedTextKey{ "dmLegalTitleYear", TXT_DM_LEGAL_TITLE_YEAR },
    LocalizedTextKey{ "dmLegalTitleSect", TXT_DM_LEGAL_TITLE_SECT },
    LocalizedTextKey{ "dmLegalTitlePar", TXT_DM_LEGAL_TITLE_PAR },
    LocalizedTextKey{ "dmLegalTitlePoint", TXT_DM_LEGAL_TITLE_POINT },
    LocalizedTextKey{ "signature", TXT_SIGNATURE },
    LocalizedTextKey{ "signatureValid", TXT_SIGNATURE_VALID },
    LocalizedTextKey{ "signatureInvalid", TXT_SIGNATURE_INVALID },
    LocalizedTextKey{ "signer", TXT_SIGNER },
    LocalizedTextKey{ "trust", TXT_TRUST },
    LocalizedTextKey{ "trustInfo", TXT_TRUST_INFO },
    LocalizedTextKey{ "event", TXT_EVENT },
    LocalizedTextKey{ "invalidZfo", TXT_INVALID_ZFO },
    LocalizedTextKey{ "invalidXml", TXT_INVALID_XML },
    LocalizedTextKey{ "decodeFailed", TXT_DECODE_FAILED },
    LocalizedTextKey{ "externalAttachment", TXT_EXTERNAL_ATTACHMENT },
    LocalizedTextKey{ "invalidBase64", TXT_INVALID_BASE64 },
    LocalizedTextKey{ "saveFailed", TXT_SAVE_FAILED },
    LocalizedTextKey{ "openFailed", TXT_OPEN_FAILED },
    LocalizedTextKey{ "openFolderFailed", TXT_OPEN_FOLDER_FAILED },
    LocalizedTextKey{ "unexpectedError", TXT_UNEXPECTED_ERROR },
    LocalizedTextKey{ "oneFile", TXT_ONE_FILE },
    LocalizedTextKey{ "helpText", TXT_APPLICATION_HELP },
};

/// Active language used by shared components that request text through identifiers.
static AppLanguage currentLanguage = LANG_EN_US;

/// Returns the compile-time checked translation for a valid language and named text key.
const wchar_t* GetLocalizedText(AppLanguage language, TextId id) {
    return localizedLanguages[language].texts[id];
}

/// Returns the translated XML field or application key, or empty text when the key is unknown.
const wchar_t* GetLocalizedText(AppLanguage language, std::string_view key) {
    for (const LocalizedTextKey& entry : localizedTextKeys) {
        if (entry.key == key) {
            return GetLocalizedText(language, entry.id);
        }
    }
    return L"";
}

/// Maps Czech and Slovak Windows language identifiers to supported languages, otherwise using US English.
AppLanguage ApplicationLanguageFromLangId(LANGID language) {
    switch (PRIMARYLANGID(language)) {
        case LANG_CZECH:
            return LANG_CS;
        case LANG_SLOVAK:
            return LANG_SK;
        default:
            return LANG_EN_US;
    }
}

/// Returns the interface language currently used by shared components.
int GetApplicationLanguage() {
    return currentLanguage;
}

/// Sets the active interface language, falling back to US English for an unsupported index.
void SetApplicationLanguage(int language) {
    currentLanguage = language >= 0 && language < LANG_COUNT
        ? static_cast<AppLanguage>(language)
        : LANG_EN_US;
}

/// Returns the complete Help text in the active interface language.
const wchar_t* LocalizedHelpText() {
    return GetLocalizedText(currentLanguage, TXT_APPLICATION_HELP);
}

/// Returns the requested shared-component text in the active interface language.
const wchar_t* T(TextId id) {
    return GetLocalizedText(currentLanguage, id);
}
