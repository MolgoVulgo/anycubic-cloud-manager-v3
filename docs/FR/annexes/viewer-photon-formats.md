# Formats Photon/PWMB et viewer — annexe technique

> Statut : EXPÉRIMENTAL / PARTIEL. Cette annexe ne déclare pas un viewer prêt pour la production.

Statut : `IMPLEMENTE` pour le cœur isolé PWSZ decode/mesh, la représentation GPU compacte et les sections Z fermées dynamiques, `PARTIEL` pour le viewer desktop Qt Quick/OpenGL activé dans les presets de développement et `SPEC` pour l’intégration production et le LOD.

## Position produit

Le viewer reste isolé de `accloud_infra`. Le preset `default` l’active ; `dev-debug` et `local-full` héritent de cette valeur. `prod` et `protected-core` conservent explicitement `ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER=OFF`.

Deux presets de validation dédiés existent également :

```text
experimental-viewer-core
-> sans Qt
-> tests lecture PWSZ, décodage, masques, mesh, chunks, plage et caméra

experimental-viewer-qt
-> hérite de local-full
-> Qt Quick + Qt OpenGL
-> bouton 3D conditionnel sur chaque fichier PWSZ
-> téléchargement temporaire puis dialogue de visualisation
-> décodage/maillage PWSZ asynchrones et upload GPU
```

`VolumeViewerPage.qml` et `VolumeViewerDialog.qml` sont présentes dans le bundle de ressources normal afin de conserver un packaging QML unique. L’action 3D est présente sur chaque ligne PWSZ. Lorsque le flag est désactivé, l’action est désactivée, le dialogue n’est pas chargé et le type `Accloud.Render3D` n’est pas enregistré.

## Pipeline implémenté

```text
ZIP PWSZ
-> métadonnées machine/couches
-> index numérique
-> décodage pw0Img
-> masque matière bit-packed
-> mesh par empilement
-> chunks de 8 couches
-> surfaces axis-alignées compactées sur 8 octets
-> file CPU-vers-GPU bornée
-> buffers d'instances OpenGL sans vertices/index dupliqués
-> budget GPU vérifié avant allocation
-> clipping Z dynamique exact
-> surfaces de section basse/haute dérivées des masques de frontière
-> navigation et contrôles de plage Qt Quick
-> ouverture depuis le bouton 3D de la ligne du fichier PWSZ
```

La lecture PWSZ et le maillage ne s’exécutent jamais sur le thread GUI. Un job coordinateur distribue les tâches de chunks à un pool de workers configurable. La valeur par défaut est 4, la plage utilisateur autorisée va de 1 à 16 et la clé persistée est `render3d.workerCount`. Les tâches adjacentes relisent un échantillon de frontière afin de conserver exactement les transitions horizontales et les parois verticales. Chaque rectangle de surface est conservé dans un `PackedSurfaceQuad` de 8 octets. Les chunks passent par une `UploadQueue` bornée ; le thread de rendu vide cette file et crée uniquement des buffers d'instances compacts.

## Familles de formats étudiées

- `PWMB` ;
- `PWS` ;
- `PHZ` ;
- `PHOTONS` ;
- `PWSZ`.

PWSZ reste le format pilote. Les autres drivers demeurent scaffold ou partiels tant que leur parsing et leurs tests ne sont pas fermés indépendamment.

## Contrat du conteneur PWSZ

Le lecteur supporte les ZIP mono-disque classiques avec entrées Store ou Deflate. Il rejette ZIP64, chiffrement, méthodes non supportées, noms dupliqués et chemins dangereux.

Entrées requises :

```text
anycubic_photon_resins.pwsp
layers_controller.conf
layer_images/layer_<index-numerique>.pw0Img
```

Les couches sont triées numériquement. Le nombre doit correspondre à `layers_controller.conf` et l’indexation doit être continue depuis zéro. Les pitches X/Y indépendants sont conservés lorsqu’ils sont fournis.

## Contrat de décodage `pw0Img`

Le RLE observé est mixte :

```text
couleur 0 ou 15 :
  deux octets, mot 16 bits big-endian
  nibble haut = index couleur
  12 bits bas = longueur

couleur 1 à 14 :
  un octet
  nibble haut = index couleur
  nibble bas = longueur
```

Règles :

- `run_len == 0` invalide ;
- run deux octets tronqué invalide ;
- dernier run bornable au raster restant ;
- octets après complétion ignorés avec diagnostic ;
- niveaux gris détectés dans les couches, pas déduits des métadonnées slicer ;
- antialiasing optionnel : un fichier valide peut ne contenir que `0` et `15`.

Les valeurs 4 bits exactes ne sont conservées que sur demande. La géométrie peut ne garder qu’un masque matière bit-packed.

## Vérité géométrique

