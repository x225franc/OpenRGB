# Correctif de memorisation des avertissements OpenRGB

Branche : `feature/fluent-interface`. Base : `151c821e19c50532045e82ec93b920cfba303b8a`.

## Cause et correction

`OpenRGBDialog::onShowDialogMessage` stockait les messages masques dans `LogManager.dialog_no_show_hashes`, mais ce champ manquait au schema enregistre dans `ResourceManager`. `SettingsManager::FilterSettingsAgainstSchema` supprimait donc ce choix avant sauvegarde. Le fichier de configuration observe sur ce PC ne contenait aucun hash de masquage.

Le correctif declare le tableau dans le schema, conserve les anciens hash, verifie le type du tableau et lit l'etat final de la case. Il relit les reglages apres la fermeture de la boite modale pour conserver les modifications survenues entretemps. Le texte utilise pour identifier et afficher la boite est capture avant son ouverture.

L'initialisation I2C/SMBus reste inchangee. Masquer cette fenetre ne repare pas un acces materiel qui echoue. PawnIO 2.2.0.0 est installe sur le PC de la carte. L'API plugins reste 5.

## Compilation sur l'autre PC

Les modifications sont aussi versionnees sur `feature/fluent-interface`. Si vous les recuperez avec `git pull --ff-only origin feature/fluent-interface`, ne reappliquez pas le patch : passez directement a la compilation (etape 3 ci-dessous).

Les outils Qt/MSVC ne sont pas installes sur le PC de capture. Aucune compilation ni validation du nouveau binaire n'a donc ete effectuee ici. Le paquet contient le diff des trois fichiers modifies et cette procedure, sans profil ni configuration personnelle.

1. Utiliser le checkout `feature/fluent-interface`, idealement a la base ci-dessus. Preserver ses changements locaux.
2. Verifier puis appliquer le patch avec `git apply --check OpenRGB-dialog-persistence.patch`, puis `git apply OpenRGB-dialog-persistence.patch`. Ne pas remplacer les fichiers entiers si le checkout contient d'autres modifications.
3. Avec l'environnement de compilation habituel, executer `scripts\CompileModern.cmd`, puis `scripts\test-modern.cmd`. Si les chemins ont change, definir `QT_ROOT` et ajuster le chemin MSVC aux outils installes.
4. La modification de `OpenRGBDialog.h` impose de reconstruire tous ses dependants. Executer la verification `scripts\VerifyModernBuild.ps1` fournie par le projet.

## Validation a realiser sur le binaire compile

- Afficher l'avertissement, cocher Ne plus afficher, valider OK. Verifier que le fichier OpenRGB.json du repertoire de configuration actif contient le tableau LogManager.dialog_no_show_hashes.
- Quitter normalement et relancer deux fois : ce meme avertissement ne doit plus apparaitre.
- Modifier ensuite un reglage de journalisation et redemarrer : le choix de masquage doit rester conserve.
- Sur une configuration de test separee, cocher puis decocher la case avant OK : le message doit reapparaitre au prochain lancement.
- Verifier le chargement des plugins API 5 et de l'interface Fluent.

Ne pas lancer des detections OpenRGB pendant les captures de RGB Tool.

## Sauvegarde et retour arriere avant deploiement

Avec OpenRGB ferme, sauvegarder integralement `C:\OpenRGB` et le repertoire de configuration actif (sur le PC observe : `C:\Users\Administrator\AppData\Roaming\OpenRGB`) dans des dossiers dates. Conserver les profils, plugins et donnees Visual Map. Ne pas livrer ces sauvegardes personnelles dans le paquet source.

Deployer uniquement apres compilation reussie, puis verifier le comportement reel. En cas de probleme, quitter OpenRGB et restaurer le repertoire d'application sauvegarde ; restaurer egalement la configuration sauvegardee si necessaire. Aucun binaire n'a ete remplace dans cette session.
