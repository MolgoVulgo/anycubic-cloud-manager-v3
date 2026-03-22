# UI — découpage conceptuel MQTT

Statut documentaire : `SPEC`

**Objectif**

Définir les bases du découpage de l’interface pour la supervision et le suivi d’imprimantes résine Anycubic pilotées par un socle HTTP + MQTT.

Ce document reste volontairement **conceptuel** :

- pas de code ;
- pas de choix de composants techniques ;
- pas de détail de style graphique ;
- pas de binding direct à des structures JSON.

Le but est de délimiter clairement les différentes parties de l’UI, leur rôle, leur niveau de priorité et la nature des informations qu’elles doivent afficher.

---

# 1. Principe directeur

L’interface ne doit pas refléter le protocole MQTT brut.

Elle doit refléter une **lecture métier consolidée** de l’état de l’imprimante.

Autrement dit, l’UI ne présente pas :

- des topics ;
- des payloads JSON ;
- des codes internes isolés.

Elle présente :

- un **état global machine** ;
- un **état job** ;
- des **causes de blocage** ;
- des **alertes compréhensibles** ;
- des **détails secondaires** utiles au suivi.

---

# 2. Logique générale de découpage

Le découpage recommandé repose sur 4 niveaux visuels :

1. **niveau synthèse**
2. **niveau opérationnel**
3. **niveau diagnostic**
4. **niveau historique / détail**

Chaque niveau doit répondre à une question distincte.

---

# 3. Niveau synthèse

## 3.1 Rôle

Donner en un coup d’œil l’état principal de l’imprimante.

## 3.2 Question à laquelle cette zone répond

```text
Est-ce que l’imprimante est disponible, occupée, bloquée ou hors ligne ?
```

## 3.3 Informations attendues

- nom de l’imprimante ;
- état global principal ;
- état de connectivité ;
- signal réseau synthétique ;
- présence éventuelle d’une alerte active.

## 3.4 Forme conceptuelle

Cette zone doit être la plus visible de l’écran.

Elle doit afficher une formulation simple, par exemple :

- `Disponible`
- `Impression en cours`
- `En attente d’intervention`
- `Imprimante hors ligne`

## 3.5 Contraintes

- aucun code brut mis en avant ;
- aucune surcharge de détails ;
- aucune ambiguïté entre état machine et état job.

---

# 4. Niveau opérationnel

## 4.1 Rôle

Afficher ce qui est en train de se passer du point de vue de la tâche active.

## 4.2 Question à laquelle cette zone répond

```text
Que fait l’imprimante maintenant ?
```

## 4.3 Cas couverts

### Imprimante libre

Quand l’imprimante est libre, cette zone devient légère et informative.

Elle peut afficher :

- dernière activité ;
- dernier fichier imprimé ;
- dernier changement d’état ;
- quelques métriques secondaires.

### Imprimante occupée en impression

Quand un job est actif, cette zone devient centrale.

Elle doit afficher :

- nom du fichier ;
- état du job ;
- progression ;
- couche courante / total ;
- temps restant ;
- temps écoulé.

### Imprimante occupée mais bloquée

Quand le job est suspendu, bloqué ou en attente, cette zone ne doit plus ressembler à une zone de progression normale.

Elle doit afficher :

- état de blocage ;
- raison lisible ;
- étape de reprise éventuelle ;
- statut de l’action attendue.

## 4.4 États conceptuels à supporter

- `Disponible`
- `Téléchargement`
- `Préparation`
- `Préchauffe`
- `Impression en cours`
- `Pause`
- `Reprise`
- `Terminé`
- `En attente`
- `Bloqué`
- `Arrêté`

---

# 5. Niveau diagnostic

## 5.1 Rôle

Expliquer pourquoi l’état principal est ce qu’il est.

## 5.2 Question à laquelle cette zone répond

```text
Pourquoi l’imprimante est dans cet état ?
```

