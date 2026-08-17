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
-> bit sémantique conservateur pièce/support dérivé de la géométrie des couches
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

## Sémantique estimée des supports

Les couches raster PWSZ ne contiennent pas d'étiquettes exactes du slicer pour la pièce, les supports ou le radeau. Le masque d'exposition reste donc l'unique vérité géométrique : les pixels non noirs définissent la matière et l'analyse sémantique peut seulement associer une catégorie visuelle à cette matière existante.

Le viewer Qt possède désormais deux chemins runtime distincts :

```text
Supports désactivés
-> ouverture du PWSZ
-> décodage uniquement des couches nécessaires à l'aperçu fixe une couche sur deux
-> construction du mesh classique par chunks, sans analyse sémantique

Supports activés
-> passe sémantique 1 (bas vers haut) : décodage de toutes les couches natives du PWSZ
   et propagation de la provenance support depuis la première couche après le radeau
-> passe sémantique 2 (haut vers bas) : relecture des couches natives depuis la dernière
   couche et propagation indépendante de la provenance pièce vers le bas
-> réconciliation des deux provenances par composant/run en support, pièce ou matière mixte
-> reconstruction du mesh habituel une couche sur deux depuis les masques originaux avec
   matérialisation de l'index radeau/support réconcilié
```

Le chemin désactivé ne lance pas `SupportAnalyzer`, ne décode aucune couche de contexte pour classifier les supports et ne transmet aucun provider de masque support à `LayerStackMesher`. Sa géométrie, son découpage en chunks et sa représentation GPU restent ceux du viewer classique.

Le chemin activé exige de la matière de radeau dès la couche 1 et attribue les phases globales `Raft`, `SupportsOnly`, `ModelAndSupports` et `ModelMostly`. Le radeau est le préfixe de masques natifs répétés qui commence à la couche 1 ; un nombre borné de pixels d'antialias différents est toléré, mais aucune quantité fixe de couches ni aucune fenêtre de hauteur physique ne le définit. Son premier successeur matériellement différent est la première couche de supports et constitue la vérité racine de la passe bas vers haut. Indépendamment, chaque pixel exposé de la dernière couche native constitue la vérité racine de la passe pièce haut vers bas. La hauteur de couche du PWSZ sert uniquement au placement Z et ne peut pas modifier la classification sémantique d'une même séquence de couches.

L'analyseur construit d'abord les mêmes informations de composants connexes et de relations natives pendant le parcours montant, mais la passe sémantique 1 enregistre désormais une **preuve de provenance support**, pas un verdict final de pièce. La provenance enracinée dans le radeau est propagée au moyen du recouvrement, de la distance matière exacte, du mouvement borné, des branches et des fusions de supports. Les ratios de surface, l'historique de rétrécissement, les résidus de mouvement et la couverture de fusion deviennent des valeurs de confiance/preuve : ils peuvent signaler une frontière terminale plausible du support, mais ne possèdent plus à eux seuls la décision sémantique finale. La passe sémantique 2 relit les masques natifs dans l'ordre décroissant et propage indépendamment un masque clairsemé de pièce vers le bas. Un îlot de pièce déconnecté de la lignée descendante courante ne peut initialiser une nouvelle lignée pièce que si la passe 1 ne le revendique pas déjà comme provenance support et jamais sous la première couche pièce observée par la passe montante. La réconciliation confronte ensuite les deux provenances sur le même composant. Une preuve support seule donne support, une preuve pièce seule donne pièce et les deux preuves simultanées sont séparées en runs mixtes au lieu de forcer tout le composant connexe dans une seule catégorie.

