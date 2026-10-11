# Ambient : optimisation Windows et objectif 60 FPS

Travail réalisé le 11 octobre 2026 dans `C:\laragon\www\OpenRGB\Plugin\OpenRGBEffectsPlugin`, branche `feature/ambient-performance`.

## Compatibilité et état de référence

- Dépôt officiel cloné avec ses sous-modules, référence master `8288ef45eb27d98a237b11ae21fa76be17978c2a`, conservée par le tag `ambient-performance-baseline`.
- OpenRGB installé : v1.0, commit `81bbe18a84c2e507006f19dd252e397e40a56bfe`, Qt 6.8.3, API de plugins 5. Vérifier le SHA exact dans le sous-module : sa référence correspond à celle du journal du logiciel installé.
- Compilation Windows x64 Release : MSVC 14.51 (Visual Studio 2026), paquet Qt 6.8.3 MSVC2022 x64, qmake et jom. Le chargement effectif de la DLL avec Qt 6.8.3/API 5 est testé.
- La DLL officielle installée correspond à un commit Effects plus ancien, `0e0f1b4708c302bad4270ab62349b2efe5dcc439`. Les comparaisons automatisées portent sur le master officiel cloné et sa version modifiée, pas sur des mesures du processus OpenRGB installé.
- DLL officielle sauvegardée dans `../artifacts/baseline/OpenRGBEffectsPlugin-installed.dll`. SHA256 : `8A82B7D0D19E012AAA2B5C96AFCB67B9EA4D6F673F99082B57BD3045287CE4FA`. Aucun remplacement dans Program Files.
- Configuration et profils sauvegardés dans `../artifacts/baseline/configuration-20261011`. Le dépôt OpenRGB principal et le sous-module OpenRGB ne sont pas modifiés.

## Diagnostic du pipeline original

La capture Windows utilisait GDI/BitBlt à la résolution native, recréait les DC et le bitmap à chaque frame et convertissait HBITMAP → QPixmap → QImage dans le worker. Sur l'écran testé, chaque image native représente environ 20 Mo. Le pipeline Ambient copiait cette image sous mutex, puis conservait ce mutex pendant des redimensionnements et calculs par zone. Le calcul de moyenne et les allocations se répétaient. L'aperçu exécutait un second traitement ; le scheduler transmettait systématiquement aux contrôleurs.

La capture GDI mesurée prend environ 47–52 ms en moyenne : elle dépasse déjà largement le budget de 16,7 ms à 60 FPS. Ce coût concerne aussi le chemin de capture/composition Windows ; réduire uniquement le calcul des couleurs ne suffit pas. Une variante GDI avec buffers réutilisés et réduction précoce restait à environ 27 ms dans un essai de déplacement à 20 FPS. DXGI réduit bien davantage le temps passé dans la capture.

Windows Graphics Capture a été étudié comme alternative, sans être ajouté : DXGI est disponible dans le SDK Windows, permet une réduction GPU avant lecture CPU et obtient ici un résultat satisfaisant sans couche WinRT supplémentaire. Un fallback GDI reste disponible. Les conclusions de fluidité DWM ne sont pas déduites de la seule consommation CPU.

## Optimisations implémentées

