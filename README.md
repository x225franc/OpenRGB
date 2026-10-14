# OpenRGB LightSync — plugin natif Windows x64

LightSync expose les Govee segmentés, Yeelight et Lenovo Lighting dans Devices. Les effets et le placement sont gérés par Effects et Visual Map, sans SDK bridge ni LightSync standalone.

## Installation / autre PC

Le ZIP contient une copie portable complète et `payload` contenant les binaires à installer. Windows 11 x64 et les runtimes Visual C++ 2022 sont nécessaires. LAN Control doit être activé dans Govee/Yeelight, sur le même réseau IPv4. Aucun SDK Windows ni clé privée n'est nécessaire pour installer.

1. Sauvegarder le profil (inclure Effects et LightSync) et VMap → Save. Quitter OpenRGB, LightSync standalone et désactiver Toolkit LightSync.
2. Extraire entièrement le ZIP. Dans PowerShell **administrateur**, sous le compte Windows qui utilisera OpenRGB :
   `powershell -ExecutionPolicy Bypass -File .\Install-LightSync.ps1`
   Pour un autre dossier : ajouter `-Target 'D:\OpenRGB'`. Ce dossier doit contenir une installation compatible Qt 6.8.3 / API plugins 5 ; le dossier `portable` fourni peut servir de base.
3. Le script sauvegarde les anciens binaires et `config`, importe uniquement le certificat public local dans TrustedPeople et enregistre le package signé OpenRGB.LightSync. Ce n'est pas un service Windows. La clé privée reste sur le PC de développement.
4. Lancer directement OpenRGB.exe. Windows → Personnalisation → Éclairage dynamique → Contrôle en arrière-plan : mettre **OpenRGB LightSync** en tête. Garder le contrôle Windows activé.
5. LightSync → Search again. Autoriser OpenRGB dans le pare-feu sur le réseau privé (Govee UDP 4001–4003, Yeelight découverte UDP 1982, contrôle TCP 55443 et connexion entrante musique sur port dynamique).

Le certificat est valide trois ans à compter de sa création. En cas de renouvellement, il faut recevoir un nouveau package signé avec son certificat public. Ne pas distribuer de PFX/clé privée.

## Retour arrière / suppression de l'identité

Quitter OpenRGB. Utiliser le dossier de sauvegarde affiché par l'installation :
`powershell -ExecutionPolicy Bypass -File .\Uninstall-LightSync.ps1 -Backup 'C:\Program Files\OpenRGB\Backup-installation\LightSync-AAAA...' -RemoveCertificate`

Ajouter `-RestoreConfiguration` uniquement pour restaurer aussi les réglages sauvegardés (cela écrase les fichiers correspondants actuels). Le script restaure les binaires, retire OpenRGB.LightSync et, avec l'option, le certificat TrustedPeople. Il ne touche pas Toolkit. Il ne supprime pas la clé de signature dans CurrentUser\My du PC de développement. Une éventuelle ancienne identité OpenRGB.LightSync n'est pas automatiquement réenregistrée ; ses métadonnées sont conservées dans previous-package.json. Relancer Install-LightSync.ps1 pour refaire.

## Utilisation

- Les modèles testés : H61A0 (10 segments), H6061 (10), H6056 (6), Yeelight color (1 LED), Lenovo (319 lampes).
- Dans Ambient, sélectionner Lenovo Lighting directement pour qu'il suive tout l'écran ; laisser le clavier hors de la VMap Razer.
- Placer individuellement les segments LAN dans VMap ; ne pas sélectionner à la fois la VMap et ses périphériques physiques dans un effet.
- Désactiver les détecteurs natifs LAN ou supprimer leurs entrées manuelles pour ces mêmes lumières : un seul transport doit les contrôler.
- Les noms peuvent être personnalisés par OpenRGB. Settings inclut extinction au sommeil, réveil, interpolation, seuil noir/fade Yeelight, fréquences, log et diagnostic.
- Désactiver/restart garde les identités des appareils pour préserver VMap. Les recherches manquées n'interrompent pas Govee. Yeelight libère son mode musique à l'arrêt et respecte une extinction externe détectée ; Restart ou Control réactive ce dernier cas.
- Ambient ajoute Real Ambilight (bord inférieur) et Ultrawide (centre 16:9), en conservant saturation et pause écran de veille déjà présents.

## Validation et limites

