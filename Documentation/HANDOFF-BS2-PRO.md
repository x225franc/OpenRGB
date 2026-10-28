# Handoff — OpenRGB modifié / préparation Flydigi BS2 Pro

Mis à jour le **29 octobre 2026**. Document destiné à une IA qui n'a pas accès à la conversation. Lire ce document puis `tests/FlydigiBS2Pro/README.md` et `STATIC-EVIDENCE.json` avant toute action matérielle.

## 1. Demande et périmètre

L'utilisateur veut ajouter le **RGB du refroidisseur Flydigi BS2 Pro** à son OpenRGB modifié : appareil dans Devices, LED/zones réellement disponibles dans Effects/Ambient/Visual Map, effets matériels si possible. Il attend le retour physique de son refroidisseur et espère pouvoir terminer avant le **4 novembre 2026**. Aucun rendez-vous, rappel ni travail automatique futur n'a été créé.

Le BS2 Pro n'est pas disponible pour les validations actuelles. Une pré-intégration **inactive** a été préparée. Ne pas annoncer son support comme terminé et ne pas créer un faux périphérique actif. Ne pas inventer de mesures, de UUID, de nombre de LED, de protocole ou de cadence certifiée.

**Périmètre RGB seulement : préserver ventilation, RPM, courbes, modes automatiques, alimentation, températures et protections thermiques.** L'utilisateur évoque filaire/2,4 GHz et pilotage RGB Bluetooth : vérifier réellement les transports de données. Un câble USB peut ne servir qu'à l'alimentation ; ne pas déduire un protocole USB ou un dongle de cette description.

## 2. Dépôt, branches et état livré

- Fork utilisateur : https://github.com/x225franc/OpenRGB.git
- Travail local : `C:\Laragon\www\OpenRGB\new_version`.
- Remote `fork` = GitHub utilisateur ; `origin` = GitLab officiel. Pousser vers `fork`, jamais vers l'officiel par erreur.
- Branche applicative stable de cette session : `feature/fluent-interface`.
- Branche de préparation/handoff : **`feature/bs2-pro-preparation`**, créée depuis `964e02d9e669038fe5f9e43b404310f7e8e3dba7`.
- Le code réellement installé et signé est celui du commit **964e02d9** (intégration Apex 5). Les commits ultérieurs de préparation BS2 n'impliquent pas une nouvelle application installée.
- Installation : `C:\Program Files\OpenRGB`. Configuration/profils/logs dans son dossier `config`.
- Dernière sauvegarde d'installation : `C:\Program Files\OpenRGB\Backup-installation\Modern-20261029-003435`.
- Dernier package applicatif complet : `C:\Users\Administrator\Downloads\OpenRGB-Modern-Preview-x64-20261029-003521-964e02d9.zip`.
- API plugins **5**, Qt **6.8.3**, Release x64. Effects, Visual Map et LightSync restent installés.

Une fois le backend BS2 validé, intégrer la préparation dans la branche applicative après revue/tests avant livraison. `PackageModern.ps1` indique actuellement `feature/fluent-interface` dans son manifest : vérifier/corriger la provenance si un futur build est livré depuis une autre branche.

Commencer par `git status`, `git log`, `git remote -v` et les éventuels AGENTS.md. Ne pas écraser un travail nouveau, changer de branche avec des modifications étrangères, réinitialiser ou forcer un push. Le dossier parent `C:\Laragon\www\OpenRGB` contient aussi des sources : ne pas y modifier le mauvais checkout.

## 3. Préparation BS2 déjà réalisée

Fichiers suivis :

- `Controllers/FlydigiController/FlydigiBS2ProProtocol.h` : encodeur RGB pur, **aucun I/O**, aucun détecteur ni contrôleur actif. Commandes autorisées limitées aux cinq opcodes RGB repérés.
- `tests/FlydigiBS2Pro/ProtocolTests.cpp`, `Run-Tests.cmd` : format, checksum, découpage, ordre des étapes et rejets.
- `tests/FlydigiBS2Pro/STATIC-EVIDENCE.json` : provenance et inconnues explicites.
- `tests/FlydigiBS2Pro/BluetoothSurvey/` : outil .NET en lecture seule. Par défaut il énumère uniquement l'appairage en cache, sans demander de connexion BLE. `--inspect` doit être demandé seulement une fois le matériel revenu/connecté. Aucune écriture GATT, souscription, modification d'appairage ou commande de ventilation.

