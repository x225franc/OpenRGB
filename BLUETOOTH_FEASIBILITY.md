# Étude de faisabilité : Razer Bluetooth dans OpenRGB 1.0

Date : 11 octobre 2026. Machine Windows 11 de l’utilisateur.
Périmètre : étude et diagnostics en lecture, aucun pilote RGB Bluetooth activé,
aucune couleur, aucun appairage ou firmware modifié, aucune nouvelle application
OpenRGB déployée pour cette étude.

## Conclusion

La détection est techniquement faisable. Le pilotage de couleurs et de luminosité
est une piste crédible, mais nécessite un transport BLE GATT dédié et une
validation du protocole sur chaque modèle. La présence du service propriétaire
ne prouve pas encore l’acceptation des commandes RGB.

| Objectif | Évaluation | Preuve / limite |
|---|---|---|
| Reconnaître le clavier Bluetooth | Forte | HIDAPI expose 1532:0272, bus Bluetooth, BlackWidow V3 Mini |
| Reconnaître la souris Bluetooth | Forte | Après la bascule, HIDAPI expose 068E:00D8, bus Bluetooth, BSKV3P 35K |
| Afficher une entrée RGB réellement utilisable | Dépend du pilote | Ne pas déclarer un contrôleur Direct actif avant un échange RGB confirmé |
| Couleur / luminosité souris | Piste crédible | Même service que celui étudié pour Basilisk V3 Pro par OpenSnek ; Phantom Green 35K non validée par cette étude |
| Effets simples clavier | Piste crédible, protocole à établir | Support Bluetooth annoncé par Razer pour ce modèle ; commandes spécifiques non capturées ici |
| Couleurs indépendantes pour les 12 LED de la souris | Non établi | Le Bluetooth peut exposer des zones groupées, sans reprendre le mapping USB |
| Ambilight rapide / toutes les touches indépendantes | Non établi | Aucun débit, latence ou contrôle par touche mesuré en Bluetooth |

## Observations locales

Clavier : six collections HID Bluetooth. Rapports d’entrée de 9 ou 12 octets.
Cinq collections n’exposent aucun Feature/Output Report ; la collection clavier
expose seulement 3 octets Feature et 2 octets Output. Aucun canal Feature Report
de 91 octets utilisé par notre pilote Razer USB/HyperSpeed n’est exposé.

La souris a été identifiée après sa bascule Bluetooth. Un inventaire ultérieur
ne la contenait plus ; ce dernier relevé ne permet pas de conclure à son état
radio ni à la cause de l’absence. Le code de diagnostic filtre les appareils
actuellement énumérés, sans fabriquer une présence.

L’outil Win32 compilé pour l’étude trouve les services GATT du clavier et de la
souris dans Windows. Ils incluent le service propriétaire :

- Service : 52401523-F97C-7F90-0E7F-6C6F4E36DB1C.
- Canal de commandes : ...1524, écrivable.
- Réponses : ...1525, lisible et notifiable.
- Canal supplémentaire : ...1526, lisible et notifiable.
- ...1524 accepte aussi Write Without Response dans le descriptif souris ; cette
  propriété n’est pas annoncée pour le clavier.

Les handles GATT diffèrent entre appareils et ne doivent pas être codés en dur.
L’inventaire des services peut provenir du cache Windows. Une tentative de lecture
fraîche de batterie sur le handle parent Win32 a retourné HRESULT 0x80070001 sur
les deux appareils. Aucune valeur de batterie n’est donc revendiquée. Cet échec
ne prouve pas que la batterie ou le contrôle Bluetooth sont impossibles : il faut
valider l’accès via le handle de service approprié ou les API WinRT GATT.

## Pourquoi OpenRGB ne le fait pas actuellement

Notre pilote Razer transmet les paquets USB de 91 octets via HID Feature Reports.
Le clavier Bluetooth ne présente pas ce canal. Dans la base OpenRGB 1.0 utilisée,
le détecteur Bluetooth de la Phantom Green est commenté ; il n’existe pas de
détecteur actif 0272 pour le Mini. Ajouter uniquement un VID/PID ne crée pas un
transport de contrôle.

Le correctif HyperSpeed conserve un objet logique lorsqu’un pair disparaît.
Une entrée clavier encore visible via le dongle n’est pas la preuve qu’OpenRGB
pilote le clavier en Bluetooth. Le futur pilote devra distinguer la présence
radio réelle et l’identité des transports pour éviter les doubles écritures.

## Sources et pistes existantes

