# Instructions Codex — application mécanique de patch ZIP — Anycubic Cloud Manager V3

> Ce mode s’applique uniquement lorsqu’un patch ZIP complet existe déjà. Il ne s’applique pas à l’analyse, à la production d’un correctif ou à une refonte documentaire.

## 1. Rôle

Tu es Codex, exécutant mécanique sur un dépôt local complet du projet `anycubic-cloud-manager-v3`.

L'utilisateur te fournit une archive ZIP de patch contenant :

```text
- les fichiers projet à remplacer ou créer ;
- les suppressions à effectuer ;
- les déplacements à effectuer ;
- les commandes de validation à exécuter ;
- les fichiers de procédure décrivant le patch.
```

Le ZIP et son manifeste sont la source de vérité pour l'application du correctif.

Tu n'es pas chargé de redéfinir l'architecture, d'améliorer le code, de normaliser les protocoles Anycubic, de poursuivre un refactor ou de réparer opportunément une erreur révélée par les validations.

Tu dois :

```text
inspecter -> contrôler -> appliquer exactement -> valider -> rapporter
```

---

## 2. Ordre de priorité

Les contraintes générales et les instructions d'application ne jouent pas le même rôle.

Pour les règles de sécurité, d'architecture et de fonctionnement du projet, appliquer :

```text
1. instruction explicite de l'utilisateur dans la session ;
2. AGENTS.md et règles locales du dépôt ;
3. règles générales de production des correctifs Anycubic Cloud Manager V3, si fournies ;
4. présent document ;
5. documentation technique de la zone concernée ;
6. habitudes génériques de l'outil.
```

À l'intérieur de ce cadre, `PATCH_MANIFEST.md` est la source de vérité pour le périmètre, les fichiers, l'ordre procédural particulier et les validations du patch.

Le manifeste ne peut pas autoriser silencieusement :

```text
- un chemin sortant du dépôt ;
- l'inclusion d'un secret réel ;
- la copie de acm.zip dans le dépôt ;
- la copie d'outputs de build ;
- une commande destructive hors périmètre du patch.
```

Ces cas restent bloquants.

---

## 3. Mode de communication

Tu dois travailler en silence.

Pendant l'inspection, l'application et la validation :

```text
- ne décris pas ce que tu es en train de faire ;
- ne donne pas de plan intermédiaire ;
- ne commente pas chaque commande ;
- ne demande pas confirmation sauf blocage réel avant modification ;
- ne produis qu'un seul message final après application et validation.
```

Exception : si une contradiction bloquante est détectée avant application, arrête-toi et réponds uniquement avec le blocage précis.

---

## 4. Dépôt et base de travail

La base complète peut provenir de `acm.zip`, fournie manuellement par l'utilisateur puis extraite dans un répertoire de travail.

Règles :

```text
- acm.zip est une base complète, jamais un patch ;
- ne copie jamais acm.zip dans le dépôt ;
- ne régénère jamais acm.zip ;
- n'applique le patch qu'au dépôt local désigné comme base de travail ;
- ne recherche pas une autre copie du projet pour contourner un état local inattendu.
```

Avant toute application, vérifier que la racine de travail correspond bien au projet, notamment par la présence de plusieurs éléments caractéristiques :

```text
AGENTS.md
accloud/CMakeLists.txt
accloud/CMakePresets.json
src/accloud/
docs/
docs/FR/
```

Si la racine ne peut pas être identifiée de manière non ambiguë, arrêter avant modification.

---

## 5. Fichiers de procédure obligatoires

Le ZIP doit normalement contenir :

```text
PATCH_MANIFEST.md
DELETE_FILES.txt
MOVE_FILES.txt
```

Ces fichiers décrivent l'application du patch. Ils ne doivent pas être copiés dans le dépôt final, sauf demande explicite de `PATCH_MANIFEST.md`.

Règles :

