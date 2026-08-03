# Photon / Viewer / Formats — documentation unifiée

Statut documentaire : `PARTIEL`

> Cette source technique complète l’annexe active `docs/FR/annexes/viewer-photon-formats.md`. En cas d’écart, l’annexe active et le code testé prévalent.

## 1. Position réelle

Le viewer reste expérimental pour la production, mais il est activé dans les presets desktop de développement. Deux gates séparent le cœur portable du runtime desktop :

```text
experimental-viewer-core
-> PWSZ, PW0, masque, mesh, chunks, range, caméra, upload queue
-> sans Qt

experimental-viewer-qt
-> hérite de local-full
-> Qt Quick + Qt OpenGL
-> item VolumeViewer et dialogue QML conditionnel ouvert depuis chaque ligne PWSZ
```

Le QML de la page et du dialogue est empaqueté dans `resources.qrc`. Le bouton 3D reste visible sur les lignes PWSZ. Lorsque `ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER=OFF`, il est désactivé, le dialogue n’est pas chargé et aucun type `Accloud.Render3D` n’est enregistré.

## 2. Lecture et décodage

Le lecteur PWSZ supporte ZIP mono-disque Store/Deflate, refuse ZIP64, chiffrement, chemins dangereux et doublons, puis lit :

```text
anycubic_photon_resins.pwsp
layers_controller.conf
layer_images/layer_<index>.pw0Img
```

Le RLE `pw0Img` est mixte : deux octets pour 0/15, un octet pour 1..14. L’antialiasing est facultatif et détecté depuis les rasters. La vérité matière reste tout pixel non noir. Les valeurs intermédiaires peuvent être conservées pour analyse, mais le masque géométrique principal est bit-packed.

## 3. Maillage par empilement

Le mesh est produit à partir des transitions matière/vide XY et Z. Cette méthode conserve :

- surfaces extérieures ;
- parois de cavités ;
- trous traversants ;
- supports et pointes ;
- radeau ;
- composants indépendants.

Les couches sont traitées séquentiellement avec leurs voisines. Les faces coplanaires sont fusionnées en rectangles/runs et les murs identiques sont prolongés sur les couches successives d’un chunk.

`LayerStackMesher` expose désormais des callbacks :

```text
consumeChunk(MeshChunk&&)
progress(completed, total)
isCancelled()
```

Le mode callback évite de conserver une seconde copie de tous les chunks dans le résultat.

`MeshBuildOptions::layerStride` contrôle l’échantillonnage Z de l’aperçu. Le viewer utilise `2` par défaut : couches `0, 2, 4, ...` plus la dernière couche exacte. Chaque masque échantillonné est extrudé jusqu’au prochain échantillon, ce qui conserve la hauteur Z totale et les bornes de plage. Le mode détaillé remet `layerStride=1`. Cet échantillonnage est une approximation visuelle : un détail présent uniquement sur une couche ignorée peut disparaître en mode rapide.

## 4. Upload et rendu

Le chemin desktop utilise :

```text
std::jthread coordinateur
-> pool de 1 à 16 workers de chunks (4 par défaut)
-> PwszArchiveReader concurrent
-> LayerStackMesher
-> UploadQueue bornée
-> thread de rendu QQuickFramebufferObject
-> QOpenGLBuffer vertex/index par chunk
-> shader lumière + clipping Z
```

Le nombre de workers est lu depuis `render3d.workerCount`, borné de 1 à 16 et fixé à 4 par défaut. Les chunks sont distribués dynamiquement ; chaque tâche relit au maximum les masques voisins nécessaires à ses frontières. La file accepte au maximum 8 chunks et 256 Mio en attente par défaut. Le worker applique une back-pressure légère lorsque la file est pleine et s’arrête sur demande lors d’un rechargement ou de la destruction de l’item.

Le backend OpenGL est activé dans `default`, `dev-debug`, `local-full` et `experimental-viewer-qt`. `QQuickWindow::setGraphicsApi(OpenGL)` n’est pas appelé dans `prod` ni `protected-core`.

## 5. Plage dynamique

La plage utilisateur reste inclusive et base 1. Le renderer la convertit en plans Z exacts :

```text
firstLayer = 415
lastLayer  = 1021
pitchZ     = 0,05

minimumZ = (415 - 1) * 0,05 = 20,70 mm
maximumZ = 1021 * 0,05      = 51,05 mm
```

Les chunks non intersectés ne sont pas dessinés. Les triangles des chunks frontières sont clipés dans le fragment shader, ce qui garantit une plage exacte sans remeshing à chaque mouvement du `RangeSlider`.

Le mode UI actuel est une coupe ouverte. Les bouchons dynamiques pour une coupe fermée restent un incrément distinct.

## 6. Navigation Qt Quick

`VolumeViewerPage.qml` fournit :

- champ de chemin local PWSZ ;
- chargement asynchrone et progression ;
- affichage machine, couches, chunks et triangles ;
- orbite par glisser gauche ;
- pan par glisser droit/milieu ou Maj ;
- zoom molette ;
- reset/cadrage ;
- choix entre aperçu rapide « 1 couche sur 2 » et détail complet ;
- `RangeSlider` à deux poignées ;
- SpinBox début/fin et valeurs Z calculées.

Le thread GUI ne décode aucune couche et ne construit aucun mesh.

## 7. Journal de diagnostic 3D

Les phases archive, décodage/maillage, back-pressure de la file et upload GPU produisent des événements structurés avec `source=render3d`. Le logger crée le sink dédié `render3d.jsonl` dans le répertoire de logs ACM. Les événements incluent notamment :

```text
archive.open_started / archive.open_completed / archive.open_failed
mesher.build_started / build_progress / chunk_ready / build_completed
gpu.shader_ready / chunk_uploaded / buffer_allocation_failed
```

Les chemins temporaires complets et les URL signées ne sont pas journalisés.

## 8. Limites

- backend OpenGL seulement ;
- pas d’éviction des buffers déjà uploadés ;
- pas de LOD/simplification ;
- pas de bouchons de coupe dynamiques ;
- pas de séparation sémantique modèle/support ;
- risque de volume de triangles élevé sur supports complexes ;
- l’aperçu rapide peut ignorer un détail limité à une seule couche source ; le mode détaillé reste disponible.

## 9. Tests

Gate agent hors Qt :

```text
accloud_experimental_viewer_architecture
accloud_experimental_viewer_scaffold
accloud_pw0_decode
accloud_pwsz_reader
accloud_layer_stack_mesher
accloud_render_pipeline
accloud_viewer_controls
```

Gate local obligatoire :

```bash
cmake --preset experimental-viewer-qt
cmake --build --preset experimental-viewer-qt --clean-first
ctest --preset experimental-viewer-qt --output-on-failure
```

Les fichiers PWSZ utilisateurs restent hors patch et hors fixtures distribuées.