## 5.3 Utilité

Cette zone devient essentielle dès qu’un comportement n’est plus nominal.

Elle permet de distinguer :

- une impression normale ;
- un blocage machine ;
- une reprise en attente ;
- une interruption manuelle ;
- une perte de connectivité ;
- un problème de précondition.

## 5.4 Informations attendues

Cette zone doit transformer les sous-statuts machine en messages compréhensibles.

Exemples de dimensions à remonter :

- plateau ;
- résine ;
- nivellement ;
- état d’attente ;
- état de reprise ;
- signal réseau si pertinent.

## 5.5 Principe de présentation

Le diagnostic ne doit pas être une liste brute de codes.

Il doit être présenté comme :

- une **cause principale** ;
- éventuellement des **conditions secondaires** ;
- un **niveau de gravité** ;
- une **interprétation lisible**.

## 5.6 Distinction importante

Cette zone doit séparer clairement :

- **cause machine** ;
- **état du workflow** ;
- **action utilisateur**.

Exemple conceptuel :

- cause : vérification plateau requise ;
- workflow : job en attente ;
- action : arrêt manuel ensuite.

---

# 6. Niveau alertes

## 6.1 Rôle

Porter les messages qui nécessitent une attention immédiate ou prioritaire.

## 6.2 Question à laquelle cette zone répond

```text
Y a-t-il quelque chose à traiter maintenant ?
```

## 6.3 Cette zone doit être distincte du statut principal

Le statut principal décrit l’état courant.
L’alerte attire l’attention sur un événement important.

Il ne faut pas confondre :

- `Impression en cours`
- et `Signal Wi-Fi faible`

## 6.4 Niveaux conceptuels d’alerte

### Critique

Pour les événements qui empêchent le fonctionnement ou nécessitent une action immédiate.

Exemples :

- imprimante hors ligne pendant un job ;
- job bloqué ;
- intervention requise ;
- impression arrêtée de manière inattendue.

### Importante

Pour les événements qui n’arrêtent pas forcément le job mais méritent une attention visible.

Exemples :

- Wi-Fi faible ;
- préparation longue ;
- reprise en cours ;
- téléchargement anormalement long.

### Information

Pour les changements d’état non critiques.

Exemples :

- impression démarrée ;
- impression reprise ;
- impression terminée.

## 6.5 Règle de présentation

Une alerte doit être :

- courte ;
- compréhensible ;
- orientée action ou compréhension ;
- éventuellement accompagnée d’un détail technique repliable.

---

# 7. Niveau historique et détail

## 7.1 Rôle

Donner un contexte sans surcharger les zones principales.

## 7.2 Question à laquelle cette zone répond

```text
Qu’est-ce qui s’est passé avant, et quels détails secondaires sont disponibles ?
```

## 7.3 Informations adaptées à cette zone

- derniers changements d’état ;
- dernière impression ;
- transitions majeures ;
- journal d’événements ;
- métriques secondaires ;
- données cumulées machine.

## 7.4 Informations à placer ici plutôt qu’au centre

- compteurs de cycles ;
- couches cumulées ;
- détails de paramètres d’impression ;
- identifiants techniques ;
- horodatages détaillés.

## 7.5 Règle

Tout ce qui n’aide pas à répondre immédiatement à :

- “quel est l’état ?”
- “que faut-il faire ?”

ne doit pas polluer les zones de premier niveau.

---

# 8. Découpage par grandes zones écran

Le découpage conceptuel recommandé peut être formulé en 5 blocs.

## 8.1 Bloc A — identité et synthèse machine

Rôle :

- identifier l’imprimante ;
- afficher son état principal ;
- afficher sa connectivité.

Contenu recommandé :

- nom ;
- modèle ;
- statut global ;
- online/offline ;
- Wi-Fi synthétique.

## 8.2 Bloc B — suivi de job

Rôle :

- afficher la tâche en cours ou la dernière tâche.