**Vérifications effectuées** : tests C++ de format passés ; compilation du survey réussie sans avertissement ; exécution du mode par défaut, une identité BS2 Pro appairée trouvée. Aucune validation des commandes RGB sur le matériel, aucune connexion GATT demandée par le survey par défaut, aucune nouvelle DLL/exe de production installée pour le BS2.

### Identité en cache, pas preuve de présence radio

Windows a montré `FlyDigi BS2PRO`, HID-over-GATT `1812`, identité PnP exposée **VID 37D7 / PID 1002 / révision 0110**. Le fingerprint local d'appairage du survey était `973786B7BF8F9A91` (change sur un autre PC). La source PnP doit être relue ; ne pas transformer automatiquement ces champs en identifiant USB filaire ni en version de firmware.

**Ne pas confondre** avec l'Apex 5 : USB/RF 37D7:2501 ; Bluetooth Xbox classique 045E:02E0. Ne jamais enregistrer le VID/PID Xbox générique comme s'il identifiait sûrement Flydigi.

### Pistes logicielles locales

Space Station est installé dans `C:\Program Files\Flydigi Space Station`, version lue littéralement dans `version` : `36.9.5`.

Archive locale `resources\app.asar`, SHA256 lors de l'analyse :
`A93889E7518B7CE964541DE6538787A2636C6BEC6461C9934AC231425E284482`.

Ressources pertinentes de cette version :

- `.vite/renderer/main_window/assets/index-CGwMero6.js` : classe CoolerDevice, `deviceCode=bs2pro`, méthodes `getRgbLightSwitch`, `setRgbLightSwitch`, `setRgbConfig`.
- `.vite/renderer/main_window/assets/index-DfobnxJB.js` : encodeur interne exporté sous l'alias `bf` dans le module précédent ; classe minifiée `en` dans cette version.
- Modèle global CoolerMappingConfig : sections ventilation, RGB et système distinctes. **Ne jamais envoyer sa configuration globale alors qu'on vise seulement RGB.** Le schéma binaire exact de `RgbConfig` reste à vérifier.

Les copies diagnostiques de ressources sont dans `build/flydigi-apex5/` (ignoré). Elles peuvent manquer sur un autre PC et les noms hashés changent selon la version : réinspecter l'archive locale si nécessaire. Ne pas commiter ni redistribuer les fichiers JavaScript complets, app.asar, exécutables constructeur ou environnements Python.

### Format observé statiquement seulement

Trame intérieure : `5A A5 opcode longueur payload checksum`, puis zéros. `longueur=payload.size+2`. Checksum = somme octet modulo 256 depuis opcode jusqu'à la fin du payload, sans `5A A5`.

- `45` hex (69 déc.) : lecture interrupteur RGB ; taille intérieure 32.
- `46` hex (70 déc.) : interrupteur RGB, payload 0/1 ; taille 32.
- `41` hex (65 déc.) : préparation de transfert de configuration RGB ; taille 24.
- `47` hex (71 déc.) : chunks, index 0 puis données, **10 octets de données maximum** ; taille 24.
- `43` hex (67 déc.) : finalisation ; taille 24.

Le code constructeur attend une réussite à chaque étape (`response[4]==1`). **Ce n'est pas encore un parseur d'ACK validé** : relever le cadrage réel, UUID, opcode/réponse, checksum, délais et erreurs avant d'exécuter un plan. Le plan C++ exige une réponse vérifiée mais n'implémente volontairement aucun exécuteur.

