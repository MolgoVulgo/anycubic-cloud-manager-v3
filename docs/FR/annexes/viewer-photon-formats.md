# Formats Photon/PWMB et viewer — annexe technique

> Statut : EXPÉRIMENTAL / PARTIEL. Cette annexe ne déclare pas un viewer prêt pour la production.


Statut : `PARTIEL` implémentation, `SPEC` contrat cible.

## Position produit

Le viewer Photon/PWMB est une trajectoire, pas le flux principal terminé. Le dépôt contient les contrats de domaine ainsi que des drivers, composants de décodage, jobs, futurs caches RAM/disque et un squelette `render3d` encore placeholders. Ces fichiers `.cpp` de scaffold sont exclus des builds normaux et de `accloud_cli`.

Le seul opt-in supporté est `ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER=ON`, exposé par le preset `experimental-viewer-core`. Il construit la cible objet isolée `accloud_experimental_viewer` et un smoke test de compilation. Il n'expose aucun contrôle viewer dans le QML de production, ne décode pas de fichiers réels et ne fournit aucun workflow rendu/navigation/export.

## Familles étudiées

`PWMB`, `PWS`, `PHZ`, `PHOTONS`, `PWSZ`. Chaque format doit passer par un driver exposant metadata, previews, index layers, niveaux de décodage et diagnostics.

## Parsing PWMB

Tables-first : `FileMark -> table addresses -> section table -> sections -> layer index -> layer decode`. Valider signature/version, gérer versions, tolérer inconnus, ne pas supposer adresses triées, borner les lectures, fallback legacy seulement si nécessaire.

## Sections versionnées

`>=515 LayerImageColorTable`, `>=516 Extra/Machine`, `>=517 Software/Model`, `>=518 SubLayerDefinition/Preview2`. Une section optionnelle absente produit warning, pas crash.

## Décodage layers

`pw0Img` : mots 16-bit big-endian, high nibble `color_index`, low 12 bits `run_len`, `run_len==0` invalide, clamp dernier run possible, trailing ignoré avec diagnostic.

`pwsImg` : byte RLE, bit 7 exposé, bits 0..6 répétitions, conventions `reps` et `reps+1` sélectionnées par dry-run déterministe, AA accumulé puis projeté en `uint8`.

## Vérité géométrique

`matière = tout pixel non-noir`. Seuil 0. Les pixels gris AA sont matière pour géométrie, mesures et exports.

`mask_truth` sert au viewer, dimensions, aire, volume, exports, goldens. `mask_analysis` sert uniquement aux heuristiques. La géométrie principale ne dépend pas des contours.

## Mapping raster/monde

Raster flat row-major `W*H`, `x=i%W`, `y=i//W`, origine haut-gauche, pitch XY `PixelSizeUm/1000`, pitch Z `LayerHeight`, Y monde inversé.

## Performance

Metadata/previews rapides, index layers avant full decode, batch decode/mask, fenêtre glissante RAM, LRU disque, annulation, stages progress `read/decode/mask/build/upload/draw/cache/done`.

## Goldens

Nonzero count, bbox px, checksum échantillon, layer count/dimensions, orientation flip/mirror.

## Décision

Le cloud manager ne dépend pas du viewer complet. Les presets default, local-full et production gardent `ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER=OFF`, et le QML de production ne contient aucun réglage ni action viewer. Le viewer avance uniquement via la cible isolée, des contrats stricts et des tests diagnostics ; il reste `EXPÉRIMENTAL` jusqu’à fermeture decode/render/navigation/export.
