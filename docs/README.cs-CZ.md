# Prohlížeč datových zpráv

[English](../README.md) · [Slovensky](README.sk-SK.md)

Nativní aplikace Win32 pro zprávy a doručenky ISDS ve formátu ZFO.
Verze 1.0.0.0 je ve vývoji a dosud nebyla vydána.

## Soubory a karty

Zprávy otevřete přes **Soubor > Otevřít** (`Ctrl+O`), **Otevřít složku**, přetažením nebo
předáním názvů souborů na příkazovém řádku. Každá zpráva má vlastní kartu. Opětovné otevření
stejného souboru vybere existující kartu. `Ctrl+Tab` a `Ctrl+Shift+Tab` přepínají karty,
`Ctrl+W` a prostřední tlačítko kartu zavřou. **Zavřít vše** zavře všechny karty.
Nabídka Soubor obsahuje osm posledních souborů.

**Soubor > Uložit kopii jako** uloží nezměněnou kopii původního ZFO včetně obálky a příloh.
Aktuální karta zůstane u původního souboru. U položky **Konec** je uvedeno `Alt+F4`.
**Soubor > Zobrazit ve složce** vybere aktuální ZFO v Průzkumníku. Stejný příkaz je v nabídce karty.

Přehled zahrnuje všechna metadata XML, včetně prázdných polí, atributů a rozšíření.
Datum a čas se zobrazují podle kultury zvoleného jazyka, také v exportech. Časy obsahují hodiny a minuty
ve středoevropském čase (SEČ/SELČ). České a slovenské datum má mezery za tečkami a nemá úvodní nuly.
Zdrojové XML a export XML uchovávají původní sekundy, zlomky sekund i číselný posun časového pásma.
Kliknutí na přílohu ji otevře, sousední odkaz Uložit uloží její kopii. Každá karta má vlastní krátký
dočasný adresář, například `%LOCALAPPDATA%\FortSoft\ZfoViewer\ZFO3A7C.tmp\faktura.pdf`. Názvy příloh
zůstávají zachované. Opakované otevření ve stejné kartě použije nezměněnou kopii; jiná karta má
vždy jiný adresář, i když otevřené soubory ZFO mají stejný název.

Při zavření karty se aplikace pokusí její adresář rekurzivně odstranit. Zamčené soubory přeskočí
a úklid zopakuje při spuštění, otevírání příloh, zavírání karet a ukončení aplikace. Úklid zachovává
adresáře používané jinou spuštěnou instancí a neprochází adresářové odkazy.

**Zobrazení > Zobrazit zdroj XML** (`Ctrl+U`) otevře odsazené XML v nové kartě. Opakovaný příkaz
vybere již otevřenou zdrojovou kartu stejné zprávy. Značky, atributy, hodnoty, entity a komentáře jsou
barevně zvýrazněné při zachování systémového písma. Vysoký kontrast ponechává systémové barvy textu.
Zobrazení a export XML používají stejné formátování. Příkaz pro zobrazení zdroje je na zdrojové kartě neaktivní.
**Zobrazení > Barevné zvýraznění** přepíná zvýraznění aktivního zobrazení a sdílí uloženou volbu s Předvolbami.
Zprávy a XML mají nezávislé nastavení; při přepnutí karty se podle něj aktualizuje zaškrtnutí nabídky.

Kontextová nabídka textu obsahuje Kopírovat a Vybrat vše bez klávesových zkratek.
Nabídka karty obsahuje kopírování názvu souboru, celé cesty, zobrazení ve složce a zavření karty.
Stavový řádek zobrazuje Připraven nebo cestu aktivního souboru a umožňuje svůj text zkopírovat.

## Hledání, export a tisk

**Úpravy > Najít** (`Ctrl+F`), `F3` a `Shift+F3` hledají ve zprávě, zdrojovém XML, nápovědě i licenci.
Regulární výrazy používají ECMAScript ze staticky připojené běhové knihovny C++, bez PCRE2.
Výběr textu zajišťuje RichEdit; běžná textová pole nápovědy a okna O programu používají SelectionEdit.

**Soubor > Exportovat** (`Ctrl+E`) nabízí v tomto pořadí ANSI, UTF-8, UTF-8 s BOM, UTF-16 BE,
UTF-16 LE, RTF, ODT, DOCX, HTML 4.01 Strict, XHTML 1.1, odsazené XML, BMP, GIF, JPEG, PNG, TIFF a bezeztrátové WebP.
Textové kodéry vycházejí z ověřených vzorových implementací. Všech šest obrazových formátů používá stejnou
komponentu ImageExport jako spořiče: nativní kodeky Windows pro BMP, GIF, JPEG, PNG a TIFF
a přibalenou knihovnu WebP s komponentou PackedResource pro bezeztrátové WebP.
ODT 1.2 a DOCX vznikají přímo z metadat zprávy nebo zdrojového XML společným vestavěným zápisem ZIP.
Zachovávají sekce, tučné popisky, Unicode, mezery, tabulátory a konce řádků bez instalace Office,
OpenOffice, LibreOffice, .NET nebo další DLL.
HTML, XHTML, ODT a DOCX přebírají rodinu, velikost písma a výšku řádku z aktivního RichEditu.
Obrazové exporty zachycují nativní vykreslení včetně neviditelné části zprávy, aktuálního zalamování,
zvýraznění a barev odkazů. Zachovávají původní výběr a polohu posunu. Při překročení mezí formátu nebo
paměti se export odmítne bez oříznutí zprávy.
HTML a XHTML používají UTF-8 bez BOM a zachovávají lokalizované sekce, metadata a popisy příloh.
Export zdrojové karty zachovává XML jako escapovaný předformátovaný text.
RTF ukládá přímo RichEdit, XML formátuje XmlLite. Dodaná x86 knihovna libwebp je zabalená do EXE.
Ikony a knihovna jsou v Resources.

