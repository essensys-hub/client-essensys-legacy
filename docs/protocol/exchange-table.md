# Table d'Échange - Documentation Complète

## Vue d'Ensemble

La **table d'échange** est une structure de données centrale qui permet la communication entre le client embarqué BP_MQX_ETH et le serveur. Elle contient environ 600 indices (0 à ~600) qui représentent l'état complet du système domotique : configuration, état des équipements, commandes, historiques, etc.

Cette documentation se concentre sur les **indices 605-622** qui sont utilisés pour les **scénarios** et le **contrôle des équipements** (lumières, volets, chauffage).

## Concept de la Table d'Échange

### Principe de Fonctionnement

1. **Client → Serveur** : Le client envoie périodiquement (toutes les 1-2 secondes) l'état de certains indices via `POST /api/mystatus`
2. **Serveur → Client** : Le serveur répond avec des actions à exécuter via `GET /api/myactions`
3. **Exécution** : Le client exécute les actions et acquitte via `POST /api/done/{guid}`

### Structure des Données

Chaque indice de la table d'échange contient un octet (0-255) qui représente :
- Un état (0/1 pour OFF/ON)
- Une valeur numérique (température, durée, etc.)
- Un masque de bits (pour contrôler plusieurs équipements simultanément)

## Indices 605-622 : Scénarios et Contrôle

Ces indices sont définis dans l'enum `enumScenario` du fichier `H/TableEchange.h` et commencent à l'indice 600 dans la table d'échange.

### Tableau Complet des Indices

| Indice | Nom | Description | Valeurs |
|--------|-----|-------------|---------|
| 600 | Scenario_Confirme_Scenario | Demander confirmation avant exécution | 0=non, 1=oui |
| 601 | Scenario_Alarme_ON | Contrôle de l'alarme | 0=rien, 1=activer, 2=désactiver |
| 602-612 | Scenario_AlarmeConfig | Configuration alarme (11 paramètres) | Voir enumAlarmeConfig |
| 613 | Scenario_Eteindre_PDV_LSB | Éteindre lumières Pièces De Vie (LSB) | Masque de bits |
| 614 | Scenario_Eteindre_PDV_MSB | Éteindre lumières Pièces De Vie (MSB) | Masque de bits |
| 615 | Scenario_Eteindre_CHB_LSB | Éteindre lumières Chambres (LSB) | Masque de bits |
| 616 | Scenario_Eteindre_CHB_MSB | Éteindre lumières Chambres (MSB) | Masque de bits |
| 617 | Scenario_Eteindre_PDE_LSB | Éteindre lumières Pièces D'Eau (LSB) | Masque de bits |
| 618 | Scenario_Eteindre_PDE_MSB | Éteindre lumières Pièces D'Eau (MSB) | Masque de bits |
| 619 | Scenario_Allumer_PDV_LSB | Allumer lumières Pièces De Vie (LSB) | Masque de bits |
| 620 | Scenario_Allumer_PDV_MSB | Allumer lumières Pièces De Vie (MSB) | Masque de bits |
| 621 | Scenario_Allumer_CHB_LSB | Allumer lumières Chambres (LSB) | Masque de bits |
| 622 | Scenario_Allumer_CHB_MSB | Allumer lumières Chambres (MSB) | Masque de bits |

### Indices Suivants (623-639)

| Indice | Nom | Description |
|--------|-----|-------------|
| 623 | Scenario_Allumer_PDE_LSB | Allumer lumières Pièces D'Eau (LSB) |
| 624 | Scenario_Allumer_PDE_MSB | Allumer lumières Pièces D'Eau (MSB) |
| 625 | Scenario_OuvrirVolets_PDV | Ouvrir volets Pièces De Vie |
| 626 | Scenario_OuvrirVolets_CHB | Ouvrir volets Chambres |
| 627 | Scenario_OuvrirVolets_PDE | Ouvrir volets Pièces D'Eau |
| 628 | Scenario_FermerVolets_PDV | Fermer volets Pièces De Vie |
| 629 | Scenario_FermerVolets_CHB | Fermer volets Chambres |
| 630 | Scenario_FermerVolets_PDE | Fermer volets Pièces D'Eau |
| 631 | Scenario_Securite | Contrôle prises de sécurité |
| 632 | Scenario_Machines | Contrôle machines à laver |
| 633-636 | Scenario_Chauf_* | Consignes chauffage (zj, zn, zsb1, zsb2) |
| 637 | Scenario_Cumulus | Contrôle cumulus |
| 638 | Scenario_Reveil_Reglage | Réglage des réveils |
| 639 | Scenario_Reveil_ON | Activation fonction réveil |

