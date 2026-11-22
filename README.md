# Kontrol Lights : extension Gig Performer pour le light guide du S88 MK2

Extension C++ (SDK officiel Gig Performer) qui pilote les LEDs des touches d'un
Komplete Kontrol S-series MK2 (S49/S61/S88) directement en USB HID, et expose
des fonctions GPScript utilisables dans chaque rackspace.

## Comment ça marche
- Le light guide MK2 accepte un rapport HID `0x81` suivi d'un octet de couleur
  (index de palette) par touche, après un rapport d'init `0xA0 00 00`.
  Protocole non officiel, documenté par SynthesiaKontrol et LightKontrol.
- Un thread d'arrière-plan envoie l'état au clavier (~30 images/s max) et se
  reconnecte tout seul si le clavier est débranché.

## Affichage automatique des splits (par défaut)
À chaque changement de rackspace, de variation ou de partie de chanson, l'extension
lit les blocs **MIDI In** du rackspace et allume la plage de notes acceptée par
chacun, dans une couleur différente. Les touches couvertes par plusieurs blocs
(layers) s'allument en blanc.

- **Important :** donne un *handle* à chaque bloc MIDI In (clic droit sur le bloc
  dans le câblage). Gig Performer ne montre aux extensions que les blocs qui ont un handle.
- Couleur : automatique (bleu, vert, orange, violet, jaune, rouge, cyan, rose, dans
  l'ordre), ou mets un nom de couleur dans le nom du bloc (« Basse bleu », « Pad violet »,
  « Lead rouge »…), ou force-la en GPScript avec `KK_SetZoneColor("handle", couleur)`.
- S'il y a des blocs MIDI In du Komplete Kontrol (ou OMNI), les blocs des autres
  contrôleurs sont ignorés.
- Menu Extensions > « diagnostic des blocs MIDI In » écrit dans la console ce que
  l'extension a lu (noms des paramètres, plages trouvées).

## Fonctions GPScript
| Fonction | Rôle |
|---|---|
| `KK_SetKey(note, color)` | allume une touche (note MIDI, 21 = A0 sur S88) |
| `KK_SetRange(low, high, color)` | allume une plage |
| `KK_Clear()` | éteint tout |
| `KK_Color(hue, shade)` | index de palette : hue 1..16 (1 rouge, 7 vert, 11 bleu…), 17 blanc ; shade 0..3 |
| `KK_IsConnected()` | clavier détecté ? |
| `KK_SetAutoClear(bool)` | éteint à chaque changement de rackspace (si zones auto désactivées) |
| `KK_SetAutoZones(bool)` | active/désactive les zones automatiques (actif par défaut) |
| `KK_RefreshZones()` | relit les blocs MIDI In et réaffiche |
| `KK_SetZoneColor(handle, color)` | force la couleur d'une zone |
| `KK_SetYield(bool)` | `true` = laisse le light guide au plugin Komplete Kontrol dans ce rackspace |

Voir `scripts/example_rackspace.gpscript`.

## Installer sur Windows (version précompilée)
1. Ferme Gig Performer.
2. Copie `dist/windows/KontrolLights.dll` dans `C:\ProgramData\Gig Performer\Extensions` (crée le dossier s'il n'existe pas).
3. Relance Gig Performer et accepte l'extension « Kontrol Lights » quand il la propose.
4. Menu Extensions > « Kontrol Lights : test arc-en-ciel » : les touches doivent s'allumer.

## Compiler
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --config Release   # copie dans le dossier Extensions de GP
```
Windows : Visual Studio 2022 + CMake (ou, depuis Linux, `-DCMAKE_TOOLCHAIN_FILE=mingw-toolchain.cmake`). macOS : Xcode CLT + CMake (binaire universel).

## Tester d'abord sans Gig Performer
`pip install hidapi` puis `python3 tools/test_lightguide.py` : un balayage arc-en-ciel doit parcourir le clavier.

## Plugin Kontrol Lights Map (couleur par note)
Plugin VST3 à insérer dans un rackspace pour choisir la couleur de chaque note, une par une ou par plages.

1. Décompresser `dist/windows/KontrolLightsMap-vst3.zip` dans `C:\Program Files\Common Files\VST3\`.
2. Dans Gig Performer, relancer l'analyse des plugins, puis insérer « Kontrol Lights Map » dans le rackspace (pas besoin de le câbler).
3. Lui donner un handle (clic droit sur le bloc) : c'est ce qui permet à l'extension de relire ses couleurs à chaque changement de rackspace.
4. Ouvrir son éditeur : clic sur une touche, glisser ou Maj+clic pour une plage, Ctrl+clic pour ajouter des notes, puis clic sur une couleur. Le clavier se met à jour en direct.

« Transparent » laisse voir la zone du bloc MIDI In, « Éteint » force la touche éteinte, et « Remplacer les zones MIDI In » n'affiche que la carte. Les couleurs sont enregistrées avec le rackspace. Sources : `plugin/` (JUCE 7.0.12).

## Cohabitation avec le plugin Komplete Kontrol
Le plugin Komplete Kontrol (via NI Hardware Integration Agent) écrit lui aussi sur le light guide. L'extension renvoie son image toutes les 250 ms et se reconnecte en 0,5 s si l'écriture échoue, pour reprendre la main. Dans un rackspace où tu préfères l'affichage de Komplete Kontrol, appelle `KK_SetYield(true)` dans `On Activate`.

## Points à vérifier sur la vraie machine
- Noms exacts des paramètres de plage des blocs MIDI In (l'extension cherche « min/low » + « note/key » et « max/high » + « note/key ») : à confirmer avec le diagnostic.
- Changer la plage d'un bloc pendant qu'on est dans le rackspace n'est pas détecté tout seul : utiliser « réafficher les zones » ou `KK_RefreshZones()`.
- Ordre de dépilement des arguments GPScript (supposé inverse) et syntaxe des signatures.
- Table de couleurs exacte de la palette MK2.
- Cohabitation avec NI Hardware Integration Agent / un plugin Komplete Kontrol chargé dans GP, qui écrivent aussi le light guide.
