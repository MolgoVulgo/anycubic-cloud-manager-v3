# Opérations, logs, cache et sécurité

Statut : `IMPLEMENTE` pour la base, `PARTIEL` pour la politique complète.

## Chemins runtime

Racine par défaut `~/.local/share/accloud`. Fichiers : `accloud.ini`, `session.json`, `settings.ini`, `accloud_cache.db`, `tmp/`, `thumbnails/`, `logs/`. Overrides : `ACCLOUD_PATHS_INI`, `ACCLOUD_SESSION_PATH`, `ACCLOUD_DB_PATH`, `ACCLOUD_LOG_DIR`, `ACCLOUD_THUMBNAIL_DIR`, `ACCLOUD_MQTT_OPENSSL_CONF_PATH`.

## Logs

Logs JSONL : un événement par ligne, champs stables, `op_id` sur opérations utilisateur, request id sur HTTP. Sinks recommandés : app, HTTP, render/viewer, tail debug.

## Redaction

Redacter token, id_token, access_token, refresh_token, authorization, cookie, signature, nonce, timestamp signé, password, email, query URL signée, chemins révélant clés privées.

## Cache

Rôles : accélération startup, fallback, mémoire sync, thumbnails, futures données dérivées viewer. Politique : fenêtre RAM pour layers/masks, LRU disque downloads/previews/derived, cache cloud avec fraîcheur/source.

## Purge

Purge globale, par fichier/projet, par type. Une purge ne casse pas l’app : tout doit être reconstruisible ou rechargeables.

## Debug bundle

Inclure settings sanitizés, chemins runtime, logs redacted, résumé endpoint sans secrets, observations MQTT sans credentials, stats cache, diagnostics viewer. Exclure HAR, tokens, clés privées, URLs signées complètes.

## Décision

Les diagnostics sont indispensables sur un système reverse-engineered, mais ils ne doivent pas fuiter les credentials.
