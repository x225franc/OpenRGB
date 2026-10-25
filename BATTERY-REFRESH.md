# Batterie Razer : actualisation automatique et manuelle

Une premiere lecture est effectuee au demarrage, puis toutes les cinq minutes, en USB, HyperSpeed et Bluetooth. Le pourcentage et sa derniere date valide restent visibles si le peripherique dort ou ne repond pas.

Dans Devices, selectionner le clavier ou la souris Razer : le bouton Refresh battery a cote du niveau demande une lecture immediatement planifiee sur le thread de fond existant. Updating indique une lecture ou demande en cours ; les clics repetes sont regroupes. Cette demande manuelle peut ne pas aboutir si la souris dort : reveiller normalement la souris puis reessayer. Aucun acces materiel n'est effectue sur le thread Qt. L'affichage du cache est actualise une fois par seconde sans nouvelle requete au peripherique.

Le delai est recalcule apres une lecture, y compris manuelle. Il n'y a ni file de requetes accumulees ni nouveau thread par clic. Le reglage du capteur a 500 Hz est conserve. La cadence des effets lumineux est independante de ces lectures de batterie. Aucun gain d'autonomie n'a ete mesure dans cette livraison.

Validation : compilation Windows x64, signature valide, 13 controles du moniteur de batterie et 18 controles du worker Bluetooth avec transport simule. Ces tests verifient notamment l'execution hors du thread appelant, la coalescence, le cache en veille, l'arret pendant l'attente de cinq minutes et la requete manuelle Bluetooth.