1. Worker de capture dédié, priorité inférieure à la normale, temporisation par condition_variable, arrêt avec join, reprise des ressources après perte de duplication. Les appels Qt liés au choix du moniteur restent sur le thread GUI.
2. DXGI Desktop Duplication : adaptateur et sortie correspondant au moniteur sélectionné, shader léger de réduction sur GPU, deux buffers de lecture bornés et Map(DO_NOT_WAIT). Aucune attente GPU synchrone pour lire les pixels. Les anciens résultats disponibles sont remplacés par le plus récent, sans file de frames.
3. Libération immédiate de la frame DXGI après soumission des commandes GPU, inspirée de LightSync. Sur cette machine, conserver la frame jusqu'au prochain tick limitait le débit à environ 39–40 FPS ; la libération immédiate permet environ 59 FPS. AcquireNextFrame attend au plus 2 ms, dans le worker uniquement.
4. Résolution d'analyse configurable : native, 160, 320, 640 ou 1280 pixels de largeur, ratio conservé. À 320 pixels, l'écran 3440×1440 produit une image 320×133, soit environ 0,17 Mo transféré vers le CPU par frame. Une copie native reste faite sur GPU pour le shader.
5. Publication d'une seule image QImage immuable avec partage implicite ; mutex limité à l'échange du dernier résultat. Analyse, moyenne et mapping exécutés hors de ce mutex. Échantillonnage direct des zones, sans redimensionnement par périphérique.
6. Mappings linéaires, inversés, matrices, trous et segments préservés. Les indices invalides sont vérifiés avant lecture/écriture ; les offsets de zone sont pris en compte.
7. Capture, analyse, transmission RGB et aperçu réglables séparément. Valeur recommandée : 60/60/60 FPS et aperçu 10 FPS ; l'aperçu peut lui aussi être réglé jusqu'à 60 FPS. Le FPS général d'OpenRGB reste un plafond supplémentaire. Limiteur RGB propre à Ambient pour ne pas dépendre des arrondis du scheduler ; les modifications différées restent en attente de transmission.
8. Lissage exponentiel basé sur le temps écoulé, avec état fractionnaire pour éviter les blocages d'arrondi. Seuil RGB configurable ; convergence finale et petits changements progressifs conservés. Un maintien de flux à 1 Hz demeure quand les couleurs sont stables. Le seuil 0 désactive la suppression.
9. Aperçu limité à sa fréquence, suspendu lorsqu'il est masqué, minimisé ou désactivé ; réutilisation de la dernière frame. Aucun second appel de capture pour l'aperçu.
10. Métriques facultatives et bornées : `OPENRGB_AMBIENT_METRICS=1` produit un résumé à l'arrêt/destruction, sans journalisation ni écriture de fichier par frame.

Le scheduler commun reçoit seulement les conditions Ambient explicitement autorisées. Les autres effets conservent leur parcours existant. Les appels matériels restent dans le scheduler OpenRGB existant : un périphérique lent peut encore limiter le débit et retenir son verrou global. Cette architecture globale n'a pas été changée.

## Interface et anciens profils

Le groupe **Optimisation des performances** contient les modes Standard / Équilibré / Performance / Personnalisé, les FPS de capture, analyse, transmission et aperçu, la résolution, le lissage, le seuil et l'activation de l'aperçu.

- Équilibré : DXGI, capture/analyse/RGB 60 FPS, largeur 320, seuil 2, aperçu 10 FPS.
- Performance : DXGI, 30 FPS, largeur 160, seuil 3.
- Standard : GDI, résolution native et seuil 0, pour conserver les paramètres des anciens profils et offrir un retour au backend compatible.
- Personnalisé : chaque paramètre peut être réglé ; DXGI avec fallback GDI.

Les champs existants sont conservés ; les nouveaux champs sont enregistrés dans un objet `performance`. Un ancien profil sans cet objet revient en Standard. **Il faut donc sélectionner Équilibré après chargement d'un ancien profil pour bénéficier de la capture optimisée**, puis enregistrer le profil. Vérifier également que les FPS généraux de l'effet sont à 60. L'aperçu n'affiche pas nécessairement 60 FPS si le scheduler de l'effet est réglé plus bas.

## Validation et mesures

La DLL x64 a été compilée et réellement chargée par QPluginLoader avec Qt 6.8.3/API 5. Tests sur les paramètres, anciens profils, sauvegarde/chargement, offsets et mappings, luminosité, lissage, seuils, dernière frame, aperçu jusqu'à 60 FPS, limitation RGB, captures réelles et arrêt/redémarrage des workers. Les logs livrés précisent les assertions exécutées.

Le benchmark utilise la vraie capture et le vrai code Ambient, avec un contrôleur simulé de 256 LED et une fenêtre Qt visible au-dessus du bureau. Chaque scénario dure 12 secondes. Écran Odyssey G85SB 3440×1440 à 174,962 Hz, Windows 11, RTX 5080 Laptop, 32 processeurs logiques. Mode Copie d'écran, consigne 60 FPS ; référence native contre DXGI/320 pixels. Les données finales figurent dans `../artifacts/benchmark/*-60fps-copy.json` et les journaux associés.

