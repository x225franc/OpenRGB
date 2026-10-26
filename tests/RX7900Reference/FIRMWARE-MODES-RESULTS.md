# RX 7900 XTX Reference / Avatar : effets firmware observes

Campagne du **26 octobre 2026**, en reponse a `HANDOFF-FIRMWARE-MODES.md`, base `a0becaa7d`. Branche : `investigation/rx7900-firmware-effects`. **Aucun fichier du controleur n'a ete modifie.**

## Conclusion pour l'autre Codex

**Breathing a couleur fixe, Breathing Random Color, Color Cycle et le motif Morse existant continuent apres fermeture des processus officiels.** Les captures montrent des configurations ponctuelles, sans flux de couleurs regulier pendant les attentes. Ces effets peuvent etre proposes comme effets materiels autonomes dans les conditions testees, distincts de Direct/Breathing/Color Cycle calcules par OpenRGB.

Breathing et Color Cycle disposent des preuves les plus completes : variations de parametres isolees, messages complets, retours au meme reglage et tests d'autonomie. Morse a volontairement ete limite a l'inventaire, au motif existant et a l'autonomie, a la demande de l'utilisateur. Ne pas annoncer un encodeur Morse generique, un controle par LED, une synchronisation entre appareils ou une cadence Ambient valides par cette campagne.

## Materiel, logiciel et portee

- PCI : `1002:744C`, sous-systeme `1002:0E3B`, revision C8 ; instance complete dans `firmware-effects-20261026/hardware.json`.
- Pilote Windows : `32.0.32015.7029`. Une Radeon 780M est aussi presente. Les echanges complets de cette campagne concernent l'index ADL **5**, sans en faire une constante portable.
- Application : `AMD_Radeon_RX_7900_XTX_Series_RGB_LED_x64.exe`, version de fichier **1.0.0.1**, SHA256 **F9082F12304AD5FE7B97817CEEE5EB57CF73B81FEAC322F87D5F3C7EC80DDA9A**. Voir `software.json` pour tous les fichiers observes. Cette empreinte differe de celle de la premiere investigation : le numero de version seul ne suffit pas a identifier le paquet.
- Transport observe : legacy `ADL_Display_WriteAndReadSMUI2C`, ligne 0, B4/B5, offset 0, 100 kHz. Le pilote appelle aussi son export ADL2 SMU depuis la voie legacy ; ce ne sont pas deux commandes physiques distinctes.
- Aucun processus OpenRGB pendant les manipulations. Chroma Sync Off demande et visible dans les captures. Aucun service correspondant aux noms RGB/Razer/Chroma/Cooler identifies dans l'inventaire de services. L'observateur ne transmet aucune commande au GPU.

## Tableau de validation

Les confirmations visuelles sont celles de l'utilisateur observant la carte ; les images fournies sont celles de **l'interface**, pas une video de la carte. Toutes les durees ci-dessous proviennent des inventaires sans producteurs RGB repertories ; les processus officiels sont aussi suivis lors des changements de PID.