```text
matière = tout pixel non noir
```

Modèle, supports et radeau partagent le même masque et doivent être conservés. Le mesh ne doit pas garder uniquement le plus grand composant, supprimer les pointes de support, remplir les vides, réduire une couche à un contour extérieur, fusionner des composants séparés ni dépendre de l’antialiasing.

Le mesher CPU n’émet que les interfaces matière/vide :

```text
transition XY dans une couche
-> paroi verticale

couche précédente vide, couche courante matière
-> face horizontale basse

couche courante matière, couche suivante vide
-> face horizontale haute
```

La méthode conserve surfaces externes, parois internes, trous, supports, radeau et îlots indépendants sans générer un cube par pixel. Les runs coplanaires et spans verticaux identiques sont fusionnés lorsque possible.

### Représentation GPU compacte

Le chemin actif n'envoie plus quatre `MeshVertex` et six indices par rectangle. Un `PackedSurfaceQuad` encode l'orientation, le plan fixe, les deux bornes du rectangle et les coordonnées Z relatives au chunk dans deux mots 32 bits :

```text
ancien format équivalent : 4 vertices + 6 indices = 120 octets/quad
format actif             : 2 × uint32             = 8 octets/quad
réduction structurelle   : 15×
```

Le vertex shader reconstruit les six sommets avec `gl_VertexID`, le pitch XYZ du chunk et sa couche de base. La normale vient de l'orientation encodée. Le rendu utilise `glDrawArraysInstanced` ; aucun VBO de positions/normales ni IBO par chunk n'est créé. Cette compression est sans perte : elle ne retire aucune couche, aucun contour, aucune cavité, aucun support et aucun îlot.

Le format principal accepte les coordonnées X jusqu'à 16 383, Y jusqu'à 8 191 et une étendue Z relative de 63 couches par chunk. Les valeurs hors contrat sont rejetées avant génération au lieu d'être tronquées.

## Chunks et plage visible

Le mesh est découpé en chunks inclusifs de 8 couches. Ce défaut est issu du benchmark Beetle : il réduit la latence du premier chunk tout en conservant une durée totale comparable aux chunks de 16 et 32 couches. Chaque chunk porte ses bornes exactes et sa bounding box monde. Les frontières de chunks peuvent segmenter une même paroi coplanaire en davantage de triangles, mais elles ne doivent produire ni triangle superposé, ni variation de surface exposée, ni variation des bornes du volume.

L’UI utilise des valeurs inclusives base 1 ; le cœur et le renderer utilisent des index inclusifs base 0. Pour 1 247 couches :

```text
plage utilisateur 415..1021
-> plage interne 414..1020
-> clipping Z 20,70..51,05 mm à 0,05 mm/couche
-> 607 couches visibles
```

Le fragment shader OpenGL clippe chaque triangle sur les plans Z exacts. Le renderer sélectionne d’abord les chunks intersectés. Le déplacement du curseur ne reconstruit donc pas le mesh complet.

Lorsqu’une borne coupe le document, un worker dédié décode uniquement le masque de la couche frontière et construit une surface compacte sur le plan Z correspondant. La section basse porte une normale Z négative et la section haute une normale Z positive. La matière vient exactement du masque utilisé par le mode d’échantillonnage actif : une pièce pleine produit une section pleine, une coque conserve sa cavité, et des supports ou îlots séparés restent séparés. Les faces horizontales déjà présentes sur le plan de clipping sont supprimées pendant la passe du mesh principal afin d’éviter leur superposition avec le bouchon dynamique.

Le changement de plage est transactionnel. Le renderer conserve la plage et les sections actuellement affichées pendant la construction de la nouvelle demande. La nouvelle paire « plans de clipping + surfaces de section » est uploadée dans un buffer de préparation, puis remplacée dans une même frame. Une section ne peut donc jamais disparaître avant que sa remplaçante soit prête. Les demandes intermédiaires rendues obsolètes par un déplacement rapide du curseur sont abandonnées sans modifier l’état affiché.

Les sections XY décodées sont conservées dans un cache CPU LRU compact, indépendant de l’orientation et du plan Z. Chaque rectangle occupe 8 octets. Le cache est borné à 64 Mio et 2 048 couches ; seules les une ou deux sections actives résident sur le GPU. Une même couche peut ainsi être réutilisée immédiatement lors d’un retour du curseur sans dupliquer les faces haute et basse.

## Navigation

La page Qt Quick mappe :

- glisser gauche : orbite ;
- glisser droit, milieu ou Maj : pan ;
- molette : zoom ;
- action de reset : cadrage sur la bounding box complète chargée.

La caméra permet d’inspecter la pièce sur toutes ses faces. Le shader applique un éclairage directionnel double face simple pour rendre lisibles les parois internes lors d’une coupe Z.

