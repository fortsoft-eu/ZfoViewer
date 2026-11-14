# Prehliadač dátových správ

[English](../README.md) · [Česky](README.cs-CZ.md)

Natívna aplikácia Win32 pre správy a doručenky ISDS vo formáte ZFO.
Verzia 1.0.0.0 je vo vývoji a zatiaľ nebola vydaná.

## Súbory a karty

Správy otvoríte cez **Súbor > Otvoriť** (`Ctrl+O`), **Otvoriť priečinok**, presunutím alebo
zadaním názvov súborov na príkazovom riadku. Každá správa má vlastnú kartu. Opätovné otvorenie
rovnakého súboru vyberie existujúcu kartu. `Ctrl+Tab` a `Ctrl+Shift+Tab` prepínajú karty,
`Ctrl+W` a prostredné tlačidlo kartu zatvoria. **Zavrieť všetko** zatvorí všetky karty.
Ponuka Súbor obsahuje osem posledných súborov.

**Súbor > Uložiť kópiu ako** uloží nezmenenú kópiu pôvodného ZFO vrátane obálky a príloh.
Aktuálna karta zostane pri pôvodnom súbore. Pri položke **Koniec** je uvedené `Alt+F4`.
**Súbor > Zobraziť v priečinku** vyberie aktuálne ZFO v Prieskumníkovi. Rovnaký príkaz je v ponuke karty.

Prehľad zahŕňa všetky metadáta XML vrátane prázdnych polí, atribútov a rozšírení.
Dátum a čas sa zobrazujú podľa kultúry zvoleného jazyka, aj v exportoch. Časy obsahujú hodiny a minúty
v stredoeurópskom čase (SEČ/LSEČ). Český a slovenský dátum má medzery za bodkami a nemá úvodné nuly.
Zdrojové XML a export XML uchovávajú pôvodné sekundy, zlomky sekúnd aj číselný posun časového pásma.
Kliknutie na prílohu ju otvorí, susedný odkaz Uložiť uloží jej kópiu. Každá karta má vlastný krátky
dočasný priečinok, napríklad `%LOCALAPPDATA%\FortSoft\ZfoViewer\ZFO3A7C.tmp\faktura.pdf`. Názvy príloh
zostávajú zachované. Opakované otvorenie v rovnakej karte použije nezmenenú kópiu; iná karta má
vždy iný priečinok, aj keď otvorené súbory ZFO majú rovnaký názov.

Pri zatvorení karty sa aplikácia pokúsi jej priečinok rekurzívne odstrániť. Zamknuté súbory preskočí
a čistenie zopakuje pri spustení, otváraní príloh, zatváraní kariet a ukončení aplikácie. Čistenie zachováva
priečinky používané inou spustenou inštanciou a neprechádza adresárové odkazy.

**Zobrazenie > Zobraziť zdroj XML** (`Ctrl+U`) otvorí odsadené XML v novej karte. Opakovaný príkaz
vyberie už otvorenú zdrojovú kartu rovnakej správy. Značky, atribúty, hodnoty, entity a komentáre sú
farebne zvýraznené pri zachovaní systémového písma. Vysoký kontrast ponecháva systémové farby textu.
Zobrazenie a export XML používajú rovnaké formátovanie. Príkaz na zobrazenie zdroja je na zdrojovej karte neaktívny.
**Zobrazenie > Farebné zvýraznenie** prepína zvýraznenie aktívneho zobrazenia a zdieľa uloženú voľbu s Predvoľbami.
Správy a XML majú nezávislé nastavenie; pri prepnutí karty sa podľa neho aktualizuje začiarknutie ponuky.

Kontextová ponuka textu obsahuje Kopírovať a Vybrať všetko bez klávesových skratiek.
Ponuka karty obsahuje kopírovanie názvu súboru, celej cesty, zobrazenie v priečinku a zatvorenie karty.
Stavový riadok zobrazuje Pripravený alebo cestu aktívneho súboru a umožňuje svoj text skopírovať.