| Effet / variante | Disponible | Apres fermeture reelle | Commandes observees | Couleurs / autres options | Limites |
|---|---|---|---|---|---|
| Static | Oui | Non anime ; tests precedents | `51 2C` mode 00, `51 A0` selection 00, `51 28 ... E0` | RGB global ; luminosite ; vitesse desactivee | Validation Static/Off et native deja documentee dans `NATIVE-CONTROLLER.md` ; pas un nouveau test d'autonomie anime |
| Breathing / Select Color | Oui | **Oui**, 15,069 s, 4 inventaires vides ; rouge continuant a pulser | `51 2C` mode 01 ; `51 A0` selection 01 ; `51 28` | Une couleur ; rouge, vert, bleu puis rouge testes ; luminosite et vitesse testees | Pas deux couleurs ni direction visibles ; pas de periode physique mesuree |
| Breathing / Random Color | Oui | **Oui**, 35,179 s, 8 inventaires vides ; changement de couleur entre pulsations confirme | Memes commandes, octet 6 passant de 20 a A0 | Couleur choisie automatiquement a chaque pulsation ; selection RGB desactivee | Distribution aleatoire et sequence non caracterisees |
| Color Cycle | Oui | **Oui**, 15,072 s, 4 inventaires vides ; defilement confirme | `51 2C` mode 02 ; `51 A0` selection 02 ; `51 28` | **Uniquement luminosite et vitesse** ; choix RGB/Random desactives | Aucun choix de palette ou direction valide ; pas de periode physique mesuree |
| Morse Code / texte existant, repetition active | Oui | **Oui**, 20,097 s, 5 inventaires vides ; clignotement confirme | `51 73` selecteurs 00/01, `51 2C` mode 0B, `51 A0` selection 0B, `51 28` | Texte converti en impulsions ; couleur choisie en haut ; luminosite disponible ; vitesse desactivee | Texte/memoires non modifies volontairement ; pas de matrice RGB/vitesse ; Random semble sans effet selon utilisateur, donc non valide |
| Light Off | Oui, option globale | Non anime ; tests precedents | Selection FE de `51 A0` dans les captures precedentes | Extinction globale | Pas une nouvelle exploration ici |

Preuves d'absence : `breathing-fixed-close-process-evidence.json`, `breathing-random-close-process-evidence.json`, `color-cycle-close-process-evidence.json`, `morse-close-process-evidence.json`, dans le dossier de campagne. Les confirmations et demandes sont reliees par les marqueurs UTC du fichier `markers.jsonl` et de `raw.jsonl`.

## Champs isoles : observations, pas extrapolations

Indices **a partir de zero dans le message logique de 64 octets**, sans le prefixe de trame 40/57/AE. Les nombres du tableau sont hexadecimaux.

| Champ | Breathing fixe | Breathing Random | Color Cycle | Morse observe |
|---|---|---|---|---|
| Octets 0..3 | `51 2C 01 00` | Identiques | Identiques | Identiques |
| Octet 4, mode | `01` | `01` | `02` | `0B` |
| Octet 5, vitesse : bas / milieu / haut testes | **26 / 31 / 3C** | `31` teste dans la variante aleatoire | **68 / 80 / 96** | `6B` conserve dans le paquet ; controle UI desactive, signification non validee |
| Octet 6 | `20` | **A0** | `00` | `00` en Select Color ; `80` egalement observe lors du choix Random |
| Octet 7 | `03` | `03` | `FF` | `05` |
| Octet 8 | `FF` | `FF` | `FF` | `FF` |
| Octet 9, luminosite : bas / milieu / haut testes | **33 / 99 / FF** | `FF` teste | **10 / 40 / 7F** | `99` observe ; pas de matrice a la demande de l'utilisateur |
| Octets 10/11/12 | R/G/B : FF0000, 00FF00, 0000FF puis FF0000 | Derniere couleur retenue dans le message, mais selecteur RGB grise | `FF FF FF`, controle RGB desactive | `C5 19 19`, couleur affichee #C51919 et selection de couleur signalee fonctionnelle |
| Octets 13..15 | `00 00 00` | Identiques | Identiques | Identiques |
| Octets 16..63 | `FF` dans les exemples | Identiques | Identiques | Identiques |

Les passages par les boutons peuvent produire des valeurs intermediaires : par exemple CC pour Breathing et 50 pour Color Cycle. Seuls les trois niveaux demandes ont une confirmation explicite de sequence. Ne pas appliquer la plage de luminosite Static/Breathing a Color Cycle : **son maximum observe est 7F, pas FF**. De meme, ne pas supposer une vitesse en millisecondes ni une echelle lineaire.

Le changement 20 -> A0, a reglage identique, isole le bit 80 pour Random en Breathing. Cela **ne prouve pas** que ce bit produit une couleur aleatoire dans chaque mode : Morse illustre justement cette limite. Le role des autres bits des octets 6/7/8 n'est pas entierement caracterise.

