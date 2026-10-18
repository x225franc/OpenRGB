# Validation de la version moderne

Version Windows x64 Release, Qt 6.8.3, API des plugins 5.
Code du binaire : f403e6f6e85601fc3a4e058d2d7e86753bdc9e5a.

- Compilation effectuée avec MSVC ; exécutable signé, signature Windows valide.
- Tests Qt : 16 réussis, aucun échec.
- Interface : 192 parcours et 576 assertions sur huit pages et plusieurs dimensions.
- Bouton logo blanc : centrage en mode réduit, bascule et persistance après redémarrage vérifiés.
- Six fermetures normales de validation, code de sortie 0, après correction d'une dépendance de compilation sur un ancien en-tête.
- Accent automatique : couleur de l'application identique à AccentLight2 de Windows (#ed88d1 pendant le test). Les notifications Windows sont raccordées ; un changement réel de couleur système n'a pas été testé.
- Fond Acrylic Windows : attribut DWM et surface alpha vérifiés, avec cadre natif redimensionnable.
- Identité conservée : OpenRGB.LightSync_h6e9bjf6s4fnj. Contrôle Lenovo en arrière-plan observé à 31 FPS. Aller-retour entre les dossiers ancien et nouveau vérifié sans changer de famille de package.
- Modules de développement retirés de la livraison. Effects, LightSync et Visual Map conservés.

Les sept appareils LAN restent désactivés dans cette copie à la demande de l'utilisateur. Aucun test de couleurs LAN n'a été effectué. Pas de mesure de performances en jeu ni de test prolongé d'une heure. Le 18 octobre 2026, les binaires installés dans Program Files ont été remplacés avec sauvegarde Modern-20261018-112424. Les profils et la VMap ont été conservés. L'identité WDL garde la même famille et pointe désormais sur le dossier installé. Le démarrage automatique et la tâche de reprise existants ciblent toujours l'installation habituelle.

Consulter Documentation/MODERN-INTERFACE.md pour le lancement, l'identité Windows et le retour arrière. L'archive contient une copie de configuration personnelle et ses sauvegardes : ne pas la publier comme distribution générique.