## Décision backend

Le premier backend desktop utilise `QQuickFramebufferObject` et Qt OpenGL : API publique, compatible avec les versions Qt 6 visées par le projet et cohérente avec la frontière `render3d/gl`. Le bootstrap force le scene graph Qt Quick sur OpenGL lorsque `ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER` est activé.

Ce choix n’est pas une politique de rendu production. Un backend QRhi pourra le remplacer lorsque la version mineure Qt supportée sera figée et validée. Cloud, MQTT et les builds QML normaux restent indépendants du viewer.

## Performance et limites

Garde-fous implémentés :

- masques bit-packed ;
- maillage séquentiel avec couches voisines ;
- aperçu rapide par défaut avec `layerStride=2`, soit une couche source décodée sur deux ;
- conservation des bornes première/dernière exactes et de l’étendue Z d’origine malgré l’échantillonnage ;
- reconstruction détaillée optionnelle avec `layerStride=1` ;
- callbacks de chunks streaming ;
- file d’upload bornée : 8 chunks / 256 Mio en attente par défaut ;
- représentation GPU de 8 octets par surface au lieu de 120 octets équivalents ;
- rendu instancié sans buffers vertices/index dupliqués ;
- budget de résidence GPU de 2 Gio vérifié avant chaque allocation ;
- arrêt contrôlé, vidage de génération et message UI si le budget ou OpenGL refuse un chunk ;
- pas de conservation des rasters gris denses par défaut ;
- isolation du thread GUI ;
- annulation lors d’un rechargement ou de la destruction du viewer.

L’aperçu rapide est une approximation d’affichage uniquement. Un support ou détail présent sur une seule couche ignorée peut ne pas apparaître avant la sélection du mode détaillé. Le PWSZ et les données d’impression ne sont jamais modifiés.

### Diagnostics Render3D dédiés

Le viewer écrit des événements JSONL structurés avec la source `render3d`. Le logger crée donc `render3d.jsonl` dans le répertoire de logs ACM configuré (ou `ACCLOUD_LOG_DIR`). Les événements couvrent l’ouverture de l’archive, les dimensions source, le pas d’échantillonnage, la progression par tranche de 10 %, le nombre de surfaces, les octets compacts, l'équivalent legacy, le ratio de compression, l’attente de la file d’upload, les octets GPU résidents, le budget, les durées, annulations et erreurs. `gpu.compact_chunk_uploaded`, `gpu.cut_surface_uploaded`, `gpu.budget_exceeded` et `gpu.scene_reset` permettent de vérifier les allocations et libérations. `cut_surface.boundary_built` et `cut_surface.build_completed` décrivent la couche frontière décodée, le plan et le nombre de surfaces compactes. Le journal inclut aussi `mesher.worker_completed`. Les chemins temporaires complets et les URL signées ne sont pas journalisés.

Limites connues :

- les chunks compacts restent en GPU pour le document chargé ;
- pas encore d’éviction GPU, LOD ou simplification ;
- le budget de 2 Gio refuse proprement un modèle compact pathologique au lieu de laisser le pilote interrompre le processus ;
- les modèles exacts peuvent conserver un très grand nombre de surfaces sur des supports denses ;
- pas de couleur sémantique modèle/support/radeau ;
- backend desktop OpenGL uniquement à cette étape expérimentale.

## Validation

Le gate core hors ligne inclut :

```text
accloud_experimental_viewer_architecture
accloud_experimental_viewer_scaffold
accloud_pw0_decode
accloud_pwsz_reader
accloud_layer_stack_mesher
accloud_render_pipeline
accloud_viewer_controls
```

`accloud_render_pipeline` valide la comptabilité compacte de la file bornée, le format 8 octets, l'expansion géométrique exacte, le ratio 15×, le budget GPU simulé, une pièce synthétique complète de 250 mm, la consommation streaming, l’annulation, la sélection des chunks, les plans Z exacts, les sections pleines, les cavités conservées, les îlots de supports séparés, le format 8 octets du cache de sections et son éviction LRU bornée.

Validation desktop obligatoire sur un poste avec dépendances Qt natives :

```bash
cmake --preset experimental-viewer-qt
cmake --build --preset experimental-viewer-qt --clean-first
ctest --preset experimental-viewer-qt --output-on-failure
```

Les PWSZ réels peuvent servir d’entrées locales, mais ne deviennent pas des fixtures distribuées sans droit explicite de redistribution.

## Décision

Le viewer possède désormais un chemin de développement de bout en bout depuis chaque ligne de fichier PWSZ vers un mesh 3D navigable et filtrable par plage, avec surfaces GPU compactes et budget d'allocation contrôlé. La production reste désactivée. Sa préparation exige encore la validation locale sur de grands PWSZ et le LOD/éviction éventuels.
