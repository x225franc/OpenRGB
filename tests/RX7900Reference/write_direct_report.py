"""Render a completed campaign report from measured data and human observations."""
import json
import pathlib

root = pathlib.Path(__file__).parent
directory = root/'direct-cadence-20261026'
summaries = json.loads((directory/'native-summaries.json').read_text())
visual = {r['trace']:r['status'] for r in json.loads((directory/'visual-confirmations.json').read_text())}
endurance = next(r for r in summaries if pathlib.Path(r['trace']).name=='native-mode-once-7fps-300s.jsonl')
if not endurance['complete']:
    raise SystemExit('Five-minute trial not complete; report not replaced')
rows = []
for r in summaries:
    name = pathlib.Path(r['trace']).name
    cfg, complete, frame = r['configuration'], r['complete'], r['frames']
    short = cfg['sequence']=='static-apply-mode-selected-once'
    rows.append(f"| `{name}` | {cfg['target_fps']:g} | {r['effective_fps']:.3f} | {'2 + selection initiale' if short else '3'} | "
                f"{complete['elapsed_ms']/1000:.3f} | {r['animation_messages_per_second']:.3f} | "
                f"{frame['mean_ms']:.3f} / {frame['p95_ms']:.3f} / {frame['max_ms']:.3f} | "
                f"{len(r['invalid_exchanges'])} / {len(r['unfinished_exchange_ids'])} | {visual.get(name,'non confirme')} |")