Les supports et la pièce ne sont pas séparés par un diamètre ou une surface absolue de composant. Les supports light, normal et heavy sont suivis selon l'évolution relative de leur section. Pendant les phases mixtes, les composants sont suivis dans une forêt orientée enracinée : un seul parent structurel est conservé, une branche peut se diviser, aucune fusion structurelle de branches n'est créée et les contacts secondaires ne deviennent des croisillons que lorsque leur dérive raster native par couche respecte la plage configurée. L'association des parents utilise le recouvrement des runs raster natifs et la distance matière exacte entre runs ; un simple recouvrement de boîtes englobantes ne peut donc plus créer un parent support à travers une zone vide. Une branche enracinée dans le radeau conserve son identité de support malgré une variation locale de section, une fusion raster temporaire ou un recouvrement avec un composant de pièce déjà établi. Ce recouvrement ne peut pas, à lui seul, couper la branche. Si au moins deux sections support précédentes restent largement conservées et expliquent ensemble une part significative du composant courant, l'événement est classé `support_fusion_continuation` : le composant fusionné reste support au lieu d'ouvrir un contact pièce. Seule une fusion connectée dominée par la pièce et très supérieure au profil récent du support peut contourner cette continuité, et une expansion après rétrécissement reste toujours d'abord un contact provisoire. Un support qui démarre sur une pièce déjà établie doit également naître d'une racine localement étroite ; une excroissance large de la pièce ne devient pas un support simplement parce que son extrémité supérieure se rétrécit.

Dans la passe montante de preuve, un rétrécissement suivi d'une croissance locale ouvre l'état `contact candidat` ; il ne transforme pas immédiatement le composant courant en pièce et ne suffit plus à fixer la frontière sémantique finale. Une pointe terminale n'est pas établie par une ancienne chute isolée par rapport à la plus grande section de la fenêtre d'historique. La réduction relative doit aussi être confirmée par un effondrement final immédiat, par plusieurs diminutions significatives dans la lignée récente, ou par un rebond borné après un rétrécissement réel. Un plateau de support stable après une ancienne traverse ou fusion de branches ne peut donc plus devenir une pointe terminale. La décision reste locale à la branche suivie et ne dépend jamais de la présence de matière pièce ailleurs dans l'impression. Une croissance unique atteignant le seuil relatif configuré peut ouvrir le candidat. Une croissance plus faible ne peut l'ouvrir que si le centre sort aussi de l'enveloppe de déplacement normale **et** si la section parente, translatée sur le centre courant, ne conserve plus le recouvrement de forme minimal configuré. Le déplacement du centre seul ne constitue jamais une preuve de contact avec la pièce. Si la forme translatée et la trajectoire prédite de la branche restent cohérentes, le composant demeure support avec la raison diagnostique `support_motion_continuation`. Le candidat doit ensuite persister. Il est confirmé soit par une première section localement brutale par rapport à la pointe terminale, soit par une croissance cumulée depuis sa première couche au fil des couches natives suivantes. Une séquence stationnaire, décroissante, expliquée par le mouvement, expliquée par une fusion ou non confirmée est réintégrée à la branche support. Les supports inclinés, traverses, élargissements locaux et fusions temporaires restent ainsi des supports, tandis qu'une section de pièce qui grandit progressivement est reclassée depuis sa véritable première couche.

La confirmation montante d'un contact reste une preuve utile et enregistre toujours la paire d'échantillons tête terminale/contact, mais la frontière finale n'est engagée que lorsque la provenance pièce descendante atteint la même zone. Lorsque les deux passes concordent, la dernière section terminale valide et ses ancêtres support inférieurs restent support tandis que la région côté pièce devient pièce. Lorsqu'elles divergent, le cœur support courant issu de la passe 1 est protégé au lieu d'être érodé à partir du seul recouvrement avec un parent ; la matière pièce descendante ne peut donc pas fuir pixel par pixel au travers d'une continuation support. Inversement, lorsque toutes les provenances parentes actives enracinées dans le radeau se terminent sur l'arête inférieure et que la preuve pièce descendante atteint le composant supérieur, le cœur support est libéré et la région supérieure devient pièce. Le plan d'échantillonnage obligatoire conserve la frontière terminale/pièce ainsi validée afin que le mesh change de couleur au bon plan Z.