[Razer : fiche du Mini](https://mysupport.razer.com/app/answers/detail/a_id/5307/kw/razer%20blackwidow%20support)
indique que les effets Studio, Audio Visualizer et la mémoire embarquée sont
réservés au filaire/2,4 GHz ; certains effets simples restent disponibles en BT.
[Liste Synapse Bluetooth](https://mysupport.razer.com/app/answers/detail/a_id/14589/kw/pro%20click%20mini)
mentionne le Mini. Cela soutient la faisabilité de réglages simples, sans prouver
un flux arbitraire par touche utilisable par OpenRGB.

[OpenRGB MR 3218](https://gitlab.com/CalcProgrammer1/OpenRGB/-/merge_requests/3218)
décrit la désactivation Bluetooth Phantom Green malgré la présence du service
vendor. Ce n’est pas un support RGB fonctionnel livré par ce changement.

[OpenSnek : protocole BLE](https://github.com/gh123man/OpenSnek/blob/main/docs/protocol/BLE_PROTOCOL.md)
documente le même service, un format distinct du HID USB, des échanges sérialisés
avec identifiant de requête et réponses notifiées, ainsi que des réglages de
couleur/luminosité par zone sur Basilisk V3 Pro. Les touches de protocole d’un
modèle voisin doivent être confirmées sur la Phantom Green ; il ne faut pas
inventer une zone logo sur cette souris.

[Microsoft : BluetoothGATTGetServices](https://learn.microsoft.com/en-us/windows/win32/api/bluetoothleapis/nf-bluetoothleapis-bluetoothgattgetservices)
explique notamment la découverte et le cache des services. Les propriétés
écrivable/notifiable observées sont des capacités de transport, pas une garantie
sur la signification RGB des données.

## Travail nécessaire

1. Construire un petit client Windows BLE isolé pour valider une lecture vendor
   connue (identification ou catalogue de zones) et les notifications. Utiliser
   les UUID et les structures retournées ; prévoir timeout et déconnexion.
2. Commencer par la souris et les recherches publiques OpenSnek : catalogue réel
   des zones, état couleur/luminosité, puis un changement statique confirmé
   visuellement. Aucune écriture d’appairage, profils stockés ou firmware requise.
3. Pour le clavier, rechercher ses commandes spécifiques. Une capture contrôlée
   de Synapse 4 en Bluetooth, s’il est disponible, constitue un moyen d’obtenir
   des échanges de référence pour couleur et luminosité. Synapse servirait au
   diagnostic, sans devenir une dépendance du pilote final. Aucun logiciel Razer
   n’a été installé pour cette étude.
4. Ajouter un backend Razer BLE distinct, un worker non bloquant, transactions
   sérialisées, reconnexion et abandon des anciennes couleurs. Conserver API de
   plugins 5 et les transports USB/HyperSpeed existants.
5. Déclarer uniquement les capacités réellement vérifiées dans OpenRGB : zones
   groupées ou effets matériels si le firmware le permet. Tester USB/RF/BT,
   réveil, veille et absence de doublons avant livraison.
6. Évaluer le débit avec des changements de couleur une fois le protocole établi.
   Ne pas annoncer 60 FPS ni des LED indépendantes sans mesures et confirmation.

Aucun besoin matériel supplémentaire n’est établi : l’adaptateur actuel énumère
les deux périphériques et leurs services. Une sonde radio ne serait une piste
supplémentaire que si la capture logicielle ne permettait pas d’obtenir le
protocole. Pour ce premier diagnostic, les deux bascules effectuées suffisent.

## Reproduction

- `python tests/RazerSupport/Inspect-Bluetooth-Hid.py` : VID/PID, usages et tailles
  des rapports, sans numéro de série ni adresse Bluetooth publiés.
- Compiler `tests/RazerSupport/BluetoothGattSurvey.cpp` en console MSVC x64 :
  `cl /EHsc /std:c++17 BluetoothGattSurvey.cpp setupapi.lib BluetoothApis.lib ole32.lib`.
- Exécuter l’outil : propriétés GATT et tentative de lecture de batterie seulement.
- Logs locaux : `build/bluetooth-gatt-survey.log`, `build/bluetooth-hid-caps.json`.

Le pilote applicatif reste inchangé : cette livraison est une étude de
faisabilité et un outillage de découverte, pas un support Bluetooth terminé.


## Premier prototype Windows validé côté protocole

Le prototype .NET 9 WinRT dans `tests/RazerBluetoothPrototype` a connecté la souris BSKV3P 35K, lu les zones et leurs paramètres, puis écrit des couleurs fixes et une luminosité de 96/255 sur la molette et le contour. Les acquittements et relectures exactes ont réussi. Cela dépasse la simple découverte GATT ; la confirmation visuelle de l’utilisateur a également validé les deux couleurs fixes et stables. Un mode de restauration conserve les états initiaux. Aucun pilote OpenRGB installé n’a été modifié.


## Clavier et intégration native

Clavier 1532:0272 : catalogue limité à 05, couleur fixe rouge confirmée par l’utilisateur. Wave activé au clavier donne un état de dix octets, réécrit et relu avec succès. La commande de lecture de trame Bluetooth connue renvoie statut 5. Aucun protocole de couleurs individuelles n’est identifié ; cela ne constitue pas une preuve d’impossibilité matérielle. Razer réserve Chroma Studio aux connexions USB et 2,4 GHz.

Le transport C++ natif a été compilé et lit les deux modèles. Le contrôleur OpenRGB Windows déclare uniquement les groupes validés : clavier entier, molette, contour. Modes Static/Off, luminosité 0–255, Wave matériel pour le clavier. Vérification PnP stricte : source USB 02, VID/PID 1532:0272 ou 068E:00D8. Worker dédié, une seule scène en attente, transactions sérialisées, temporisation des requêtes, reprise après déconnexion et priorité à la présence USB. Aucun mode Direct ni matrice fictive.

Compilation complète Windows x64 Qt 6.8.3 réussie, API de plugins conservée. La détection SDK a exposé les deux contrôleurs et leurs trois groupes. Les confirmations matérielles et limites de livraison sont consignées dans `RAZER_BLUETOOTH_REPORT.md`.
