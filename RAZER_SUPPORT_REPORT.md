# OpenRGB 1.0 — support Razer HyperSpeed

Date : 11 octobre 2026. Branche : `feature/razer-hyperspeed-support`.
Base officielle et tag local `razer-support-baseline` : `81bbe18a84c2e507006f19dd252e397e40a56bfe` (release 1.0), identique à la version installée.

## Résultat matériel

Deux contrôleurs indépendants, BlackWidow V3 Mini et Basilisk V3 Pro 35K Phantom Green, sont détectés via un seul récepteur déjà appairé. Les deux récepteurs ont été examinés séparément. L’utilisateur a confirmé rouge fixe au clavier et vert fixe à la souris, avec les flèches Bas et Droite allumées, sur le dongle du clavier puis sur celui de la souris.

Il s’agit d’une modification du programme OpenRGB, pas d’un nouveau plugin. Effects et Visual Map restent les plugins existants. Aucun fichier de l’installation `C:\Program Files\OpenRGB` n’a été remplacé.

## Causes et corrections

### Clavier et souris

Les flèches Bas/Droite avaient déjà les adresses matérielles 78/79, soit ligne 4 colonnes 14/15 de la matrice 5 × 16. L’opération de mapping `SWAP_ONLY` ne pouvait pas ajouter une touche au bout de la liste. Elle devient `INSERT_SHIFT_RIGHT` uniquement pour ces deux entrées. Les tests reconstruisent ANSI, ISO, AZERTY et QWERTZ des variantes USB et RF ; les autres indices restent identiques. Les 80 emplacements de protocole ne représentent pas 80 touches physiques.

La Phantom Green comporte une molette et une bande de 11 LED, sans logo RGB : 12 couleurs. La définition déclarait 13 colonnes, entraînant une lecture de la treizième couleur hors du buffer. Les définitions concernées passent à 12, sans modifier les autres Basilisk.

Sur le clavier réel, les lignes RGB courtes étaient acquittées mais l’effet interne Spectrum Cycling continuait. Le test documenté OpenRazer, avec une charge utile de `0x47` octets, a produit un vert fixe. Ce format, complété de zéros après les 16 couleurs de chaque ligne, est désormais utilisé uniquement pour le V3 Mini USB/RF. La souris garde ses paquets courts. Le passage en mode logiciel n’a pas été ajouté : il n’était pas nécessaire au test réussi.

### Récepteur partagé et réponses

L’option de configuration est explicite : le PID du dongle ne suffit pas à identifier son appareil secondaire. Chaque cible utilise un handle indépendant vers l’interface de contrôle du même récepteur ; les commandes sont multiplexées avec transaction `0x9F` pour le clavier et `0x1F` pour la souris. Les réponses firmware, disposition et série sont vérifiées. Les journaux de tests ne publient que des empreintes de série.

Le verrou interprocessus Razer couvre maintenant l’envoi et la lecture de la réponse des requêtes. Les écritures RGB partagées consomment aussi leur propre ACK. Longueur, CRC, cible, classe et commande doivent correspondre ; les réponses BUSY ou obsolètes sont relues au maximum 16 fois, avec une attente de 2 ms, sans boucle active. Une cible indisponible ne reçoit pas de commande RGB. Le délai de réponse radio de la paire partagée est d’au moins 5 ms : OpenRazer indique 4900 µs pour le Mini et les deux cibles partagent le buffer du récepteur. Les USB et les périphériques hors de cette paire configurée conservent leur délai.

La disponibilité est réévaluée au plus une fois par seconde lorsqu’une commande est demandée, sans nouveau thread dédié. Une redécouverte des pairs manquants est exécutée sur le thread de détection existant, à intervalle d’une seconde et uniquement tant qu’une des deux entrées manque. Les contrôleurs déjà présents sont conservés, avec leurs couleurs et associations. Le clavier éteint répondait TIMEOUT, tandis que la souris continuait à répondre ; après rallumage, la même identité AZERTY a été retrouvée. Les ressources du verrou et du thread existant sont libérées lors de la destruction.

### Connexions et Identify

