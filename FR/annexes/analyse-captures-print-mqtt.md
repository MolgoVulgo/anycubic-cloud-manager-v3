# Annexe — Analyse des captures MQTT d’impression

Statut : `SNAPSHOT`.

Structure répétée : commande HTTP, MQTT `print/start`, download/progress, work report/busy, checks automatiques, préchauffe, impression, fin ou état terminal absent.

Constats : `action`, `state`, `type` sont structurants ; `taskid` segmente les jobs ; deux `start` consécutifs peuvent être deux tâches différentes ; busy/free aide mais ne suffit pas ; MQTT devient source principale après commande HTTP.

Critères d’intégration : détecter taskid actif, mapper preheating/printing, progression monotone sauf nouvelle tâche, reconnaître terminaux, préserver inconnus, ne pas crasher sur doublons ou désordre.