`51 A0` commence par `51 A0 01 00 00 03 00 00 05 06`, puis les positions 10..24 contiennent quinze copies de 01, 02 ou 0B selon l'effet. La suite observee est nulle. **Ces quinze valeurs ne constituent pas une preuve de quinze LED independamment pilotables.**

## Messages, ordre et delais

`examples.json` contient des exemples complets de configuration et leurs deux messages suivants ; `messages.json` contient les 64 octets de chaque message, les requetes/reponses de toutes les trames et leurs heures UTC. Pour les variations courantes :

1. `51 2C` : parametres du mode.
2. `51 A0` : selection du mode.
3. `51 28 00 00 E0` puis zeros : application observee.

Chaque message est envoye en 40 + 23 octets, 57 + 23 octets, AE + 18 octets, puis lu en trois trames marquees FF/E8/D1 de tailles 24/24/19. Les exemples de changements de parametres ont des reponses echo completes. L'initialisation et les requetes d'information peuvent avoir des reponses qui contiennent des donnees au lieu d'un simple echo.

Sur 166 messages de configuration/application/motif, les medianes entre retour d'un appel et entree du suivant sont : ecritures 1->2 **0,097 ms**, 2->3 **0,112 ms** ; derniere ecriture->premiere lecture **32,572 ms** (min 20,257 / max 36,532 ms) ; lectures 1->2 **0,126 ms**, 2->3 **0,119 ms**. Ce sont des delais observes, dependants de l'instrumentation et de l'ordonnanceur, **pas une cadence Direct/Ambient garantie**. Voir `timing.json`.

Pour Morse, des `51 73 00 00` et `51 73 01 00` transportent le motif existant ; les requetes au demarrage lisent aussi `52 73` avec d'autres selecteurs. Les deux exemples complets sont conserves, mais aucun encodeur texte/motif ni correspondance aux memoires M1/M2/M3 n'a ete etabli. Les boutons M1/M2/M3, Save, Reset, lecture et repetition sont inventories dans `morse-inventory.png`, sans exploration d'ecriture des memoires.

Les relances produisent notamment `12 02`, d'autres requetes `12 xx`, des commandes 40/41/42, les lectures `52 xx`, puis l'initialisation/selection du mode. Ne pas assimiler ces interrogations de demarrage a un flux d'animation. Le rapport ne propose pas de rejouer les commandes d'information inconnues.

**50 55 n'a pas ete observe dans cette campagne**, contrairement aux captures precedentes. Le fait que les effets continuent apres fermeture prouve une autonomie pendant cette mise sous tension, pas leur persistance apres coupure secteur. Le caractere volatil ou persistant de `51 28` n'est pas etabli ; aucune coupure ni aucun redemarrage n'a ete effectue pour le tester.

## Provenance et limites de capture

