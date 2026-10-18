# OpenRGB — interface moderne

Cette version conserve Qt 6, le moteur RGB et l’API 5 des plugins. Les DLL Effects, Visual Map et LightSync proviennent de l’installation existante.

## Lancement et présentation

Quitter l’autre OpenRGB, puis lancer `app/OpenRGB.exe` ou `Lancer-OpenRGB.cmd`. Le script `Install-LightSync.ps1` remplace les binaires de `C:\Program Files\OpenRGB` avec sauvegarde, tout en conservant les profils et la VMap. La copie de configuration dans `app/config` inclut les profils et Visual Map ; les modifications de cette copie ne sont pas automatiquement répercutées dans la configuration de l’installation habituelle. Le contrôle LAN a été laissé désactivé pour les sept lumières, conformément au réglage demandé pendant les essais.

Le logo blanc OpenRGB, en haut à gauche, sert de bouton pour replier ou déplier le menu et mémorise ce choix. En mode replié, il est centré sur les autres icônes. Un clic droit > **Automatic width** rétablit le repli automatique sous 1440 pixels logiques. Le même choix figure dans Settings > Appearance > Navigation. Les icônes repliées gardent leurs infobulles et libellés d’accessibilité. La barre des profils reste accessible en bas des pages. LightSync sépare **Devices** et **Settings** ; les longues pages d’effets défilent sans comprimer les réglages.

**Appearance**, en bas à gauche, propose Acrylic, Mica et Opaque. **Windows (automatic)** est le mode d’accent par défaut. Il lit `UISettings.GetColorValue(AccentLight2)`, la variante adaptée aux surfaces sombres, et suit `ColorValuesChanged` sans polling. Les notifications arrivent sur le thread Qt via un message Windows ; les textes et les coches s’adaptent au contraste. Les palettes Rose, Blue, Mint et Violet restent disponibles comme choix manuels. Le thème de fond reste sombre.

Le logo blanc officiel est utilisé dans le menu, les fenêtres, la page Information, l’icône de l’exécutable et le package Windows. La zone de notification est monochrome par défaut ; son réglage historique reste disponible. Les couleurs fonctionnelles des LED et les logos des plugins sont conservés.

## Identité Windows et clavier Lenovo

La nouvelle interface réutilise **OpenRGB.LightSync**, le même publisher **CN=OpenRGBLightSync**, le même identifiant `App` et la même famille de package que l’installation habituelle. La mise à jour ne désinstalle pas cette identité et conserve l’ordre des fournisseurs WDL. L’entrée temporaire **OpenRGB.LightSync.Preview** utilisée au début des essais est retirée. Il n’est pas nécessaire de recréer les priorités du clavier Lenovo.

Le package signé `app/OpenRGB.LightSync.msix` contient l’identité ; l’exécutable reste dans son dossier externe. Un seul chemin est enregistré à la fois pour cette identité. Les scripts sauvegardent les préférences Lighting et les informations de package dans `app/identity-backups/` avant d’intervenir. Ils utilisent le certificat existant ; aucune clé privée n’est exportée.

### Déplacer cette version

Quitter OpenRGB. Dans PowerShell, sous le compte Windows habituel :

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Register-ApplicationIdentity.ps1 -RebindOnly
```

Le script met à jour l’inscription pour le dossier `app`, en conservant la famille et les priorités existantes. Windows refuse de déplacer une même révision déjà inscrite : la livraison contient donc deux révisions signées de la même identité (`OpenRGB.LightSync.msix` et `OpenRGB.LightSync.Switch.msix`). Le script choisit une révision différente, y compris pour revenir au dossier précédent, sans désinstaller l’identité. Conserver ces deux fichiers. Pour installer le package sur un nouveau PC, utiliser le même script sans `-RebindOnly`.

### Revenir à l’installation habituelle

Quitter la nouvelle version, puis ouvrir PowerShell administrateur avec le compte habituel :

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Register-ApplicationIdentity.ps1 -RebindOnly -AppRoot "C:\Program Files\OpenRGB"
```

Relancer ensuite l’exécutable habituel. Les anciens binaires et leur configuration n’ont pas été écrasés. Ne pas désinstaller l’identité pour faire ce retour : le réenregistrement permet de conserver les préférences WDL. Ne pas lancer les deux versions simultanément.

Les raccourcis de démarrage et la tâche existante de relance après hibernation restent attachés à l’installation habituelle. Après installation, ces raccourcis lancent donc la nouvelle interface. La copie `app` reste disponible séparément. Son démarrage automatique est désactivé ; le raccourci de démarrage optionnel de cette copie est distinct et ne doit pas être activé en parallèle de celui de l’installation habituelle.

### Installer sur un autre PC

