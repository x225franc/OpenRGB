# BS2 Pro — préparation inactive

Lire [le handoff complet](../../Documentation/HANDOFF-BS2-PRO.md). Le refroidisseur n'était pas disponible ; ce dossier ne constitue pas une prise en charge matérielle.

- Encodeur : `Controllers/FlydigiController/FlydigiBS2ProProtocol.h`, pur C++17, aucun I/O ni enregistrement de périphérique.
- Provenance : `STATIC-EVIDENCE.json` ; format dérivé du code local de Space Station, pas de captures matérielles. Le schéma RGB et le transport BLE restent inconnus.
- Tests : `cmd /c tests\FlydigiBS2Pro\Run-Tests.cmd`. Les assertions valident uniquement la construction statique.
- Survey : `dotnet build tests\FlydigiBS2Pro\BluetoothSurvey\BluetoothSurvey.csproj -c Release`. Lancer le DLL sans option énumère seulement l'appairage, sans connexion demandée.
- `--inspect` : uniquement une fois le BS2 revenu/connecté ; lecture de PnP et métadonnées GATT. Aucune écriture ni souscription. Un PnP inconnu/différent bloque l'accès automatique au catalogue constructeur.

Le plan de transfert n'est pas un exécuteur : chaque étape exige une réponse réellement validée, dont le format reste à capturer. Ne pas transmettre un fichier CoolerMappingConfig global à la place de sa section RGB. Ne jamais toucher aux ventilateurs pour obtenir du RGB.

Préparation vérifiée le 29 octobre 2026 : tests C++ passés, survey compilé sans avertissement, appairage BS2 Pro trouvé en cache. Pas d'inspection GATT ni d'envoi au refroidisseur, pas de nouvelle installation OpenRGB.