| Scénario / version | Capture moyenne / P95 / max (ms) | Rendu moyenne / P95 / max (ms) | CPU machine (%) | Images/s | RGB simulé/s | Timer Qt P95 / max (ms) |
| --- | --- | --- | --- | --- | --- | --- |
| Repos / baseline | 46.864 / 56.340 / 66.243 | 0.239 / 1.843 / 5.806 | 2.061 | 18.49 | 59.79 | 16.870 / 30.591 |
| Repos / optimized | 0.724 / 2.504 / 3.405 | 0.027 / 0.044 / 0.173 | 0.154 | 47.07 | 13.95 | 16.697 / 19.061 |
| Déplacement / baseline | 51.987 / 64.017 / 84.909 | 0.282 / 1.765 / 8.229 | 2.756 | 16.54 | 59.84 | 16.905 / 39.827 |
| Déplacement / optimized | 0.334 / 0.457 / 4.148 | 0.032 / 0.049 / 0.348 | 0.657 | 58.76 | 58.26 | 16.752 / 18.093 |
| Redimensionnement / baseline | 49.871 / 60.767 / 74.020 | 0.209 / 1.002 / 4.464 | 2.865 | 17.48 | 59.92 | 16.718 / 30.533 |
| Redimensionnement / optimized | 0.329 / 0.453 / 3.625 | 0.032 / 0.048 / 0.121 | 0.877 | 58.68 | 58.10 | 16.628 / 21.166 |

Les durées de capture/poll et rendu sont des **durées murales**, incluant les éventuelles attentes, pas des durées CPU actives. Le CPU du processus de benchmark inclut également la fenêtre de test. Les sorties RGB sont simulées ; les délais réseau/USB et la latence des LED physiques ne sont pas mesurés. Les intervalles du timer Qt ne mesurent pas les présentations DWM.

Les fichiers `*-gpu.json` contiennent l'utilisation du moteur GPU le plus chargé appartenant au processus, sans additionner les moteurs. Une instance absente est `null`, pas 0. Des erreurs PDH transitoires au début ou à la fin sont enregistrées. Aucun pourcentage GPU fiable du processus baseline n'a été obtenu. Les valeurs DWM disponibles ne prouvent pas l'absence de micro-saccades.

| Scénario / version | GPU processus max (%) | GPU DWM max (%) | Erreurs PDH transitoires |
| --- | --- | --- | --- |
| Repos / baseline | indisponible | 1.898 | 0 |
| Repos / optimized | 0.239 | 0.438 | 1 |
| Déplacement / baseline | indisponible | 2.901 | 0 |
| Déplacement / optimized | 0.342 | 1.665 | 1 |
| Redimensionnement / baseline | indisponible | 1.840 | 0 |
| Redimensionnement / optimized | 0.312 | 1.758 | 0 |

## Recompiler et refaire les essais

Dépendances : Visual Studio C++ x64 avec SDK Windows, Qt 6.8.3 MSVC x64 (base et Core5Compat), jom et sous-modules Git initialisés. Le kit utilisé se trouve dans `../build-tools/qt/msvc2022_64`. Depuis le dépôt :

```powershell
.\Effects\Ambient\tests\Build-Ambient.ps1
```

Les chemins du kit peuvent être passés avec `-QtRoot`, `-VcVars` et `-Jom`. Le script appelle vcvarsall x64, qmake en Release et jom -j4, puis construit les tests. La DLL est dans `build/optimized/release/OpenRGBEffectsPlugin.dll`.

```powershell
$env:PATH = 'C:\Program Files\OpenRGB;' + $env:PATH
$env:QT_QPA_PLATFORM_PLUGIN_PATH = 'C:\Program Files\OpenRGB\platforms'
$env:QT_FORCE_STDERR_LOGGING = '1'
$env:OPENRGB_AMBIENT_METRICS = '1'
.\build\tests\AmbientTests.exe "$PWD\build\optimized\release\OpenRGBEffectsPlugin.dll"
.\Effects\Ambient\tests\Measure-Ambient.ps1 -Flavor baseline -Scenario move -Fps 60 -EffectMode copy -Seconds 12
.\Effects\Ambient\tests\Measure-Ambient.ps1 -Flavor optimized -Scenario move -Fps 60 -EffectMode copy -Seconds 12
```

Les deux exécutables de benchmark ont déjà été construits ; leurs projets qmake et sources instrumentées sont conservés dans `../artifacts/benchmark/baseline` et `optimized`, et leurs objets de référence dans `build/baseline`. Le kit de livraison inclut les exécutables pour reproduire les mesures sans recompilation. Pour reconstruire une référence ailleurs, repartir du tag `ambient-performance-baseline` dans une copie isolée avec ses sous-modules ; ne pas reconstruire les objets baseline à partir des sources modifiées. Les instrumentation copies modifient seulement la mesure de temps, et l'en-tête original autorise l'accès du harness par une déclaration friend sans modifier sa disposition mémoire.

