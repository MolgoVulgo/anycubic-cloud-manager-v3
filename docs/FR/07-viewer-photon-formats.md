# Viewer Photon et formats

Statut : `PARTIEL` implémentation, `SPEC` contrat cible.

## Position produit

Le viewer Photon/PWMB est une trajectoire, pas le flux principal terminé. Le dépôt contient drivers, décodage PWMB/PWS, previews, jobs/cache et squelette render3d, mais pas encore un viewer complet.

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

Le cloud manager ne dépend pas du viewer complet. Le viewer avance par contrats stricts et tests diagnostics ; il reste `PARTIEL` jusqu’à fermeture decode/render/navigation/export.