Les contrôleurs de la paire partagée préfèrent le transport USB lorsqu’il apparaît et gardent leur identité logique, puis reviennent au récepteur quand le câble disparaît. Le hotplug existant évite les doublons par série réelle et rétablit les détecteurs filaires lorsqu’un récepteur est retiré avec un câble encore présent.

La configuration utilisateur avait `Detectors.hid_safe_mode = true`, ce qui désactive le hotplug, même avec l’interface graphique. Un changement de dongle dans ce mode a effectivement laissé les anciennes entrées. La configuration portable active le hotplug (`false`) ; l’installation officielle reste inchangée. Le passage clavier RF → USB avec souris RF a été validé physiquement : clavier bleu, souris verte et seulement deux entrées. Lors du test inverse, le dongle entier s’est réénuméré pendant que le clavier RF n’était pas encore prêt ; ce cas a motivé la redécouverte ciblée des pairs manquants. Le test de démarrage avec clavier éteint a ensuite donné une seule entrée ; après son rallumage, deux entrées ont été retrouvées automatiquement, sans redémarrage ni détection manuelle.

Le correctif Direct de la branche officielle récente est repris : les buffers de couleurs ne remplacent plus un effet Static/Off par Custom. Des traces Razer indiquent les transitions vers une frame noire, verte ou d’une autre couleur, sans journaliser chaque frame. Dans Visual Map, Identify sélectionne le vert pour les zones choisies et le noir pour les autres ; des zones superposées peuvent donc avoir des intentions contradictoires. La stabilité d’une couleur verte fixe est validée, mais le bouton Identify de Visual Map et les interactions de zones doivent encore être testés dans l’interface : la cause complète du symptôme initial vert puis noir n’est pas considérée comme prouvée.

### Windows Dynamic Lighting

Une exclusion opt-in `HIDLampArray.ExcludeLenovoLighting = true` ignore le matériel observé `048D:C193` avant même l’ouverture HID. Windows Dynamic Lighting reste activé ; ni pilotes, ni appairage, ni réglages globaux Windows ne sont modifiés. Le SDK de test ne présente pas le Lenovo. Cette exclusion vise ce matériel précis, pas tous les Lenovo.

## Inventaire vérifié

| Connexion | PID (VID 1532) | Interface de contrôle observée | Cibles |
|---|---|---|---|
| Clavier USB | 0258 | 3, usage 000C:0001 | 1F, implémentation existante |
| Dongle clavier | 0271 | 3, usage 000C:0001 | 9F clavier / 1F souris, vérifiés |
| Souris USB | 00D6 | 0, usage 0001:0002 | 1F, implémentation existante |
| Dongle souris | 00D7 | 0, usage 0001:0002 | 9F clavier / 1F souris, vérifiés |
| Bluetooth | 0272 / 00D8 | Aucun transport RGB utilisable établi | RGB non pris en charge |

Les LampArrays des récepteurs et les interfaces de saisie sont distincts de cette interface de contrôle. Aucun chemin HID Windows n’est fixé dans le code. Le Bluetooth est examiné mais aucun contrôleur RGB fictif n’est créé : découvrir une entrée Bluetooth ou GATT ne prouve pas un contrôle couleur. Le support Bluetooth de cette Phantom Green est également désactivé dans la modification officielle qui l’ajoute.

## Fichiers modifiés

- `Controllers/RazerController/RazerDevices.cpp` : flèches et dimensions Phantom Green.
- `Controllers/RazerController/RazerController/RazerController.cpp` et `.h` : transport partagé, validation des réponses, présence, routage USB, padding Mini et diagnostics.
- `Controllers/RazerController/RazerController/RGBController_Razer.cpp` : mises à jour en Direct uniquement.
- `Controllers/RazerController/RazerControllerDetect.cpp` : paire explicite sur les deux récepteurs et absence de doublon USB.
- `Controllers/RazerController/RazerDeviceGuard.cpp` et `.h` : fermeture du mutex Windows.
- `Controllers/HIDLampArrayController/HIDLampArrayControllerDetect.cpp` : exclusion Lenovo ciblée.
- `DetectionManager.cpp` et `.h` : hotplug et redécouverte ciblée de la paire sur le worker existant, conservation des contrôleurs déjà enregistrés.
- `tests/RazerSupport/` : tests de mapping/transport, requêtes HID en lecture seule, inventaire SDK, test RGB explicite et contrôle API des plugins.
- Ce rapport. Aucun SDK ni code de plugin modifié.