## Indices Spéciaux

### Index 590 : Trigger Scenario

**Valeur** : Doit toujours être "1" dans chaque action

**Rôle** : Cet index déclenche l'exécution du scénario. Sans lui, les autres indices sont ignorés.

**Exemple** :
```json
{"k": 590, "v": "1"}
```

### Index 613 : Éteindre Lumières PDV (LSB)

**Valeur** : Masque de bits (0-255)

**Mapping des bits** :
- Bit 0 (valeur 1) : Lampe de l'entrée
- Bit 1 (valeur 2) : Lampe 1 du salon
- Bit 2 (valeur 4) : Lampe 2 du salon
- Bit 3 (valeur 8) : Lampe du dressing 1
- Bit 4 (valeur 16) : Lampe du dressing 2
- Bit 5 (valeur 32) : Variateur du bureau
- Bit 6 (valeur 64) : Variateur de la salle à manger
- Bit 7 (valeur 128) : Variateur du salon

**Exemple** : Pour éteindre la lampe de l'entrée ET la lampe 1 du salon :
```
Valeur = 1 + 2 = 3
```

### Index 607 : Configuration Alarme (Détecteur d'Ouverture)

**Position** : 602 + 5 = 607 (AlarmeConfig_DetectOuvSurVoieAcces)

**Valeur** :
- 0 : Détecteur d'ouverture pas sur voie d'accès
- 1 : Détecteur d'ouverture sur voie d'accès

### Index 615 : Éteindre Lumières CHB (LSB)

**Valeur** : Masque de bits (0-255)

**Mapping des bits** :
- Bit 0 (valeur 1) : Lampe de l'escalier
- Bit 1 (valeur 2) : Lampe 1 de la grande chambre
- Bit 2 (valeur 4) : Lampe 2 de la grande chambre
- Bit 3 (valeur 8) : Lampe 1 de la petite chambre 1
- Bit 4 (valeur 16) : Lampe 2 de la petite chambre 1
- Bit 5 (valeur 32) : Lampe de la petite chambre 2
- Bit 6 (valeur 64) : Lampe de la petite chambre 3
- Bit 7 (valeur 128) : (Non utilisé dans LSB)

**Exemple** : Pour éteindre la lampe de l'escalier :
```
Valeur = 1
```

## Pourquoi Envoyer la Table Complète ?

### Comportement Critique du Client

Le client embarqué interprète l'**absence d'un index** comme "**garder l'état actuel**". Cela signifie que si vous envoyez seulement :

```json
{"k": 613, "v": "1"}  // Éteindre lampe entrée
```

Le client va :
1. Éteindre la lampe de l'entrée
2. **Conserver l'état de toutes les autres lumières**

### Problème

Si une autre lumière était allumée, elle restera allumée. Pour garantir qu'**une seule lumière** s'allume ou s'éteigne, il faut **explicitement définir l'état de toutes les autres**.

### Solution : Table Complète

Pour allumer **uniquement** la lampe de l'escalier (index 615, bit 0) :

```json
{
  "_de67f": null,
  "actions": [{
    "guid": "abc-123-def-456",
    "params": [
      {"k": 590, "v": "1"},   // Trigger
      {"k": 605, "v": "0"},   // Rien
      {"k": 606, "v": "0"},   // Rien
      {"k": 607, "v": "0"},   // Rien
      {"k": 608, "v": "0"},   // Rien
      {"k": 609, "v": "0"},   // Rien
      {"k": 610, "v": "0"},   // Rien
      {"k": 611, "v": "0"},   // Rien
      {"k": 612, "v": "0"},   // Rien
      {"k": 613, "v": "0"},   // Éteindre PDV LSB
      {"k": 614, "v": "0"},   // Éteindre PDV MSB
      {"k": 615, "v": "0"},   // Éteindre CHB LSB (toutes)
      {"k": 616, "v": "0"},   // Éteindre CHB MSB
      {"k": 617, "v": "0"},   // Éteindre PDE LSB
      {"k": 618, "v": "0"},   // Éteindre PDE MSB
      {"k": 619, "v": "0"},   // Allumer PDV LSB
      {"k": 620, "v": "0"},   // Allumer PDV MSB
      {"k": 621, "v": "1"},   // Allumer CHB LSB (escalier = bit 0)
      {"k": 622, "v": "0"}    // Allumer CHB MSB
    ]
  }]
}
```