La passe descendante possède la provenance pièce. Elle part de la dernière couche native, suit le recouvrement exact ou une translation bornée qui maximise la matière pièce conservée et utilise le masque descendant immédiatement antérieur pour départager les translations à recouvrement égal. Lorsqu'une preuve support concurrence une translation à recouvrement égal, le déplacement pièce retenu maximise d'abord les pixels pièce conservés hors de ce cœur support, puis applique le départage historique et le déplacement le plus court. La lignée pièce ne peut ainsi pas sauter vers un support voisin. Dans un composant mixte, le cœur support courant issu de la passe 1 est conservé en entier tant qu'au moins un parent support actif continue ; reconstruire seulement l'intersection avec le parent immédiat éroderait progressivement les supports mobiles. Les pixels pièce forment donc le reste connexe situé hors de ce cœur protégé. Si tous les parents support actifs apportent une preuve terminale sur l'arête inférieure, le cœur peut s'arrêter à cet endroit et la provenance pièce descendante revendique la région supérieure. Cet arbitrage symétrique permet à la pièce de rester pièce lorsque des supports s'y attachent plus haut, sans permettre ni à la provenance support d'absorber la pièce, ni à la provenance pièce descendante d'absorber un support encore continu.

Le résultat d'analyse reste compact. Chaque couche conserve sa phase, les identifiants triés et uniques des composants entièrement support et des runs support projetés clairsemés uniquement pour les composants mixtes ; les masques décodés et les bitmaps sémantiques complets ne sont pas retenus dans l'index final. Chaque contact pièce validé enregistre deux échantillons source obligatoires : la dernière couche libre de la tête terminale et la première couche de contact. Pendant la construction du mesh, `materializeLayerSemantics()` réextrait uniquement une couche retenue et un `SupportMaskProvider` traduit le `Raft`, les composants entièrement `Support` et les runs support projetés vers le bit support existant. L'index final compact est produit après les deux parcours sémantiques indépendants. Chaque couche PWSZ native est désormais décodée et soumise à l'extraction des composants connexes une seule fois, pendant la fenêtre de préparation montante. Les `LayerDescription` compactes obtenues (bornes, surfaces, centres et runs natifs des composants) sont conservées pendant l'analyse des supports puis réutilisées directement par la passe sémantique descendante. Les deux parcours sémantiques restent strictement ordonnés entre couches natives. P4 utilise un seul ordonnanceur persistant à priorité, dimensionné par `render3d.workerCount`, pour tout le pipeline d'analyse des supports au lieu d'empiler un pool de préparation des couches et un exécuteur sémantique. Les tâches de faible priorité décodent et décrivent les prochaines couches natives, tandis que des lots prioritaires distribuent dynamiquement sur le même pool la classification indépendante des composants de la couche courante, la recherche de lignée pièce et la réconciliation descendante. Le coordinateur d'analyse conserve la création des nœuds, les mutations du graphe, les diagnostics et les commits sémantiques dans l'ordre original des composants, et les lots prioritaires passent toujours avant toute nouvelle tâche de préparation en attente. La fenêtre de préparation est bornée indépendamment à quatre masques natifs non consommés, même lorsque 8 ou 16 workers sémantiques sont sélectionnés ; les masques sont libérés dès que la comparaison du radeau n'en a plus besoin. Les sources déclarant les lectures concurrentes utilisent directement cette fenêtre bornée ; les autres sérialisent uniquement `loadMask()` tandis que l'ordonnanceur partagé continue de paralléliser le travail indépendant autour. Cette organisation supprime la surallocation de pools propre à l'analyseur, conserve des résultats sémantiques identiques à 1 et N workers et permet au coordinateur ordonné de se recouvrir avec la préparation sans faire croître la mémoire des masques retenus avec le nombre de workers sémantiques. La géométrie d'un appariement parent est calculée une seule fois par paire candidate et le tuple recouvrement/distance matière/distance centre est réutilisé pour la classification structurelle, les contrôles de contact et la propagation sémantique de la même couche au lieu de recalculer la même paire. Aucun bitmap sémantique complet n'est conservé entre les couches ni entre les passes. Lorsque l'analyse Supports est active, le mesher fusionne les échantillons obligatoires avec le stride fixe de l'aperçu rapide, puis trie et déduplique le plan. Une extrusion ne peut ainsi plus traverser une transition support/pièce ignorée tout en préservant les deux sémantiques dans un composant mixte. Lorsque l'analyse est désactivée, la liste obligatoire est ignorée et le stride classique reste strictement inchangé. Le résultat sémantique ne dépend ni de la taille des chunks de rendu ni du nombre de workers. La pression d'allocation de ce chemin CPU est en plus bornée par la réutilisation des buffers de scanlines et de préparation par couche, un index dense des racines du disjoint-set avec réservation exacte des runs par composant, des nœuds de graphe qui référencent les composants immuables déjà conservés au lieu de dupliquer leurs vecteurs de runs, ainsi que la normalisation et l'intersection sur place des masques de runs clairsemés en ne parcourant que les lignes touchées. P5 ajoute une couche d’accélération CPU portable sans changer cette sémantique : l’extraction des composants connexes lit directement les lignes de mots contigus du `BinaryMask` et, sur les CPU x86 disposant d’AVX2, saute quatre mots 64 bits nuls à la fois avant de retomber sur exactement la même extraction scalaire des runs. Les lignes clairsemées très fragmentées peuvent aussi être promues vers des bitsets 64 bits bornés uniquement lorsque l’empreinte en mots du bitset ne dépasse pas un quart du nombre d’intervalles canoniques ; `countSet`, les recouvrements translatés et les intersections de masques peuvent alors utiliser des opérations par mots, avec dispatch AVX2 pour l’intersection sur les CPU x86 compatibles et implémentation scalaire partout ailleurs. Les runs clairsemés restent autoritaires, le cache bitset reste local à la ligne et transitoire, et aucun bitmap sémantique complet n’est conservé entre les couches. Les sections dynamiques basse et haute sélectionnent le dernier échantillon sémantique retenu inférieur ou égal à la couche matière demandée et conservent donc les couleurs du mesh principal.

