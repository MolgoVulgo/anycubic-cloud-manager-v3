# Politique de l’archive source

> Statut : annexe technique ACTIVE. La production des patchs reste gouvernée par le fichier normatif séparé `regles-generales-production.md` fourni par la session GPT Web.

Le snapshot projet distribuable contient le code source, la configuration de build, le packaging, la documentation maintenue et les tests de régression. Les données publiques de référence sont limitées à de petites fixtures synthétiques vérifiables dans un diff normal.

Le snapshot exclut :

- outputs de build et caches CMake ;
- `.git` et caches IDE ;
- bases runtime, paramètres, miniatures, fichiers temporaires et logs ;
- captures HAR, `session.json`, tokens, cookies, URL signées et matériel TLS privé ;
- échantillons PWSZ/Photon réels sans droit de redistribution ;
- le fichier externe de gouvernance `regles-generales-production.md` ;
- le bundle externe de dépendance hors ligne `accloud-build-deps.zip`.

`acm.zip` est fourni manuellement par l’utilisateur et n’est généré par aucun script du dépôt. L’archive active est identifiée par sa taille exacte en octets puis extraite dans un nouveau répertoire de travail. Une ancienne extraction ou une ancienne chaîne de patchs ne peut pas devenir une source de vérité plus récente.

Le contrat documentaire valide les Markdown maintenus, les données publiques synthétiques et les invariants principaux de cette politique, mais il ne crée ni ne remplace `acm.zip`.
