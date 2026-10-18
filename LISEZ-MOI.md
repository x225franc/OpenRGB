# OpenRGB — nouvelle interface

Quitter l’autre OpenRGB puis lancer **app\OpenRGB.exe** ou **Lancer-OpenRGB.cmd**.

Un clic sur le logo blanc OpenRGB replie ou déplie le menu et conserve ton choix. Un clic droit rétablit le mode automatique. Le logo et l’icône OpenRGB sont blancs. **Appearance** propose Acrylic, Mica ou Opaque et suit la couleur d’accent Windows par défaut, avec actualisation à chaud.

Cette version conserve l’identité **OpenRGB LightSync** et les priorités WDL du clavier Lenovo. Le script Install-LightSync.ps1 permet de remplacer les binaires installes avec sauvegarde. Sa configuration dans `app\config` contient une copie des profils et de Visual Map. Les sept lumières LAN sont volontairement désactivées dans cette copie, conformément au dernier réglage demandé.

Si le dossier est déplacé, ou pour revenir à l’installation habituelle, réenregistrer le chemin avec les commandes du [guide](Documentation/MODERN-INTERFACE.md). Il n’est pas nécessaire de supprimer l’identité ni de recréer les priorités WDL. Ne pas lancer les deux versions simultanément.

Voir le [rapport de validation](VALIDATION-MODERN.md) et le [guide complet](Documentation/MODERN-INTERFACE.md) pour les tests, les limites, la compilation et la sauvegarde.

## Installation et restauration

Le package complet contient `app` (copie portable avec configuration), `payload` (fichiers a deployer), `scripts` et `validation`. `validation/historical` contient les resultats de l'ancienne livraison, pas de nouveaux tests. Les rapports HIBERNATION et YEELIGHT sont conserves comme references historiques.

Pour remplacer l'installation habituelle : fermer OpenRGB, ouvrir PowerShell en administrateur dans ce dossier puis lancer `./Install-LightSync.ps1`. Les profils, la VMap et les choix LAN de l'installation sont conserves. Une sauvegarde est creee dans `C:\Program Files\OpenRGB\Backup-installation\Modern-DATE`. L'identite WDL reste la meme et est rattachee au dossier installe.

Retour arriere : `./Uninstall-LightSync.ps1 -Backup "CHEMIN_DE_LA_SAUVEGARDE"`. Ajouter `-RestoreConfiguration` uniquement pour restaurer aussi les anciens reglages. Aucun desenregistrement de l'identite Windows n'est necessaire.