Conserver le dossier complet, y compris `app`, `scripts`, le package `.msix` et le certificat public `.cer`. Vérifier l’origine de cette livraison puis importer le certificat public dans PowerShell administrateur :

```powershell
Import-Certificate -FilePath .\app\OpenRGBLightSync.cer -CertStoreLocation Cert:\LocalMachine\TrustedPeople
```

Sous le compte qui utilisera OpenRGB :

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Register-ApplicationIdentity.ps1 -PrioritizeKeyboard
```

Cette option place OpenRGB en premier pour les priorités globales et le clavier Lenovo VID_048D/PID_C193, après sauvegarde. Sur ce PC où les priorités existent déjà, l’inscription normale les conserve. Pour désenregistrer complètement OpenRGB, quitter l’application puis employer `-Remove` ; cela concerne l’identité commune, pas seulement cette copie. Ne pas supprimer le certificat partagé si une autre installation l’utilise.

La préparation d’une nouvelle signature avec `Prepare-ApplicationIdentity.ps1` nécessite la clé privée du certificat local existant et les outils Windows SDK. Le certificat public fourni ne permet pas de signer un nouveau binaire.

## Matériaux et compatibilité

Acrylic et Mica utilisent `DWMWA_SYSTEMBACKDROP_TYPE` et une surface Qt avec canal alpha, tout en conservant le cadre Windows et le redimensionnement natifs. Windows peut substituer un fond opaque suivant sa version, ses réglages de transparence, le contraste élevé ou la session. Aucun flou CPU, capture du fond, animation permanente ou timer de polling n’est ajouté.

Les pages restent possédées par leurs QTabWidget d’origine. La navigation conserve des QPointer vers les pages, et ne manipule pas de pointeurs matériels. Les signaux et les contrôles des plugins sont conservés lors des adaptations de mise en page.

## Compilation et vérifications

Depuis le dépôt source complet :

- Qt 6.8.3 MSVC x64 ; Visual Studio 2026 / outils MSVC x64.
- `scripts/CompileModern.cmd` génère le projet et compile en Release x64. Définir `QT_ROOT` si nécessaire ; adapter le chemin Visual Studio si l’installation diffère.
- `scripts/VerifyModernBuild.ps1` vérifie que `startup.obj` dépend du bon `OpenRGBDialog.h` et a été recompilé après celui-ci.
- `scripts/test-modern.cmd` lance les tests Qt.
- `tests/modern-interface/ReviewPlugin.*` et `ExercisePreview.ps1` servent uniquement à la validation de développement. Le module de revue et ses commandes sont exclus de la livraison. Le parcours automatique des tailles suppose le mode Navigation **auto**.
- `scripts/PackageModern.ps1 -SourceCommit <SHA complet>` crée la sauvegarde privée dans Downloads, avec les profils et un manifeste SHA-256, sans journaux, dumps, clé privée ni DLL de revue.

Cette copie est imbriquée dans un autre dépôt OpenRGB. Les chemins du projet sont résolus avant la génération des dépendances et qmake utilise `-nocache`. Une dépendance vers l’ancien en-tête avait laissé un objet avec une ancienne taille de fenêtre, signalée par le garde de pile MSVC à la fermeture. Les chemins corrigés et le contrôle de build empêchent cette divergence. Une inclusion erronée de `GenesisXenon200Controller.cpp` a également été remplacée par son en-tête pour permettre la compilation propre.

Les vérifications réellement effectuées figurent dans `VALIDATION-MODERN.md`. Les essais visuels humains de déplacement de fenêtres et de jeux restent utiles ; les tests automatisés ne remplacent pas leur appréciation.

## Références techniques

- https://learn.microsoft.com/en-us/windows/apps/develop/ui/theming
- https://learn.microsoft.com/en-us/uwp/api/windows.ui.viewmanagement.uisettings.colorvalueschanged
- https://learn.microsoft.com/en-us/powershell/module/appx/add-appxpackage
- https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwm_systembackdrop_type
- https://raw.githubusercontent.com/qt/qtbase/v6.8.3/src/plugins/platforms/windows/qwindowswindow.cpp

## Package complet et retour aux anciens binaires

`payload` contient la nouvelle version à installer. `validation/historical` conserve les mesures de la livraison du 14 octobre ; elles ne valident pas à elles seules la nouvelle interface. `installation-backup` contient la sauvegarde du remplacement effectué sur ce PC.

Pour restaurer les anciens binaires depuis cette archive : fermer OpenRGB puis exécuter en administrateur `./Uninstall-LightSync.ps1 -Backup "./installation-backup"`. Le chemin cible est celui enregistré lors du déploiement. Ne pas utiliser cette sauvegarde personnelle sur un autre PC. Pour une nouvelle installation, utiliser `Install-LightSync.ps1`.