Certaines couches appellent ce booléen un affichage de palier/gear light : vérifier sa sémantique réelle et la distinguer du RGB décoratif et du fonctionnement du ventilateur. Ne pas supposer que 46 est déjà une extinction RGB matériellement validée.

Ne pas préfixer arbitrairement ces trames avec le Report ID 03 de l'Apex 5. Leur enveloppe Bluetooth, les caractéristiques GATT, le mode d'écriture et l'éventuelle fragmentation de transport sont **inconnus**.

## 4. Reprise dès que le refroidisseur revient

1. Demander son branchement habituel et confirmer que le refroidissement fonctionne avec les réglages usuels. Relever modèle/firmware et versions logicielles. Sauvegarder la configuration avant modification.
2. Énumérer les périphériques/identités en lecture seule. Séparer USB d'alimentation, interfaces de données, récepteur et BLE ; ne pas inférer leurs liens sur le nom seul.
3. Builder le survey (commandes ci-dessous), lancer d'abord sans option. Une identité appairée en cache n'est pas une connexion. Avec le BS2 présent/connecté et l'accord de test déjà donné, utiliser `--inspect` pour lire PnP et cataloguer les services/caractéristiques.
4. Si plusieurs candidats, choisir explicitement le fingerprint. Si identité différente/inconnue, arrêter les opérations constructeur et comprendre la différence, pas supprimer le garde-fou.
5. Lire UUID/propriétés/tailles, sans écrire aux caractéristiques inconnues ni souscrire arbitrairement. Les identités standard lues par le survey ne comprennent pas le numéro de série.
6. Observer l'application officielle lors d'un changement RGB isolé : rouge, vert, bleu purs, luminosité, Off, puis un effet. Toujours changer réellement de couleur (un test bleu alors que l'appareil est déjà bleu ne valide rien).
7. Ne pas arrêter le service Space Station ni sa politique de refroidissement pour contourner un verrou GATT sans vérifier les conséquences et convenir de la manipulation avec l'utilisateur. `AccessDenied` peut venir d'un accès exclusif : préférer lecture app fermée ou capture ciblée des écritures RGB constructeur.
8. Valider le format puis un essai RGB borné, en excluant toutes commandes RPM/courbes/Smart/Overclock/stop/save/global config. Comparer les réponses et vérifier visuellement les LED. Lire l'état de refroidissement avant/après si une lecture sûre est identifiée.
9. Tester si plusieurs couleurs sont simultanées et fixes, puis compter/identifier les zones. Ne pas présenter une animation temporelle comme de l'ARGB indépendant.
10. Effets matériels : envoyer une fois, arrêter l'outil de test, vérifier que l'effet continue. Tester Off et restauration, reconnexion/veille, absence d'interférence entre transports et autres programmes.
11. Seulement après cela, ajouter un RGBController BS2 natif et son backend BLE. Discovery sans prise de contrôle, worker propriétaire du transport, interface Qt non bloquée, dernière scène seulement, pas de polling actif ni sauvegarde NVRAM par frame, ACK stricts et arrêt propre.
12. Direct/Ambient : vérifier qu'un chemin **volatile** et une cadence soutenable existent. Ne pas boucler sur une commande de sauvegarde ou de finalisation flash à 20/60 FPS. Ne pas convertir les résultats Apex en limite certifiée BS2.
13. Compiler/tester réellement puis signer, sauvegarder, installer, archiver dans Downloads, commiter et pousser. Documenter précisément ce qui n'a pas pu être validé.

## 5. Commandes de préparation réelles

Depuis `C:\Laragon\www\OpenRGB\new_version` :

```powershell
cmd /c tests\FlydigiBS2Pro\Run-Tests.cmd
dotnet build tests\FlydigiBS2Pro\BluetoothSurvey\BluetoothSurvey.csproj -c Release
# Appairage uniquement : ne demande aucune connexion GATT
 dotnet tests\FlydigiBS2Pro\BluetoothSurvey\bin\Release\net9.0-windows10.0.26100.0\BluetoothSurvey.dll
# À utiliser seulement quand le BS2 est revenu et connecté :
 dotnet tests\FlydigiBS2Pro\BluetoothSurvey\bin\Release\net9.0-windows10.0.26100.0\BluetoothSurvey.dll --inspect
```

Une ambiguïté se résout avec `--fingerprint=HASH`. Pas de dépendance d'exécution .NET ajoutée à OpenRGB : ce projet est un outil de diagnostic séparé.

## 6. Contexte et acquis à préserver

### Razer — incident important

Le passage global V4 en mode logiciel **3** pour Direct contournait les boutons enregistrés dans sa mémoire sans Synapse. Ils n'étaient pas effacés : retour au mode matériel **0** + couleurs Direct rétablissait boutons ET RGB. Correctif livré **36384d90** : V4 garde le mode matériel pour tous ses modes d'éclairage. Après l'installation Apex, souris et clavier ont été relus en `0,0`. **Ne pas réintroduire le mode 3 pour obtenir du RGB.**

V4 USB 1532:00E2, récepteur 00E3, IF3/000C:0001 ; sept LED (logo 1, molette 1, underglow 5). Bluetooth : 068E:00E4, trois groupes natifs (logo 4, molette 1, underglow 10), pas de streaming sept pixels validé. USB/RF utilise un RGB 0F:03 **write-only** confirmé par capture officielle ; GET de cette commande retourne BUSY. Ne pas relâcher les ACK des autres commandes ni modifier la V3.

Les luminosités par groupe survivent aux changements de transport. Ancien bug : logo/underglow à zéro en BT puis toujours éteints en USB ; la V4 réapplique maintenant les trois luminosités connues. Conserver.

La V3 Phantom Green reste supportée, géométrie différente (12 positions, pas de logo). Le clavier BlackWidow V3 Mini, son dongle et la paire HyperSpeed restent supportés. Paramètre du PC : `RazerHyperspeed.BlackWidowV3MiniAndBasiliskV4ProSharedPair=true`, ancien paramètre de paire V3 false ; conserver les deux fonctionnalités, pas remplacer la V3 par la V4. Batteries : **5 minutes + bouton manuel**, ne pas revenir à 30 secondes. Préserver DPI, polling 500 Hz demandé sur l'ancienne souris, boutons, veille et Smart Dimming.

Références : `tests/RazerV4Support/README.md`, `VALIDATION.json`, `tests/RazerSupport`, `tests/RazerBluetoothReconnect`. Des documents historiques peuvent être moins récents que le correctif V4.

### Apex 5

Support livré en **964e02d9** : USB/RF 37D7:2501, IF2 FFA0:0001, 32 octets, 12 LED. Métadonnées d'éclairage et données couleur, aucun A6/save. Effets natifs Static/Off/Breathing/Gradient/Default/Dynamic glow ; Direct pour Ambient et VMap. Cible 20 FPS, environ **81 scènes / 5,02 s** réellement mesurées, pas une certification constructeur.

Bluetooth testé : Xbox classique 045E:02E0, entrée HID 16, sortie/Feature 0, pas de service BLE correspondant ou RFCOMM trouvé. L'utilisateur confirme que Space Station ne la contrôle pas non plus dans ce mode et **accepte de garder USB/2,4 GHz**. Ne pas rouvrir cette investigation au lieu du BS2 sauf nouvelle demande.

Références : `Controllers/FlydigiController`, `tests/FlydigiApex5/README.md`, `VALIDATION.json`. Ne pas réutiliser ses UUID/rapports/12 LED pour le BS2. Les entrées USB/RF sont distinctes, avec priorité USB pour éviter des écritures concurrentes.

### Autres intégrations

- LightSync OpenRGB : Govee/Yeelight LAN + clavier Lenovo via Windows Dynamic Lighting. Ne pas réactiver le LightSync de Legion Toolkit que l'utilisateur a désactivé.
- Sources plugins : `C:\Laragon\www\OpenRGB\Plugin\OpenRGBLightSyncPlugin`, `OpenRGBEffectsPlugin`, `OpenRGBVisualMapPlugin`. Autres itérations historiques : `C:\Laragon\www\RazerAmbilight` et `C:\Laragon\www\LenovoLegionToolkit`.
- Ambient : optimisations, saturation, black boost, blending, screensaver, modes ultrawide/real ambilight déjà ajoutés. Ne pas empêcher l'écran de veille OLED ni appeler des fonctions de maintien éveillé pour le RGB.
- Visual Map : ancien historique de crash par concurrence et contrôleurs détruits encore référencés. Respecter Shutdown, locks et hooks de détection ; ne pas remplacer/supprimer des contrôleurs vivants pendant les effets.
- RX 7900 XTX Reference se trouve sur **l'autre PC**, pas ici. Modes natifs conservés ; Direct asynchrone, cadence documentée. Ne pas prétendre la tester sur ce PC.
- Apparence moderne Qt (pas une réécriture WPF) : menu gauche pliable via logo blanc, accents Windows, un seul Settings en bas et Appearance dans General Settings. Préserver cette identité.

## 7. Build, signature, installation et sauvegardes

Environnement local : Windows 11, PowerShell, VS **18 Community** MSVC 14.51, Qt **6.8.3 MSVC2022_64**, jom. Qt : `C:\Laragon\www\OpenRGB\Plugin\build-tools\qt\msvc2022_64` ; jom dans `Plugin\build-tools\jom`.

```powershell
cmd /c scripts\CompileModern.cmd
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\VerifyModernBuild.ps1
```

Le script appelle les tests Radeon et qmake `-nocache`. La génération peut prendre plusieurs minutes. Conserver le journal et le vrai code de sortie. Le dossier parent contient une autre copie : de mauvais headers de fenêtre ont déjà causé une corruption de tas. **Ne jamais contourner VerifyModernBuild** : startup.obj doit utiliser `new_version\qt\OpenRGBDialog\OpenRGBDialog.h` à jour, pas celui du parent. Avertissements qmake de chemins dupliqués à examiner, jamais preuve automatique de succès ou d'échec.

Vérifications à conserver : protocol/worker Flydigi, régressions Razer pertinentes, identité et signatures. Pas besoin de reconstruire/réinstaller l'application pour cette préparation header/outil/docs inactive.

Identité Windows installée : **OpenRGB.LightSync_h6e9bjf6s4fnj**, Publisher `CN=OpenRGBLightSync`, certificat public thumbprint `D40C51EF8E99E81AFFAB82705D405784DDB9E1E7`. Version de package lors de la livraison Apex : **1.0.0.18**. Elle donne le contrôle durable du Lenovo en arrière-plan : ne pas créer une nouvelle identité accidentelle, désenregistrer le package ou lancer un exe non signé à la place de l'installation sans comprendre l'effet WDL.

Processus reproductible avec les scripts **suivis** (les helpers dans build sont locaux, pas garantis ailleurs) :

1. Demander de sauvegarder profil + VMap et quitter OpenRGB depuis le tray avant remplacement. Ne pas tuer une instance utilisateur et perdre sa carte.
2. Copier l'exécutable compilé vers `app\OpenRGB.exe` seulement quand il n'est pas verrouillé.
3. `scripts\Prepare-ApplicationIdentity.ps1 -InstalledRoot ...\app` sous le **compte bureau** ayant la clé locale existante. Lire `build\identity\prepared.json`, recopier son Exe signé, Package, SwitchPackage et Certificate public dans app. Contrôler Authenticode `Valid`.
4. `Install-LightSync.ps1` sauvegarde binaires/configuration et conserve les profils, puis installe et enregistre l'identité. Lire son résultat/`deployment.json`. Ne pas remplacer à la main les dossiers de configuration.
5. `scripts\PackageModern.ps1 -OfflineSnapshot -SourceCommit <SHA40> -LightSyncCommit <SHA-plugin-réel> -Destination 'C:\Users\Administrator\Downloads'`. Dernier SHA LightSync utilisé : `a94f560f785e57379bc81b7e07c5b6973c86591c`, **à revérifier** lors d'une modification du plugin.
6. Le ZIP complet doit contenir app, payload, scripts, documentation, validation, sauvegarde de retour arrière, manifests et hashes, pas juste un exe. Vérifier les fichiers et leurs hashes, pas seulement l'existence du ZIP.
7. Jamais de clé privée PFX/P12/KEY ou plugin de développement dans l'archive/le dépôt. Sur un autre PC, ne pas supposer que la clé de signature existe : lire les docs d'identité et préparer explicitement la procédure locale.
8. Relancer l'installation, contrôler logs/détection/réactivité et les appareils réellement disponibles. Le paquet public .cer n'est pas la clé permettant de signer de nouveaux builds.

## 8. Pièges de l'environnement et du SDK

Le bac à sable de cette session n'autorise pas directement les écritures dans OpenRGB : demander l'élévation de l'outil pour les edits/build/git ; ne pas déplacer les sources vers un autre projet pour contourner cela. Même certains `Get-PnpDevice` ont besoin d'élévation.

WinRT Bluetooth et certificats sont **liés au compte utilisateur**. L'agent a parfois tourné avec un SID secondaire alors que le bureau/les appairages/la clé étaient sur le compte Administrator (RID 500). Vérifier le contexte ; un accès refusé ne prouve pas l'absence d'un service. Des helpers PowerShell lancés via `Start-Process -Verb RunAs -WindowStyle Hidden` ont servi ici. `WaitForExit(timeout)` sur le helper, pas `-Wait` qui peut attendre tout l'arbre de processus. Sur une autre machine, ne pas supposer que cette mécanique change l'utilisateur de la même manière.

Utiliser des chemins explicites et le bon dossier de travail, `rg` pour rechercher, pas de suppression récursive ni reset non autorisé. Ne pas exporter de secrets dans les logs. Les appairages en cache ne sont pas une mesure de connexion radio.

SDK : privilégier **v6, IDs stables et ACK vérifiés** après hotplug. Le premier test Apex utilisait v3 avec index zéro : après USB → RF, les requêtes semblaient lister le bon appareil mais les écritures n'atteignaient pas le nouveau contrôleur. Corriger le client de test a rétabli le fonctionnement ; ne pas incriminer le firmware ni modifier le SDK global sans preuve. `tests/FlydigiApex5/Test-SDK.py` est la référence locale pour un serveur isolé.

Le script d'essai OpenRGB doit activer uniquement les détecteurs cibles et désactiver plugins/LAN dans un **dossier config séparé**, port SDK de test 6745 par exemple, sans écraser les réglages utilisateur. `--server --server-port 6745`, pas `--server 6745`. Ne pas lancer deux instances qui contrôlent les mêmes lumières. Les helpers de fermeture doivent viser uniquement l'instance cachée créée par l'IA, pas un OpenRGB quelconque.

## 9. Pratiques de collaboration attendues

L'utilisateur veut du travail concret et autonome : inspecter, implémenter, compiler, tester, sauvegarder, documenter, commiter et pousser dans le périmètre convenu. Ne pas s'arrêter à un plan ou après une question si du travail indépendant reste possible. Ne pas redemander une permission déjà donnée. Demander les manipulations physiques manquantes et la fermeture propre d'un logiciel verrouillant les fichiers au moment nécessaire.

Expliquer clairement les causes prouvées versus hypothèses. Une réussite HID/GATT/SDK n'est pas une confirmation visuelle ni une garantie de débit continu/batterie. Faire confirmer une mire différente de l'état précédent, puis les couleurs simultanées et l'autonomie après arrêt de l'émetteur. Tenir un journal compact, pas de spam par frame ; annoncer honnêtement tout test qui manque.

Ne pas masquer le manque de matériel en ajoutant un périphérique factice, en inventant une limite, en assouplissant les ACK ou en désactivant des protections. Si un test ne peut avancer, terminer la préparation, les tests et la documentation sans prétendre que la prise en charge est active.
