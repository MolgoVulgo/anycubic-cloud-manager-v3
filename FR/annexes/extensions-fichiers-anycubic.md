# Annexe — Extensions fichiers résine Anycubic

Statut : `SNAPSHOT`.

| Extension | Famille | Notes |
| --- | --- | --- |
| `.pwmb` | Photon Workshop Binary | Format principal ciblé par le parser tables-first. |
| `.pws` | Photon Workshop | Famille layer/image utilisée par plusieurs workflows. |
| `.phz` | Container Photon | Nécessite gestion archive/container. |
| `.photons` | Famille Photon S | Famille legacy avec driver propre. |
| `.pwsz` | Photon Workshop zipped/scene-line | Fréquent dans les captures cloud print. |

L’extension seule ne garantit pas la compatibilité imprimante. La compatibilité vient des metadata cloud, du modèle, du profil machine et de la réponse API.