Build Release x64 Qt 6.8.3, API plugins 5, MSVC. Tests des paquets Govee, découverte Yeelight et chargement réel de DLL/metadata. Tests Ambient de capture, sauvegarde, saturation, pause écran de veille et nouveaux mappings. Test matériel : sept lumières LAN colorées indépendamment ; clavier Lenovo rouge confirmé après enregistrement Windows.

Tester après installation : découverte des huit appareils, Ambient sur les segments, reprise après veille/hibernation, Google éteint les Yeelight après fermeture OpenRGB, désactivation/réactivation et redémarrage avec profil/VMap. Ces scénarios système complets demandent une observation humaine ; aucune mesure de fluidité/latence n'est inventée.

Yeelight utilise le mode musique, sans quota TCP normal sur les couleurs, avec une cadence logicielle configurable de 1 à 20 FPS (10 par défaut). Cette borne ne certifie pas la cadence matérielle continue. Les Govee inconnus utilisent un nombre de segments par défaut et peuvent nécessiter une entrée de modèle ; cette version est validée sur les modèles ci-dessus. Un changement de nombre de lampes nécessite de relancer OpenRGB. Les sessions Yeelight peuvent ne pas acquitter la libération malgré l'envoi ; les logs distinguent cela.

## Sources et compilation

`build.cmd`, puis `test.cmd`. Qt/MSVC sont configurés pour ce PC ; adapter leurs chemins sur une autre machine. `Prepare-Identity.ps1` est réservé à une reconstruction/signature locale et requiert les outils Windows SDK. Les scripts d'installation utilisent les fichiers précompilés dans payload, jamais la clé privée.

Core OpenRGB : arrêt explicite des callbacks du contrôleur virtuel avant destruction. Effects : uniquement les options Ambient de bord inférieur et centre 16:9. Les autres effets et les contrôleurs Razer restent inchangés.

## Contrôle Lenovo en arrière-plan et cadence

La section Lenovo de LightSync affiche un état vert lorsque Windows accorde le contrôle, un avertissement lorsque la priorité est perdue, et les FPS réellement envoyés (limités au minimum matériel et à 60 FPS). Un thread dédié transmet la dernière image sans attendre les connexions Yeelight ou la découverte LAN. Les handles Windows sont conservés entre recherches ; ils sont libérés au sommeil, à la désactivation et à la fermeture. Le clavier ne reçoit pas un second lissage LAN par-dessus le lissage Ambient.

Windows choisit le contrôleur en arrière-plan selon sa priorité. Si OpenRGB est derrière Toolkit ou Windows Lighting, cliquer hors de sa fenêtre peut lui retirer le clavier. Mettre OpenRGB LightSync en premier dans les paramètres Windows pour le clavier Lenovo. Sur le matériel VID_048D/PID_C193, le script `Configure-KeyboardPriority.ps1` permet aussi cette correction, sous le compte Windows concerné. Il ne change que la liste Providers de ce clavier, conserve l'ordre relatif des autres applications et sauvegarde la clé avant modification. Cette clé Windows est un détail interne observé sur cette version de Windows ; privilégier l'interface Windows si elle évolue.

`powershell -ExecutionPolicy Bypass -File .\Configure-KeyboardPriority.ps1`

Pour défaire ce changement, utiliser le chemin de sauvegarde affiché :
`powershell -ExecutionPolicy Bypass -File .\Configure-KeyboardPriority.ps1 -Restore 'C:\...\Lenovo-priority-AAAA.reg'`

La désinstallation des binaires n'écrase pas automatiquement les préférences Windows de priorité ; restaurer cette sauvegarde séparément si souhaité. Les contrôleurs Razer et leur priorité ne sont pas modifiés par ce script.

La livraison inclut également la correction de fermeture Visual Map : son contrôleur virtuel est détruit avant son objet de rappel. Effects retire ses callbacks avant déchargement. Les trois DLL sont sauvegardées par l'installateur et restaurées par le script de désinstallation.

Fermeture OpenRGB : les pages Devices et Information, y compris les membres masqués d'une carte, sont détruites avant que les plugins suppriment leurs contrôleurs. Cela évite un accès à un contrôleur déjà libéré pendant le retrait des callbacks Qt.

## Enregistrer / désenregistrer une application dans Éclairage dynamique