L'étiquette sémantique reste dans le bit 60 de `PackedSurfaceQuad` ; l'orientation de face reste dans les bits 61..63. La taille d'instance reste de 8 octets et la réduction structurelle de 15× est inchangée. Le provider sémantique est contrôlé avant maillage : ses dimensions doivent correspondre au masque matière et il ne peut introduire aucun pixel hors de la matière PWSZ native.

La case **Supports** commande ce chemin optionnel. Un fichier chargé initialement avec l'option désactivée utilise le chemin classique sans analyse. Son activation sur une telle scène relance les deux parcours sémantiques puis reconstruit le mesh d'aperçu. Sa désactivation après une construction analysée restaure immédiatement la couleur classique dans le shader ; l'index déjà calculé peut rester attaché à cette scène, mais les chargements suivants effectués avec l'option désactivée ignorent totalement l'analyse. Aucun mode ne masque, n'ajoute ou ne retire de matière.

La modal du viewport expose la phase active via `viewer.loadingPhase`. La progression est pondérée par le travail réel : toutes les couches natives sont décodées/décrites une seule fois pendant le parcours montant, les descriptions mises en cache sont ensuite consommées par le parcours sémantique descendant, puis le plan effectif de couches retenues est maillé pour l'aperçu. Les diagnostics runtime sont écrits dans `render3d.jsonl` avec le composant `support_analysis` et les événements `started`, `progress`, `phase_detected`, `completed`, `skipped`, `cancelled`, `failed` et `materialization_failed`. L'événement `started` indique `requested_workers` ainsi que `concurrent_mask_loads` selon le contrat de la source ; la fin de l'analyse indique le nombre d'échantillons sémantiques obligatoires. Les événements de début et de fin du mesher indiquent `base_sample_count`, `forced_semantic_sample_count` et `effective_sample_count`. L'événement de fin indique les runs de supports par composants entiers, les runs support projetés utilisés par les composants mixtes, `terminal_support_stops`, `expanding_model_contacts`, `maximum_model_expansion_ratio`, les pixels d'expansion rejetés ainsi que les contacts sans rétrécissement. `projected_contact_pixels` reste nul, car aucun pixel support n'est projeté dans une zone de contact pièce confirmée. Les logs contiennent uniquement des compteurs agrégés et les bornes de phases, jamais les composants ou runs individuels.