V nabídce Soubor je nejprve **Vzhled stránky**, potom **Tisk** (`Ctrl+P`). Tisk funguje také
v nápovědě a okně O programu. Windows 11 používá moderní tiskové rozhraní, starší systémy klasickou cestu.
Tisk zprávy a XML zachovává zvolená písma, barvy, tučný text, kurzívu i podtržení.

## Předvolby

První dva řádky vybírají nezávisle písmo zprávy a XML zdroje. Tlačítka Výchozí obnoví aktuální systémové
písmo zprávy a Consolas 9 bodů pro zdroj. Písmo rozhraní, nápovědy ani licence se nemění.
Obě zvýraznění jsou výchozí zapnutá. U zprávy se mění pouze barva nadpisů sekcí na systémovou barvu odkazů;
po vypnutí zůstávají tučné nadpisy, popisky i odkazy. XML používá styl Firefoxu včetně tučných názvů značek
a atributů. HTML, XHTML, ODT, DOCX, RTF i obrazové exporty respektují písmo obsahu a stav zvýraznění.
ODT a DOCX vznikají v paměti a zapisují se přímo do cíle bez dočasných souborů.

**Možnosti > Předvolby** (`Ctrl+G`) nabízejí v tomto pořadí češtinu, slovenštinu a americkou angličtinu.
Při prvním spuštění se jazyk odvodí od rozhraní Windows; ostatní jazyky použijí americkou angličtinu.
Volba Automaticky není v seznamu. Ručně vybraný jazyk se zapamatuje.

Dále lze nastavit délku názvu karty (výchozí 80) a názvu v záhlaví (výchozí 40), zavírání dvojklikem,
více instancí nebo vymazat
poslední soubory a zapnout plynulé posouvání. OK a Použít uloží připravené změny, Zrušit zahodí změny od posledního Použít.
Vymazání historie je okamžité. Výchozí je jediná instance; další spuštění jí předají soubory.
Plynulé posouvání je ve výchozím stavu vypnuté. Běžné kolečko myši posouvá zprávu i zdrojové XML
okamžitě podle systémové volby řádků či stránek. Ctrl+kolečko zachovává standardní chování RichEditu.
Předvolby mají kompaktní nativní odsazení a při malé výšce pracovní plochy svislý posuvník.

Rozhraní používá systémové písmo; licence v okně O programu používá neproporcionální písmo ze vzoru.
PersistWindowState obnovuje stav oken a při chybějících
rozměrech hlavního okna použije Windows Default Bounds. Nápověda a O programu vycházejí
z existujících aplikačních formulářů. Předvolby a O programu si pamatují polohu jen po dobu běžící instance.
Nápověda a Hledání ukládají stav svých oken do registru. Hledané výrazy a historie zůstávají pouze pro aktuální běh.
Překlady i nápověda jsou v Localization.cpp.

Nastavení je v `HKEY_CURRENT_USER\Software\FortSoft\ZfoViewer`. Přepínače sdílejí DWORD Options:
více instancí má bit `0x04`, zavírání dvojklikem `0x40`, stejně jako ve vzorovém projektu.
Plynulé posouvání používá bit `0x80`.

## Sestavení

Otevřete ZfoViewer.slnx ve Visual Studiu 2026 nebo v jeho vývojářském příkazovém řádku spusťte:

```text
msbuild ZfoViewer.slnx /p:Configuration=Release /p:Platform=x86
```

Projekt cílí na Windows Vista a novější, používá v145, Windows SDK 10.0, Unicode Win32, C++20
a staticky připojenou běhovou knihovnu v Release. Varování překladače mají v obou konfiguracích
úroveň 4 (`/W4`). Nastavení SDK a linkeru používá výchozí hodnoty jako vzorové projekty, bez výslovného
přepisování verze Windows. Windows XP není podporován.
Výstupy jsou v kořeni řešení ve složce zvolené konfigurace, stejně jako ve vzorových projektech.

Příprava prostředků zabalí dodanou knihovnu WebP. PackageRelease.ps1 vychází z existujícího
postupu podepisování a balení již sestaveného Release. Běžné sestavení nevytváří ZIP.

Dočasné testy zůstávají v tests a jsou vyloučené z Gitu. Všechny jejich výstupy jsou také pod tests.
Spouštějí se skriptem tests/RunTests.ps1 na izolované ploše bez aktivace oken, s odděleným registrem
a náhradou schránky. Nezobrazují tiskové dialogy ani neposílají fyzické tiskové úlohy.

## Podpis a licence

Crypt32 dekóduje CMS/PKCS#7, MSXML 6 zpracuje XML ISDS. DTD a externí zdroje XML jsou zakázané.
Externí přílohy a staré formuláře Software602 nejsou podporované. Kontroluje se kryptografický podpis
obálky, nikoli důvěryhodnost certifikátů, odvolání, časová razítka nebo dlouhodobá platnost.

Aplikace má licenci MIT. Přiložená libwebp má vlastní licenci; viz [license.txt](../license.txt).