Les icônes du haut sont des périphériques HID détectés par Windows. Les entrées du bas (« OpenRGB LightSync », « Lenovo Legion Toolkit ») sont des applications enregistrées pour le contrôle en arrière-plan. Désactiver LightSync dans Toolkit arrête son moteur mais ne retire pas son identité Windows de cette liste. Il n'est pas nécessaire de retirer Toolkit pour utiliser OpenRGB : laisser LightSync Toolkit désactivé et OpenRGB LightSync en première position suffit. Garder « Utiliser l'éclairage dynamique » activé pour le clavier Lenovo.

Toutes les commandes suivantes s'exécutent dans PowerShell sous le compte Windows qui utilise les applications. Quitter l'application concernée avant de changer son enregistrement. L'enregistrement est propre au compte utilisateur ; utiliser un autre compte administrateur enregistre l'application pour cet autre compte.

### OpenRGB : enregistrer uniquement l'identité

Depuis le dossier extrait du ZIP, dans PowerShell administrateur (même utilisateur) :

```powershell
Import-Certificate -FilePath '.\payload\OpenRGBLightSync.cer' -CertStoreLocation Cert:\LocalMachine\TrustedPeople
Add-AppxPackage -Path '.\payload\OpenRGB.LightSync.msix' -ExternalLocation 'C:\Program Files\OpenRGB'
Get-AppxPackage -Name OpenRGB.LightSync | Select-Object Name, PackageFullName, Status
```

ExternalLocation doit désigner le dossier de l'exécutable OpenRGB signé fourni dans ce ZIP. Pour installer aussi les binaires, préférer Install-LightSync.ps1. Lancer OpenRGB, rouvrir les paramètres Windows et placer OpenRGB LightSync en premier dans le contrôle en arrière-plan.

### OpenRGB : retirer uniquement l'identité

```powershell
Get-AppxPackage -Name OpenRGB.LightSync | Remove-AppxPackage
```

Cela ne supprime ni OpenRGB.exe, ni ses plugins, ni les profils. Le contrôle Lenovo durable en arrière-plan exige cette identité ; le contrôle LAN Govee/Yeelight n'en dépend pas. Pour la rétablir, refaire les commandes d'enregistrement ci-dessus. Pour restaurer aussi les anciens binaires, utiliser Uninstall-LightSync.ps1 avec une sauvegarde contenant rollback.json.

### Toolkit : arrêter LightSync sans retirer Toolkit

Dans Toolkit → LightSync, désactiver Enabled et conserver ce réglage. Arrêter aussi LightSync standalone. Toolkit peut continuer ses autres fonctions ; son entrée dans Éclairage dynamique peut rester visible. Ne pas activer deux moteurs LightSync pour les mêmes lumières.

### Toolkit : retirer son entrée Windows et la remettre plus tard

Sur ce PC, le manifeste installé a pour identité `eef45acd-2cf3-4d7d-9d33-92f37c74cc31`. Après avoir désactivé LightSync et quitté Toolkit :

```powershell
Get-AppxPackage -Name 'eef45acd-2cf3-4d7d-9d33-92f37c74cc31' |
    Select-Object Name, PackageFullName, InstallLocation
Get-AppxPackage -Name 'eef45acd-2cf3-4d7d-9d33-92f37c74cc31' | Remove-AppxPackage
```

Cette opération retire l'identité du package Toolkit pour ce compte, pas seulement un périphérique. Les fichiers et réglages Toolkit restent sur disque ; son contrôle Dynamic Lighting en arrière-plan sera indisponible jusqu'au réenregistrement. Le certificat Toolkit reste installé. Pour réenregistrer cette installation, dont le package est déjà signé :

```powershell
Add-AppxPackage -Path 'C:\Program Files\LenovoLegionToolkit\LenovoLegionToolkit.LampArray.msix' `
    -ExternalLocation 'C:\Program Files\LenovoLegionToolkit'