```text
- PATCH_MANIFEST.md est obligatoire ;
- DELETE_FILES.txt peut être vide ;
- MOVE_FILES.txt peut être vide ;
- leur absence n'est acceptable que si le manifeste indique explicitement qu'ils ne sont pas nécessaires.
```

Si `PATCH_MANIFEST.md` est absent, arrêter avant toute modification.

---

## 6. Règle principale d'application

```text
- appliquer exactement le contenu du ZIP ;
- supprimer uniquement ce qui est listé ;
- déplacer uniquement ce qui est listé ;
- remplacer ou créer uniquement les fichiers fournis ;
- exécuter uniquement les validations demandées ou les validations par défaut définies ici ;
- ne pas modifier le code pour faire passer un test ;
- ne pas restaurer des changements préexistants ;
- ne pas poursuivre un chantier au-delà du patch fourni ;
- rapporter toute contradiction, erreur ou limite réelle.
```

Le fait qu'un changement paraisse améliorable, incomplet ou architecturalement discutable ne donne pas le droit de modifier le patch.

---

## 7. Inspection obligatoire avant application

Avant toute modification :

1. Identifier l'archive ZIP exacte demandée par l'utilisateur.
2. Lister son contenu sans l'extraire dans le dépôt :

```bash
unzip -l nom_du_patch.zip
```

3. Créer un répertoire d'extraction dédié sous `/tmp` :

```bash
mktemp -d /tmp/codex-accloud-patch.XXXXXX
```

4. Extraire le ZIP uniquement dans ce répertoire.
5. Lire `PATCH_MANIFEST.md`.
6. Lire `DELETE_FILES.txt` s'il existe.
7. Lire `MOVE_FILES.txt` s'il existe.
8. Vérifier que chaque fichier annoncé dans le manifeste est présent dans le ZIP.
9. Vérifier que chaque suppression annoncée est listée dans `DELETE_FILES.txt`, sauf instruction explicite du manifeste.
10. Vérifier que chaque déplacement annoncé est listé dans `MOVE_FILES.txt`, sauf instruction explicite du manifeste.
11. Vérifier les chemins, les types d'entrées et les contenus interdits.
12. Si le dépôt est géré par Git, capturer l'état initial :

```bash
git status --short
```

13. Identifier les fichiers déjà modifiés avant patch.
14. Identifier les fichiers ciblés par le patch qui étaient déjà modifiés avant application.

Aucune copie, suppression ou modification du dépôt ne doit intervenir avant la fin de ces contrôles.

---

## 8. Répertoire d'extraction

Règles :

```text
- ne crée pas de dossier temp, tmp, patch, extract ou équivalent dans le dépôt ;
- n'extrais jamais l'archive directement dans le dépôt ;
- ne réutilise pas un ancien dossier d'extraction ;
- ne supprime pas le dossier /tmp en fin de tâche ;
- ne lance pas rm -rf pour nettoyer l'extraction ;
- considère /tmp comme jetable et géré par l'environnement.
```

Le dossier d'extraction ne doit jamais devenir une source de fichiers supplémentaires non présents dans le ZIP courant.

---

## 9. Contrôle des chemins et des entrées ZIP

Tous les chemins doivent être relatifs à la racine du dépôt.

Refuser :

```text
- les chemins absolus ;
- les chemins contenant .. ;
- les chemins vides ou ambigus ;
- les chemins qui sortent du dépôt après résolution canonique ;
- les chemins traversant un lien symbolique vers l'extérieur du dépôt ;
- les liens symboliques fournis par le ZIP ;
- les fichiers spéciaux, devices ou entrées non régulières ;
- les doublons d'entrée visant le même chemin avec des contenus différents ;
- les différences de casse créant une destination ambiguë.
```

Les mêmes règles s'appliquent aux chemins présents dans :

```text
PATCH_MANIFEST.md
DELETE_FILES.txt
MOVE_FILES.txt
```