## Hľadanie, export a tlač

**Úpravy > Hľadať** (`Ctrl+F`), `F3` a `Shift+F3` hľadajú v správe, zdrojovom XML, pomocníkovi aj licencii.
Regulárne výrazy používajú ECMAScript zo staticky pripojenej behovej knižnice C++, bez PCRE2.
Výber textu zabezpečuje RichEdit; bežné textové polia pomocníka a okna O programe používajú SelectionEdit.

**Súbor > Exportovať** (`Ctrl+E`) ponúka v tomto poradí ANSI, UTF-8, UTF-8 s BOM, UTF-16 BE,
UTF-16 LE, RTF, ODT, DOCX, HTML 4.01 Strict, XHTML 1.1, odsadené XML, BMP, GIF, JPEG, PNG, TIFF a bezstratové WebP.
Textové kodéry vychádzajú z overených vzorových implementácií. Všetkých šesť obrazových formátov používa rovnaký
komponent ImageExport ako šetriče: natívne kodeky Windows pre BMP, GIF, JPEG, PNG a TIFF
a pribalenú knižnicu WebP s komponentom PackedResource pre bezstratové WebP.
ODT 1.2 a DOCX vznikajú priamo z metadát správy alebo zdrojového XML spoločným vstavaným zápisom ZIP.
Zachovávajú sekcie, tučné popisy, Unicode, medzery, tabulátory a konce riadkov bez inštalácie Office,
OpenOffice, LibreOffice, .NET alebo ďalšej DLL.
HTML, XHTML, ODT a DOCX preberajú rodinu, veľkosť písma a výšku riadka z aktívneho RichEditu.
Obrazové exporty zachytávajú natívne vykreslenie vrátane neviditeľnej časti správy, aktuálneho zalamovania,
zvýraznenia a farieb odkazov. Zachovávajú pôvodný výber a polohu posunu. Pri prekročení medzí formátu alebo
pamäte sa export odmietne bez orezania správy.
HTML a XHTML používajú UTF-8 bez BOM a zachovávajú lokalizované sekcie, metadáta a popisy príloh.
Export zdrojovej karty zachováva XML ako escapovaný predformátovaný text.
RTF ukladá priamo RichEdit, XML formátuje XmlLite. Dodaná x86 knižnica libwebp je zabalená do EXE.
Ikony a knižnica sú v Resources.

V ponuke Súbor je najprv **Vzhľad strany**, potom **Tlačiť** (`Ctrl+P`). Tlač funguje aj
v pomocníkovi a okne O programe. Windows 11 používa moderné tlačové rozhranie, staršie systémy klasickú cestu.
Tlač správy a XML zachováva zvolené písma, farby, tučný text, kurzívu aj podčiarknutie.

## Predvoľby

Prvé dva riadky vyberajú nezávisle písmo správy a XML zdroja. Tlačidlá Predvolené obnovia aktuálne systémové
písmo správy a Consolas 9 bodov pre zdroj. Písmo rozhrania, pomocníka ani licencie sa nemení.
Obe zvýraznenia sú predvolene zapnuté. Pri správe sa mení iba farba nadpisov sekcií na systémovú farbu odkazov;
po vypnutí zostávajú tučné nadpisy, popisy aj odkazy. XML používa štýl Firefoxu vrátane tučných názvov značiek
a atribútov. HTML, XHTML, ODT, DOCX, RTF aj obrazové exporty rešpektujú písmo obsahu a stav zvýraznenia.
ODT a DOCX vznikajú v pamäti a zapisujú sa priamo do cieľa bez dočasných súborov.

**Možnosti > Predvoľby** (`Ctrl+G`) ponúkajú v tomto poradí češtinu, slovenčinu a americkú angličtinu.
Pri prvom spustení sa jazyk odvodí od rozhrania Windows; ostatné jazyky použijú americkú angličtinu.
Voľba Automaticky nie je v zozname. Ručne vybraný jazyk sa zapamätá.