L'exécutable de diagnostic est disponible uniquement avec le core expérimental du viewer :

```bash
accloud_support_analysis_probe entree.pwsz --output analyse.json --workers 4
accloud_support_analysis_probe entree.pwsz --verify-materialization --workers 4
accloud_support_analysis_probe entree.pwsz \
  --dump-layer 101 --dump-ppm couche-101.ppm --downsample 8 --workers 4
```

`--workers N` accepte `1..16`, vaut `1` par défaut pour des diagnostics reproductibles et dimensionne le pool partagé de l'analyse des supports utilisé par la préparation bornée des couches, la classification montante des composants, la recherche de lignée pièce et la réconciliation descendante ; la fenêtre de préparation reste elle-même limitée à quatre masques natifs, les commits couche/composant restent ordonnés et la sortie sémantique doit rester identique. Le rapport JSON indique la valeur choisie dans `analysis_workers`. `--verify-materialization` relit toutes les couches natives et vérifie que l'index compact recrée exactement les totaux de runs radeau/support enregistrés. Les dumps PPM sont uniquement des diagnostics et ne constituent pas des ressources runtime.

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

La plage visible est contrôlée par un double curseur vertical fixé au bord droit du viewport. La couche maximale est indiquée au-dessus et la couche `1` au-dessous. Chaque poignée affiche dans un tooltip le numéro de couche courant au survol. La molette utilisée au-dessus du contrôle déplace uniquement la borne haute ; la borne basse reste modifiable exclusivement par glisser-déposer avec la souris. Les anciens champs numériques de saisie des bornes ne sont plus exposés.

Le titre du dialogue reste générique (`Vue 3D`). L’overlay du viewport affiche, dans cet ordre, le nom de la machine, puis `nom.pwsz · N couches`, puis le rappel des commandes. Il n’affiche ni nombre de workers, ni mode d’échantillonnage, ni nombre de chunks ou de triangles, ni bornes de couches visibles, ni valeurs Z. Une case compacte **Supports** est ancrée dans l'angle inférieur gauche du viewport et active la couleur estimée des supports. Dans l’en-tête, **Réinitialiser la vue** est placé immédiatement à gauche de **Plein écran** ; **Quitter le plein écran** restaure la taille de travail. Le pied du dialogue place **Imprimer** immédiatement à gauche de **Fermer**. **Imprimer** ferme le viewer puis déclenche exactement le même flux de configuration d’impression distante que l’action **Imprimer** du listing des fichiers cloud.
Pendant toute la reconstruction initiale, une modal limitée au viewport couvre uniquement la zone du viewer et bloque ses interactions. Une barre de progression déterminée est centrée dans cette modal et suit `viewer.progress` de 0 à 100 %. La modal reste affichée tant que `viewer.loading` est vrai, même si des chunks partiels sont déjà disponibles, puis disparaît à la fin de la construction ou en cas d’erreur. L’en-tête et le pied du dialogue restent hors de cette modal.

Lorsqu’une borne coupe le document, un worker dédié décode uniquement le masque de la couche frontière et construit une surface compacte sur le plan Z correspondant. La section basse porte une normale Z négative et la section haute une normale Z positive. La matière vient exactement du dernier masque retenu inférieur ou égal à la couche demandée : le plan classique suit `layerStride=2`, tandis que le plan Supports inclut aussi les couches terminales et de contact obligatoires. Une pièce pleine produit une section pleine, une coque conserve sa cavité, et des supports ou îlots séparés restent séparés. Les faces horizontales déjà présentes sur le plan de clipping sont supprimées pendant la passe du mesh principal afin d’éviter leur superposition avec le bouchon dynamique.