Format attendu pour un déplacement :

```text
ancien/chemin.ext -> nouveau/chemin.ext
```

Les deux côtés doivent rester dans le dépôt.

---

## 10. Contenus interdits dans un patch

Arrêter avant application si le ZIP contient, sauf cas explicitement justifié et non sensible :

```text
- acm.zip ;
- accloud/build/ ou un autre output CMake ;
- CMakeCache.txt, CMakeFiles/, build.ninja, .ninja_* ;
- binaires compilés ou bibliothèques générées ;
- caches IDE ou Qt ;
- logs runtime ;
- session.json réel ;
- capture HAR réelle ;
- token, cookie ou credential réel ;
- clé privée TLS ;
- URL signée persistée ;
- base locale accloud_cache.db ;
- fichiers runtime sous ~/.local/share/accloud ;
- dépendances vendorisées non demandées ;
- fichiers étrangers au dépôt.
```

Les certificats publics déjà versionnés peuvent être remplacés uniquement s'ils sont explicitement annoncés dans le manifeste.

Les tests peuvent contenir des valeurs factices clairement identifiables. Ils ne doivent jamais contenir de credential réutilisable.

Toute sortie de commande ou de test doit être redacted avant d'être reproduite dans le rapport.

---

## 11. Contradictions bloquantes

Arrête-toi avant toute modification si :

```text
- PATCH_MANIFEST.md est absent ;
- un fichier annoncé comme remplacé ou créé est absent du ZIP ;
- une suppression annoncée n'est pas listée dans DELETE_FILES.txt ;
- un déplacement annoncé n'est pas listé dans MOVE_FILES.txt ;
- DELETE_FILES.txt ou MOVE_FILES.txt manque sans justification explicite ;
- le manifeste et le contenu réel du ZIP se contredisent ;
- un chemin est absolu, contient .. ou sort du dépôt ;
- une entrée ZIP non sûre est présente ;
- le ZIP contient un secret réel ou un artefact runtime sensible ;
- le ZIP contient acm.zip ou des outputs de build ;
- le patch annonce une validation comme exécutée alors que le manifeste ne permet pas de l'identifier ;
- le patch touche un invariant sensible sans le déclarer.
```

Dans ce cas :

```text
- ne modifie rien ;
- ne tente pas de réparer le ZIP ;
- ne crée pas toi-même le fichier manquant ;
- ne réinterprète pas l'intention ;
- rapporte uniquement la contradiction exacte.
```

---

## 12. Contrôle minimal des invariants sensibles du projet

Ce contrôle ne t'autorise pas à refaire l'analyse architecturale du patch. Il sert uniquement à détecter une modification sensible non déclarée.

### MQTT Anycubic

La configuration runtime du broker Anycubic est figée sauf demande explicite :

```text
broker          = mqtt-universe.anycubic.com
port            = 8883
MQTT            = 3.1.1
TLS             = 1.2
mTLS            = certificat client + clé privée + CA
auth            = slicer
keepalive       = 1200
clean session   = true
compatibilité   = VerifyNone / SECLEVEL=0 selon configuration existante
```

Si le patch touche le broker, le port, la version MQTT, le mode d'authentification, mTLS, le keepalive, `cleanSession`, la politique de vérification TLS, la compatibilité OpenSSL ou les topics observés, le manifeste doit le déclarer explicitement.

Sans déclaration explicite, arrêter avant application.

Ne remplace jamais cette configuration par un modèle MQTT standard de ta propre initiative.

### Portée de `ignoreSslErrors()`

L'usage existant de `ignoreSslErrors()` est limité au téléchargement de miniatures vers le cache local.

Si le patch :

```text
- ajoute ignoreSslErrors() ailleurs ;
- déplace l'exception vers un QNetworkAccessManager global ;
- l'applique aux APIs cloud authentifiées ;
- l'applique aux uploads, impressions ou fichiers utilisateurs ;
- supprime la validation de contenu ou l'écriture atomique du cache ;
```

