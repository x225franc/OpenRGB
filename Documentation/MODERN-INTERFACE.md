# OpenRGB — préversion de l’interface moderne

Cette branche conserve Qt 6 et l’API 5 des plugins. Elle remplace la navigation de la fenêtre principale par une barre latérale et modernise les surfaces, la typographie et les contrôles. Le moteur RGB et les binaires Effects, Visual Map et LightSync sont ceux de l’installation existante.

## Lancer

1. Quitter complètement l’autre OpenRGB depuis sa zone de notification.
2. Ouvrir `new_version/app/OpenRGB.exe` (ou `Lancer-OpenRGB.cmd`).
3. Utiliser **Appearance**, en bas à gauche, pour choisir Acrylic, Mica ou Opaque et l’accent Rose, Blue, Mint ou Violet.

Le dossier `app/config` est une copie indépendante de la configuration installée, incluant les profils et Visual Map. Les modifications dans cette préversion ne sont pas répercutées automatiquement sur l’installation actuelle. La barre des profils reste disponible dans les pages des plugins. LightSync s�pare d�sormais les vues **Devices** et **Settings**. Les longues pages d�effets d�filent sans comprimer les r�glages. En dessous de 1440 pixels logiques de largeur, le menu principal devient une barre d�ic�nes avec infobulles ; un d�filement de secours conserve l�acc�s aux plugins trop larges.

Le démarrage automatique de la préversion est désactivé. Son raccourci, s’il est activé dans Settings, se nomme **OpenRGB Preview** pour préserver le raccourci existant **OpenRGB**. Ne pas activer les deux simultanément.

## Clavier Lenovo / Windows Dynamic Lighting

Le contrôle durable en arrière-plan requiert une identité Windows et un exécutable signé. La préversion utilise **OpenRGB.LightSync.Preview**, distincte de **OpenRGB.LightSync** : l’identité de la version installée est conservée.

Les scripts de préparation et d’enregistrement sont dans `scripts/`. La préparation demande le certificat local existant **OpenRGBLightSync** avec sa clé privée, sous le compte Windows propriétaire. Aucun certificat ni clé privée n’est créé/exporté automatiquement par ces scripts.

Après préparation du package signé et copie dans `app`, enregistrer sous le compte de bureau :

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Register-PreviewIdentity.ps1
```

Dans Windows > Personnalisation > Dynamic Lighting > Background light control, placer **OpenRGB Preview** au-dessus des autres applications. L’option `-PrioritizeKeyboard` du script fait cette opération pour les priorités globales et le clavier Lenovo identifié VID_048D/PID_C193, après sauvegarde du registre.

Pour retirer uniquement l’identité de cette préversion :

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Register-PreviewIdentity.ps1 -Remove
```

Quitter ensuite la préversion et relancer l’OpenRGB habituel. Ne pas supprimer le certificat partagé : l’installation actuelle l’utilise aussi. Les exports `identity-backup-*.reg` permettent de restaurer les préférences Lighting antérieures si nécessaire ; une importation restaure l’ensemble des préférences contenues dans l’export.

La tâche existante de relance après hibernation reste attachée à l’installation habituelle. Cette préversion ne la remplace pas.

## Matériaux Windows

Acrylic et Mica sont appliqués avec les API DWM documentées de Windows 11 (DWMWA_SYSTEMBACKDROP_TYPE). Le système peut substituer un fond opaque selon la version Windows, les paramètres de transparence, le contraste élevé ou la session. Aucun flou CPU, capture du fond ou effet animé permanent n’est ajouté.

Référence : https://learn.microsoft.com/windows/win32/api/dwmapi/ne-dwmapi-dwm_systembackdrop_type

## Compilation et tests

- Qt 6.8.3 MSVC x64, Visual Studio 2026 / outils MSVC x64.
- `scripts/CompileModern.cmd` : génération qmake et build Release x64 ; définir `QT_ROOT` si nécessaire.
- `scripts/test-modern.cmd` : tests Qt de navigation.
- `tests/modern-interface/ReviewPlugin.*` : module de validation temporaire, exclu de la livraison. Il sert aux captures, au contrôle des dimensions et aux essais des vraies pages, via un dossier de commandes explicitement demandé par l’environnement de test.

La navigation conserve les QTabWidget et la propriété des pages par OpenRGB, et stocke des QPointer vers les pages plutôt que des pointeurs matériels. Les changements sont traités par événements ; aucun timer de rafraîchissement n’est ajouté à l’interface livrée.

Une correction minimale de compilation remplace l’inclusion erronée de GenesisXenon200Controller.cpp par son en-tête, pour éviter des symboles en double lors d’un build propre.

Les mesures et contrôles réellement effectués sont consignés dans `VALIDATION-MODERN.md` à la racine de la livraison.
