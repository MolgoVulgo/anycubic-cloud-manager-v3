# Annexe — Écrans client cloud

Statut : `IMPLEMENTE` pour les workflows cloud/imprimantes actifs, `EXPERIMENTAL` pour le viewer PWSZ et les diagnostics de développement.

## Fenêtre principale

`qrc:/qml/MainWindow.qml` est le shell desktop. La navigation principale expose **Fichiers**, **Imprimantes** et **MQTT**. **Logs** n'est disponible que lorsque le build expose les outils de logs debug ; sinon l'onglet est désactivé. **Analyse des supports** n'est chargée que lorsque `ACCLOUD_DEBUG` et `ACCLOUD_ENABLE_EXPERIMENTAL_VIEWER` sont tous deux actifs. Les Paramètres passent par le menu de l'application et non par un onglet principal.

## Fichiers

La page Fichiers est l'espace de travail des fichiers cloud. Elle fournit rafraîchissement, état cloud/cache déterministe, sélection, suppression multiple, téléchargement, upload standard, impression directe, impression normale depuis le cloud et détails. `CloudFilesWorkflowBridge` porte la séquence de suppression multiple ; `PrintWorkflowBridge` porte l'orchestration des impressions distante/directe ; QML transmet les intentions utilisateur et affiche uniquement les progrès/résultats sémantiques.

Les lignes PWSZ compatibles exposent aussi une action **3D** dans les builds de développement où le viewer expérimental est actif. Le fichier est téléchargé vers un chemin local temporaire avant l'ouverture de la modal. La complétion/remplacement des previews PWSZ reste un workflow cloud gardé distinct et ne réécrit jamais la source locale avant le succès du remplacement cloud.

## Détails d'un fichier

La modal privilégie les métadonnées utiles à l'impression, un aperçu résolu localement, le format, la taille, la date d'upload, les informations machine/matériau/couches, l'exposition, la consommation et les imprimantes compatibles. Les détails techniques optionnels sont contrôlés par `ui.cloudFiles.showAdvancedDetails`. Les métadonnées cloud brutes restent réservées au développement. Les URL signées et chemins locaux du cache de miniatures ne sont pas affichés.

## Upload et impression directe

**Ajouter au cloud** enregistre le fichier dans le cloud Anycubic et s'arrête là. **Impression directe** conserve le fichier local comme entrée d'opération, vérifie la compatibilité, l'upload puis envoie l'ordre imprimante lorsque le fichier cloud est prêt. La politique de nettoyage appartient à l'opération directe et est coordonnée par `PrintWorkflowBridge` ; QML n'implémente ni la persistance, ni la réconciliation des tâches, ni l'ordre des suppressions locale/cloud.

## Viewer 3D

Le viewer PWSZ est un workflow de développement expérimental, pas un dialogue draft. L'overlay du viewport affiche dans l'ordre le nom de l'imprimante, le nom du fichier avec le nombre de couches, puis l'aide de navigation. Les actions d'en-tête sont **Réinitialiser la vue** et **Plein écran** ; les actions de pied de page sont **Imprimer** et **Fermer**. Imprimer ferme le viewer puis transmet la même intention d'impression cloud que la liste Fichiers.

La preview utilise un chemin de mesh fixe à une couche sur deux ; aucun mode détail complet n'existe dans l'UI. La case optionnelle **Supports** exécute deux passes sémantiques sur chaque couche native, réconcilie les indices support/pièce, injecte les `forcedSampleLayers` validées aux transitions support/pièce, puis construit le mesh stride-2. La géométrie provient toujours du masque d'exposition PWSZ d'origine.

## Imprimantes

La page Imprimantes affiche la synthèse du parc, les onglets limités aux noms, l'état et les informations de l'imprimante sélectionnée, les fichiers locaux et les tâches récentes. Les tâches récentes restent textuelles. L'état live dérivé de MQTT est projeté via le store temps réel normalisé ; la page ne parse pas les payloads MQTT bruts. Les actions sur fichiers locaux sont désactivées lorsque l'imprimante sélectionnée ne peut pas les accepter.

## MQTT

La page MQTT est une UI de diagnostic/exploitation au-dessus du bridge MQTT C++ et du store temps réel. Le routing cœur, la corrélation des ordres et les mises à jour du store continuent lorsque la page est masquée ; le formatage diagnostic et la publication de la queue brute ne sont activés que lorsque la page est visible.

## Logs

La page Logs est réservée au debug. Elle lit des snapshots JSONL structurés bornés lorsqu'elle est active et ne doit pas devenir une dépendance de production.

## Espace Analyse des supports

Lorsque les outils debug et le viewer sont actifs, l'espace **Analyse des supports** lance le probe externe via `SupportAnalysisBridge`. Il combine viewer 3D, diagnostics par couche et JSON par couche chargé à la demande sans déplacer l'analyse, les accès filesystem ou le parsing lourd dans QML.

## Session et paramètres

Les paramètres de session importent un HAR vers la session normalisée sans afficher les tokens bruts. Le menu Paramètres porte aussi thème/langue, le mode MQTT Slicer figé, les préférences de complétion des previews PWSZ, le nettoyage sur échec d'impression directe, les détails techniques des fichiers cloud, ainsi que le nombre de workers et la palette du viewer lorsqu'il est disponible.