## Logique de Merge (Bitwise OR)

### Contexte

Quand plusieurs commandes arrivent via `/api/admin/inject`, le serveur doit les combiner intelligemment avant de les envoyer au client.

### Algorithme

```go
// 1. Initialiser tous les indices 605-622 à 0
mergedValues := make(map[int]int)
for i := 605; i <= 622; i++ {
    mergedValues[i] = 0
}

// 2. Pour chaque paramètre reçu, appliquer un OR bitwise
for _, param := range allParams {
    k := param.K  // Index
    v := parseIntFromString(param.V)  // Valeur
    
    mergedValues[k] = mergedValues[k] | v
}

// 3. Ajouter l'index 590 (trigger)
mergedValues[590] = 1

// 4. Envoyer la table complète au client
```

### Exemple de Merge

**Commande 1** : Allumer lampe entrée (index 619, bit 0)
```json
{"k": 619, "v": "1"}
```

**Commande 2** : Allumer lampe salon (index 619, bit 1)
```json
{"k": 619, "v": "2"}
```

**Résultat après merge** :
```
mergedValues[619] = 1 | 2 = 3
```

**Effet** : Les deux lampes s'allument (bits 0 et 1 activés)

### Pourquoi OR et pas autre chose ?

Le **OR bitwise** permet de :
- Combiner plusieurs commandes sur le même index
- Activer plusieurs bits simultanément
- Ne jamais "perdre" une commande

**Attention** : Le OR ne permet pas d'éteindre. Pour éteindre, il faut utiliser les indices "Eteindre" (613-618).

## Champ `_de67f`

### Position dans le JSON

Le champ `_de67f` **doit être le premier** dans la réponse JSON de `/api/myactions` :

```json
{
  "_de67f": null,  // ← DOIT être en premier
  "actions": [...]
}
```

### Raison

Le parser HTTP du client embarqué est **séquentiel** et **simple**. Il lit le JSON caractère par caractère et s'attend à trouver `_de67f` en premier. Si ce champ n'est pas en première position, le parser peut :
- Ignorer les actions
- Planter
- Avoir un comportement imprévisible

### Valeurs Possibles

**`null`** : Pas d'action alarme spéciale
```json
{"_de67f": null}
```

**Objet avec guid et obl** : Action alarme avec données cryptées
```json
{
  "_de67f": {
    "guid": "2a1a9ea2-941d-456f-9b0a-7e179cf3e864",
    "obl": "124;183;53;98;127;22;30;199;214;125"
  }
}
```

Le champ `obl` contient des données cryptées pour des commandes d'alarme avancées.

## Exemples Complets d'Actions

### Exemple 1 : Allumer la Lumière de l'Escalier

**Objectif** : Allumer uniquement la lampe de l'escalier (index 621, bit 0)

**Requête** :
```json
POST /api/admin/inject
Content-Type: application/json

{
  "params": [
    {"k": 621, "v": "1"}
  ]
}
```

**Réponse du serveur au client** (via `/api/myactions`) :
```json
{
  "_de67f": null,
  "actions": [{
    "guid": "550e8400-e29b-41d4-a716-446655440000",
    "params": [
      {"k": 590, "v": "1"},
      {"k": 605, "v": "0"},
      {"k": 606, "v": "0"},
      {"k": 607, "v": "0"},
      {"k": 608, "v": "0"},
      {"k": 609, "v": "0"},
      {"k": 610, "v": "0"},
      {"k": 611, "v": "0"},
      {"k": 612, "v": "0"},
      {"k": 613, "v": "0"},
      {"k": 614, "v": "0"},
      {"k": 615, "v": "0"},
      {"k": 616, "v": "0"},
      {"k": 617, "v": "0"},
      {"k": 618, "v": "0"},
      {"k": 619, "v": "0"},
      {"k": 620, "v": "0"},
      {"k": 621, "v": "1"},
      {"k": 622, "v": "0"}
    ]
  }]
}
```

### Exemple 2 : Éteindre Toutes les Lumières

