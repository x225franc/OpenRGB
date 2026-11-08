# Reprise et relance natives Windows

## Fonctionnement

Cette version conserve l’identité OpenRGB.LightSync et les profils existants. Elle n’utilise aucun script PowerShell ni tâche planifiée pour récupérer après un crash.

- Les opérations de plugins/profils de veille sont différées dans la boucle Qt, hors WM_POWERBROADCAST.
- À la reprise, la restauration attend 2,5 secondes de stabilisation des changements d’affichage, avec une limite de 10 secondes. Les notifications dupliquées sont regroupées, y compris les doublons tardifs.
- Le paramètre Settings > General Settings > Start at Login > Automatically restart after a crash active un observateur natif caché. Deux processus OpenRGB.exe sont alors attendus : l’application et son observateur. L’observateur ne crée pas de QApplication, ne charge aucun plugin et ne contrôle aucun périphérique.
- Il attend le handle du vrai processus, y compris pendant l’écriture d’un dump WER, puis relance uniquement après une sortie de type exception. La relance est minimisée, conserve les arguments/configuration et respecte la protection d’instance unique. Un délai laisse les écrans se stabiliser après une reprise.
- Quitter, la fermeture normale, l’arrêt Windows et la fermeture de session ne relancent pas l’application. Au plus trois relances consécutives sont permises ; le compteur repart après dix minutes de durée de vie du processus surveillé.
- RegisterApplicationRestart est également enregistré pour Restart Manager Windows ; ce n’est pas ce mécanisme qui assure seul la récupération automatique.

## Démarrage après redémarrage du PC

Activer Enable Start at Login dans les paramètres. L’application crée un raccourci Windows natif OpenRGB.lnk dans le dossier de démarrage de l’utilisateur, avec le bon exécutable, répertoire de travail et chemin de configuration. L’ancienne entrée OpenRGB Preview est retirée. Ce démarrage se produit à l’ouverture de session, pas avant connexion à Windows.

L’ancienne tâche OpenRGB LightSync Resume Recovery doit être désactivée après installation pour ne plus déclencher Resume-OpenRGB.ps1. Cela relève de la migration d’installation ; aucun script n’est requis ensuite pour la reprise.

## Diagnostic et limites

Journal natif : %LOCALAPPDATA%\OpenRGB\NativeRecovery.log. Le journal indique le démarrage de l’observateur, la sortie observée et une éventuelle relance. Les tests isolés vérifient les codes d’exception, les sorties normales, l’annulation, la limite de relances, les arguments avec espaces, et l’ordonnancement des événements Qt.

Le déplacement du chargement des plugins corrige un risque de réentrance identifié. Il ne prouve pas la cause exacte de tous les crashes Qt6Gui observés. Les nouveaux tests de veille/hibernation réelles sur cette machine restent nécessaires. L’observateur ne force pas l’arrêt d’une application bloquée : un blocage sans fin de processus n’est pas un crash récupéré automatiquement.

## Vérification utilisateur après installation

1. Vérifier Enable Start at Login et Automatically restart after a crash.
2. Sauvegarder profil/VMap, puis faire une veille et une hibernation normales. Vérifier la présence du processus principal, de son observateur et des effets après reprise.
3. Redémarrer Windows et ouvrir la session ; OpenRGB doit apparaître minimisé avec la même configuration.
4. Quitter via le tray ; les deux processus doivent se terminer et rester arrêtés.

Ne pas confondre ces tests manuels à venir avec les tests automatisés déjà exécutés. Le refroidissement, Edge/WebView2, leurs services de mise à jour et le DNS ne sont pas modifiés par cette correction.

Référence Microsoft : https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-registerapplicationrestart
