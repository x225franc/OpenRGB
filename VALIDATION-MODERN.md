# Validation de la version moderne

Version Windows x64 Release, Qt 6.8.3, API des plugins 5.
Code du binaire : fd67e6f98d703d986ad2d4257164a034f95a30a0.

- Compilation effectuée avec MSVC ; exécutable signé, signature Windows valide.
- Tests Qt : 17 réussis, aucun échec.
- Interface : 192 parcours et 576 assertions sur huit pages et plusieurs dimensions.
- Bouton logo blanc : centrage en mode réduit, bascule et persistance après redémarrage vérifiés.
- Six fermetures normales de validation, code de sortie 0, après correction d'une dépendance de compilation sur un ancien en-tête.
- Accent automatique : couleur de l'application identique à AccentLight2 de Windows (#ed88d1 pendant le test). Les notifications Windows sont raccordées ; un changement réel de couleur système n'a pas été testé.
- Fond Acrylic Windows : attribut DWM et surface alpha vérifiés, avec cadre natif redimensionnable.
- Identité conservée : OpenRGB.LightSync_h6e9bjf6s4fnj. Contrôle Lenovo en arrière-plan observé à 31 FPS. Aller-retour entre les dossiers ancien et nouveau vérifié sans changer de famille de package.
- Modules de développement retirés de la livraison. Effects, LightSync et Visual Map conservés.

Les sept appareils LAN restent désactivés dans cette copie à la demande de l'utilisateur. Aucun test de couleurs LAN n'a été effectué. Pas de mesure de performances en jeu ni de test prolongé d'une heure. Le 18 octobre 2026, les binaires installés dans Program Files ont été remplacés avec sauvegarde Modern-20261018-112424. Les profils et la VMap ont été conservés. L'identité WDL garde la même famille et pointe désormais sur le dossier installé. Le démarrage automatique et la tâche de reprise existants ciblent toujours l'installation habituelle.

Consulter Documentation/MODERN-INTERFACE.md pour le lancement, l'identité Windows et le retour arrière. L'archive contient une copie de configuration personnelle et ses sauvegardes : ne pas la publier comme distribution générique.

Mise a jour navigation : un seul bouton Settings fixe en bas, icone ecrou. Apparence integree dans General Settings. Test de non-duplication et capture de la page reelle verifies.

Instance unique Windows : trois relancements graphiques testes (pendant le demarrage, fenetre masquee, fenetre visible), tous termines avec code 0 et une seule instance restante. La fenetre existante est reactivee. Test avec --nodetect pour isoler le demarrage graphique du materiel ; aucun test de couleur. Fermeture normale de cette instance : code 0. Le tray accepte maintenant le clic simple et le double-clic. Les services et commandes sans interface restent hors du verrou graphique.

## Reconnexion Bluetooth

Le worker controle maintenant le statut et la generation de connexion meme quand une trame identique est en attente. Un abonnement Windows ConnectionStatusChanged memorise les changements entre deux passages du worker. Apres reouverture GATT, le cache des etats envoyes est invalide et toutes les zones controlees sont reappliquees. Les callbacks ne capturent pas le worker et sont desabonnes a la fermeture.

Tests du worker de production avec faux transport : 13 assertions reussies, notamment reconnexion breve sans nouvelle trame, trame identique, derniere scene pendant une tentative, absence de prise de controle non demandee et arret pendant le delai de nouvelle tentative. L'ancien worker echoue au scenario de reconnexion breve ; le nouveau passe. Compilation Windows x64 effectuee. La notification GATT Windows et la reprise sur materiel reel restent a verifier par l'utilisateur.

HyperSpeed/USB : revue du chemin Direct/Ambient. Il renvoie deja la trame complete et active le mode personnalise a chaque envoi, avec controle de presence et reapplication de luminosite. Aucun changement preventif ni validation materielle supplementaire sur ce transport dans cette livraison.

Test manuel Bluetooth : appliquer une couleur ou un mode Bluetooth pris en charge, eteindre l'appareil dix secondes, puis le rallumer sans relancer OpenRGB ni changer le mode. La derniere scene doit revenir apres reconnexion GATT et les eventuelles nouvelles tentatives. L'appareil doit deja avoir ete decouvert en Bluetooth ; ce correctif n'ajoute pas une detection de nouveaux appareils en cours d'execution.

## Version candidate pour la souris du second PC

Retour utilisateur : le clavier reprend mais pas la souris. Les observations sont faites sur un autre PC, sans logs locaux accessibles. La cause sur ce PC distant n'est pas confirmee. Cette version ajoute une verification de l'etat lumineux reel des deux zones de souris toutes les cinq secondes minimum, hors du thread graphique. Aucun renvoi si les couleurs et la luminosite correspondent. En cas de perte d'etat sans evenement Windows, reappliquer la derniere scene demandee. Les tentatives en echec sont journalisees avec limitation de frequence.

16 assertions du worker avec transport simule passent, dont lecture reguliere sans renvois inutiles et perte silencieuse d'etat sans deconnexion. Compilation x64 et signature valide. Cette version n'a pas ete testee sur la souris du second PC et n'a pas remplace l'installation locale. Les sauvegardes d'installation et les validations historiques incluses correspondent aux deploiements precedents.

## Gestion des appareils et changement de routeur (21 octobre 2026)

LightSync 0.1.2, source d647eee813c278514718b1544d24da7a6c45bc84. DLL x64 compilee et chargee avec API 5. Les 24 controles du programme de tests passent, dont six nouveaux controles : mise a jour IP/port/interface sans doublon, conservation des couleurs, rejet de multicast, oubli avec stockage differe, absence de resurrection automatique et nouvelle recherche explicite. Le changement de routeur reel et les nouvelles IP de l utilisateur restent a tester. La DLL installee a ete sauvegardee et remplacee ; aucun autre binaire local ni adresse configuree n a ete modifie.

Correction visuelle du 21 octobre : colonne Actions de largeur explicite avec marge de 20 pixels. Captures reelles verifiees en 1280x920 et 1440x920 ; les deux boutons sont dans leurs cellules. A 1080x720, la zone de travail utilise son defilement horizontal existant. Test visuel avec profils et controle LAN desactives dans la copie de test, configuration restauree ensuite ; sortie normale code 0. DLL locale remplacee avec sauvegarde a94f560f785e57379bc81b7e07c5b6973c86591c.