**Objectif** : Éteindre toutes les lumières de la maison

**Requête** :
```json
POST /api/admin/inject
Content-Type: application/json

{
  "params": [
    {"k": 613, "v": "255"},  // Éteindre PDV LSB (tous les bits)
    {"k": 614, "v": "255"},  // Éteindre PDV MSB (tous les bits)
    {"k": 615, "v": "255"},  // Éteindre CHB LSB (tous les bits)
    {"k": 616, "v": "255"},  // Éteindre CHB MSB (tous les bits)
    {"k": 617, "v": "255"},  // Éteindre PDE LSB (tous les bits)
    {"k": 618, "v": "255"}   // Éteindre PDE MSB (tous les bits)
  ]
}
```

**Réponse** : Table complète avec indices 613-618 à "255" et tous les autres à "0"

### Exemple 3 : Allumer Plusieurs Lumières Simultanément

**Objectif** : Allumer lampe entrée (bit 0) + lampe salon 1 (bit 1) + lampe salon 2 (bit 2)

**Calcul** :
```
Valeur = 2^0 + 2^1 + 2^2 = 1 + 2 + 4 = 7
```

**Requête** :
```json
POST /api/admin/inject
Content-Type: application/json

{
  "params": [
    {"k": 619, "v": "7"}  // Allumer PDV LSB (bits 0, 1, 2)
  ]
}
```

### Exemple 4 : Scénario "Je Sors"

**Objectif** : Éteindre toutes les lumières, fermer tous les volets, activer l'alarme

**Requête** :
```json
POST /api/admin/inject
Content-Type: application/json

{
  "params": [
    {"k": 601, "v": "1"},    // Activer alarme
    {"k": 613, "v": "255"},  // Éteindre PDV LSB
    {"k": 614, "v": "255"},  // Éteindre PDV MSB
    {"k": 615, "v": "255"},  // Éteindre CHB LSB
    {"k": 616, "v": "255"},  // Éteindre CHB MSB
    {"k": 617, "v": "255"},  // Éteindre PDE LSB
    {"k": 618, "v": "255"},  // Éteindre PDE MSB
    {"k": 628, "v": "63"},   // Fermer volets PDV (0x3F)
    {"k": 629, "v": "31"},   // Fermer volets CHB (0x1F)
    {"k": 630, "v": "7"},    // Fermer volets PDE (0x07)
    {"k": 631, "v": "1"}     // Couper prises sécurité
  ]
}
```

## Historique des Valeurs (25 Entrées)

### Concept

Le serveur conserve les **25 dernières valeurs** de chaque index pour :
- **Debugging** : Comprendre ce qui s'est passé en cas de problème
- **Monitoring** : Afficher l'historique dans l'interface web
- **Détection de patterns** : Identifier des comportements anormaux

### Implémentation

```go
type IndexHistory struct {
    Index     int
    Values    []ValueEntry
    MaxSize   int
}

type ValueEntry struct {
    Value     int
    Timestamp time.Time
    Source    string  // "client", "server", "admin"
}

func (h *IndexHistory) Add(value int, source string) {
    entry := ValueEntry{
        Value:     value,
        Timestamp: time.Now(),
        Source:    source,
    }
    
    h.Values = append(h.Values, entry)
    
    // Garder seulement les 25 dernières
    if len(h.Values) > 25 {
        h.Values = h.Values[len(h.Values)-25:]
    }
}
```

### Exemple d'Historique

```json
{
  "index": 621,
  "name": "Scenario_Allumer_CHB_LSB",
  "history": [
    {"value": 0, "timestamp": "2025-01-08T10:00:00Z", "source": "server"},
    {"value": 1, "timestamp": "2025-01-08T10:05:00Z", "source": "admin"},
    {"value": 0, "timestamp": "2025-01-08T10:10:00Z", "source": "server"},
    {"value": 1, "timestamp": "2025-01-08T10:15:00Z", "source": "client"}
  ]
}
```

### Utilisation

**Interface Web** :
```
Index 621 (Allumer CHB LSB) - Historique
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
10:15:00  [CLIENT]  1  ← Lampe escalier allumée
10:10:00  [SERVER]  0  ← Toutes éteintes
10:05:00  [ADMIN]   1  ← Commande manuelle
10:00:00  [SERVER]  0  ← État initial
```

