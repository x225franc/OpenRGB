# Prototype Bluetooth Razer — souris

Outil Windows isolé, sans modification du pilote OpenRGB ni de son installation.
Prérequis : Windows 11, SDK .NET 9, souris Bluetooth appairée nommée `BSKV3P 35K`.

```powershell
dotnet run --project tests/RazerBluetoothPrototype
dotnet run --project tests/RazerBluetoothPrototype -- --color-test
dotnet run --project tests/RazerBluetoothPrototype -- --restore
```

Sans argument : lectures uniquement. Le test écrit du bleu sur la molette (01), du vert sur le contour (0A), luminosité 96/255. Il relit les deux paramètres après chaque écriture. La zone logo (04), sans éclairage physique sur Phantom Green, est exclue des écritures.

Le premier test sauvegarde les états dans `original-lighting.json` à côté du binaire. `--restore` réutilise cette sauvegarde ; ne pas supprimer le dossier bin avant restauration. Pour sauvegarder un nouvel état initial après restauration, supprimer manuellement ce fichier.

Les échanges sont séquentiels, bornés par des délais, avec abonnement temporaire aux notifications et désabonnement en sortie. Une erreur interrompt les écritures : une zone déjà modifiée peut rester dans son nouvel état ; utiliser la restauration lorsque la connexion fonctionne à nouveau.

Validation réelle du 11 octobre 2026 : compilation et exécution réussies, catalogue 04/01/0A, lectures des états et luminosités, acquittements des écritures, relecture exacte des couleurs et de 96/255 sur 01 et 0A. Confirmation visuelle obtenue : molette bleue et contour vert, corrects et stables.

Limites : prototype ciblé sur la souris de test, filtrage par nom Bluetooth, pas encore de contrôle d'identité PnP robuste pour un pilote distribué. Pas de prise en charge OpenRGB, de clavier Bluetooth, d'animation continue, de reconnect automatique ou de débit validé. Les diagnostics HID/GATT voisins restent des outils de lecture.

Protocole de référence : https://github.com/gh123man/OpenSnek/blob/main/docs/protocol/BLE_PROTOCOL.md