Contenu recommandé :

- nom du fichier ;
- progression ;
- couches ;
- temps restant ;
- temps écoulé ;
- état job lisible.

## 8.3 Bloc C — diagnostic machine

Rôle :

- expliquer les blocages, attentes ou défauts.

Contenu recommandé :

- cause principale ;
- contrôles machine pertinents ;
- état d’attente / reprise ;
- lecture simplifiée des préconditions.

## 8.4 Bloc D — alertes actives

Rôle :

- isoler les événements qui demandent une attention immédiate.

Contenu recommandé :

- message principal ;
- gravité ;
- moment ;
- détail technique optionnel.

## 8.5 Bloc E — historique et métriques secondaires

Rôle :

- conserver le contexte sans alourdir le cœur de l’écran.

Contenu recommandé :

- derniers événements ;
- dernière activité ;
- compteurs machine ;
- détails secondaires.

---

# 9. Logique d’affichage selon les scénarios

## 9.1 Scénario 1 — imprimante `free`

Priorité visuelle :

1. synthèse machine ;
2. dernière activité ;
3. métriques secondaires.

Le bloc job devient secondaire.
Le bloc diagnostic peut être masqué ou réduit.
Le bloc alertes ne doit être visible que s’il y a une alerte résiduelle utile.

## 9.2 Scénario 2 — `busy` avec impression normale

Priorité visuelle :

1. suivi de job ;
2. synthèse machine ;
3. alertes secondaires ;
4. détail.

Le bloc suivi de job devient central.
Le bloc diagnostic reste discret tant qu’il n’y a pas d’anomalie.

## 9.3 Scénario 3 — `busy` avec défaut ou attente

Priorité visuelle :

1. diagnostic machine ;
2. alerte active ;
3. état du job ;
4. synthèse machine.

Dans ce scénario, la progression ne doit plus être l’information dominante.
La cause et l’action attendue doivent prendre le dessus.

## 9.4 Scénario 4 — hors ligne

Priorité visuelle :

1. état de connectivité ;
2. dernière information connue ;
3. historique récent.

L’UI doit clairement indiquer qu’une partie des données affichées peut être non fraîche.

---

# 10. Ce qui ne doit pas apparaître au premier niveau

Les éléments suivants ne doivent pas être affichés en façade dans l’UI principale :

- topics MQTT ;
- payload JSON brut ;
- `msgid` ;
- structures internes ;
- codes techniques sans traduction ;
- listes de `checkStatus[]` non interprétées.

Ces éléments peuvent exister dans une vue technique, debug ou avancée, mais pas dans la lecture métier principale.

---

# 11. Règles d’écriture UI

## 11.1 Le texte affiché doit être métier

L’interface doit afficher des formulations comme :

- `Disponible`
- `Impression en cours`
- `En attente d’intervention`
- `Vérification plateau requise`
- `Signal Wi-Fi faible`

et non des formulations comme :

- `code=1306`
- `platform=1206`
- `state=waiting`

## 11.2 Le code technique peut exister, mais en second niveau

Le code brut peut être visible :

- dans un détail repliable ;
- dans une vue support ;
- dans un journal ;
- dans une zone technique.

Mais il ne doit jamais être la seule information portée à l’utilisateur.

---

# 12. Conclusion

Le découpage UI recommandé repose sur une séparation nette entre :

- **ce que l’imprimante est** ;
- **ce qu’elle fait** ;
- **pourquoi elle ne peut pas continuer** ;
- **ce qui doit attirer l’attention** ;
- **ce qui relève du détail et de l’historique**.

La structure conceptuelle cible est donc :

1. **Synthèse machine**
2. **Suivi de job**
3. **Diagnostic machine**
4. **Alertes**
5. **Historique et métriques secondaires**

Cette séparation permet d’éviter une UI trop proche du protocole et de construire une interface lisible, stable et orientée usage.

