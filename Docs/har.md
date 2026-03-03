# HAR — Capture & import de session Anycubic Cloud

## 1) Comment générer le HAR

### Chrome / Chromium (recommandé)
1. Ouvrir le site Anycubic Cloud / Workshop et se connecter.
2. `F12` → onglet **Network**.
3. Cocher :
   - **Preserve log** (sinon tu perds la séquence login)
   - **Disable cache** (optionnel mais utile)
4. (Optionnel) Filtrer par domaine Anycubic pour réduire le bruit.
5. Refaire une action qui force une authent (login, refresh, call API qui renvoie 200).
6. Dans la liste des requêtes : clic droit → **Save all as HAR with content**.

### Firefox
1. `F12` → **Réseau**.
2. Cocher **Conserver les journaux**.
3. Refaire login / action cloud.
4. Bouton export → **Exporter en HAR** (avec contenu si l’option existe).

### Contraintes pratiques
- Il faut **le contenu des réponses** (sinon tu ne récupères pas les tokens qui arrivent dans un JSON de réponse).
- Un HAR “propre” contient : au moins un endpoint d’auth (login/token/refresh) **en 200/201**.
- Si tu exportes après plusieurs heures : tu risques de capturer une session déjà périmée.

---

## 2) Comment le HAR est analysé

### Format exploité
Le parseur lit le JSON HAR standard :
- `log.entries[*].request` : `url`, `method`, `headers[]`, …
- `log.entries[*].response` : `status`, `content.text`, `content.encoding`, …

### Filtrage
- Par défaut, seules les entrées dont l’URL contient `anycubic` sont traitées (`host_contains="anycubic"`).
- Tout le reste est ignoré (bruit analytics/cdn/etc.).

### Pipeline d’extraction (ordre réel)
Pour chaque entry :
1. Lire `request.url` (skip si vide)
2. Lire `request.method` (GET/POST…)
3. Lire `response.status`
4. Marquer l’entrée comme “auth-likely” si l’URL contient un des marqueurs : ` /login | /auth | /token | /refresh `.
5. Extraire des tokens candidats depuis :
   - **Query string** (fallback) : `access_token`, `token`, `authorization`
   - **Headers de requête** :
     - `Authorization`
     - `X-Access-Token`
     - `X-Auth-Token`
     - plus généralement `X-*token*`
   - **Body JSON de réponse** (source principale)

### Lecture du JSON de réponse
- Source : `response.content.text`
- Si `response.content.encoding == "base64"` : décodage base64 → UTF‑8 → JSON.
- Si ce n’est pas un JSON dict : ignoré.

### Sélection du “meilleur” candidat
Deux listes de candidats peuvent exister :
- `response_candidates` (tokens trouvés dans un JSON de réponse)
- `header_candidates` (tokens trouvés dans des headers de requête)

Le choix se fait par score (max) :
- HTTP 200/201 : +100
- endpoint “auth-likely” : +10
- méthode POST : +1

Priorité finale :
1. **Réponse JSON** (si trouvée)
2. Sinon **headers**
3. Sinon **query params** (dernier recours)

---

## 3) Comment les tokens sont récupérés

### Champs reconnus (dans le JSON réponse)
Le parseur fait une collecte “flat” de champs intéressants, sans dépendre d’une structure fixe (il parcourt récursivement dict/list).

Il tente ensuite :
- `access_token` (ou variantes) : `access_token`, `accesstoken`
- `token` : `token`
- `id_token` (ou variantes) : `id_token`, `idtoken`
- `refresh_token` : `refresh_token`, `refreshtoken`
- `token_type` : `token_type`, `tokentype` (défaut : `Bearer`)
- `expires_in` : `expires_in`, `expiresin` (converti en int > 0)

### Règles de fallback / normalisation
- Si `access_token` absent mais `token` présent → `access_token = token`.
- Si un champ `authorization/auth/bearer` contient déjà `Bearer <…>` → extraction du token.
- Si `access_token` est déjà préfixé `Bearer ` → strip du préfixe.

### Construction runtime
En sortie, la session runtime contient un mapping normalisé :
- `access_token` : toujours **sans** préfixe
- `id_token` : si absent, il est **déduit** de `access_token` (fallback)
- `token` : récupéré de `token` ou des headers `X-Access-Token` / `X-Auth-Token` (fallback)
- `Authorization` : garantie sous la forme `Bearer <access_token>`

Notes :
- La présence exacte des 4 clés n’est pas obligatoire : le loader accepte une session partielle.
- L’app privilégie `Authorization` et/ou `access_token` pour signer les appels.

---

## 4) Description de `session.json`

### Rôle
Fichier de persistance locale des tokens importés (HAR → session.json). Il sert à relancer l’app sans refaire une capture HAR.

### Emplacement
- Par défaut : `./session.json` (répertoire de lancement / cwd)
- Override via variable d’environnement :
  - `ACCLOUD_SESSION_PATH=/chemin/vers/session.json`

### Permissions
- Écrit en mode strict **0600** (user read/write uniquement).

### Structure canonique (v2)
```json
{
  "last_update": "DD/MM/YYYY",
  "tokens": {
    "Authorization": "Bearer <...>",
    "access_token": "<token>",
    "id_token": "<token>",
    "token": "<token>"
  }
}
```

### Compatibilité (chargement)
Le loader est tolérant et accepte :
- `tokens` partiel
- ancienne clé `headers` (legacy), avec reprise de :
  - `Authorization`
  - `X-Access-Token`
  - `X-Auth-Token`
- des clés au niveau racine (`access_token`, `id_token`, `token`, `Authorization`) si présentes

---

## 5) Comment les tokens sont stockés, et où

### Stockage en mémoire (runtime)
- Une fois chargés/importés, les tokens vivent dans un objet `SessionData(tokens={...})`.
- Les clés sont **normalisées** (format stable) avant usage.

### Stockage sur disque
- Les tokens persistés sont ceux de `SessionData.tokens`, après normalisation “storage”.
- Fichier : `session.json` (voir chemin plus haut).
- Les permissions sont forcées à `0600` via `os.open(..., 0o600)`.

### Logs & sécurité (important)
- Les logs doivent **masquer** les secrets : jamais de dump brut d’Authorization / tokens.
- L’implémentation utilise un masquage court (`abcdef...wxyz`) + une empreinte SHA‑256 tronquée pour debug sans fuite.
- Ne pas committer `session.json` (c’est littéralement une clé).

---

## 6) Résumé opérationnel
- Tu génères un HAR “avec contenu” après un login.
- L’app filtre sur `anycubic`, cherche d’abord un JSON de réponse contenant des champs token.
- Elle normalise (`access_token`, `Authorization`, etc.) et écrit `session.json` en 0600.
- Au démarrage suivant, l’app recharge `session.json` et réutilise la session tant qu’elle n’est pas expirée.