Le changement de plage est transactionnel. Le renderer conserve la plage et les sections actuellement affichées pendant la construction de la nouvelle demande. La nouvelle paire « plans de clipping + surfaces de section » est uploadée dans un buffer de préparation, puis remplacée dans une même frame. Une section ne peut donc jamais disparaître avant que sa remplaçante soit prête. Les demandes intermédiaires rendues obsolètes par un déplacement rapide du curseur sont abandonnées sans modifier l’état affiché.

Les sections XY décodées sont conservées dans un cache CPU LRU compact, indépendant de l’orientation et du plan Z. Chaque rectangle occupe 8 octets. Le cache est borné à 64 Mio et 2 048 couches ; seules les une ou deux sections actives résident sur le GPU. Une même couche peut ainsi être réutilisée immédiatement lors d’un retour du curseur sans dupliquer les faces haute et basse.

## Navigation

La page Qt Quick mappe :

- glisser gauche : orbite ;
- glisser droit, milieu ou Maj : pan dans le plan de l’écran ;
- molette : zoom ;
- action de reset : cadrage sur la bounding box complète chargée.

Le pan utilise les axes droite/haut réels de la caméra. Le déplacement reste orthogonal à la direction de vue : un glisser horizontal ou vertical ne peut donc pas déplacer simultanément la pièce en profondeur.

La caméra permet d’inspecter la pièce sur toutes ses faces. Le shader applique un éclairage directionnel double face simple pour rendre lisibles les parois internes lors d’une coupe Z.

## Décision backend

Le premier backend desktop utilise `QQuickFramebufferObject` et Qt OpenGL : API publique, compatible avec les versions Qt 6 visées par le projet et cohérente avec la frontière `render3d/gl`. Le bootstrap force le scene graph Qt Quick sur OpenGL lorsque `ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER` est activé.

Ce choix n’est pas une politique de rendu production. Un backend QRhi pourra le remplacer lorsque la version mineure Qt supportée sera figée et validée. Cloud, MQTT et les builds QML normaux restent indépendants du viewer.

## Performance et limites

Garde-fous implémentés :

- masques bit-packed ;
- maillage séquentiel avec couches voisines ;
- rendu unique avec un `layerStride=2` de base ; lorsque l'analyse Supports est active, seules les couches obligatoires de transition tête/contact complètent ce plan ;
- conservation des bornes première/dernière exactes et de l’étendue Z d’origine malgré l’échantillonnage ;
- callbacks de chunks streaming ;
- file d’upload bornée : 8 chunks / 256 Mio en attente par défaut ;
- représentation GPU de 8 octets par surface au lieu de 120 octets équivalents ;
- rendu instancié sans buffers vertices/index dupliqués ;
- budget de résidence GPU de 2 Gio vérifié avant chaque allocation ;
- arrêt contrôlé, vidage de génération et message UI si le budget ou OpenGL refuse un chunk ;
- pas de conservation des rasters gris denses par défaut ;
- isolation du thread GUI ;
- annulation lors d’un rechargement ou de la destruction du viewer.

Le rendu à une couche sur deux est une approximation d’affichage assumée : le viewer sert à reconnaître et inspecter rapidement une pièce, notamment lorsqu’un fichier tiers est récupéré. Un support ou détail présent uniquement sur une couche source ignorée peut ne pas apparaître. Aucun mode détaillé n’est exposé et le PWSZ comme les données d’impression ne sont jamais modifiés.

### Diagnostics Render3D dédiés

Le viewer écrit des événements JSONL structurés avec la source `render3d`. Le logger crée donc `render3d.jsonl` dans le répertoire de logs ACM configuré (ou `ACCLOUD_LOG_DIR`). Les événements couvrent l’ouverture de l’archive, les dimensions source, le pas d’échantillonnage, la progression par tranche de 10 %, le nombre de surfaces, les octets compacts, l'équivalent legacy, le ratio de compression, l’attente de la file d’upload, les octets GPU résidents, le budget, les durées, annulations et erreurs. `gpu.compact_chunk_uploaded`, `gpu.cut_surface_uploaded`, `gpu.budget_exceeded` et `gpu.scene_reset` permettent de vérifier les allocations et libérations. `cut_surface.boundary_built` et `cut_surface.build_completed` décrivent la couche frontière décodée, le plan et le nombre de surfaces compactes. Le journal inclut aussi `mesher.worker_completed`. Les chemins temporaires complets et les URL signées ne sont pas journalisés.