## Compilation et tests

Windows x64 Release, Qt 6.8.3 MSVC2022, MSVC 14.51, JOM 1.1.4 ; HIDAPI-hotplug, libusb et PawnIO du dépôt 1.0. L’API de plugins reste 5. La distribution contient les DLL Qt, les plugins de plateforme et le runtime Visual C++ local. Le test de chargement Qt initial manquait de plugin de plateforme ; il a été corrigé en fournissant le chemin Qt, puis les deux DLL ont été chargées avec succès.

- 36 vérifications de mapping/dimensions réussies.
- 23 vérifications de transport réussies : séparation des cibles, erreurs/CRC, BUSY borné, ACK vide, réponse obsolète, exclusion des écritures indisponibles, USB/RF simulé, concurrence, padding clavier et paquets souris.
- 2 chargements de DLL réussis : Effects et Visual Map, Qt 6.8.3 / API 5. Même contrôle réussi sur les DLL copiées, avec uniquement les dépendances locales de la portable.
- 10 séries RGB répétées sur le dongle souris, sans timeout ni réponse attribuée à l’autre cible après respect du délai commun de la paire.
- SDK réel : deux périphériques indépendants, souris 12 LED, clavier avec Down/Right à 78/79 ; Lenovo absent.
- Matériel réel : lecture des deux cibles sur chaque dongle ; extinction/rallumage RF du clavier ; couleurs indépendantes confirmées par l’utilisateur sur les deux dongles et dans les deux configurations mixtes USB/RF ; redécouverte automatique d’un clavier absent au démarrage.

Le chargement API ne remplace pas un test d’animation d’Effects ou d’édition dans Visual Map. Aucune mesure de performance ni validation prolongée en jeu n’est annoncée. Le message SMBus/PawnIO du test sans privilèges système concerne les contrôleurs I2C, pas ce protocole HID Razer ; aucun pilote n’a été installé pour ces tests.

| Scénario | État |
|---|---|
| Clavier et souris sur dongle clavier | Validé sur matériel : identités et couleurs indépendantes stables |
| Clavier et souris sur dongle souris | Validé sur matériel : identités et couleurs indépendantes stables |
| Clavier USB seul | Support existant, mapping/padding testés par logiciel ; RGB USB validé avec le dongle présent, test USB isolé restant |
| Souris USB seule | Support existant, dimensions testées par logiciel ; couleur physique USB non testée |
| Clavier HyperSpeed seul / souris HyperSpeed seule | Support existant conservé ; isolation physique complète non testée |
| USB clavier + RF souris | Validé sur matériel : bleu clavier, vert souris, deux entrées sans doublon |
| RF clavier + USB souris | Souris USB détectée et pilotée ; clavier RF rouge et souris USB magenta confirmés sur matériel, deux entrées sans doublon |
| Disparition et retour radio du clavier | Présence OFF/ON validée matériel ; redécouverte d’un clavier absent au démarrage validée sans redémarrage, puis renvoi RGB réussi |
| Changement de dongle sans redémarrage | Échec observé en mode HID sûr ; hotplug activé dans la portable, validation physique restante |
| Redémarrage du programme | Détection et RGB réussis sur les récepteurs après plusieurs relances |
| Contrôle LED individuel | Adresses testées par logiciel ; illumination globale des deux flèches confirmée matériel, sélection individuelle restante |
| Identify | Direct corrigé et vert stable validé ; bouton/zone Visual Map restant à tester |
| Changement de profil | Format SDK conservé et fichiers sauvegardés ; réassociation des nouveaux noms à vérifier |
| OpenRGB Effects / Visual Map | Chargement des DLL/API validé ; fonctionnement simultané graphique restant à tester |
| Bluetooth des deux appareils | RGB non pris en charge, aucun protocole validé |

## Installation portable et retour arrière

