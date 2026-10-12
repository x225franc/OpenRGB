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

Yeelight est limité à 2 FPS et une quota de commandes conservateur pour éviter les blocages W4. Les Govee inconnus utilisent un nombre de segments par défaut et peuvent nécessiter une entrée de modèle ; cette version est validée sur les modèles ci-dessus. Un changement de nombre de lampes nécessite de relancer OpenRGB. Les sessions Yeelight peuvent ne pas acquitter la libération malgré l'envoi ; les logs distinguent cela.

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
