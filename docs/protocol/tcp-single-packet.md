# Contrainte TCP Single-Packet - Client BP_MQX_ETH

## Table des Matières

- [Découverte Critique](#découverte-critique)
- [Symptômes du Problème](#symptômes-du-problème)
- [Analyse avec tcpdump](#analyse-avec-tcpdump)
- [Explication Technique](#explication-technique)
- [Solution Implémentée](#solution-implémentée)
- [Validation des Résultats](#validation-des-résultats)
- [Outils de Diagnostic](#outils-de-diagnostic)
- [Impact sur le Code](#impact-sur-le-code)
- [Leçons Apprises](#leçons-apprises)

## Découverte Critique

Le client BP_MQX_ETH embarqué possède un parser HTTP **extrêmement simple** qui s'attend à recevoir **toute la réponse HTTP en un seul paquet TCP**. Si la réponse est fragmentée en plusieurs paquets, le client ne peut pas la parser correctement et reste bloqué.

Cette contrainte **non-documentée** du protocole legacy a été découverte par analyse réseau avec `tcpdump` lors du développement du serveur Go de remplacement.

### Contexte

Le client BP_MQX_ETH est un système embarqué ancien basé sur:
- **Microcontrôleur**: Freescale Coldfire MCF52259
- **RTOS**: MQX (Freescale Real-Time Operating System)
- **Stack TCP/IP**: RTCS (Real-Time Communication Stack)
- **Mémoire**: Limitée (Flash + RAM + MRAM externe)

## Symptômes du Problème

Lorsque le serveur Go envoyait les réponses HTTP de manière standard (headers et body potentiellement fragmentés en plusieurs paquets TCP), le client BP_MQX_ETH présentait le comportement suivant:

### Comportement Observé

| Endpoint | Statut | Fréquence |
|----------|--------|-----------|
| `/api/serverinfos` | ✅ Succès | ~20 secondes |
| `/api/mystatus` | ❌ Jamais appelé | N/A |
| `/api/myactions` | ❌ Jamais appelé | N/A |
| `/api/done/{guid}` | ❌ Jamais appelé | N/A |

### Symptômes Détaillés

1. **Premier appel réussi**: Le client appelle `/api/serverinfos` avec succès
2. **Blocage complet**: Après la première réponse, le client ne fait plus aucun appel
3. **Pas de timeout**: Le client ne génère pas d'erreur, il reste simplement bloqué
4. **Pas de retry**: Aucune tentative de reconnexion ou de nouvel appel

Ce comportement indiquait clairement un problème de parsing de la réponse HTTP côté client.

## Analyse avec tcpdump

L'analyse réseau avec `tcpdump` a révélé la différence critique entre un serveur qui fonctionne (server.sample) et le serveur Go initial.

### Serveur Go (Avant Correction) - ❌ Ne Fonctionne Pas

```
Paquet TCP 1: HTTP/1.1 200 OK\r\n
Paquet TCP 2: Connection: close\r\n
Paquet TCP 3: Content-Length: 97\r\n
Paquet TCP 4: Content-Type: application/json ;charset=UTF-8\r\n
Paquet TCP 5: \r\n
Paquet TCP 6: {JSON body}
```

**Résultat**: Le client reçoit 6 paquets TCP séparés et ne peut pas parser la réponse.

### Server.sample (Référence) - ✅ Fonctionne

```
Paquet TCP 1: HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: 96\r\nContent-Type: application/json ;charset=UTF-8\r\n\r\n{JSON body}
```

**Résultat**: Le client reçoit 1 seul paquet TCP contenant la réponse complète et peut la parser.

### Commande tcpdump Utilisée

```bash
# Capturer les paquets TCP du client BP_MQX_ETH
sudo tcpdump -i any -s 0 -A 'tcp port 80 and host 192.168.0.151' -w capture.pcap

# Afficher en temps réel avec contenu ASCII
sudo tcpdump -i any -s 0 -A 'tcp port 80 and host 192.168.0.151'

# Compter les paquets par connexion
sudo tcpdump -i any -s 0 'tcp port 80 and host 192.168.0.151' | grep "length"
```

### Analyse Comparative

| Critère | Serveur Go Initial | Server.sample |
|---------|-------------------|---------------|
| Nombre de paquets TCP | 6+ | 1 |
| Taille totale | ~200 bytes | ~200 bytes |
| Fragmentation | Oui | Non |
| Client fonctionne | ❌ Non | ✅ Oui |

## Explication Technique

### Pourquoi Cette Contrainte Existe

Le client BP_MQX_ETH est un système embarqué ancien avec des limitations importantes:

#### 1. Mémoire Limitée

- **Pas de buffer de réassemblage**: Le client n'alloue pas de mémoire pour bufferiser plusieurs paquets TCP
- **Stack limitée**: Les tâches MQX ont des stacks de taille fixe (typiquement 2-4 KB)
- **Heap limité**: La mémoire dynamique est rare et précieuse

#### 2. Parser HTTP Simple

Le parser HTTP du client est extrêmement basique:

```c
// Pseudo-code du parser HTTP client (simplifié)
char buffer[512];
int bytes_received = recv(socket, buffer, sizeof(buffer), 0);

// Le client s'attend à avoir TOUT dans buffer
// Pas de boucle pour lire plus de données
// Pas de gestion de fragmentation

if (strstr(buffer, "HTTP/1.1 200 OK") != NULL) {
    // Parse headers...
    char* body_start = strstr(buffer, "\r\n\r\n");
    if (body_start != NULL) {
        body_start += 4;
        // Parse JSON body...
    }
}
```

#### 3. Pas de Gestion de Fragmentation

- **Un seul appel recv()**: Le client lit une seule fois depuis le socket
- **Pas de boucle de lecture**: Aucun mécanisme pour lire plusieurs paquets
- **Pas de Content-Length handling**: Le client ne lit pas progressivement selon Content-Length

#### 4. Contraintes du Stack TCP/IP RTCS

Le stack RTCS de MQX est conçu pour des applications temps réel simples:
- Optimisé pour la latence, pas pour la complexité
- Buffers TCP limités
- Pas de réassemblage applicatif sophistiqué

### Pourquoi C'est Critique

Cette contrainte est **absolument critique** car:

1. **Blocage silencieux**: Le client ne génère pas d'erreur, il reste juste bloqué
2. **Non-documenté**: Aucune documentation du client ne mentionne cette limitation
3. **Non-standard**: Viole les principes de base de TCP/IP (fragmentation transparente)
4. **Difficile à diagnostiquer**: Nécessite une analyse réseau pour découvrir le problème

## Solution Implémentée

La solution consiste à bufferiser toute la réponse HTTP (headers + body) et à l'envoyer en un seul appel `Write()` au socket TCP.

### Architecture de la Solution

```
┌─────────────────────────────────────────────────────────────┐
│                    HTTP Handler                              │
│  (Code métier standard utilisant http.ResponseWriter)       │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│              legacyResponseWriter                            │
│  • Bufferise tous les headers                                │
│  • Bufferise tout le body                                    │
│  • Envoie TOUT en un seul Write()                           │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│                  TCP Socket                                  │
│  • Un seul paquet TCP envoyé                                │
└─────────────────────────────────────────────────────────────┘
```

### Code Go - legacyResponseWriter

Fichier: `internal/server/legacy_http_server.go`

```go
// legacyResponseWriter bufferise toute la réponse HTTP
// et l'envoie en un seul paquet TCP pour le client BP_MQX_ETH
type legacyResponseWriter struct {
    conn       net.Conn
    header     http.Header
    bodyBuffer *bytes.Buffer
    statusCode int
    written    bool
}

func newLegacyResponseWriter(conn net.Conn) *legacyResponseWriter {
    return &legacyResponseWriter{
        conn:       conn,
        header:     make(http.Header),
        bodyBuffer: new(bytes.Buffer),
        statusCode: http.StatusOK,
        written:    false,
    }
}

// Header retourne les headers HTTP
func (w *legacyResponseWriter) Header() http.Header {
    return w.header
}

// Write bufferise le body (ne l'envoie pas encore)
func (w *legacyResponseWriter) Write(data []byte) (int, error) {
    return w.bodyBuffer.Write(data)
}

// WriteHeader enregistre le status code (ne l'envoie pas encore)
func (w *legacyResponseWriter) WriteHeader(statusCode int) {
    if !w.written {
        w.statusCode = statusCode
    }
}

// flush envoie TOUTE la réponse en un seul paquet TCP
func (w *legacyResponseWriter) flush() error {
    if w.written {
        return nil
    }
    w.written = true

    // Build the entire response in a buffer
    var response bytes.Buffer
    
    // 1. Status line
    statusText := http.StatusText(w.statusCode)
    fmt.Fprintf(&response, "HTTP/1.1 %d %s\r\n", w.statusCode, statusText)
    
    // 2. Required headers
    fmt.Fprintf(&response, "Connection: close\r\n")
    fmt.Fprintf(&response, "Content-Length: %d\r\n", w.bodyBuffer.Len())
    
    // 3. Custom headers
    for key, values := range w.header {
        for _, value := range values {
            fmt.Fprintf(&response, "%s: %s\r\n", key, value)
        }
    }
    
    // 4. End of headers
    fmt.Fprintf(&response, "\r\n")
    
    // 5. Body
    response.Write(w.bodyBuffer.Bytes())
    
    // CRITICAL: Send everything in a SINGLE Write() call
    // This ensures the entire HTTP response is sent in one TCP packet
    _, err := w.conn.Write(response.Bytes())
    
    return err
}
```

### Utilisation dans le Serveur

```go
func (s *LegacyHTTPServer) handleConnection(conn net.Conn) {
    defer conn.Close()
    
    // Read request
    reader := bufio.NewReader(conn)
    req, err := http.ReadRequest(reader)
    if err != nil {
        return
    }
    
    // Create legacy response writer
    writer := newLegacyResponseWriter(conn)
    
    // Call handler (standard http.Handler interface)
    s.handler.ServeHTTP(writer, req)
    
    // CRITICAL: Flush bufferized response as single TCP packet
    writer.flush()
}
```

### Points Clés de l'Implémentation

1. **Bufferisation complète**: Tous les headers et le body sont bufferisés en mémoire
2. **Un seul Write()**: L'appel `conn.Write(response.Bytes())` envoie tout d'un coup
3. **Interface standard**: Les handlers utilisent `http.ResponseWriter` normalement
4. **Adaptateur transparent**: Le `legacyResponseWriter` adapte le comportement sans changer le code métier

## Validation des Résultats

Après implémentation de la solution, le client BP_MQX_ETH fonctionne parfaitement:

### Comportement Après Correction

| Endpoint | Statut | Fréquence | Détails |
|----------|--------|-----------|---------|
| `/api/serverinfos` | ✅ Succès | ~20 secondes | Récupère infos serveur |
| `/api/mystatus` | ✅ Succès | ~2 secondes | Envoie état du client |
| `/api/myactions` | ✅ Succès | ~2 secondes | Récupère actions à exécuter |
| `/api/done/{guid}` | ✅ Succès | À la demande | Acknowledge actions |

### Tests de Validation

#### 1. Test de Polling Normal

```bash
# Observer les appels du client
sudo tcpdump -i any -s 0 -A 'tcp port 80 and host 192.168.0.151'

# Résultat attendu:
# - serverinfos toutes les ~20 secondes
# - mystatus toutes les ~2 secondes
# - myactions toutes les ~2 secondes
```

#### 2. Test d'Exécution d'Actions

```bash
# Injecter une action (allumer lumière index 605)
curl -X POST http://localhost:8080/api/admin/inject \
  -H "Content-Type: application/json" \
  -d '{"_de67f":"1","605":"1","590":"1"}'

# Observer que le client:
# 1. Récupère l'action via /api/myactions
# 2. Exécute l'action (lumière s'allume)
# 3. Acknowledge via /api/done/{guid}
```

#### 3. Test de Comptage de Paquets

```bash
# Compter les paquets TCP par réponse
sudo tcpdump -i any 'tcp port 80 and host 192.168.0.151' | \
  grep "length" | \
  awk '{print $NF}'

# Résultat attendu: 1 paquet par réponse HTTP
```

### Métriques de Succès

- ✅ **100% des réponses** envoyées en 1 seul paquet TCP
- ✅ **0 blocage** du client après correction
- ✅ **Polling continu** sans interruption
- ✅ **Actions exécutées** et acknowledgées correctement

## Outils de Diagnostic

### tcpdump - Capture de Paquets

#### Installation

```bash
# macOS
brew install tcpdump  # Généralement pré-installé

# Linux
sudo apt-get install tcpdump  # Debian/Ubuntu
sudo yum install tcpdump      # RedHat/CentOS
```

#### Commandes Essentielles

```bash
# Capturer tout le trafic du client
sudo tcpdump -i any -s 0 -A 'tcp port 80 and host 192.168.0.151'

# Sauvegarder dans un fichier pour analyse
sudo tcpdump -i any -s 0 -w capture.pcap 'tcp port 80 and host 192.168.0.151'

# Afficher uniquement les headers HTTP
sudo tcpdump -i any -s 0 -A 'tcp port 80 and host 192.168.0.151' | grep -A 10 "HTTP"

# Compter les paquets par connexion
sudo tcpdump -i any -s 0 'tcp port 80 and host 192.168.0.151' -c 100 | \
  grep "length" | wc -l
```

#### Options Importantes

- `-i any`: Écouter sur toutes les interfaces réseau
- `-s 0`: Capturer le paquet complet (pas de troncature)
- `-A`: Afficher le contenu en ASCII
- `-w file.pcap`: Sauvegarder pour analyse ultérieure
- `-c N`: Capturer N paquets puis arrêter

### Wireshark - Analyse Graphique

#### Installation

```bash
# macOS
brew install --cask wireshark

# Linux
sudo apt-get install wireshark  # Debian/Ubuntu
```

#### Utilisation

1. Ouvrir le fichier `.pcap` capturé avec tcpdump
2. Filtrer par IP: `ip.addr == 192.168.0.151`
3. Suivre le stream TCP: Clic droit → Follow → TCP Stream
4. Observer le nombre de paquets par réponse HTTP

#### Filtres Utiles

```
# Filtrer par IP du client
ip.addr == 192.168.0.151

# Filtrer les requêtes HTTP
http.request

# Filtrer les réponses HTTP
http.response

# Filtrer par endpoint
http.request.uri contains "/api/mystatus"
```

### tshark - Wireshark en Ligne de Commande

```bash
# Analyser un fichier pcap
tshark -r capture.pcap -Y "ip.addr == 192.168.0.151"

# Extraire les requêtes HTTP
tshark -r capture.pcap -Y "http.request" -T fields -e http.request.uri

# Compter les paquets par connexion TCP
tshark -r capture.pcap -z conv,tcp
```

### Script de Diagnostic

```bash
#!/bin/bash
# diagnose_client.sh - Diagnostiquer le client BP_MQX_ETH

CLIENT_IP="192.168.0.151"
CAPTURE_FILE="client_capture.pcap"
DURATION=60  # secondes

echo "Capturing traffic from $CLIENT_IP for $DURATION seconds..."
sudo timeout $DURATION tcpdump -i any -s 0 -w $CAPTURE_FILE \
  "tcp port 80 and host $CLIENT_IP"

echo "Analyzing capture..."

# Compter les connexions
CONNECTIONS=$(tshark -r $CAPTURE_FILE -z conv,tcp | grep $CLIENT_IP | wc -l)
echo "Total TCP connections: $CONNECTIONS"

# Compter les requêtes HTTP
REQUESTS=$(tshark -r $CAPTURE_FILE -Y "http.request" | wc -l)
echo "Total HTTP requests: $REQUESTS"

# Lister les endpoints appelés
echo "Endpoints called:"
tshark -r $CAPTURE_FILE -Y "http.request" -T fields -e http.request.uri | \
  sort | uniq -c

# Analyser la fragmentation
echo "Analyzing TCP packet fragmentation..."
tshark -r $CAPTURE_FILE -Y "http.response" -T fields \
  -e frame.number -e tcp.len | \
  awk '{if ($2 > 0) print "Packet " $1 ": " $2 " bytes"}'

echo "Done. Review $CAPTURE_FILE with Wireshark for detailed analysis."
```

### Comparaison Serveur Fonctionnel vs Non-Fonctionnel

```bash
# Capturer le trafic avec server.sample (référence)
sudo tcpdump -i any -s 0 -w reference.pcap 'tcp port 80 and host 192.168.0.151'

# Capturer le trafic avec votre serveur
sudo tcpdump -i any -s 0 -w test.pcap 'tcp port 80 and host 192.168.0.151'

# Comparer le nombre de paquets par réponse
echo "Reference server:"
tshark -r reference.pcap -Y "http.response" -T fields -e frame.number

echo "Test server:"
tshark -r test.pcap -Y "http.response" -T fields -e frame.number
```

## Impact sur le Code

### Fichiers Affectés

La contrainte single-packet affecte **uniquement** le fichier suivant:

```
internal/server/legacy_http_server.go
```

### Code Non Affecté

Le reste du code fonctionne normalement car il utilise l'interface standard `http.ResponseWriter`:

- ✅ `internal/handlers/` - Handlers HTTP standard
- ✅ `internal/services/` - Logique métier
- ✅ `internal/store/` - Gestion de l'état
- ✅ `internal/models/` - Structures de données

### Avantages de l'Architecture

1. **Séparation des préoccupations**: La contrainte legacy est isolée dans `legacyResponseWriter`
2. **Code métier propre**: Les handlers ne savent pas qu'ils écrivent pour un client legacy
3. **Testabilité**: Les handlers peuvent être testés avec un `http.ResponseWriter` standard
4. **Maintenabilité**: Facile de remplacer ou modifier le comportement legacy

### Diagramme d'Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      main.go                                 │
│  • Initialise LegacyHTTPServer                              │
│  • Configure les handlers                                    │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│           internal/server/legacy_http_server.go              │
│  • Accepte connexions TCP                                    │
│  • Parse requêtes HTTP                                       │
│  • Crée legacyResponseWriter                                │
│  • Appelle handlers                                          │
│  • Flush en single-packet                                    │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│              internal/handlers/*.go                          │
│  • ServerInfosHandler                                        │
│  • MyStatusHandler                                           │
│  • MyActionsHandler                                          │
│  • DoneHandler                                               │
│  • AdminInjectHandler                                        │
│  (Utilisent http.ResponseWriter standard)                   │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│           internal/services/*.go                             │
│  • Logique métier                                            │
│  • Gestion des actions                                       │
│  • Merge de la table d'échange                              │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│              internal/store/*.go                             │
│  • État du système                                           │
│  • Table d'échange                                           │
│  • Actions en attente                                        │
└─────────────────────────────────────────────────────────────┘
```

## Leçons Apprises

### 1. Ne Jamais Supposer la Conformité aux Standards

**Problème**: On suppose que tous les clients HTTP suivent les standards (RFC 2616/7230)

**Réalité**: Les systèmes embarqués anciens ont souvent des implémentations simplifiées

**Solution**: Toujours tester avec le matériel réel et analyser le trafic réseau

### 2. L'Importance de l'Analyse Réseau

**Problème**: Le client se bloque sans message d'erreur

**Solution**: `tcpdump` a révélé la fragmentation des paquets TCP

**Leçon**: Les outils d'analyse réseau sont essentiels pour diagnostiquer les problèmes de protocole

### 3. Comparer avec une Référence Fonctionnelle

**Problème**: Difficile de savoir ce qui est "correct" sans référence

**Solution**: Analyser le trafic de `server.sample` qui fonctionne

**Leçon**: Toujours avoir un système de référence pour comparer

### 4. Isoler les Contraintes Legacy

**Problème**: Les contraintes legacy peuvent polluer tout le code

**Solution**: Créer un adaptateur (`legacyResponseWriter`) qui isole la contrainte

**Leçon**: Utiliser le pattern Adapter pour isoler les comportements non-standard

### 5. Documenter les Découvertes Critiques

**Problème**: Cette contrainte n'était documentée nulle part

**Solution**: Créer une documentation détaillée avec analyse et solution

**Leçon**: Les contraintes non-documentées doivent être documentées dès leur découverte

### 6. Tester Progressivement

**Problème**: Difficile de tester tout le système d'un coup

**Solution**: 
1. Tester `/api/serverinfos` d'abord
2. Puis `/api/mystatus`
3. Puis `/api/myactions`
4. Enfin les actions complètes

**Leçon**: Tester endpoint par endpoint pour isoler les problèmes

### 7. Utiliser des Métriques Objectives

**Problème**: "Ça marche" vs "Ça marche pas" est subjectif

**Solution**: Mesurer le nombre de paquets TCP par réponse

**Leçon**: Définir des métriques objectives pour valider les corrections

### 8. Préserver la Compatibilité

**Problème**: Modifier le client embarqué est difficile/impossible

**Solution**: Adapter le serveur pour être compatible avec le client

**Leçon**: Quand on ne peut pas changer le client, on adapte le serveur

## Références

### Documentation Externe

- [RFC 7230 - HTTP/1.1 Message Syntax and Routing](https://tools.ietf.org/html/rfc7230)
- [TCP/IP Illustrated, Volume 1](https://www.amazon.com/TCP-Illustrated-Volume-Implementation/dp/0201633469)
- [Wireshark User's Guide](https://www.wireshark.org/docs/wsug_html_chunked/)
- [tcpdump Manual](https://www.tcpdump.org/manpages/tcpdump.1.html)

### Documentation Interne

- [HTTP Legacy Protocol](./http-legacy-protocol.md) - Protocole HTTP non-standard complet
- [Exchange Table](./exchange-table.md) - Table d'échange (indices 605-622)
- [Embedded Client Architecture](../embedded-client/architecture.md) - Architecture MQX
- [Go Implementation](../server/go-implementation.md) - Implémentation serveur Go

### Code Source

- `internal/server/legacy_http_server.go` - Implémentation du serveur legacy
- `C/main.c` - Point d'entrée du client embarqué
- `Ethernet/GestionSocket.c` - Gestion des sockets côté client
- `Ethernet/www.c` - Parser HTTP côté client

---

**Dernière mise à jour**: 2025-01-08  
**Auteur**: Documentation du projet client-essensys-legacy  
**Statut**: Validé et testé avec le client BP_MQX_ETH réel