- Plage brute : **00:03:33,856 a 00:34:43,368 UTC**. Les marqueurs restent en UTC ; les evenements Windows portent aussi leur decalage local +01:00.
- **369 messages logiques canoniques** de 64 octets, chacun avec six transactions completes et des marqueurs de reponse valides. L'observation des deux exports ADL produit 738 reconstructions : utiliser `canonical: true` pour eviter de doubler les comptes. Le decodeur a ete verifie contre les 312 messages complets de la capture precedente.
- Les 60 retours -1 bruts se trouvent exclusivement sur les indices ADL 0..4 pendant les essais de detection officiels au demarrage ; ils sont doubles par la couche legacy/ADL2. Aucun des 369 echanges logiques complets sur l'index 5 n'a de retour ADL en echec. Les reponses aux requetes d'information ne sont pas toutes des echos ; ne pas les classer comme erreurs pour cette raison.
- Les PIDs x64 observes sont 14836, 4504, 16248, 19836, 5872, 15736 et 7376. Aucun auxiliaire HID officiel actif n'a ete observe pendant les fenetres d'autonomie. Tout nouveau PID officiel est reattache ; les appels avant l'attachement restent explicitement hors couverture.
- Inventaires des processus toutes les 5 secondes ; detection des apparitions/disparitions officielles toutes les 0,5 seconde. Les conclusions sont limitees aux producteurs repertories, sans tracage noyau. Ce ne sont pas des preuves de persistance hors tension.
- **Limite de l'observateur utilise :** le callback Frida `detached` avait une signature incorrecte et echouait a serialiser un objet Session lors de la fin des processus. Les callbacks ADL et les inventaires independants ont continue ; les evenements de disparition sont presents par polling. Le script livre corrige cette signature et ajoute un arret cooperatif. Les tests d'autonomie reposent sur les inventaires, pas sur les callbacks `detached` manquants.
- L'interruption de la session outil a laisse l'observateur Python actif ; il a ete arrete apres verification exacte de sa commande et de l'absence de processus officiels. La fin n'est pas un evenement `campaign-stop` propre. Voir `capture-notes.json` ; les messages valides anterieurs n'ont pas ete modifies.
- **Onglet Version :** l'utilisateur rapporte un crash. Des evenements Application Error 1000 avec exception `0xc0000005` sont conserves dans `version-crash-events.json`. Cause non determinee sous instrumentation ; pas de tentative de correction ni de firmware update. Les versions du rapport sont celles des fichiers, faute de page Version exploitable.
- Rouge fixe maximal declare retabli par l'utilisateur. Aucune transaction posterieure au marqueur de restauration ni nouvelle image finale n'a ete observee ; la derniere configuration Static rouge capturee precede le crash Version. L'etat final est donc une confirmation utilisateur, pas une nouvelle capture de restauration.

## Reprise pratique et recommandations

- Les donnees permettent de preparer **Breathing materiel** (fixe et Random) et **Color Cycle materiel** avec des plages de parametres propres a chacun. Garder des noms distincts des effets logiciels prepares par l'autre Codex.
- Direct, Ambient, synchronisation entre appareils et effets calcules restent des sujets logiciels separes ; cette campagne ne valide aucune boucle rapide ni aucun nouveau chemin de sauvegarde.
- Morse est autonome pour le motif existant avec repetition. Son encodeur, ses memoires, ses limites de texte, sa luminosite et Random restent incompletement caracterises ; l'utilisateur ne souhaite pas prioriser cette exploration.
- Conserver l'identification PCI ciblee, le controle des reponses, l'arret sur erreur et l'exclusion des producteurs concurrents. L'implementation sera a valider sur le binaire OpenRGB complet ; ce rapport n'ajoute aucun nouveau mode au controleur.

Outils livres : `capture_firmware_campaign.py` (observation multi-PID), `CaptureOfficialWindow.ps1` (image de la seule fenetre officielle), `analyze_firmware_campaign.py` (reconstruction et deduplication), `check_firmware_autonomy.py` (absence des producteurs observee). Ils ne rejouent aucune commande.

Verifications effectuees : syntaxe Python, equivalence du decodeur avec les 312 messages historiques, validite des 369 echanges complets actuels, quatre preuves d'absence >=15 secondes, generation du manifeste SHA256. Le callback de detachement corrige a ete teste avec la version Frida installee sur un sous-processus Python temporaire sans acces GPU ; l'arret cooperatif a ete teste sur une campagne de duree nulle, sans attachement a RGB Tool. Aucune compilation OpenRGB n'est necessaire pour ces fichiers d'investigation.

```powershell
python tests\RX7900Reference\analyze_firmware_campaign.py tests\RX7900Reference\firmware-effects-20261026
```

Pour une nouvelle campagne, choisir un **nouveau dossier** ; l'observateur ajoute aux journaux existants. Un futur arret propre peut etre demande avec `--directory DOSSIER --stop`. Ne pas relancer cette campagne pour reproduire le crash Version. Aucun binaire OpenRGB, profil OpenRGB, plugin ou reglage Visual Map n'a ete remplace.