## Installation réversible

1. Fermer complètement OpenRGB ; ne pas tenter de charger deux DLL Effects simultanément.
2. Déplacer la DLL officielle `C:\Program Files\OpenRGB\plugins\OpenRGBEffectsPlugin.dll` dans un dossier de sauvegarde **extérieur au répertoire plugins**. La sauvegarde préparée permet également de revenir en arrière. Ne pas écraser le fichier original.
3. Copier la DLL de livraison dans le chemin devenu libre, sous le nom `OpenRGBEffectsPlugin.dll`. Une élévation Windows est nécessaire pour Program Files. Les autres DLL Qt déjà installées restent celles d'OpenRGB.
4. Relancer OpenRGB, vérifier le chargement d'Effects, choisir Ambient → Équilibré, capture/analyse/transmission et FPS général à 60. Commencer avec résolution 320 et aperçu 10 ; essayer ensuite l'aperçu à 60 si souhaité. Enregistrer un nouveau profil de test.
5. Retour arrière : fermer OpenRGB, déplacer la DLL modifiée hors de plugins, remettre la DLL officielle sauvegardée, puis restaurer le profil/configuration si nécessaire. Une copie supplémentaire de la configuration est incluse dans la sauvegarde.

La DLL installée n'a pas été changée automatiquement : la mission demande de préserver la DLL officielle et de fournir une procédure d'installation.

## Validation humaine reproductible restante

Comparer Ambient désactivé, la DLL officielle, puis la DLL optimisée. Garder les mêmes périphériques, profil, luminosité et écran ; fermer les builds et benchmarks pendant le test. Faire au moins 30 secondes de chaque scénario : bureau au repos, déplacement rapide d'une fenêtre, redimensionnement continu, défilement d'une page longue, vidéo réellement à 60 FPS, puis jeu ou application 3D avec compteur de frametime si disponible. Tester aperçu désactivé, 10 et 60 FPS séparément.

Observer le Gestionnaire des tâches pour CPU/moteur GPU d'OpenRGB, et un outil de mesure des présentations pour les frametimes DWM/jeu si disponible. Un enregistrement caméra au ralenti du bureau et des LED peut comparer la latence physique. Exécuter chaque variante deux ou trois fois pour distinguer un effet reproductible d'un pic ponctuel.

Le critère final reste l'absence de saccades perceptibles sur ton bureau avec les vrais périphériques. Les tests automatisés portent sur repos/déplacement/redimensionnement ; navigateur, vidéo, jeu, latence physique, HDR, écran pivoté et reprise après hibernation prolongée n'ont pas été validés en situation réelle. Une consigne de 60 ne garantit pas 60 images nouvelles sur un bureau immobile ; certains contenus protégés ne sont pas capturables. Le fallback GDI peut rester coûteux à 60 FPS.

## Fichiers concernés

- `Effects/Ambient/Ambient.cpp`, `.h`, `.ui` : pipeline, paramètres et interface.
- `Effects/Ambient/PerformanceMetrics.h` : diagnostics bornés.
- `ScreenCapturer/ScreenCapturer.h` : configuration minimale du backend et fréquence atomique.
- `ScreenCapturer/windows/WindowsScreenCapturer.cpp`, `.h` : DXGI/GDI et worker.
- `EffectManager.cpp` : conditions de transmission/aperçu, exclusivement Ambient.
- `Effects/Ambient/tests/AmbientTests.cpp`, `.pro`, `MockController.h`, `AmbientBenchmark.cpp`, `Measure-Ambient.ps1`, `Build-Ambient.ps1` : validation et compilation.
- Ce rapport `Effects/Ambient/PERFORMANCE.md`.

## Sources techniques

- Dépôt officiel : https://gitlab.com/OpenRGBDevelopers/OpenRGBEffectsPlugin
- Paquets Qt OpenRGB : https://gitlab.com/OpenRGBDevelopers/OpenRGB-Qt-Packages
- Microsoft Desktop Duplication : https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/desktop-dup-api
- D3D11 Map/DO_NOT_WAIT : https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-map
- Windows Graphics Capture : https://learn.microsoft.com/en-us/windows/apps/develop/media-authoring-processing/screen-capture
- Contraintes Qt GUI/threads : https://doc.qt.io/qt-6/threads-modules.html
- Inspiration locale : `C:\Laragon\www\LightSync\LightSync.Plugin\Core\DesktopDuplication\DesktopDuplicator.cs`, inspection seulement.