Limites connues :

- les chunks compacts restent en GPU pour le document chargé ;
- pas encore d’éviction GPU, LOD ou simplification ;
- le budget de 2 Gio refuse proprement un modèle compact pathologique au lieu de laisser le pilote interrompre le processus ;
- les modèles exacts peuvent conserver un très grand nombre de surfaces sur des supports denses ;
- la couleur des supports reste heuristique, car le masque d'exposition PWSZ ne contient aucune étiquette sémantique exacte du slicer ; la matière fusionnée ou ambiguë conserve volontairement la couleur de la pièce ;
- backend desktop OpenGL uniquement à cette étape expérimentale.

## Validation

Le gate core hors ligne inclut :

```text
accloud_experimental_viewer_architecture
accloud_experimental_viewer_scaffold
accloud_pw0_decode
accloud_pwsz_reader
accloud_layer_stack_mesher
accloud_support_analyzer
accloud_support_analysis_diagnostics
accloud_render3d_worker_benchmark_selftest
accloud_render_pipeline
accloud_viewer_controls
```

`accloud_support_analyzer` valide les préfixes de radeau en plaque, grille et semelles avec une variation d'antialias bornée, les phases supports seuls et mixtes, une forêt de supports enracinée sans fusion structurelle, les divisions de branches, les croisillons natifs aux couches, l'indépendance vis-à-vis des diamètres light/normal/heavy, les petites premières sections de pièce et leur croissance progressive après une pointe rétrécie, les continuations inclinées de type Torus qui doivent rester support, les plateaux stables après un ancien rétrécissement qui ne doivent pas rouvrir de contact, les fusions support multi-parents qui restent support, le rejet des faux parents à travers les zones vides d'une boîte englobante, la décision locale limitée au parent structurel, l'invariance sémantique face à la hauteur de couche PWSZ, les supports démarrant sur la pièce, la continuité malgré un recouvrement temporaire avec une pièce établie, le rejet d'un faux contact interne, la confirmation différée et le repositionnement rétroactif de la frontière pour le motif Beetle 632-637, la lignée pièce persistante après un contact confirmé, la continuité pièce tenant compte d'un mouvement borné sur plusieurs déplacements raster consécutifs, la prédiction indépendante de l'empreinte support dans un composant mixte, la séparation sémantique d'un composant raster mixte pièce/support, le rejet des coques creuses, les index compacts déterministes, l'annulation et la rematérialisation exacte couche par couche.

`accloud_support_analysis_diagnostics` valide le schéma du bundle complet, trois panneaux PNG visibles plus une carte de sélection PNG alignée et un JSON chargé à la demande par couche, les identifiants de sélection, la géométrie de recadrage, les comparaisons de surfaces, les sémantiques choisies, les codes de raison stables et les explications lisibles des décisions.

`accloud_render_pipeline` valide la comptabilité compacte de la file bornée, le format 8 octets, l'encodage du bit sémantique, l'expansion géométrique exacte, le ratio 15× inchangé, le budget GPU simulé, une pièce synthétique complète de 250 mm, la consommation streaming, l’annulation, la sélection des chunks, les plans Z exacts, les sections pleines, les cavités conservées, les îlots de supports séparés, la classification conservatrice support/fusion/radeau, la conservation de la sémantique dans le cache de sections de 8 octets et son éviction LRU bornée.

Validation desktop obligatoire sur un poste avec dépendances Qt natives :

```bash
cmake --preset experimental-viewer-qt
cmake --build --preset experimental-viewer-qt --clean-first
ctest --preset experimental-viewer-qt --output-on-failure
```

Les PWSZ réels peuvent servir d’entrées locales, mais ne deviennent pas des fixtures distribuées sans droit explicite de redistribution.

## Décision