metrics_path = directory/'native-mode-once-7fps-300s.jsonl.metrics.jsonl'
metrics = [json.loads(line) for line in metrics_path.read_text().splitlines()]
cpu = next(r for r in metrics if r['event']=='end')
interval = endurance['frame_start_intervals']
text = f'''# RX 7900 XTX Reference / Avatar : cadence Direct mesuree

Campagne du 26 octobre 2026. Base : `364a3c470`, branche
`investigation/rx7900-direct-cadence`. Aucun changement du controleur de production,
de `feature/fluent-interface`, de l'API plugins, des profils ou de Visual Map.

## Conclusion pour l'autre PC

La limite actuelle de 4/s n'est pas une limite materielle mesuree. La sequence
complete `51 2C -> 51 A0 -> 51 28`, avec trois attentes de 30 ms, plafonne ici
a **environ 5,36 mises a jour/s effectives**. La cible 10/s a ete essayee,
mais ne signifie pas dix changements physiques de LED par seconde.

Un test isole avec les commandes connues a etabli visuellement que Static peut
etre selectionne une fois, puis que `51 2C -> 51 28` suffit pour les changements
de couleur dans la meme session. Ce chemin court atteint **8,04 mises a jour/s**
sur 30 secondes, avec confirmation visuelle et tous les ACK valides.

La cible **7/s**, avec marge sous ce plafond court, a ensuite tenu
**{endurance['complete']['elapsed_ms']/1000:.3f} secondes** a
**{endurance['effective_fps']:.3f} mises a jour/s effectives**. Confirmation humaine :
**{visual.get('native-mode-once-7fps-300s.jsonl','en attente')}**.
Ce palier constitue une proposition experimentale pour une future integration,
pas une certification constructeur ni une validation du binaire OpenRGB complet.
La cadence de depart proposee est cette cible 7/s avec conservation des ACK,
attentes, exclusion PCI et arret sur erreur. Ne pas annoncer 10 FPS reels.

**L'essai de 30 minutes a ete explicitement annule par l'utilisateur** : il a
demande de conclure apres celui de cinq minutes. Pas de certification d'endurance,
de veille/reprise, de jeu 3D ni de cadence physique des LED. Les confirmations
portent sur des changements juges reguliers et un PC normal, pas une mesure video.

## Resultats complets

Une frame represente une couleur globale du GPU, pas une LED independante.
Les durees de frame ci-dessous couvrent les transactions et leur journalisation ;
les intervalles de debut de frame incluent aussi l'attente de cadence et les
inventaires de producteurs. Tous les chiffres sont extraits des traces.

| Trace | Cible /s | Effective /s | Messages/frame | Duree s | Messages/s animation | Frame moyenne / P95 / max ms | ACK invalides / echanges incomplets | Observation humaine |
|---|---:|---:|---|---:|---:|---|---|---|
{chr(10).join(rows)}

`not-observed` signifie que l'utilisateur etait absent ; les repetitions ont ete
demandees par lui. `regular-changes-pc-normal` et `visible-changes-pc-normal`
correspondent a ses confirmations, reproduites dans `visual-confirmations.json`.
Les messages d'initialisation sont exclus du debit messages/s animation, mais
inclus dans les statistiques generales d'Exchange et la verification des ACK.

## Observation officielle et essai raccourci

Chroma absent ou inutilisable selon le proprietaire : **aucun flux Chroma reel
n'a ete observe**. L'alternative a ete l'automatisation de RGB Tool pendant une
capture passive. Rouge, vert et bleu Static, puis 23 changements du curseur R,
ont chacun envoye la sequence complete de trois messages, sans bouton Apply.
Le mode est donc repete a chaque mise a jour dans ce chemin officiel.
Les changements declenchent des appels GPU pendant le mouvement du curseur ;
la cadence visuelle officielle n'a pas ete mesuree.

La capture officielle ne prouve pas que `51 A0` est facultatif. L'omission repetee
a ete testee ensuite explicitement : selection Static initiale avec Apply, puis
deux messages par frame, d'abord a 2/s avec confirmation humaine, puis aux cibles
6/8/10/s et enfin 7/s pendant cinq minutes. Ne pas generaliser cette optimisation
aux transitions entre modes, a Off, a une reprise ou a une autre session.
Lors d'une future integration, reinitialiser la selection apres ces transitions.

Les messages de 64 octets et six trames sont dans les traces. Le GPU cible exact
est `1002:744C`, sous-systeme `1002:0E3B`, PCI 3:0:0 ; l'index ADL 5 a ete resolu
par enumeration dans chaque processus, jamais code en dur. Le mutex PCI de
production est conserve. Aucun processus officiel/OpenRGB/Synapse/Chroma ne
coexiste avec les ecritures natives. Le probe verifie les producteurs a chaque
frame et refuse un fichier de sortie existant.

## Delais, maintien eveille et charge

Les attentes restent `Sleep(30)` pour chaque Exchange. Elles mesurent environ
37 a 41 ms en moyenne selon les essais ; aucune resolution de timer n'a ete
changee, aucun ACK n'a ete sonde avant le delai et aucune reemission n'est faite.
La disponibilite reelle des ACK avant 30 ms demeure inconnue. Avec deux attentes
de 30 ms, **60 FPS sont deja impossibles sur ce chemin**, meme avant les couts
ADL. Cela ne prouve pas une limite universelle du transport ou du firmware.

Les traces separent creation/destruction PMLog, KeepAwake, chaque appel ADL,
attente, Exchange et frame. KeepAwake_Start retourne -1 sur ce pilote ; les
reponses SMU restent obligatoires. Ni KeepAwake ni PMLog n'ont ete retires ou
partages differemment pour gagner du debit. La comparaison avec un autre
maintien eveille n'a pas ete effectuee.

Pendant les cinq minutes, l'intervalle de debut de frame vaut en moyenne
**{interval['mean_ms']:.3f} ms**, P95 **{interval['p95_ms']:.3f} ms**,
maximum **{interval['max_ms']:.3f} ms**. CPU cumule du probe :
**{cpu['cpu_seconds']:.3f} s** sur **{cpu['wall_seconds']:.3f} s** murales,
soit **{cpu['process_cpu_percent_machine']:.3f}%** de la capacite des 16 processeurs
logiques. Ce chiffre concerne le processus, pas l'utilisation totale du PC.
Les compteurs moteurs GPU Windows non nuls sont releves environ toutes les cinq
secondes ; ils incluent les autres applications et ne constituent pas une mesure
de charge isolee de la carte reference. Aucun capteur de temperature du controleur
RGB n'est disponible dans cette mesure ; aucune temperature GPU n'a ete relevee.
Les premiers essais sans lanceur n'ont pas de surveillance CPU/GPU continue.

## Provenance, ecarts et limites

`official-manual` est une capture sans commande pendant l'attente de l'utilisateur,
terminee normalement a sa borne de 600 s. `official-automated` conserve les vrais
appels officiels, leurs heures UTC d'entree/sortie et la fermeture propre.
La voie legacy appelle ADL2 dans ce pilote : utiliser `canonical: true` pour ne
pas compter deux fois. Les timestamps Frida sont ceux de reception cote hote,
pas une horloge du controleur. Les traces natives utilisent une horloge monotone
avec ancrage UTC Unix ; les couts de journalisation sont inclus.

Les premiers clics automatiques ont ete ignores ou decales par le DPI Windows.
Ces intervalles de preparation sont conserves mais exclus du benchmark Static.
La sequence finale a ete controlee par captures de fenetre et messages observes.
Un clic officiel Apply a emis **50 55**, avec un ensemble de reglages et un ecran
de progression. Ce comportement a ete constate dans la capture ; Apply a alors
ete retire du script livre et n'a plus ete utilise. Aucun Save/Reset, onglet
Version ou firmware update n'a ete intentionnellement selectionne. Le probe
natif n'emet jamais 50 55. Il ne faut pas affirmer qu'Apply officiel est volatil.

Toutes les transactions natives ont ete verifiees par leurs retours et ACK,
independamment recontrolees hors materiel. Cela ne mesure pas la latence entre
ACK et lumiere visible. Pas de video ralentie, de mire ni de test LED par LED.
Le PC etait utilise normalement ; aucune sequence de jeu/video controlee n'a
ete imposee. Aucun incident n'a ete rapporte sur les paliers confirmes.

## Reproduction, fichiers et retour arriere

Compiler `DirectCadenceProbe_Windows.cpp` en C++17 Windows avec l'include
`dependencies/display-library/include`. Compilation locale reussie avec le
Zig/Clang portable deja present. Aucun binaire n'a ete place dans l'installation
OpenRGB. Il n'y a donc pas de remplacement a annuler ; le controleur de production
et les reglages sont inchanges. Les couleurs des essais sont appliquees en direct.
L'etat final reste la derniere couleur du dernier essai, sans restauration rouge
automatique ni sauvegarde explicite.

```powershell
DirectCadenceProbe.exe 2 30 NOUVELLE_TRACE.jsonl
DirectCadenceProbe.exe 2 30 NOUVELLE_TRACE_COURTE.jsonl mode-once
powershell -NoProfile -ExecutionPolicy Bypass -File tests/RX7900Reference/RunDirectCadence.ps1 -Probe .build/DirectCadenceProbe.exe -Trace NOUVELLE_TRACE_5MIN.jsonl -Fps 7 -Seconds 300 -ModeOnce
python tests/RX7900Reference/analyze_direct_cadence.py NOUVELLE_TRACE.jsonl
python tests/RX7900Reference/test_direct_cadence_analysis.py
```

Les traces longues sont archivees integralement en `.jsonl.gz` avec verification
SHA256 apres decompression ; les originaux restent locaux. Decompresser avant
analyse. `manifest.json` donne les tailles et SHA256 des preuves publiees.
`hardware.json` et `software.json` decrivent le materiel, le pilote et les fichiers.
Le SHA du probe correspond au dernier build, incluant mode-once ; les premiers
essais ont precede ces ajouts. Pas de compilation complete Qt/MSVC OpenRGB ici.

Trois tests hors materiel valident le decodeur natif sur une trace complete,
une reponse volontairement corrompue et une fin d'echange manquante. Les tests
de protocole existants ont aussi reussi, dont les 15 messages firmware captures.
L'arret Ctrl+C ou fichier `TRACE.jsonl.stop` est prevu a une frontiere de frame,
sans retry/restauration apres erreur ; ce chemin d'arret n'a pas ete provoque sur
le materiel. Le lanceur demarre un processus masque et releve les compteurs OS.

La suite appartient au PC de compilation : examiner l'optimisation, garder les
garanties de transport et tester le vrai binaire OpenRGB avec Ambient/Visual Map.
Un worker serie et un dernier etat remplacant les anciens eviteraient de bloquer
l'appelant ou d'accumuler les couleurs. Aucune de ces modifications n'est livree
dans cette branche d'investigation.
'''
(root/'DIRECT-CADENCE-RESULTS.md').write_text(text,encoding='utf-8')
print('Report written')
