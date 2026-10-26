# Mission — inventorier les effets firmware RX 7900 XTX Reference / Avatar

## But
Determiner tous les effets proposes par l'application officielle Cooler Master/AMD et identifier ceux executes de facon autonome par le firmware, avec leurs commandes exactes. L'autre Codex prepare en parallele Direct, Breathing et Color Cycle calcules par OpenRGB a partir du chemin Static valide. Ne pas confondre ces effets logiciels avec les effets firmware recherches ici.

## Depot et coordination
- Fork : https://github.com/x225franc/OpenRGB.git ; branche de travail principale : feature/fluent-interface.
- Recuperer les derniers commits puis creer une branche `investigation/rx7900-firmware-effects`.
- Pour cette mission, privilegier captures, documentation et tests de protocole ; ne pas modifier les fichiers du controleur en parallele sans coordination.
- Le controleur natif Static/Off a ete ajoute par `0c12f6379`. Relire `tests/RX7900Reference/NATIVE-CONTROLLER.md`, `README.md` et les captures existantes.

## Materiel et chemin deja identifies
- AMD RX 7900 XTX de reference, personnalisation Avatar.
- PCI 1002:744C, sous-systeme 1002:0E3B. Relever a nouveau l'identite et la version du pilote.
- Une seule zone RGB globale a ete exposee ; aucun controle individuel de LED valide.
- Outil officiel : `C:\Program Files (x86)\AMD\AMD Radeon RX 7900 XTX Series RGB LED`.
- Transport observe : fonction legacy `ADL_Display_WriteAndReadSMUI2C`, ligne 0, adresses B4/B5, offset 0, 100 kHz. Ne pas remplacer par ADL2_Display_WriteAndReadI2C ni supposer USB.
- Ne jamais fixer l'adapter index a 0 ou 5 : identifier l'adaptateur PCI reel, car une Radeon integree peut etre presente.
- Les utilitaires de capture sont dans `tests/RX7900Reference/capture_adl.py`, `summarize_capture.py`, etc. Ils observent l'application officielle ; ils ne doivent pas rejouer de commandes inconnues.

## Travail a realiser
1. Fermer normalement OpenRGB pour eviter deux applications ecrivant sur la carte. Garder les profils et sauvegardes existants.
2. Relever la version de RGB Tool et inventorier tous les effets visibles, y compris les variantes. Conserver une capture de l'interface et les noms exacts. Distinguer les options indisponibles des options testables.
3. Observer chaque processus RGB Tool concerne, y compris les auxiliaires x64/HID et les processus recrees. Une capture d'un ancien PID ne suffit pas.
4. Capturer un effet a la fois avec une heure UTC et un libelle non ambigu. Partir d'un etat connu, puis isoler les changements :
   - choix d'effet ;
   - couleur(s) rouge, vert, bleu lorsque applicable ;
   - luminosite minimum non nulle, moyenne, maximum ;
   - vitesse minimum, moyenne, maximum ;
   - direction, sens et autres options lorsqu'elles existent ;
   - retour au meme reglage pour verifier les octets stables et variables.
5. Pour chaque effet anime, confirmer visuellement son comportement. Puis quitter reellement RGB Tool et ses auxiliaires de controle, sans eteindre le GPU. Verifier au moins 15 secondes si l'animation continue. Relever les processus restant actifs : fermer seulement la fenetre ou laisser un service auxiliaire ne prouve pas l'autonomie firmware.
6. Etablir si le logiciel envoie une configuration ponctuelle ou des trames de couleurs regulieres. Si l'effet continue apres l'arret de tous ses producteurs, le qualifier d'autonome dans les conditions testees. Sinon, le classer logiciel ou indetermine, sans conclure sur un simple ACK.
7. Reconstituer les messages logiques complets de 64 octets et les sequences write/read, leur ordre, les delais observes et les reponses. Documenter separement initialisation, reglage, application et eventuelle sauvegarde persistante.
8. Si necessaire, ajouter des fixtures et tests de decodage ne transmettant rien au GPU. L'implementation des nouveaux effets sera faite ensuite a partir de ces preuves.

## Messages deja observes — pas des commandes a rejouer au hasard
- `51 2C ...` : configuration Static ; octet 9 = luminosite observee 33/66/99/CC/FF ; octets 10/11/12 = RGB. L'octet 4 etait 00 en Static ; son role pour les autres modes n'est pas encore confirme.
- `51 A0 ...` : commande de mode/on-off, avec FE sur les positions observees pour Off.
- `51 28 ... E0` : application observee.
- `12 02`, `41 80`, `51 96` : sequence d'initialisation deja documentee.
- `50 55` apparait dans le logiciel officiel mais sa semantique de persistance reste a caracteriser. Ne pas l'integrer a une boucle d'animation et ne pas la rejouer pour explorer.
- Framing observe : 40 + 23 octets, 57 + 23 octets, AE + 18 octets ; reponses FF/E8/D1. Les captures completes font foi.

## Precautions
- Pas de scan I2C aveugle, de commande de firmware update, de flash, de changement d'horloge/ventilation ou d'ecriture hors des manipulations normales de RGB Tool.
- Pas de boucle d'essai de codes d'effets inconnus.
- Un retour ADL egal a 0 ou un echo du message ne constitue pas une validation visuelle.
- Ne pas remplacer les binaires OpenRGB installes pour cette investigation.
- Ne pas redemarrer ni eteindre ce PC automatiquement. Si une verification de persistance l'exige, l'organiser avec l'utilisateur.

## Livrables a rapporter
- Un tableau : nom d'effet, disponible, autonome apres fermeture, commandes/opcodes, parametres, couleurs, vitesse/direction, luminosite, preuve visuelle, limites.
- Les traces JSONL brutes et messages complets reconstruits, avec une chronologie etiquetee reliant chaque manipulation aux messages.
- Pour chaque champ suppose : separer preuve observee, hypothese et point non resolu.
- Un resume de ce qui peut etre implemente en firmware, de ce qui doit rester logiciel et des donnees manquantes.
- Commit/push des seuls fichiers d'investigation sur `investigation/rx7900-firmware-effects`, puis communiquer le hash et le nom de branche a l'autre Codex. Ne pas faire de force-push ni ecraser fluent-interface.