Le viewer possède désormais un chemin de développement de bout en bout depuis chaque ligne de fichier PWSZ vers un mesh 3D navigable et filtrable par plage, avec surfaces GPU compactes et budget d'allocation contrôlé. La production reste désactivée. Sa préparation exige encore la validation locale sur de grands PWSZ et le LOD/éviction éventuels.

## Bundle complet de diagnostic de l’analyse des supports

Le probe de développement peut exporter la trace complète des décisions pour un fichier PWSZ local :

```bash
./build/experimental-viewer-core/accloud_support_analysis_probe \
  ../pwsz/Beetle.pwsz \
  --bundle /tmp/beetle-support-analysis \
  --verify-materialization
```

Le bundle est réservé au diagnostic de développement et contient :

```text
manifest.json                 index couches/images et métadonnées source
summary.json                  résumé global compact utilisé par l’interface
analysis.json                 analyse complète des couches, nœuds et arêtes
decisions.json                toutes les décisions sémantiques et comparaisons
images/layer_XXXXXX_raw.png       masque d’exposition brut
images/layer_XXXXXX_semantic.png  résultat pièce/support/radeau
images/layer_XXXXXX_nodes.png     décisions et identifiants de nœuds rendus
images/layer_XXXXXX_pick.png      carte RGB24 invisible de sélection des composants
layers/layer_XXXXXX.json          décisions et géométrie des images chargées à la demande
```

`decisions.json` enregistre les surfaces courante et parente, leur ratio, le recouvrement brut et aligné sur les centres, la distance matière exacte, les pixels ajoutés et retirés après alignement, le mouvement prédit de la branche, le résidu de mouvement, tous les identifiants et recouvrements des parents support correspondants, les nombres et taux de parents conservés, les preuves de diminution de la pointe terminale, l’état avant et après la décision, la sémantique choisie, un `reason_code` stable et un champ `why` lisible. Les décisions de composants mixtes exposent également `model_lineage_pixels`, `model_lineage_shift_pixels` et `model_lineage_continued`, ainsi que `reverse_model_evidence_pixels`, `reverse_support_core_pixels`, `final_support_pixels`, `final_model_pixels`, `reverse_model_lineage_continued`, `reverse_model_seed` et `bidirectional_conflict`. Ces champs rendent auditables les preuves indépendantes bas-vers-haut/haut-vers-bas et leur réconciliation finale. La capture de la trace est optionnelle et n’est pas activée par le viewer normal. Chaque couche source exporte trois images pleine largeur distinctes : masque brut, résultat sémantique et décisions. L’image des décisions affiche le véritable `node_id` sur les décisions pièce, contact, composant mixte et support non standard ; les continuations support ordinaires restent sans étiquette pour préserver la lisibilité des grilles denses. Une quatrième image invisible encode `component_id + 1` en RGB24 pour chaque pixel diagnostic sous-échantillonné. Le JSON de couche enregistre les limites du recadrage, les dimensions, les chemins des panneaux et le même `selection_id` local à la couche.

Avec `ACCLOUD_DEBUG` et le viewer expérimental actifs, `MainWindow.qml` expose l’onglet **Analyse des supports**. L’utilisateur sélectionne une pièce `.pwsz` locale, puis `SupportAnalysisBridge` lance le probe de manière asynchrone à côté de l’exécutable desktop. L’onglet est divisé entre le viewer 3D synchronisé, une pile défilable verticalement des trois images diagnostiques et un inspecteur JSON inférieur. Des boutons donnent un accès direct aux vues brute, sémantique et identifiants, et la barre de défilement verticale reste visible. Un clic sur l’image sémantique ou l’image des identifiants lit la carte de sélection invisible, choisit le composant exact sous le curseur, encadre sa zone et remplace la liste des décisions par l’objet JSON correspondant. Un changement de plage 3D ou de couche efface la sélection et actualise les trois images ainsi que le JSON de couche. Les gros fichiers globaux ne sont pas parsés de manière eager dans QML : le bridge charge uniquement `summary.json` et le fichier `layers/layer_XXXXXX.json` sélectionné. L’onglet et le bridge sont absents des ressources de production.