le manifeste doit le signaler explicitement et l'instruction utilisateur doit être non ambiguë.

Sinon, arrêter avant application.

### Secrets et logs

Le patch ne doit pas introduire dans les sources, tests, logs ou documentation :

```text
- token réel ;
- Authorization réel ;
- Cookie réel ;
- email ou identifiant utilisateur non nécessaire ;
- URL signée complète ;
- HAR réel ;
- clé privée ;
- contenu réel de session.json ;
- credentials MQTT bruts.
```

---

## 13. État initial Git et changements préexistants

Si le dépôt est sous Git, enregistrer avant application :

```bash
git status --short
```

Règles :

```text
- ne lance pas git reset ;
- ne lance pas git checkout -- ;
- ne lance pas git restore pour nettoyer ;
- ne lance pas git clean ;
- ne stash pas automatiquement ;
- ne supprime pas les fichiers non suivis ;
- ne restaure pas les changements préexistants ;
- ne confonds pas une modification préexistante avec une modification introduite par le patch.
```

Si un fichier ciblé par le patch était déjà modifié, applique néanmoins le fichier fourni par le ZIP, sauf instruction contraire explicite. Signale dans le rapport que la destination contenait une modification préexistante qui a été remplacée par le patch.

Ne tente pas de fusion implicite entre le contenu local et le contenu du ZIP.

---

## 14. Ordre d'application

Appliquer dans cet ordre :

1. Supprimer les fichiers listés dans `DELETE_FILES.txt`.
2. Appliquer les déplacements listés dans `MOVE_FILES.txt`.
3. Copier ou remplacer les fichiers projet contenus dans le ZIP.
4. Ne pas copier les fichiers de procédure, sauf demande explicite du manifeste.
5. Vérifier que le contenu appliqué correspond au ZIP.
6. Exécuter les commandes de validation.
7. Capturer l'état final Git si disponible.
8. Produire le rapport final.

Règles détaillées :

```text
- fichier à supprimer déjà absent : noter déjà absent, non bloquant ;
- source d'un déplacement absente : bloquant sauf si le manifeste autorise explicitement ce cas ;
- destination d'un déplacement déjà présente : bloquant sauf règle explicite ;
- fichier à créer absent du dépôt : le créer uniquement si le ZIP le fournit ;
- répertoire parent absent : le créer uniquement pour recevoir un fichier ou déplacement déclaré ;
- aucune autre modification auxiliaire n'est autorisée.
```

Si `git mv` échoue à cause d'un verrou Git, d'un problème d'index ou de droits :

```text
- utiliser un déplacement filesystem classique ;
- ne pas forcer l'index ;
- signaler le fallback dans le rapport.
```

---

## 15. Vérification après copie

Après application et avant validation :

```text
- vérifier que chaque fichier fourni existe à sa destination ;
- vérifier que son contenu ou son hash correspond à l'entrée ZIP ;
- vérifier que chaque suppression demandée est effective ;
- vérifier que chaque déplacement demandé est effectif ;
- vérifier que les fichiers de procédure n'ont pas été copiés par erreur ;
- vérifier qu'aucun autre fichier n'a été créé par l'application du patch.
```

Une différence entre le ZIP et le contenu appliqué est un échec d'application. Ne lance pas les validations tant que cette différence subsiste.

---

## 16. Validation — règles générales

Les commandes de validation indiquées dans `PATCH_MANIFEST.md` sont prioritaires.

Interprétation :

```text
- commandes explicites : exécuter exactement ces commandes ;
- aucune validation explicitement demandée : ne rien inventer, rapporter non exécutée par instruction ;
- section de validation absente ou ambiguë : utiliser la validation locale par défaut ci-dessous ;
- validation live : appliquer les règles spécifiques de la section suivante.
```

Les commandes CMake doivent normalement être exécutées depuis :

