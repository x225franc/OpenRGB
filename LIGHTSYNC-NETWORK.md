# LightSync — changement de routeur

Plugin 0.1.2, commit d647eee813c278514718b1544d24da7a6c45bc84.

Dans LightSync > Devices :

- Search again relance la decouverte en utilisant les interfaces IPv4 actuelles du PC. Une reponse du meme appareil actualise son adresse sans recreer le controleur.
- Change address permet de saisir la nouvelle IPv4 d'un appareil LAN. Son identite, son nombre de LED, son etat de controle et ses associations sont conserves. Le champ Address fait foi ; le nom historique du controleur peut encore contenir l'ancienne IP afin de conserver les associations existantes.
- Forget libere le controle, retire l'appareil de l'inventaire sauvegarde et de la liste OpenRGB. Ses associations peuvent devoir etre recreees apres ajout. Il ne reapparait pas pendant les recherches automatiques de cette session ; Search again autorise son ajout a nouveau. Au prochain lancement, la decouverte automatique peut egalement le retrouver s'il est toujours sur le reseau.

Pour un simple changement d'IP, preferer Change address a Forget. Le PC doit avoir une interface IPv4 sur le sous-reseau indique ; verifier aussi le controle LAN dans Govee/Yeelight et l'absence d'isolation Wi-Fi. Aucun bouton LAN n'est propose pour Lenovo Lighting.

Installation sur un autre PC : fermer OpenRGB, extraire toute l'archive puis executer Install-LightSync.ps1 en administrateur. La configuration deja installee est conservee et sauvegardee. Ne pas restaurer la sauvegarde personnelle installation-backup du premier PC sur un autre PC.

Sur le PC de travail, seule la DLL LightSync a ete remplacee apres sauvegarde. Le coeur OpenRGB et les adresses de la configuration installee ont ete conserves. Les tests ne comprennent pas de changement physique de routeur ni d'envoi de couleurs aux appareils.
