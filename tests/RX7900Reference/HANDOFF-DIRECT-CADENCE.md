# Mission pour le PC RX 7900 XTX : mesurer une cadence Direct viable

## Objectif et état actuel

Travailler sur une nouvelle branche `investigation/rx7900-direct-cadence`, créée depuis le dernier `fork/feature/fluent-interface` (`https://github.com/x225franc/OpenRGB.git`). Ne pas modifier la branche Fluent en parallèle de l'autre Codex ; rendre les résultats et outils dans `tests/RX7900Reference/`.

Le GPU est le modèle de référence PCI 1002:744C, sous-système 1002:0E3B. Ne jamais coder en dur l'index ADL 5 : résoudre la carte exacte par ses identifiants et son emplacement PCI, sans écrire sur l'iGPU.

Le propriétaire confirme que Breathing/Color Cycle HARDWARE sont fluides ; seuls les anciens modes SOFTWARE étaient saccadés. Ils sont retirés du contrôleur. Direct reste pour Ambient. La limite actuelle de **4 mises à jour/s est imposée par OpenRGB et n'est pas une limite matérielle mesurée**.

Lire FIRMWARE-MODES-RESULTS.md, NATIVE-CONTROLLER.md, RadeonReferenceProtocol.h et RadeonReferenceADL_Windows.h avant les essais. Le chemin courant envoie trois messages par frame : configuration Static `51 2C`, sélection Static `51 A0`, application `51 28`. Chaque Exchange attend actuellement 30 ms avant ses trois lectures. Le plancher logiciel est donc déjà d'environ 90 ms/frame, avant les coûts ADL et KeepAwake ; retirer le limiteur 250 ms ne donnera pas 60 FPS.

## 1. Observer avant d'écrire

Avec l'accord du propriétaire, capturer passivement les appels ADL du RGB Tool officiel pendant une animation logicielle/Chroma réelle. Un seul producteur RGB : aucun OpenRGB pendant l'observation officielle. Reprendre l'observateur de l'audit, avec logs bornés et arrêt propre. Ne pas activer de mode firmware update ni tester des opcodes inconnus.

Déterminer ce qui est envoyé réellement à chaque changement de couleur : `51 2C` seul, avec Apply, avec sélection de mode, ou autre séquence. Fournir les messages COMPLETS de 64 octets, six trames, ACK, timestamps d'entrée/sortie et contexte. Dédupliquer la voie legacy et ADL2 comme dans l'audit. Ne pas confondre ACK avec changement visible des LED.

Mesurer si la sélection du mode est envoyée une seule fois ou à chaque frame. Identifier si les délais de lecture sont fixes ou liés à la disponibilité de l'ACK. Mesurer séparément création/destruction du handle KeepAwake, appels ADL, attente, ACK et frame complète. Ne pas supprimer les garanties de maintien éveillé sans essai comparatif.

## 2. Essais bornés de débit avec les commandes déjà vérifiées

Fermer RGB Tool, Synapse/Chroma et OpenRGB avant le probe indépendant. Conserver l'exclusion par mutex PCI, les contrôles d'ACK exacts, et l'arrêt immédiat au premier échec. Ne jamais continuer en boucle après erreur ou réémettre automatiquement une transaction partielle.

Première série : conserver la séquence actuelle et ses 30 ms, cibles 2, 4, 6, 8 et 10 FPS. Mesurer la cadence EFFECTIVE ; si une étape est physiquement trop longue, ne pas accumuler de frames ni lancer de transactions concurrentes. Le dernier état remplace les anciens. Commencer par de courts essais de 30 s, puis prolonger seulement les paliers visuellement stables.

N'essayer une séquence plus courte (ex. mode sélectionné une fois) que si la capture officielle en apporte la preuve ou si un test explicite, isolé et confirmé visuellement établit son fonctionnement avec les commandes déjà connues. Ne pas inventer de nouvelles commandes pour gagner du temps. Ne pas diminuer arbitrairement le délai de lecture : distinguer délai observé, disponibilité réelle des ACK et hypothèse à vérifier.

Utiliser des transitions progressives de couleur et des rampes claires/sombres, sans stroboscope. Le GPU n'a qu'UNE zone RGB globale. Observer couleur visible, étapes/manques, scintillements, latence et comportement après reprise. Une vidéo ralentie avec timestamps ou mire de référence peut aider à distinguer cadence d'envoi et cadence réelle des LED.

Pour chaque palier : FPS demandés/effectifs, messages/s, durée moyenne/P95/max par Exchange et frame, ACK manquants/corrompus, CPU/GPU, fluidité du bureau/jeu, confirmation humaine de l'éclairage. Ne pas qualifier un palier de fluide à partir des seuls codes de retour.

Arrêter au premier symptôme (erreur, blocage, flicker, dégradation système) et conserver la trace. Revenir à Static uniquement si le canal est encore fonctionnel ; ne pas relancer aveuglément les écritures sur un transport bloqué. Aucun `50 55`, flash, Save ou écriture mémoire persistante inconnue.

## 3. Endurance et restitution

Valider le meilleur palier plausible d'abord 5 min, puis au moins 30 min avec bureau, vidéo et application 3D si disponibles. Noter veille/reprise et température/charge GPU sans prétendre mesurer la température du contrôleur RGB si aucun capteur ne l'expose.

Livrer `DIRECT-CADENCE-RESULTS.md`, les traces complètes des essais pertinents et leur manifeste SHA256, les sources du probe et les commandes reproductibles. Tableau explicite : cadence cible/effective, séquence, latences, erreurs, constat visuel, durée testée. Séparer ce qui est observé de ce qui reste inconnu.

Proposer la plus haute cadence VISUELLEMENT STABLE mesurée avec marge, pas la plus haute cadence qui accepte un ACK. Un essai réussi ne certifie pas une durée de vie ou une limite constructeur : sans documentation matérielle, parler de cadence validée expérimentalement dans ces conditions. Si 60 FPS sont impossibles avec ce transport, le dire avec les durées mesurées.

Ne pas changer les réglages de polling souris, batterie, autres appareils, profils ou VMap. Ne pas introduire de binaire non validé dans l'installation habituelle. Préparer un commit séparé de résultats/outils sur la branche d'investigation et transmettre son lien à l'autre Codex.