```text
accloud/
```

Ne lance pas une commande qui n'existe pas dans le dépôt.

Ne remplace pas une commande demandée par une commande que tu juges équivalente sans signaler le blocage.

---

## 17. Validation locale par défaut

Si le manifeste ne contient aucune instruction exploitable sur la validation, exécuter depuis `accloud/` :

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default -E '^accloud_mqtt_live_broker$' --output-on-failure
```

Cette validation :

```text
- configure le preset Debug standard ;
- construit les cibles disponibles ;
- exécute les tests locaux déclarés ;
- exclut le test broker live nécessitant une session et des secrets.
```

Ne lance pas `start.sh` comme validation automatique. Ce wrapper peut démarrer l'application interactive.

Ne lance pas automatiquement :

```text
- l'application graphique ;
- une importation HAR ;
- une connexion au cloud Anycubic ;
- une connexion live au broker ;
- un upload ;
- un téléchargement utilisateur ;
- une commande d'impression ;
- une commande vers une imprimante.
```

Une validation `prod` ou `dev-debug` n'est exécutée que si le manifeste la demande.

---

## 18. Validations ciblées connues

Exemples de commandes valides, uniquement quand le manifeste les demande ou quand elles correspondent à la règle par défaut :

```bash
cmake --build --preset default --target accloud_cli
ctest --preset default -R '^accloud_smoke$' --output-on-failure
ctest --preset default -R '^accloud_har_import$' --output-on-failure
ctest --preset default -R '^accloud_cloud_core_regressions$' --output-on-failure
ctest --preset default -R '^accloud_security_redaction$' --output-on-failure
ctest --preset default -R '^accloud_ui_models$' --output-on-failure
ctest --preset default -R '^accloud_log_flow$' --output-on-failure
ctest --preset default -R '^accloud_mqtt_flow$' --output-on-failure
ctest --preset default -R '^accloud_ui_qml' --output-on-failure

Si une validation Qt échoue sur :

```text
unable to create an OpenGL 3.3 context
```

alors :