1. Extraire l’archive de Downloads dans un dossier neuf et garder tous les fichiers ensemble.
2. Quitter les autres OpenRGB via la zone de notification.
3. Lancer `Lancer-OpenRGB.cmd`. Il vérifie qu’aucune autre instance OpenRGB n’est ouverte et utilise le sous-dossier `config` propre à cette copie.
4. Garder un seul dongle partagé connecté, avec l’appairage effectué auparavant. La paire explicite et l’exclusion Lenovo sont déjà activées dans la copie de configuration.
5. Dans Périphériques, sélectionner Direct puis une couleur différente sur chaque appareil. Les deux nouvelles entrées Razer ont le suffixe `USB/HyperSpeed auto`.
6. Vérifier les associations de zones dans Visual Map et des effets avant de les réactiver. Les noms/locations logiques ont changé : un profil ancien peut demander une nouvelle association. Les fichiers originaux sont conservés dans `Backup-installation` et les profils copiés dans `config/profiles`.

Pour revenir à l’officielle : quitter la portable puis lancer `C:\Program Files\OpenRGB\OpenRGB.exe`. Rien à réinstaller. `Backup-installation` contient les trois binaires et la configuration d’origine sauvegardés avant la livraison ; le plugin Effects sauvegardé est bien celui avec la correction Ambient déjà installée par l’utilisateur.

## Procédure de validation restante

USB : quitter les effets, garder un seul appareil testé, brancher son câble et le mettre en USB. Vérifier son identité, Direct rouge puis bleu et les flèches/LED de bande. Avec le dongle présent, enlever/remettre le câble ; une seule entrée doit subsister et suivre les couleurs. Sans dongle, vérifier la détection USB autonome. Refaire pour la souris.

RF : avec le hotplug activé, changer de récepteur en cours d’exécution, vérifier le retrait des anciennes entrées puis les deux nouvelles identités. Réappliquer la couleur ou le profil. Éteindre seulement le clavier puis le rallumer et vérifier la reprise de l’effet.

Bluetooth : le contrôle RGB n’est pas disponible dans cette version ; ne pas attendre de nouvelles entrées RGB fonctionnelles. Les fonctions de saisie Windows restent indépendantes.

Identify/LED : mettre le clavier en Direct, arrêter les effets, sélectionner Bas puis Droite individuellement, tester deux couleurs. Dans Visual Map, vérifier que la même LED n’appartient pas à plusieurs zones contradictoires, puis lancer Identify et surveiller les traces de frame noire si le problème revient.

Limites : la redécouverte automatique vise les deux modèles de la paire explicitement configurée ; un appareil déjà découvert mais éteint garde son entrée logique avec les écritures suspendues. Les profils ne se réappliquent pas automatiquement lors de tous les changements de dongle. La gestion vise cette paire explicitement configurée, pas toute combinaison Razer. Aucune commande d’appairage n’est envoyée.

## Reproduction et sources

Tests : `tests\RazerSupport\Run-Layout-Tests.cmd`, `Run-Transport-Tests.cmd`. Ils utilisent le MSVC installé. `Inspect-Razer.py --query` réalise uniquement les requêtes GET connues. `Inspect-SDK.py` lit les contrôleurs d’un serveur isolé. `Test-RGB.py --inventory inventaire.json --keyboard red --mouse green` écrit réellement les couleurs et suppose que les autres effets sont arrêtés.

Les résultats sont dans `validation` de la distribution, les traces de build complètes dans `build` du dépôt. Le correctif Git fourni s’applique à la base officielle 1.0 ci-dessus.

Sources : [OpenRGB officiel](https://gitlab.com/CalcProgrammer1/OpenRGB), [dongle partagé](https://gitlab.com/CalcProgrammer1/OpenRGB/-/issues/3307), [Phantom Green et limitation Bluetooth](https://gitlab.com/CalcProgrammer1/OpenRGB/-/merge_requests/3218), [protocole clavier OpenRazer](https://github.com/openrazer/openrazer/blob/master/driver/razerkbd_driver.c), [format matriciel](https://github.com/openrazer/openrazer/blob/master/driver/razerchromacommon.c), [délais radio](https://github.com/openrazer/openrazer/blob/master/driver/razerkbd_driver.h), [dimensions souris](https://github.com/openrazer/openrazer/blob/master/daemon/openrazer_daemon/hardware/mouse.py).