Ďalej možno nastaviť dĺžku názvu karty (predvolená 80) a názvu v záhlaví (predvolená 40), zatváranie
dvojklikom a viac inštancií alebo
vymazať posledné súbory a zapnúť plynulé posúvanie. OK a Použiť uložia pripravené zmeny, Zrušiť zahodí zmeny od posledného Použiť.
Vymazanie histórie je okamžité. Predvolená je jediná inštancia; ďalšie spustenia jej odovzdajú súbory.
Plynulé posúvanie je predvolene vypnuté. Bežné koliesko myši posúva správu aj zdrojové XML okamžite
podľa systémovej voľby riadkov či strán. Ctrl+koliesko zachováva štandardné správanie RichEditu.
Predvoľby majú kompaktné natívne odsadenie a pri malej výške pracovnej plochy zvislý posuvník.

Rozhranie používa systémové písmo; licencia v okne O programe používa neproporcionálne písmo zo vzoru.
PersistWindowState obnovuje stav okien a pri chýbajúcich
rozmeroch hlavného okna použije Windows Default Bounds. Pomocník a O programe vychádzajú
z existujúcich aplikačných formulárov. Predvoľby a O programe si pamätajú polohu iba počas bežiacej inštancie.
Pomocník a Hľadanie ukladajú stav svojich okien do registra. Hľadané výrazy a história zostávajú iba pre aktuálny beh.
Preklady aj pomocník sú v Localization.cpp.

Nastavenia sú v `HKEY_CURRENT_USER\Software\FortSoft\ZfoViewer`. Prepínače zdieľajú DWORD Options:
viac inštancií má bit `0x04`, zatváranie dvojklikom `0x40`, rovnako ako vo vzorovom projekte.
Plynulé posúvanie používa bit `0x80`.

## Zostavenie

Otvorte ZfoViewer.slnx vo Visual Studiu 2026 alebo v jeho vývojárskom príkazovom riadku spustite:

```text
msbuild ZfoViewer.slnx /p:Configuration=Release /p:Platform=x86
```

Projekt cieli na Windows Vista a novšie, používa v145, Windows SDK 10.0, Unicode Win32, C++20
a staticky pripojenú behovú knižnicu v Release. Upozornenia prekladača majú v oboch konfiguráciách
úroveň 4 (`/W4`). Nastavenia SDK a linkera používajú predvolené hodnoty ako vzorové projekty, bez výslovného
prepisovania verzie Windows. Windows XP nie je podporovaný.
Výstupy sú v koreni riešenia v priečinku zvolenej konfigurácie, rovnako ako vo vzorových projektoch.

Príprava prostriedkov zabalí dodanú knižnicu WebP. PackageRelease.ps1 vychádza z existujúceho
postupu podpisovania a balenia už zostaveného Release. Bežné zostavenie nevytvára ZIP.

Dočasné testy zostávajú v tests a sú vylúčené z Gitu. Všetky ich výstupy sú tiež pod tests.
Spúšťajú sa skriptom tests/RunTests.ps1 na izolovanej ploche bez aktivácie okien, s oddeleným registrom
a náhradou schránky. Nezobrazujú tlačové dialógy ani neposielajú fyzické tlačové úlohy.

## Podpis a licencia

Crypt32 dekóduje CMS/PKCS#7, MSXML 6 spracuje XML ISDS. DTD a externé zdroje XML sú zakázané.
Externé prílohy a staré formuláre Software602 nie sú podporované. Kontroluje sa kryptografický podpis
obálky, nie dôveryhodnosť certifikátov, odvolanie, časové pečiatky alebo dlhodobá platnosť.

Aplikácia má licenciu MIT. Priložená libwebp má vlastnú licenciu; pozri [license.txt](../license.txt).