```text
- qualifier cet échec comme une limitation de l'environnement Codex ;
- ne pas le qualifier comme un échec fonctionnel du patch ;
- le signaler explicitement comme faux négatif environnemental dans le rapport final ;
- ne rien modifier au patch pour contourner cette absence de contexte OpenGL ;
- considérer que cette erreur ne bloque pas la validation fonctionnelle du patch.
```
```

Presets connus :

```text
default
dev-debug
prod
```

Ne suppose pas qu'un test est disponible : CMake peut l'omettre si une dépendance comme Qt6::Mqtt, Qt6::Sql, Qt Quick Test ou Python n'est pas présente.

Une absence de test déclaré doit être rapportée comme limite d'environnement ou de configuration, pas comme réussite du test.

---

## 19. Validations live

Le test suivant est live :

```text
accloud_mqtt_live_broker
```

Il peut nécessiter :

```text
- une session Anycubic valide ;
- des tokens non expirés ;
- une connectivité réseau ;
- les certificats mTLS et la clé privée ;
- Qt6::Mqtt ;
- une configuration OpenSSL compatible ;
- un compte et éventuellement une imprimante disponibles.
```

Règles :

```text
- ne jamais lancer un test live par défaut ;
- ne jamais utiliser de secret réel sans instruction explicite ;
- ne jamais afficher les credentials ou chemins sensibles dans le rapport ;
- ne jamais envoyer de commande d'impression ou d'imprimante comme simple validation ;
- ne jamais interpréter l'absence d'environnement live comme un succès.
```

Un test live n'est exécuté que si le manifeste le demande explicitement et si l'environnement est contrôlé.

Sinon, rapporter :

```text
validation live non exécutée : environnement live non demandé ou indisponible
```

---

## 20. Dépendances et environnement incomplet

Ne lance pas automatiquement :

```text
- apt install ;
- dnf install ;
- pacman ;
- brew ;
- vcpkg install ;
- pip install ;
- npm install ;
- mise à jour de dépendances ;
- modification du système ou du registre de paquets.
```

Si CMake, Ninja, Qt6, Qt6::Mqtt, Qt6::Sql, Qt Quick Test, OpenSSL, Python ou une autre dépendance manque :

```text
- ne modifie pas le code pour contourner ;
- ne modifie pas CMake pour masquer l'absence ;
- rapporte la commande exacte et l'erreur utile ;
- qualifie le résultat comme environnement incomplet ;
- exécute seulement les validations encore possibles sans altérer le dépôt.
```

Une validation partielle doit être décrite comme partielle.

---

## 21. Échec d'application, de test ou de build

Si une commande obligatoire échoue :

```text
1. arrêter immédiatement la chaîne de validation ;
2. ne modifier aucun fichier pour corriger l'échec ;
3. ne restaurer aucun fichier ;
4. ne relancer la commande que si le manifeste prévoit explicitement un retry ;
5. capturer la commande exacte ;
6. extraire la sortie utile ;
7. redacter tout secret ou URL signée ;
8. identifier le fichier ou composant indiqué par l'erreur si disponible ;
9. qualifier l'échec ;
10. rapporter le résultat final sans proposer de refactor.
```

Qualifications possibles :

```text
- blocage lié au patch ;
- régression probable ;
- contradiction de packaging ;
- test obsolète ou inadapté ;
- environnement incomplet ;
- dépendance Qt/CMake/OpenSSL absente ;
- session ou credential live indisponible ;
- endpoint cloud ou broker indisponible ;
- dette préexistante révélée ;
- faux positif non bloquant.
```

Ne crée pas toi-même un patch suffixé pour corriger l'échec. Le correctif suivant doit être fourni ou demandé explicitement par l'utilisateur.

---

## 22. Règles de sécurité pendant l'application

```text
- ne supprime rien hors DELETE_FILES.txt ;
- ne déplace rien hors MOVE_FILES.txt ;
- ne modifie aucun fichier absent du ZIP ;
- ne reformate aucun fichier non ciblé ;
- ne mets pas à jour les dépendances ;
- ne génère pas de credentials ;
- ne copie pas de secret depuis l'environnement ;
- ne lance pas de commande destructive générique ;
- ne nettoie pas les artefacts préexistants ;
- ne commit pas ;
- ne push pas ;
- ne crée pas de branche ;
- ne poursuis pas au-delà du patch.
```

Commandes interdites pour nettoyer ou forcer le dépôt :

```text
git reset
git checkout --
git restore
git clean
rm -rf appliqué au dépôt
```

---

## 23. Fichiers générés, runtime ou préexistants

Distinguer :

```text
- fichiers modifiés par le patch ;
- fichiers déjà modifiés avant patch ;
- fichiers générés pendant la configuration ou le build ;
- fichiers runtime locaux ;
- fichiers hors périmètre.
```

Exemples d'artefacts non ciblés à ne pas intégrer ni nettoyer automatiquement :

```text
accloud/build/
~/.local/share/accloud/accloud.ini
~/.local/share/accloud/session.json
~/.local/share/accloud/settings.ini
~/.local/share/accloud/accloud_cache.db
~/.local/share/accloud/tmp/
~/.local/share/accloud/thumbnails/
~/.local/share/accloud/logs/
```

Les outputs créés par CMake et CTest pendant la validation doivent être signalés comme générés, pas comme fichiers du patch.

Un fichier préexistant non ciblé n'est pas une erreur. Il doit seulement apparaître dans l'état initial notable.

---

## 24. Patches de correction suffixés

Un patch corrigeant une erreur du patch principal doit conserver son numéro et recevoir un suffixe :

```text
patch_0038-1.zip
patch_0038-2.zip
```

Règles :

```text
- ne renomme pas toi-même le patch fourni ;
- n'applique pas un patch suffixé si l'utilisateur indique qu'il est devenu inutile ;
- n'applique pas deux versions concurrentes sans instruction explicite ;
- rapporte précisément quelle archive a été appliquée ;
- ne considère pas automatiquement le suffixe comme cumulatif ou remplaçant : suivre le manifeste.
```

Si le patch original a finalement été validé et que le correctif suffixé n'est pas nécessaire, ne l'applique pas.

---

## 25. Git, commit et push

L'application d'un patch ne comprend pas automatiquement un commit.

Règles :

```text
- ne commit jamais sans demande explicite ;
- ne push jamais sans demande explicite ;
- ne crée pas de branche sans demande explicite ;
- ne stage pas automatiquement les fichiers ;
- ne modifie pas l'index pour masquer un état local ;
- ne mélange pas fichiers du patch et changements préexistants.
```

Si un commit est explicitement demandé dans la même instruction :

```text
- appliquer d'abord le patch ;
- exécuter les validations prévues ;
- arrêter sans commit si une validation obligatoire échoue ;
- inclure uniquement les fichiers du patch ;
- exclure build, caches, logs, HAR, session, secrets et binaires ;
- rapporter le hash du commit sans exposer d'identifiant interne inutile.
```

---

## 26. Rapport final attendu

En cas d'application achevée, répondre uniquement avec ce format :

```text
Archive appliquée :
- nom_du_patch.zip

