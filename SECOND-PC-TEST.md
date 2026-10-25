# Test souris Bluetooth — second PC

Cette version est candidate : le clavier fonctionne d'apres votre retour, mais la reprise souris doit encore etre validee sur le second PC. Les tests simules ne remplacent pas ce test.

1. Extraire toute l'archive, sauvegarder profil et VMap, puis quitter OpenRGB.
2. Dans PowerShell administrateur ouvert dans le dossier extrait : `./Install-LightSync.ps1`. Le script sauvegarde les fichiers remplaces et conserve la configuration installee.
3. Lancer l'OpenRGB installe et verifier dans Information le commit `db8949bf`.
4. En Bluetooth, attribuer une couleur a la molette et une autre au contour. Verifier qu'elles sont appliquees.
5. Eteindre dix secondes, rallumer en Bluetooth et reveiller la souris jusqu'a ce que le pointeur fonctionne. Attendre jusqu'a trente secondes sans reappliquer l'effet dans OpenRGB.
6. Refaire en passant Bluetooth → 2,4 GHz → Bluetooth. Le dernier etat attribue a l'entree Bluetooth doit revenir.

Les entrees Bluetooth et USB/HyperSpeed restent distinctes. Ce correctif restaure la scene attribuee en Bluetooth ; il ne convertit pas un effet Direct/Ambient en protocole Bluetooth.

Si la souris ne reprend pas, fournir le dernier fichier de `C:\Program Files\OpenRGB\config\logs`, notamment les lignes `[Razer Bluetooth]`, et confirmer si la souris reste listee en Bluetooth. Les nouveaux messages sont emis uniquement lors d'une perte d'etat, d'une reprise ou d'erreurs espacees.

Retour arriere : fermer OpenRGB et executer `./Uninstall-LightSync.ps1 -Backup "CHEMIN_SAUVEGARDE"` avec le chemin affiche par l'installation sur ce second PC. Ne pas restaurer `installation-backup` provenant du premier PC sur le second.
