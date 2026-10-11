# Razer Bluetooth — version Windows x64

## Résultat livré

Pilote natif intégré à OpenRGB 1.0, sans Synapse ni dépendance .NET à l’exécution. Windows 11, Qt 6.8.3, API de plugins 5 conservée.

| Appareil validé | Identité PnP | Zones Bluetooth | Modes |
| --- | --- | --- | --- |
| BlackWidow V3 Mini | 1532:0272 | Tout le clavier : 05 | Static, Off, Wave matériel |
| Basilisk V3 Pro 35K Phantom Green | 068E:00D8 | Molette : 01 ; contour : 0A | Static, Off |

Luminosité 0–255. Le catalogue souris annonce aussi 04, mais cette zone logo sans éclairage physique est exclue des écritures.

## Validation effectuée

- Compilation complète Windows x64 Release réussie avec MSVC 2026 et Qt 6.8.3.
- Transport C++ natif : lecture des deux appareils, identités PnP et états des zones.
- OpenRGB SDK : deux contrôleurs et trois zones, pas de matrice fictive.
- Confirmation utilisateur : clavier rouge fixe, molette bleue, contour vert, tous stables.
- Confirmation utilisateur : Off éteint les trois éclairages.
- Confirmation utilisateur : Wave sur le clavier, souris en couleurs fixes, luminosité 160/255.
- Clavier éteint dix secondes puis rallumé : retour automatique en rouge confirmé par l’utilisateur.
- Relecture directe finale : clavier `01000001FF0000000000`, molette `010000010000FF000000`, contour `0100000100FF00000000`, luminosités `60` hexadécimal (96/255).
- Détecteurs USB/HyperSpeed et Bluetooth activés, dongle souris connecté et appareils en Bluetooth : seulement les deux contrôleurs Bluetooth enregistrés dans le journal de détection, aucun doublon RF.
- Tests existants des mappings et transports USB/HyperSpeed réussis.
- Effects et Visual Map : chargement Qt 6.8.3/API 5 réussi.

Les journaux locaux sont dans `build/bluetooth-*.log` et les configurations isolées dans `build/bluetooth-validation-config` et `build/bluetooth-coexistence-config`.

## Fonctionnement et limites

Un worker par appareil possède sa connexion WinRT. Les transactions sont séquentielles, avec délais bornés. Une seule scène attend l’envoi ; les nouvelles valeurs remplacent les anciennes. Pas de boucle active. Les états identiques sont ignorés. Les changements envoyés par le PC sont limités à cinq par seconde ; Wave est animé par le firmware.

Après une demande de contrôle par OpenRGB, une perte de connexion provoque des tentatives espacées et la réapplication de la dernière scène. L’USB est prioritaire : le pilote Bluetooth refuse les écritures lorsque le PID filaire correspondant est présent.

La sélection USB/HyperSpeed existante reste automatique. La bascule complète de la liste OpenRGB entre Bluetooth et USB/HyperSpeed n’est pas implémentée : utiliser Rescan après un changement de transport. Une entrée Bluetooth déjà créée peut rester visible après branchement du câble, mais ses écritures sont suspendues. Un appareil absent au démarrage nécessite également une redétection lorsqu’il devient disponible. La priorité USB est implémentée ; le branchement d’un câble pendant cette session Bluetooth n’a pas été testé physiquement.

Aucune commande de couleur individuelle des touches n’a été identifiée. La lecture de trame Bluetooth connue `10 84 00 00` retourne le statut 5 (non pris en charge). Cela ne prouve pas qu’aucune commande cachée n’existe. Razer réserve Chroma Studio à USB/2,4 GHz ; les animations matérielles Bluetooth ne sont pas un flux de couleurs personnalisées envoyé touche par touche. Pour le contrôle individuel et Ambilight, conserver USB/HyperSpeed.

Pas de mode Direct Bluetooth ni de promesse de 60 FPS. Les appareils Bluetooth partagent une couleur par groupe. Wave utilise les paramètres réellement observés sur ce clavier ; vitesse et direction ne sont pas exposées sans validation supplémentaire.

## Installation et retour arrière

1. Quitter les autres instances OpenRGB depuis la zone de notification.
2. Extraire entièrement la nouvelle archive dans un dossier distinct.
3. Appairer et réveiller les périphériques Bluetooth.
4. Ouvrir directement `OpenRGB.exe`. Le dossier `config` voisin est utilisé automatiquement.
5. Les entrées portent le suffixe `(Bluetooth)`. Choisir Static/Off, ou Wave pour le clavier ; régler la luminosité normalement.

La version installée n’est pas écrasée. Pour revenir en arrière, quitter cette version portable et ouvrir l’exécutable installé précédent. Ne pas mélanger les DLL Qt de différentes archives. La nouvelle archive contient les plugins compatibles existants et une copie de la configuration portable avec le détecteur Bluetooth activé.

## Fichiers

- `Controllers/RazerBluetoothController/RazerBluetoothTransport_Windows.h` : identité, connexion, notifications et protocole.
- `Controllers/RazerBluetoothController/RGBController_RazerBluetooth_Windows.cpp` : détection, worker, groupes et modes OpenRGB.
- `OpenRGB.pro` : liaison Windows Runtime.
- `tests/RazerBluetoothPrototype/Program.cs` et `README.md` : clavier, PnP, Wave et diagnostic de trame.
- `tests/RazerSupport/Test-Bluetooth-SDK.py` : tests isolés des modes, couleurs et luminosité, avec contrôle du mode accepté.
- `tests/RazerSupport/Inspect-SDK.py` : distinction entre clavier Bluetooth groupé et matrice USB/HyperSpeed.
- `BLUETOOTH_FEASIBILITY.md` et ce rapport : résultats et limites.

Références : [GATT Windows](https://learn.microsoft.com/en-us/windows/apps/develop/devices-sensors/gatt-client), [protocole BLE OpenSnek](https://github.com/gh123man/OpenSnek/blob/main/docs/protocol/BLE_PROTOCOL.md), [limites Bluetooth BlackWidow V3 Mini](https://mysupport.razer.com/app/answers/detail/a_id/5307/kw/razer%20blackwidow%20support).