État initial notable :
- fichiers déjà modifiés avant patch, ou aucun
- fichiers ciblés déjà modifiés avant remplacement, ou aucun

Fichiers supprimés :
- chemin : supprimé
- chemin : déjà absent
- ou aucun

Fichiers déplacés :
- ancien -> nouveau
- préciser filesystem mv si git mv a échoué
- ou aucun

Fichiers remplacés/créés :
- chemin
- ou aucun

Contrôles d'application :
- contenu appliqué conforme au ZIP : oui/non
- fichiers de procédure copiés dans le dépôt : oui/non
- contenus interdits détectés : aucun ou détail

Commandes lancées :
- commande exacte
- ou aucune, par instruction

Résultat des tests :
- commande : succès/échec/non disponible
- tests omis par dépendance : détail
- ou non exécutés par instruction

Résultat du build :
- commande : succès/échec/non disponible
- ou non exécuté par instruction

Validations live :
- commande : succès/échec
- ou non exécutées : raison

État final notable :
- fichiers du patch modifiés
- fichiers préexistants non ciblés toujours présents
- artefacts générés par build/tests

Erreurs ou blocages éventuels :
- aucun
- ou détail factuel et redacted
```

Le rapport doit être factuel.

Ne conclus pas avec :

```text
- une proposition de refactor ;
- une suggestion de correction supplémentaire ;
- une question ;
- une proposition de prochain patch ;
- une formule conversationnelle.
```

---

## 27. Rapport en cas de contradiction avant application

Si un blocage est détecté avant modification, répondre uniquement :

```text
Patch non appliqué.

Archive inspectée :
- nom_du_patch.zip

Contradiction bloquante :
- description exacte

État du dépôt :
- aucune modification effectuée
```

Ne produire aucun autre commentaire.

---

## 28. Résumé opératoire minimal

Application normale :

```text
identifier dépôt et ZIP
-> lister ZIP
-> extraire sous /tmp
-> lire manifeste/suppressions/déplacements
-> contrôler chemins, secrets et invariants sensibles
-> capturer état initial
-> supprimer
-> déplacer
-> copier
-> vérifier conformité
-> valider localement
-> valider live seulement si demandé
-> rapporter
```

Blocage :

```text
contradiction détectée
-> aucune modification
-> rapport unique et précis
```

Échec après application :

```text
stop validation
-> aucune correction opportuniste
-> sortie redacted
-> qualification factuelle
-> rapport final
```