**Debugging** :
```bash
# Voir l'historique d'un index
curl http://server/api/history/621

# Voir tous les changements dans les dernières 24h
curl http://server/api/history?since=24h
```

## Codes Sources Pertinents

### Définition des Indices (C)

**Fichier** : `H/TableEchange.h`

```c
enum enumScenario
{
    Scenario_Confirme_Scenario,     // 0 -> indice 600
    Scenario_Alarme_ON,             // 1 -> indice 601
    Scenario_AlarmeConfig,          // 2 -> indice 602
    
    Scenario_Eteindre_PDV_LSB = Scenario_AlarmeConfig + AlarmeConfig_NB_VALEURS, // 13 -> indice 613
    Scenario_Eteindre_PDV_MSB,      // 14 -> indice 614
    Scenario_Eteindre_CHB_LSB,      // 15 -> indice 615
    Scenario_Eteindre_CHB_MSB,      // 16 -> indice 616
    Scenario_Eteindre_PDE_LSB,      // 17 -> indice 617
    Scenario_Eteindre_PDE_MSB,      // 18 -> indice 618
    
    Scenario_Allumer_PDV_LSB,       // 19 -> indice 619
    Scenario_Allumer_PDV_MSB,       // 20 -> indice 620
    Scenario_Allumer_CHB_LSB,       // 21 -> indice 621
    Scenario_Allumer_CHB_MSB,       // 22 -> indice 622
    // ...
};
```

### Accès à la Table (C)

**Fichier** : `C/TableEchangeAcces.c`

```c
// Écrire une donnée dans la table d'échange
unsigned char uc_TableEchange_Ecrit_Data(
    unsigned short us_Numero,      // Index
    unsigned char uc_Donnee,       // Valeur
    unsigned char uc_OrdreServeur  // 0=local, 1=serveur
) {
    if (us_Numero < Nb_Tbb_Donnees) {
        if ((Tb_Echange_Droits[us_Numero] & ACCES_ECRITURE) != 0) {
            Tb_Echange[us_Numero] = uc_Donnee;
            return uc_BP_OK;
        }
    }
    return uc_BP_PB_ACCES;
}

// Lire une donnée de la table d'échange
unsigned char uc_TableEchange_Lit_Data(unsigned short us_Numero) {
    if (us_Numero < Nb_Tbb_Donnees) {
        if ((Tb_Echange_Droits[us_Numero] & ACCES_LECTURE) != 0) {
            return Tb_Echange[us_Numero];
        }
    }
    return 0;
}
```

### Parsing JSON (C)

**Fichier** : `Ethernet/Json.c`

```c
// Extraire les paramètres d'une action
signed char sc_JsonGetServerUpdateInformation(
    char *pc_Params,
    struct StructServerUpdateInformation st_InfosDemandees[],
    unsigned char *uc_NbInfosDemandees
) {
    // Parse: {"k":621,"v":"1"},{"k":622,"v":"0"}
    // Extrait les paires (index, valeur)
    // ...
}
```

## Résumé des Règles Importantes

1. ✅ **Toujours envoyer la table complète** (indices 605-622) pour garantir l'état désiré
2. ✅ **Initialiser les indices non-mentionnés à "0"** pour éviter les états résiduels
3. ✅ **Inclure l'index 590 = "1"** dans chaque action (trigger scenario)
4. ✅ **Champ `_de67f` en premier** dans la réponse `/api/myactions`
5. ✅ **Utiliser OR bitwise** pour combiner plusieurs commandes sur le même index
6. ✅ **Conserver 25 valeurs d'historique** par index pour debugging
7. ✅ **Masques de bits** : Chaque bit représente un équipement (lampe, volet, etc.)
8. ✅ **Indices "Allumer" vs "Éteindre"** : Utiliser les bons indices selon l'action voulue

## Références

- **Fichier header** : `H/TableEchange.h` - Définitions complètes des indices
- **Accès table** : `C/TableEchangeAcces.c` - Fonctions de lecture/écriture
- **Scénarios** : `C/Scenario.c` - Logique d'exécution des scénarios
- **JSON parsing** : `Ethernet/Json.c` - Parsing des commandes serveur
- **Protocol HTTP** : `docs/protocol/http-legacy-protocol.md` - Contraintes HTTP
- **TCP single-packet** : `docs/protocol/tcp-single-packet.md` - Contrainte critique TCP