```

Sur une autre installation, vérifier d'abord le nom Identity dans son AppxManifest.xml, puis adapter les chemins et le nom exact. Conserver le package signé et son certificat avant de retirer une identité. Ne pas supprimer « Dynamic Lighting Background Controller » : c'est le contrôleur Windows intégré. Ne pas effacer manuellement les clés de registre pour désenregistrer une application. Après modification, relancer l'application et rouvrir les paramètres Windows ; une réinstallation du package peut rétablir son entrée.

Références Microsoft : [contrôle Dynamic Lighting et identité de package](https://learn.microsoft.com/en-us/windows/uwp/devices-sensors/lighting-dynamic-lamparray), [priorités dans les paramètres Windows](https://support.microsoft.com/en-us/windows/hardware/input-devices/control-dynamic-lighting-devices-in-windows).

## Ajouter de nouvelles lumières LAN : découverte et compatibilité

Activer LAN Control sur une nouvelle ampoule Yeelight couleur, la connecter au même réseau IPv4 puis cliquer sur LightSync → Search again. Une recherche périodique a également lieu toutes les 20 secondes lorsque le moteur fonctionne. Si l'appareil répond, un contrôleur distinct apparaît dynamiquement dans Devices et dans la liste LightSync, avec son adresse IP, sans modification du code ni redémarrage pour une nouvelle ampoule. L'ajouter ensuite à l'effet ou à Visual Map ; l'inscription dans un effet existant n'est pas garantie automatiquement. Le moteur n'envoie pas de couleur tant qu'OpenRGB ne lui fournit pas une mise à jour.

La découverte multicast n'est pas une garantie de prise en charge complète. Le transport Yeelight actuel utilise notamment set_music et set_scene en mode couleur : les ampoules couleur LAN sont le cas pris en charge et testé. Chaque Yeelight est représentée par une LED ; les modèles uniquement blancs, plusieurs zones ou sans mode musique peuvent nécessiter une adaptation.

Govee : la découverte LAN est automatique, mais le contrôle segmenté dépend du protocole du modèle. Cette version déclare 6 segments pour H6056 et 10 pour les autres Govee découverts, correspondant aux H61A0/H6061 testés. Elle ne détermine pas automatiquement le nombre réel de segments d'un modèle inconnu. Un autre appareil peut être détecté tout en nécessitant une adaptation du nombre de segments ou des paquets. Aucun support universel Govee/Yeelight n'est revendiqué.

En cas d'absence : vérifier LAN Control, même réseau, isolation Wi-Fi/VLAN, pare-feu privé et absence d'un autre contrôleur. Le plugin effectue une découverte locale, pas un appairage cloud ou Bluetooth.

### Sleep / hibernation recovery

The Windows Dynamic Lighting session is released on suspend and explicitly
reopened on resume, even when the device identifier is unchanged. Rediscovery is
retried after 1.5, 5 and 15 seconds while keeping the existing OpenRGB controllers,
Visual Map routes and latest requested colors. Duplicate suspend/resume messages
are coalesced. Invalid LampArray objects retry after two seconds; an unavailable
session gets the same 45-second grace period and three-minute recovery window as
LightSync standalone/Toolkit, armed at startup, restart, resume or session unlock.
Ordinary foreground priority changes outside this window do not reopen the device.
Recovery does not override another application's Windows lighting priority.

The log records Suspend/Resume and the displayed keyboard status shows waiting,
reconnecting or controlled, plus the measured write FPS when control is granted.
OpenRGB must keep its signed OpenRGB.LightSync identity and background priority.

OpenRGB's native startup checkbox starts the app at Windows sign-in. It does not
restart a crashed app at resume. Sleep should normally preserve the process.
A separate recovery task can launch the app only if it is stopped; it is not a
substitute for fixing the crash. No such task is installed by the plugin itself.

### Optional resume recovery and Toolkit lighting deregistration

Run these scripts under the Windows user who uses OpenRGB:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Configure-ResumeRecovery.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Configure-ResumeRecovery.ps1 -Remove
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Unregister-ToolkitLighting.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\Unregister-ToolkitLighting.ps1 -Restore
```

Resume recovery registers the current user's interactive scheduled task
**OpenRGB LightSync Resume Recovery**. It waits 20 seconds after the System log's
Power-Troubleshooter resume event (ID 1), then starts the signed installed EXE
minimized only if no OpenRGB process exists in that session. It does not wake the
PC, need a password, or run as a Windows service. Logs are in
`%LOCALAPPDATA%\OpenRGB-LightSync\ResumeRecovery.log`.
Disable it in Task Scheduler or run `-Remove`.

Toolkit deregistration removes only its sparse Dynamic Lighting package and its
provider-list entries. Toolkit's installed files and external settings remain.
The lighting registry and original package identity are backed up in
`%LOCALAPPDATA%\OpenRGB-LightSync\ToolkitIdentityBackup`.
`-Restore` registers the existing Toolkit LampArray MSIX again. Restore the REG
backup only if you also want the old provider priorities; it contains the whole
lighting registry, so it can overwrite later lighting preferences.
