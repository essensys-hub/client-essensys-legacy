# Protocole HTTP Legacy - Client BP_MQX_ETH

## Table des Matières

- [Vue d'Ensemble](#vue-densemble)
- [Comportement Non-Standard](#comportement-non-standard)
- [JSON Malformé](#json-malformé)
- [Headers HTTP Requis](#headers-http-requis)
- [Endpoints API](#endpoints-api)
- [Format des Réponses](#format-des-réponses)
- [Codes de Statut](#codes-de-statut)
- [Exemples Complets](#exemples-complets)
- [Références](#références)

## Vue d'Ensemble

Le client BP_MQX_ETH est un système embarqué ancien qui communique avec le serveur via un protocole HTTP **non-standard**. Ce protocole présente plusieurs particularités qui doivent être respectées pour assurer la compatibilité avec le client.

### Caractéristiques Principales

- **JSON malformé**: Clés non-quotées dans les requêtes du client
- **Headers incomplets**: Headers HTTP non-standard ou manquants
- **Content-Type spécifique**: Format exact requis avec espace avant `;charset`
- **Codes de statut**: Utilisation de `201 Created` au lieu de `200 OK`
- **Single-packet TCP**: Toute la réponse doit être envoyée en un seul paquet (voir [tcp-single-packet.md](./tcp-single-packet.md))

### Contexte Technique

Le client est basé sur:
- **Microcontrôleur**: Freescale Coldfire MCF52259
- **RTOS**: MQX (Freescale Real-Time Operating System)
- **Stack TCP/IP**: RTCS (Real-Time Communication Stack)
- **Parser HTTP**: Implémentation simplifiée avec limitations

## Comportement Non-Standard

### 1. Parser HTTP Simplifié

Le client BP_MQX_ETH utilise un parser HTTP extrêmement basique qui:

- Ne supporte pas la fragmentation TCP (voir [tcp-single-packet.md](./tcp-single-packet.md))
- Ne gère pas tous les headers HTTP standards
- Attend un format très spécifique pour certains headers
- Ne valide pas strictement la syntaxe HTTP

### 2. Limitations de Mémoire

Le système embarqué a des contraintes de mémoire importantes:

- **Stack limitée**: Tâches MQX avec stacks de 2-4 KB
- **Heap limité**: Mémoire dynamique rare
- **Buffers fixes**: Pas de réallocation dynamique

Ces limitations expliquent pourquoi le parser HTTP est si simple et pourquoi certaines fonctionnalités HTTP standards ne sont pas supportées.

### 3. Gestion des Erreurs

Le client ne gère pas toujours les erreurs HTTP de manière standard:

- Peut se bloquer silencieusement en cas de réponse mal formatée
- Ne génère pas toujours de timeout
- Ne retry pas automatiquement en cas d'échec

## JSON Malformé

### Problème

Le client envoie du JSON avec **clés non-quotées**, ce qui viole la spécification JSON (RFC 8259):

```json
// JSON envoyé par le client (INVALIDE selon RFC 8259)
{version:"1.0",ek:[{k:613,v:"1"},{k:607,v:"0"}]}
```

```json
// JSON valide attendu
{"version":"1.0","ek":[{"k":613,"v":"1"},{"k":607,"v":"0"}]}
```

### Explication Technique

Le parser JSON du client embarqué est une implémentation simplifiée qui:
- Ne quote pas les clés pour économiser de la mémoire
- Réduit la taille des messages transmis
- Simplifie le code de génération JSON

### Solution: Normalisation

Le serveur doit normaliser le JSON reçu avant de le parser:

```go
// Fonction de normalisation du JSON malformé
func normalizeJSON(bodyStr string) string {
    // Ajouter les quotes manquantes autour des clés
    bodyStr = strings.ReplaceAll(bodyStr, "{version:", "{\"version\":")
    bodyStr = strings.ReplaceAll(bodyStr, ",version:", ",\"version\":")
    bodyStr = strings.ReplaceAll(bodyStr, "{ek:", "{\"ek\":")
    bodyStr = strings.ReplaceAll(bodyStr, ",ek:", ",\"ek\":")
    bodyStr = strings.ReplaceAll(bodyStr, "{k:", "{\"k\":")
    bodyStr = strings.ReplaceAll(bodyStr, ",k:", ",\"k\":")
    bodyStr = strings.ReplaceAll(bodyStr, ",v:", ",\"v\":")
    
    return bodyStr
}
```

### Exemple d'Utilisation

```go
// Dans le handler HTTP
func MyStatusHandler(w http.ResponseWriter, r *http.Request) {
    // Lire le body
    body, err := ioutil.ReadAll(r.Body)
    if err != nil {
        http.Error(w, "Cannot read body", http.StatusBadRequest)
        return
    }
    
    // Normaliser le JSON malformé
    bodyStr := normalizeJSON(string(body))
    
    // Parser le JSON normalisé
    var status MyStatusRequest
    err = json.Unmarshal([]byte(bodyStr), &status)
    if err != nil {
        http.Error(w, "Invalid JSON", http.StatusBadRequest)
        return
    }
    
    // Traiter la requête...
}
```

### Clés Communes à Normaliser

| Clé | Contexte | Exemple |
|-----|----------|---------|
| `version` | Toutes les requêtes | `{version:"1.0"}` |
| `ek` | mystatus (exchange keys) | `{ek:[...]}` |
| `k` | Paramètres (key) | `{k:613}` |
| `v` | Paramètres (value) | `{v:"1"}` |
| `_de67f` | myactions | `{_de67f:null}` |
| `actions` | myactions | `{actions:[...]}` |
| `guid` | Actions | `{guid:"abc-123"}` |
| `params` | Actions | `{params:[...]}` |

## Headers HTTP Requis

### Content-Type Critique

Le serveur **doit** répondre avec ce header exact:

```
Content-Type: application/json ;charset=UTF-8
```

**Points critiques**:
- Espace **avant** le point-virgule: ` ;charset` (pas `;charset`)
- Casse exacte: `application/json` (pas `Application/Json`)
- Charset UTF-8: `;charset=UTF-8` (pas `utf-8` ou autre)

### Exemple Incorrect

```
Content-Type: application/json;charset=UTF-8    ❌ Pas d'espace avant ;
Content-Type: application/json; charset=UTF-8   ❌ Espace après ;
Content-Type: Application/JSON ;charset=UTF-8   ❌ Mauvaise casse
```

### Exemple Correct

```
Content-Type: application/json ;charset=UTF-8   ✅ Format exact requis
```

### Headers Standards

Le serveur doit également inclure ces headers standards:

```
HTTP/1.1 201 Created
Connection: close
Content-Length: 123
Content-Type: application/json ;charset=UTF-8
```

**Explications**:

- `Connection: close`: Le client ne supporte pas les connexions persistantes
- `Content-Length`: Taille exacte du body en bytes
- `HTTP/1.1`: Version du protocole (le client ne supporte pas HTTP/2)

### Headers Optionnels

Ces headers peuvent être omis sans problème:

- `Date`: Le client ne l'utilise pas
- `Server`: Le client ne l'utilise pas
- `Cache-Control`: Le client ne cache pas
- `ETag`: Le client ne gère pas le caching

## Endpoints API

### Vue d'Ensemble

Le protocole définit 5 endpoints principaux:

| Endpoint | Méthode | Fréquence | Description |
|----------|---------|-----------|-------------|
| `/api/serverinfos` | GET | ~20 secondes | Informations serveur |
| `/api/mystatus` | POST | ~2 secondes | État du client |
| `/api/myactions` | GET | ~2 secondes | Actions à exécuter |
| `/api/done/{guid}` | POST | À la demande | Acknowledge action |
| `/api/admin/inject` | POST | À la demande | Injection admin |

### 1. GET /api/serverinfos

**Description**: Retourne les informations du serveur et la liste des indices à surveiller.

**Requête**:
```http
GET /api/serverinfos HTTP/1.1
Host: 192.168.0.1
Connection: close
```

**Réponse**:
```http
HTTP/1.1 200 OK
Connection: close
Content-Length: 96
Content-Type: application/json ;charset=UTF-8

{"version":"1.0","indices":[605,606,607,608,609,610,611,612,613,614,615,616,617,618,619,620,621,622]}
```

**Format de la Réponse**:
```json
{
  "version": "1.0",
  "indices": [605, 606, 607, ..., 622]
}
```

**Notes**:
- Appelé au démarrage du client et périodiquement (~20 secondes)
- Liste tous les indices de la table d'échange à surveiller
- Utilise `200 OK` (pas `201 Created`)

### 2. POST /api/mystatus

**Description**: Le client envoie son état actuel (valeurs de la table d'échange).

**Requête**:
```http
POST /api/mystatus HTTP/1.1
Host: 192.168.0.1
Content-Length: 87
Content-Type: application/json
Connection: close

{version:"1.0",ek:[{k:605,v:"0"},{k:606,v:"1"},{k:607,v:"0"},{k:613,v:"1"}]}
```

**Note**: Le JSON est malformé (clés non-quotées) et doit être normalisé.

**Réponse**:
```http
HTTP/1.1 201 Created
Connection: close
Content-Length: 2
Content-Type: application/json ;charset=UTF-8

{}
```

**Format de la Requête** (après normalisation):
```json
{
  "version": "1.0",
  "ek": [
    {"k": 605, "v": "0"},
    {"k": 606, "v": "1"},
    {"k": 607, "v": "0"},
    {"k": 613, "v": "1"}
  ]
}
```

**Notes**:
- Appelé toutes les ~2 secondes
- `ek` = "exchange keys" (clés de la table d'échange)
- Chaque élément contient `k` (key/index) et `v` (value)
- Répondre avec `201 Created` (pas `200 OK`)
- Le serveur stocke ces valeurs pour l'historique

### 3. GET /api/myactions

**Description**: Le client récupère les actions à exécuter.

**Requête**:
```http
GET /api/myactions HTTP/1.1
Host: 192.168.0.1
Connection: close
```

**Réponse (sans actions)**:
```http
HTTP/1.1 200 OK
Connection: close
Content-Length: 25
Content-Type: application/json ;charset=UTF-8

{"_de67f":null,"actions":[]}
```

**Réponse (avec action)**:
```http
HTTP/1.1 200 OK
Connection: close
Content-Length: 456
Content-Type: application/json ;charset=UTF-8

{"_de67f":null,"actions":[{"guid":"abc-123-def-456","params":[{"k":605,"v":"0"},{"k":606,"v":"0"},{"k":607,"v":"0"},{"k":608,"v":"0"},{"k":609,"v":"0"},{"k":610,"v":"0"},{"k":611,"v":"0"},{"k":612,"v":"0"},{"k":613,"v":"1"},{"k":614,"v":"0"},{"k":615,"v":"0"},{"k":616,"v":"0"},{"k":617,"v":"0"},{"k":618,"v":"0"},{"k":619,"v":"0"},{"k":620,"v":"0"},{"k":621,"v":"0"},{"k":622,"v":"0"},{"k":590,"v":"1"}]}]}
```

**Format de la Réponse**:
```json
{
  "_de67f": null,
  "actions": [
    {
      "guid": "abc-123-def-456",
      "params": [
        {"k": 605, "v": "0"},
        {"k": 606, "v": "0"},
        // ... tous les indices 605-622 ...
        {"k": 613, "v": "1"},  // Action: allumer index 613
        // ... tous les autres à 0 ...
        {"k": 622, "v": "0"},
        {"k": 590, "v": "1"}   // Trigger scenario
      ]
    }
  ]
}
```

**Notes Critiques**:
- Le champ `_de67f` **doit être en premier** dans le JSON
- Toujours inclure **tous les indices 605-622** (voir [exchange-table.md](./exchange-table.md))
- Toujours inclure l'index 590 avec valeur "1"
- Appelé toutes les ~2 secondes
- Le `guid` permet d'identifier l'action pour l'acknowledge

### 4. POST /api/done/{guid}

**Description**: Le client acknowledge qu'une action a été exécutée.

**Requête**:
```http
POST /api/done/abc-123-def-456 HTTP/1.1
Host: 192.168.0.1
Content-Length: 0
Connection: close
```

**Réponse**:
```http
HTTP/1.1 201 Created
Connection: close
Content-Length: 2
Content-Type: application/json ;charset=UTF-8

{}
```

**Notes**:
- Le `guid` dans l'URL correspond au `guid` de l'action
- Pas de body dans la requête
- Répondre avec `201 Created`
- Le serveur retire l'action de la queue après acknowledge

### 5. POST /api/admin/inject

**Description**: Endpoint d'administration pour injecter des commandes.

**Requête**:
```http
POST /api/admin/inject HTTP/1.1
Host: 192.168.0.1
Content-Type: application/json
Content-Length: 45
Connection: close

{"_de67f":"1","605":"1","613":"1","590":"1"}
```

**Réponse**:
```http
HTTP/1.1 201 Created
Connection: close
Content-Length: 2
Content-Type: application/json ;charset=UTF-8

{}
```

**Format de la Requête**:
```json
{
  "_de67f": "1",
  "605": "1",      // Index à modifier
  "613": "1",      // Autre index à modifier
  "590": "1"       // Trigger scenario
}
```

**Notes**:
- Utilisé par l'interface web ou des scripts d'administration
- Les indices sont des strings (clés JSON)
- Le serveur merge ces valeurs avec la logique bitwise OR
- Génère une action qui sera récupérée via `/api/myactions`

## Format des Réponses

### Structure Générale

Toutes les réponses HTTP doivent suivre ce format:

```
HTTP/1.1 {STATUS_CODE} {STATUS_TEXT}\r\n
Connection: close\r\n
Content-Length: {BODY_LENGTH}\r\n
Content-Type: application/json ;charset=UTF-8\r\n
\r\n
{JSON_BODY}
```

### Exemple Complet

```
HTTP/1.1 201 Created\r\n
Connection: close\r\n
Content-Length: 25\r\n
Content-Type: application/json ;charset=UTF-8\r\n
\r\n
{"_de67f":null,"actions":[]}
```

### Points Critiques

1. **Ligne de fin**: Chaque header se termine par `\r\n`
2. **Séparateur headers/body**: Double `\r\n` entre headers et body
3. **Content-Length exact**: Doit correspondre à la taille du body en bytes
4. **Single-packet**: Toute la réponse doit être envoyée en un seul paquet TCP

## Codes de Statut

### Utilisation Non-Standard

Le protocole utilise les codes de statut HTTP de manière non-standard:

| Endpoint | Méthode | Code Standard | Code Utilisé | Raison |
|----------|---------|---------------|--------------|--------|
| `/api/serverinfos` | GET | 200 OK | 200 OK | ✅ Standard |
| `/api/mystatus` | POST | 200 OK | **201 Created** | ❌ Non-standard |
| `/api/myactions` | GET | 200 OK | 200 OK | ✅ Standard |
| `/api/done/{guid}` | POST | 200 OK | **201 Created** | ❌ Non-standard |
| `/api/admin/inject` | POST | 200 OK | **201 Created** | ❌ Non-standard |

### Explication

Le client s'attend à recevoir `201 Created` pour les requêtes POST, même si aucune ressource n'est réellement "créée" au sens REST du terme. C'est une particularité du protocole legacy.

### Codes d'Erreur

En cas d'erreur, le serveur peut répondre avec:

| Code | Signification | Utilisation |
|------|---------------|-------------|
| 400 Bad Request | JSON invalide ou malformé | Erreur de parsing |
| 404 Not Found | Endpoint inconnu | Route invalide |
| 500 Internal Server Error | Erreur serveur | Erreur interne |

**Note**: Le client ne gère pas toujours bien les erreurs HTTP. Il peut se bloquer ou ignorer l'erreur.

## Exemples Complets

### Exemple 1: Séquence de Démarrage

```
1. Client → Serveur: GET /api/serverinfos
   Serveur → Client: 200 OK + liste des indices

2. Client → Serveur: POST /api/mystatus (état initial)
   Serveur → Client: 201 Created

3. Client → Serveur: GET /api/myactions
   Serveur → Client: 200 OK + actions:[]
```

### Exemple 2: Exécution d'une Action

```
1. Admin → Serveur: POST /api/admin/inject {"605":"1","590":"1"}
   Serveur → Admin: 201 Created

2. Client → Serveur: GET /api/myactions
   Serveur → Client: 200 OK + action avec guid="xyz"

3. Client exécute l'action (allume lumière 605)

4. Client → Serveur: POST /api/done/xyz
   Serveur → Client: 201 Created

5. Client → Serveur: POST /api/mystatus (nouvel état avec 605="1")
   Serveur → Client: 201 Created
```

### Exemple 3: Allumer une Lumière (Index 613)

**Étape 1: Injection de la commande**

```http
POST /api/admin/inject HTTP/1.1
Content-Type: application/json

{"_de67f":"1","613":"1","590":"1"}
```

**Étape 2: Client récupère l'action**

```http
GET /api/myactions HTTP/1.1
```

**Réponse**:
```json
{
  "_de67f": null,
  "actions": [{
    "guid": "action-001",
    "params": [
      {"k": 605, "v": "0"},
      {"k": 606, "v": "0"},
      {"k": 607, "v": "0"},
      {"k": 608, "v": "0"},
      {"k": 609, "v": "0"},
      {"k": 610, "v": "0"},
      {"k": 611, "v": "0"},
      {"k": 612, "v": "0"},
      {"k": 613, "v": "1"},  // ← Lumière à allumer
      {"k": 614, "v": "0"},
      {"k": 615, "v": "0"},
      {"k": 616, "v": "0"},
      {"k": 617, "v": "0"},
      {"k": 618, "v": "0"},
      {"k": 619, "v": "0"},
      {"k": 620, "v": "0"},
      {"k": 621, "v": "0"},
      {"k": 622, "v": "0"},
      {"k": 590, "v": "1"}   // ← Trigger
    ]
  }]
}
```

**Étape 3: Client acknowledge**

```http
POST /api/done/action-001 HTTP/1.1
```

### Exemple 4: Polling Continu

Le client effectue un polling continu:

```
T+0s:  POST /api/mystatus → 201 Created
T+0s:  GET /api/myactions → 200 OK (actions:[])
T+2s:  POST /api/mystatus → 201 Created
T+2s:  GET /api/myactions → 200 OK (actions:[])
T+4s:  POST /api/mystatus → 201 Created
T+4s:  GET /api/myactions → 200 OK (actions:[...]) ← Action disponible
T+4s:  POST /api/done/xyz → 201 Created
T+6s:  POST /api/mystatus → 201 Created (nouvel état)
T+6s:  GET /api/myactions → 200 OK (actions:[])
...
```

## Références

### Documentation Connexe

- [TCP Single-Packet Requirement](./tcp-single-packet.md) - Contrainte critique TCP
- [Exchange Table](./exchange-table.md) - Table d'échange (indices 605-622)
- [Embedded Client Architecture](../embedded-client/architecture.md) - Architecture MQX
- [API Endpoints](../server/api-endpoints.md) - Documentation détaillée des endpoints
- [Go Implementation](../server/go-implementation.md) - Implémentation serveur

### Standards HTTP

- [RFC 7230 - HTTP/1.1 Message Syntax and Routing](https://tools.ietf.org/html/rfc7230)
- [RFC 8259 - JSON Data Interchange Format](https://tools.ietf.org/html/rfc8259)

### Code Source

- `Ethernet/www.c` - Parser HTTP côté client
- `Ethernet/Json.c` - Parser JSON côté client
- `Ethernet/GestionSocket.c` - Gestion des sockets côté client

---

**Dernière mise à jour**: 2025-01-08  
**Auteur**: Documentation du projet client-essensys-legacy  
**Statut**: Validé et testé avec le client BP_MQX_ETH réel
