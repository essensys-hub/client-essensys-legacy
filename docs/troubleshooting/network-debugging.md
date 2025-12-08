# Débogage Réseau - Client BP_MQX_ETH

## Table des Matières

- [Introduction](#introduction)
- [Outils Recommandés](#outils-recommandés)
- [tcpdump - Capture de Paquets](#tcpdump---capture-de-paquets)
- [Wireshark - Analyse Graphique](#wireshark---analyse-graphique)
- [tshark - Analyse en Ligne de Commande](#tshark---analyse-en-ligne-de-commande)
- [Vérification du Single-Packet Requirement](#vérification-du-single-packet-requirement)
- [Inspection des Headers HTTP](#inspection-des-headers-http)
- [Débogage du Contenu JSON](#débogage-du-contenu-json)
- [Scripts de Diagnostic](#scripts-de-diagnostic)
- [Cas d'Usage Courants](#cas-dusage-courants)
- [Résolution de Problèmes](#résolution-de-problèmes)

## Introduction

Le débogage réseau est **essentiel** pour diagnostiquer les problèmes de communication entre le client BP_MQX_ETH embarqué et le serveur. Ce guide explique comment utiliser les outils d'analyse réseau pour:

- ✅ Vérifier que les réponses HTTP sont envoyées en un seul paquet TCP
- ✅ Inspecter les headers HTTP pour détecter les erreurs de format
- ✅ Analyser le contenu JSON des requêtes et réponses
- ✅ Diagnostiquer les blocages et timeouts du client
- ✅ Comparer le comportement avec un serveur de référence

### Pourquoi le Débogage Réseau est Critique

Le client BP_MQX_ETH a des contraintes uniques qui nécessitent une analyse réseau:

1. **Single-Packet Requirement**: Le client ne peut parser que les réponses HTTP envoyées en un seul paquet TCP
2. **JSON Malformé**: Le client envoie du JSON avec des clés non-quotées
3. **Headers Spécifiques**: Certains headers doivent avoir un format exact (espace avant `;charset`)
4. **Blocage Silencieux**: Le client se bloque sans message d'erreur en cas de problème

Sans analyse réseau, ces problèmes sont **impossibles à diagnostiquer**.

## Outils Recommandés

### Outils Essentiels

| Outil | Usage | Plateforme | Installation |
|-------|-------|------------|--------------|
| **tcpdump** | Capture de paquets en temps réel | macOS, Linux | Pré-installé / apt/yum |
| **Wireshark** | Analyse graphique de captures | macOS, Linux, Windows | brew/apt/téléchargement |
| **tshark** | Wireshark en ligne de commande | macOS, Linux | Inclus avec Wireshark |

### Outils Complémentaires


| Outil | Usage | Plateforme |
|-------|-------|------------|
| **netcat (nc)** | Test de connexions TCP | Toutes | Pré-installé |
| **curl** | Test d'endpoints HTTP | Toutes | Pré-installé / brew/apt |
| **jq** | Formatage et analyse JSON | Toutes | brew/apt |

### Comparaison des Outils

**tcpdump**: Idéal pour la capture en temps réel et le diagnostic rapide
- ✅ Léger et rapide
- ✅ Pré-installé sur la plupart des systèmes
- ✅ Parfait pour vérifier le nombre de paquets TCP
- ❌ Interface en ligne de commande uniquement

**Wireshark**: Idéal pour l'analyse détaillée et visuelle
- ✅ Interface graphique intuitive
- ✅ Filtres puissants
- ✅ Suivi de flux TCP complet
- ✅ Statistiques et graphiques
- ❌ Plus lourd que tcpdump

**tshark**: Idéal pour l'automatisation et les scripts
- ✅ Toute la puissance de Wireshark en ligne de commande
- ✅ Parfait pour les scripts de diagnostic
- ✅ Extraction de champs spécifiques
- ❌ Syntaxe plus complexe que tcpdump

## tcpdump - Capture de Paquets

### Installation

```bash
# macOS (généralement pré-installé)
which tcpdump  # Vérifier si installé
# Si absent: brew install tcpdump

# Linux Debian/Ubuntu
sudo apt-get update
sudo apt-get install tcpdump

# Linux RedHat/CentOS/Fedora
sudo yum install tcpdump
# ou
sudo dnf install tcpdump
```

### Commandes de Base

#### Capturer le Trafic du Client


```bash
# Remplacer 192.168.0.151 par l'IP de votre client BP_MQX_ETH
CLIENT_IP="192.168.0.151"

# Capture basique avec affichage ASCII
sudo tcpdump -i any -s 0 -A "tcp port 80 and host $CLIENT_IP"

# Options expliquées:
# -i any        : Écouter sur toutes les interfaces réseau
# -s 0          : Capturer le paquet complet (pas de troncature)
# -A            : Afficher le contenu en ASCII
# tcp port 80   : Filtrer uniquement le trafic HTTP
# host X.X.X.X  : Filtrer uniquement l'IP du client
```

#### Sauvegarder dans un Fichier

```bash
# Capturer et sauvegarder pour analyse ultérieure
sudo tcpdump -i any -s 0 -w capture.pcap "tcp port 80 and host $CLIENT_IP"

# Capturer pendant 60 secondes puis arrêter
sudo timeout 60 tcpdump -i any -s 0 -w capture.pcap "tcp port 80 and host $CLIENT_IP"

# Capturer 100 paquets puis arrêter
sudo tcpdump -i any -s 0 -c 100 -w capture.pcap "tcp port 80 and host $CLIENT_IP"
```

#### Afficher une Capture Existante

```bash
# Lire un fichier pcap
tcpdump -r capture.pcap -A

# Lire avec filtres
tcpdump -r capture.pcap -A "http"

# Afficher uniquement les headers (sans body)
tcpdump -r capture.pcap -A | grep -A 5 "HTTP"
```

### Options Importantes

| Option | Description | Exemple |
|--------|-------------|---------|
| `-i <interface>` | Interface réseau à écouter | `-i eth0`, `-i any` |
| `-s <snaplen>` | Taille max de capture (0 = illimité) | `-s 0` |
| `-A` | Afficher en ASCII | `-A` |
| `-X` | Afficher en hex et ASCII | `-X` |
| `-w <file>` | Sauvegarder dans un fichier | `-w capture.pcap` |
| `-r <file>` | Lire depuis un fichier | `-r capture.pcap` |
| `-c <count>` | Capturer N paquets puis arrêter | `-c 100` |
| `-n` | Ne pas résoudre les noms d'hôtes | `-n` |
| `-v`, `-vv`, `-vvv` | Verbosité (détails croissants) | `-vv` |


### Filtres tcpdump Utiles

```bash
# Filtrer par IP source
sudo tcpdump -i any "src host 192.168.0.151"

# Filtrer par IP destination
sudo tcpdump -i any "dst host 192.168.0.151"

# Filtrer par port
sudo tcpdump -i any "port 80"
sudo tcpdump -i any "port 80 or port 443"

# Filtrer les requêtes HTTP GET
sudo tcpdump -i any -A "tcp port 80" | grep "GET"

# Filtrer les réponses HTTP
sudo tcpdump -i any -A "tcp port 80" | grep "HTTP/1.1"

# Filtrer par taille de paquet (utile pour détecter la fragmentation)
sudo tcpdump -i any "tcp port 80 and greater 100"

# Combiner plusieurs filtres
sudo tcpdump -i any "tcp port 80 and host 192.168.0.151 and greater 100"
```

### Exemples de Sortie

#### Requête HTTP du Client

```
15:23:45.123456 IP 192.168.0.151.49152 > 192.168.0.100.80: Flags [P.], seq 1:234, ack 1, win 1024, length 233
E..POST /api/mystatus HTTP/1.1
Host: 192.168.0.100
Connection: close
Content-Length: 150
Content-Type: application/json ;charset=UTF-8

{_de67f:1,605:0,606:0,607:0,...}
```

#### Réponse HTTP du Serveur (Correct - 1 Paquet)

```
15:23:45.125678 IP 192.168.0.100.80 > 192.168.0.151.49152: Flags [P.], seq 1:250, ack 234, win 65535, length 249
E..HTTP/1.1 200 OK
Connection: close
Content-Length: 96
Content-Type: application/json ;charset=UTF-8

{"_de67f":"1","590":"1","605":"0",...}
```

#### Réponse HTTP Fragmentée (Incorrect - Plusieurs Paquets)

```
# Paquet 1 - Status line
15:23:45.125678 IP 192.168.0.100.80 > 192.168.0.151.49152: Flags [P.], seq 1:20, ack 234, win 65535, length 19
E..HTTP/1.1 200 OK

# Paquet 2 - Headers
15:23:45.125690 IP 192.168.0.100.80 > 192.168.0.151.49152: Flags [P.], seq 20:120, ack 234, win 65535, length 100
E..Connection: close
Content-Length: 96
Content-Type: application/json ;charset=UTF-8

# Paquet 3 - Body
15:23:45.125702 IP 192.168.0.100.80 > 192.168.0.151.49152: Flags [P.], seq 120:216, ack 234, win 65535, length 96
E..{"_de67f":"1","590":"1",...}
```

**⚠️ Attention**: Si vous voyez plusieurs paquets pour une seule réponse HTTP, le client BP_MQX_ETH va se bloquer!


## Wireshark - Analyse Graphique

### Installation

```bash
# macOS
brew install --cask wireshark

# Linux Debian/Ubuntu
sudo apt-get update
sudo apt-get install wireshark

# Linux RedHat/CentOS/Fedora
sudo yum install wireshark-gnome
# ou
sudo dnf install wireshark

# Ajouter l'utilisateur au groupe wireshark (Linux)
sudo usermod -a -G wireshark $USER
# Puis se déconnecter/reconnecter
```

### Ouvrir une Capture

1. Lancer Wireshark: `wireshark` ou via l'interface graphique
2. Ouvrir un fichier: `File → Open` → Sélectionner `capture.pcap`
3. Ou en ligne de commande: `wireshark capture.pcap`

### Filtres d'Affichage Wireshark

#### Filtres de Base

```
# Filtrer par IP du client
ip.addr == 192.168.0.151

# Filtrer par IP source
ip.src == 192.168.0.151

# Filtrer par IP destination
ip.dst == 192.168.0.151

# Filtrer le trafic HTTP
http

# Filtrer les requêtes HTTP
http.request

# Filtrer les réponses HTTP
http.response

# Filtrer par méthode HTTP
http.request.method == "POST"
http.request.method == "GET"

# Filtrer par code de statut
http.response.code == 200
http.response.code == 201
```

#### Filtres Avancés

```
# Filtrer par endpoint
http.request.uri contains "/api/mystatus"
http.request.uri == "/api/serverinfos"

# Filtrer par header spécifique
http.content_type contains "application/json"
http.connection == "close"

# Filtrer par taille de paquet
tcp.len > 100
tcp.len < 1000

# Combiner plusieurs filtres (AND)
ip.addr == 192.168.0.151 && http.request

# Combiner plusieurs filtres (OR)
http.request.uri contains "mystatus" || http.request.uri contains "myactions"

# Exclure un filtre (NOT)
ip.addr == 192.168.0.151 && !http.request
```


### Suivre un Flux TCP

Pour voir toute la conversation HTTP entre le client et le serveur:

1. Sélectionner un paquet de la connexion
2. Clic droit → `Follow` → `TCP Stream`
3. Une fenêtre s'ouvre avec la conversation complète

**Couleurs dans le flux TCP:**
- 🔴 Rouge: Données envoyées par le client
- 🔵 Bleu: Données envoyées par le serveur

**Exemple de flux TCP:**

```
🔴 POST /api/mystatus HTTP/1.1
Host: 192.168.0.100
Connection: close
Content-Length: 150
Content-Type: application/json ;charset=UTF-8

{_de67f:1,605:0,606:0,...}

🔵 HTTP/1.1 200 OK
Connection: close
Content-Length: 96
Content-Type: application/json ;charset=UTF-8

{"_de67f":"1","590":"1","605":"0",...}
```

### Statistiques Utiles

#### Conversations TCP

`Statistics → Conversations → TCP`

Affiche toutes les connexions TCP avec:
- Adresses IP et ports
- Nombre de paquets
- Nombre d'octets
- Durée de la connexion

**Utilité**: Identifier combien de connexions le client établit et leur durée.

#### Hiérarchie des Protocoles

`Statistics → Protocol Hierarchy`

Affiche la répartition des protocoles:
- Ethernet
- IP
- TCP
- HTTP

**Utilité**: Vérifier que tout le trafic est bien HTTP.

#### Graphique de Débit

`Statistics → IO Graph`

Affiche un graphique du débit réseau dans le temps.

**Utilité**: Identifier les pics de trafic et les périodes de silence.

### Exporter des Objets HTTP

`File → Export Objects → HTTP`

Permet d'extraire tous les objets HTTP (requêtes et réponses) de la capture.

**Utilité**: Sauvegarder les réponses JSON pour analyse.


## tshark - Analyse en Ligne de Commande

### Installation

tshark est inclus avec Wireshark:

```bash
# Vérifier l'installation
which tshark

# Si absent, installer Wireshark (inclut tshark)
brew install wireshark  # macOS
sudo apt-get install tshark  # Linux
```

### Commandes de Base

#### Lire une Capture

```bash
# Afficher tous les paquets
tshark -r capture.pcap

# Afficher avec détails
tshark -r capture.pcap -V

# Afficher uniquement les paquets HTTP
tshark -r capture.pcap -Y "http"
```

#### Extraire des Champs Spécifiques

```bash
# Extraire les URIs des requêtes HTTP
tshark -r capture.pcap -Y "http.request" -T fields -e http.request.uri

# Extraire les codes de statut des réponses
tshark -r capture.pcap -Y "http.response" -T fields -e http.response.code

# Extraire IP source, IP dest, et URI
tshark -r capture.pcap -Y "http.request" -T fields \
  -e ip.src -e ip.dst -e http.request.uri

# Extraire les headers Content-Type
tshark -r capture.pcap -Y "http.response" -T fields -e http.content_type
```

#### Compter les Paquets

```bash
# Compter le nombre total de paquets
tshark -r capture.pcap | wc -l

# Compter les requêtes HTTP
tshark -r capture.pcap -Y "http.request" | wc -l

# Compter les réponses HTTP
tshark -r capture.pcap -Y "http.response" | wc -l

# Compter par endpoint
tshark -r capture.pcap -Y "http.request" -T fields -e http.request.uri | \
  sort | uniq -c
```

### Statistiques avec tshark

```bash
# Statistiques des conversations TCP
tshark -r capture.pcap -z conv,tcp

# Statistiques des endpoints IP
tshark -r capture.pcap -z endpoints,ip

# Statistiques HTTP
tshark -r capture.pcap -z http,tree

# Statistiques des requêtes HTTP par méthode
tshark -r capture.pcap -z http_req,tree

# Statistiques des réponses HTTP par code
tshark -r capture.pcap -z http_srv,tree
```


## Vérification du Single-Packet Requirement

### Pourquoi C'est Critique

Le client BP_MQX_ETH **doit** recevoir toute la réponse HTTP en **un seul paquet TCP**. Si la réponse est fragmentée, le client se bloque.

Voir [TCP Single-Packet Requirement](../protocol/tcp-single-packet.md) pour les détails complets.

### Méthode 1: Compter les Paquets avec tcpdump

```bash
CLIENT_IP="192.168.0.151"

# Capturer le trafic
sudo tcpdump -i any -s 0 "tcp port 80 and host $CLIENT_IP" -w test.pcap

# Analyser: compter les paquets TCP par réponse HTTP
tcpdump -r test.pcap -n | grep "length" | grep "192.168.0.100.80 >"

# Exemple de sortie CORRECTE (1 paquet par réponse):
# 15:23:45.125678 IP 192.168.0.100.80 > 192.168.0.151.49152: Flags [P.], length 249

# Exemple de sortie INCORRECTE (plusieurs paquets):
# 15:23:45.125678 IP 192.168.0.100.80 > 192.168.0.151.49152: Flags [P.], length 19
# 15:23:45.125690 IP 192.168.0.100.80 > 192.168.0.151.49152: Flags [P.], length 100
# 15:23:45.125702 IP 192.168.0.100.80 > 192.168.0.151.49152: Flags [P.], length 96
```

### Méthode 2: Analyser avec tshark

```bash
# Extraire les paquets TCP avec données (length > 0)
tshark -r test.pcap -Y "tcp.len > 0 and ip.src == 192.168.0.100" \
  -T fields -e frame.number -e tcp.len -e tcp.stream

# Grouper par stream TCP et compter
tshark -r test.pcap -Y "tcp.len > 0 and ip.src == 192.168.0.100" \
  -T fields -e tcp.stream | sort | uniq -c

# Résultat attendu: 1 paquet par stream
#   1 0
#   1 1
#   1 2

# Résultat problématique: plusieurs paquets par stream
#   3 0    ← Stream 0 a 3 paquets (PROBLÈME!)
#   1 1
#   2 2    ← Stream 2 a 2 paquets (PROBLÈME!)
```

### Méthode 3: Suivre le Flux dans Wireshark

1. Ouvrir `test.pcap` dans Wireshark
2. Filtrer: `ip.addr == 192.168.0.151 && http.response`
3. Pour chaque réponse HTTP:
   - Clic droit → `Follow` → `TCP Stream`
   - Observer le nombre de paquets dans la fenêtre du flux
   - **Correct**: Tous les paquets bleus (serveur) sont dans un seul paquet
   - **Incorrect**: Les paquets bleus sont fragmentés en plusieurs paquets


### Script de Vérification Automatique

```bash
#!/bin/bash
# verify_single_packet.sh - Vérifier le single-packet requirement

CLIENT_IP="192.168.0.151"
SERVER_IP="192.168.0.100"
CAPTURE_FILE="verify.pcap"

echo "Capturing traffic for 30 seconds..."
sudo timeout 30 tcpdump -i any -s 0 -w $CAPTURE_FILE \
  "tcp port 80 and host $CLIENT_IP" 2>/dev/null

echo ""
echo "Analyzing HTTP responses..."
echo "================================"

# Extraire les streams TCP avec réponses HTTP
STREAMS=$(tshark -r $CAPTURE_FILE -Y "http.response" -T fields -e tcp.stream | sort -u)

TOTAL=0
VIOLATIONS=0

for stream in $STREAMS; do
    # Compter les paquets TCP avec données du serveur dans ce stream
    PACKET_COUNT=$(tshark -r $CAPTURE_FILE \
      -Y "tcp.stream == $stream and tcp.len > 0 and ip.src == $SERVER_IP" \
      2>/dev/null | wc -l)
    
    TOTAL=$((TOTAL + 1))
    
    if [ $PACKET_COUNT -eq 1 ]; then
        echo "✅ Stream $stream: 1 packet (OK)"
    else
        echo "❌ Stream $stream: $PACKET_COUNT packets (VIOLATION!)"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi
done

echo ""
echo "================================"
echo "Total responses: $TOTAL"
echo "Single-packet: $((TOTAL - VIOLATIONS))"
echo "Violations: $VIOLATIONS"

if [ $VIOLATIONS -eq 0 ]; then
    echo ""
    echo "✅ SUCCESS: All responses sent in single TCP packet"
    exit 0
else
    echo ""
    echo "❌ FAILURE: Some responses fragmented across multiple packets"
    echo "The client BP_MQX_ETH will block!"
    exit 1
fi
```

**Usage:**

```bash
chmod +x verify_single_packet.sh
./verify_single_packet.sh
```


## Inspection des Headers HTTP

### Extraire les Headers avec tcpdump

```bash
# Afficher les headers HTTP des réponses
sudo tcpdump -i any -s 0 -A "tcp port 80 and host 192.168.0.151" | \
  grep -A 10 "HTTP/1.1"

# Sauvegarder les headers dans un fichier
sudo tcpdump -i any -s 0 -A "tcp port 80 and host 192.168.0.151" | \
  grep -A 10 "HTTP/1.1" > headers.txt
```

### Extraire les Headers avec tshark

```bash
# Extraire tous les headers HTTP
tshark -r capture.pcap -Y "http.response" -T fields \
  -e http.response.code \
  -e http.connection \
  -e http.content_length \
  -e http.content_type

# Extraire un header spécifique
tshark -r capture.pcap -Y "http.response" -T fields -e http.content_type

# Afficher les headers complets
tshark -r capture.pcap -Y "http.response" -V | grep -A 20 "Hypertext Transfer Protocol"
```

### Vérifier les Headers Critiques

Le client BP_MQX_ETH nécessite des headers spécifiques avec un format exact:

#### 1. Content-Type avec Espace

**Correct:**
```
Content-Type: application/json ;charset=UTF-8
                              ↑
                         Espace requis!
```

**Incorrect:**
```
Content-Type: application/json;charset=UTF-8
                              ↑
                         Pas d'espace (ERREUR!)
```

**Vérification:**

```bash
# Vérifier le format du Content-Type
tshark -r capture.pcap -Y "http.response" -T fields -e http.content_type | \
  grep -E "json ;charset"

# Si aucun résultat, le format est incorrect!
```

#### 2. Connection: close

Le client s'attend à `Connection: close` dans chaque réponse.

**Vérification:**

```bash
# Vérifier la présence de Connection: close
tshark -r capture.pcap -Y "http.response" -T fields -e http.connection

# Résultat attendu: "close" pour chaque réponse
```

#### 3. Content-Length

Le Content-Length doit correspondre exactement à la taille du body.

**Vérification:**

```bash
# Extraire Content-Length et taille réelle du body
tshark -r capture.pcap -Y "http.response" -T fields \
  -e http.content_length \
  -e http.file_data | \
  awk '{print "Declared:", $1, "Actual:", length($2)}'
```


### Script de Vérification des Headers

```bash
#!/bin/bash
# verify_headers.sh - Vérifier les headers HTTP

CAPTURE_FILE="$1"

if [ -z "$CAPTURE_FILE" ]; then
    echo "Usage: $0 <capture.pcap>"
    exit 1
fi

echo "Verifying HTTP headers in $CAPTURE_FILE..."
echo "=========================================="

# 1. Vérifier Content-Type
echo ""
echo "1. Checking Content-Type format..."
CONTENT_TYPES=$(tshark -r $CAPTURE_FILE -Y "http.response" -T fields -e http.content_type 2>/dev/null)

if echo "$CONTENT_TYPES" | grep -q "json ;charset"; then
    echo "✅ Content-Type has correct format (space before ;charset)"
else
    echo "❌ Content-Type missing space before ;charset"
    echo "   Expected: application/json ;charset=UTF-8"
    echo "   Found: $CONTENT_TYPES"
fi

# 2. Vérifier Connection: close
echo ""
echo "2. Checking Connection header..."
CONNECTIONS=$(tshark -r $CAPTURE_FILE -Y "http.response" -T fields -e http.connection 2>/dev/null | sort -u)

if [ "$CONNECTIONS" = "close" ]; then
    echo "✅ All responses have Connection: close"
else
    echo "❌ Some responses missing Connection: close"
    echo "   Found: $CONNECTIONS"
fi

# 3. Vérifier Content-Length
echo ""
echo "3. Checking Content-Length..."
tshark -r $CAPTURE_FILE -Y "http.response" -T fields \
  -e frame.number -e http.content_length 2>/dev/null | \
  while read frame length; do
    if [ -n "$length" ] && [ "$length" -gt 0 ]; then
        echo "✅ Frame $frame: Content-Length = $length"
    else
        echo "❌ Frame $frame: Missing or invalid Content-Length"
    fi
done

echo ""
echo "=========================================="
echo "Verification complete"
```

**Usage:**

```bash
chmod +x verify_headers.sh
./verify_headers.sh capture.pcap
```


## Débogage du Contenu JSON

### Extraire le JSON avec tcpdump

```bash
# Afficher le contenu JSON des requêtes
sudo tcpdump -i any -s 0 -A "tcp port 80 and host 192.168.0.151" | \
  grep -A 5 "{"

# Afficher le contenu JSON des réponses
sudo tcpdump -i any -s 0 -A "tcp port 80 and host 192.168.0.151" | \
  grep -B 5 "{"
```

### Extraire le JSON avec tshark

```bash
# Extraire le body JSON des requêtes
tshark -r capture.pcap -Y "http.request" -T fields -e http.file_data

# Extraire le body JSON des réponses
tshark -r capture.pcap -Y "http.response" -T fields -e http.file_data

# Extraire et formater avec jq
tshark -r capture.pcap -Y "http.response" -T fields -e http.file_data | \
  while read json; do
    echo "$json" | jq '.' 2>/dev/null || echo "Invalid JSON: $json"
  done
```

### Vérifier le JSON Malformé du Client

Le client BP_MQX_ETH envoie du JSON avec **clés non-quotées**:

**JSON du client (malformé):**
```json
{_de67f:1,605:0,606:0,607:0,608:0}
```

**JSON normalisé (correct):**
```json
{"_de67f":"1","605":"0","606":"0","607":"0","608":"0"}
```

**Vérification:**

```bash
# Extraire le JSON des requêtes POST /api/mystatus
tshark -r capture.pcap \
  -Y "http.request.method == POST and http.request.uri contains mystatus" \
  -T fields -e http.file_data

# Vérifier si les clés sont quotées
tshark -r capture.pcap \
  -Y "http.request.method == POST" \
  -T fields -e http.file_data | \
  grep -E '\{[a-z_]+:' && echo "❌ Malformed JSON (unquoted keys)" || echo "✅ Valid JSON"
```

### Valider le JSON des Réponses

```bash
# Extraire et valider le JSON des réponses
tshark -r capture.pcap -Y "http.response" -T fields -e http.file_data | \
  while read json; do
    if echo "$json" | jq empty 2>/dev/null; then
        echo "✅ Valid JSON"
    else
        echo "❌ Invalid JSON: $json"
    fi
  done
```


### Vérifier le Champ _de67f

Le champ `_de67f` **doit** être le premier dans le JSON et valoir `"1"`.

**Vérification:**

```bash
# Extraire le JSON et vérifier _de67f
tshark -r capture.pcap -Y "http.response" -T fields -e http.file_data | \
  while read json; do
    if echo "$json" | grep -q '^{"_de67f":"1"'; then
        echo "✅ _de67f is first and equals 1"
    else
        echo "❌ _de67f missing or not first: $json"
    fi
  done
```

### Script de Validation JSON

```bash
#!/bin/bash
# validate_json.sh - Valider le JSON des requêtes et réponses

CAPTURE_FILE="$1"

if [ -z "$CAPTURE_FILE" ]; then
    echo "Usage: $0 <capture.pcap>"
    exit 1
fi

echo "Validating JSON in $CAPTURE_FILE..."
echo "===================================="

# 1. Vérifier les requêtes (JSON malformé attendu)
echo ""
echo "1. Checking request JSON (malformed expected)..."
REQUEST_JSON=$(tshark -r $CAPTURE_FILE -Y "http.request.method == POST" \
  -T fields -e http.file_data 2>/dev/null | head -1)

if echo "$REQUEST_JSON" | grep -qE '\{[a-z_]+:'; then
    echo "✅ Request has malformed JSON (unquoted keys) - as expected"
    echo "   Example: $REQUEST_JSON"
else
    echo "⚠️  Request JSON format unexpected"
fi

# 2. Vérifier les réponses (JSON valide attendu)
echo ""
echo "2. Checking response JSON (valid expected)..."
RESPONSE_COUNT=0
VALID_COUNT=0
INVALID_COUNT=0

tshark -r $CAPTURE_FILE -Y "http.response" -T fields -e http.file_data 2>/dev/null | \
  while read json; do
    if [ -n "$json" ]; then
        RESPONSE_COUNT=$((RESPONSE_COUNT + 1))
        
        if echo "$json" | jq empty 2>/dev/null; then
            VALID_COUNT=$((VALID_COUNT + 1))
            echo "✅ Response $RESPONSE_COUNT: Valid JSON"
        else
            INVALID_COUNT=$((INVALID_COUNT + 1))
            echo "❌ Response $RESPONSE_COUNT: Invalid JSON"
            echo "   Content: $json"
        fi
    fi
done

# 3. Vérifier _de67f
echo ""
echo "3. Checking _de67f field..."
tshark -r $CAPTURE_FILE -Y "http.response" -T fields -e http.file_data 2>/dev/null | \
  while read json; do
    if echo "$json" | jq -e '._de67f == "1"' >/dev/null 2>&1; then
        echo "✅ _de67f present and equals \"1\""
    else
        echo "❌ _de67f missing or incorrect: $json"
    fi
done

echo ""
echo "===================================="
echo "Validation complete"
```

**Usage:**

```bash
chmod +x validate_json.sh
./validate_json.sh capture.pcap
```


## Scripts de Diagnostic

### Script Complet de Diagnostic

```bash
#!/bin/bash
# diagnose_client.sh - Diagnostic complet du client BP_MQX_ETH

CLIENT_IP="${1:-192.168.0.151}"
SERVER_IP="${2:-192.168.0.100}"
DURATION="${3:-60}"
CAPTURE_FILE="client_diagnostic_$(date +%Y%m%d_%H%M%S).pcap"

echo "=========================================="
echo "BP_MQX_ETH Client Diagnostic Tool"
echo "=========================================="
echo "Client IP: $CLIENT_IP"
echo "Server IP: $SERVER_IP"
echo "Duration: $DURATION seconds"
echo "Capture file: $CAPTURE_FILE"
echo ""

# 1. Capture du trafic
echo "Step 1: Capturing traffic..."
sudo timeout $DURATION tcpdump -i any -s 0 -w $CAPTURE_FILE \
  "tcp port 80 and host $CLIENT_IP" 2>/dev/null

if [ ! -f "$CAPTURE_FILE" ]; then
    echo "❌ Failed to capture traffic"
    exit 1
fi

echo "✅ Capture complete"
echo ""

# 2. Statistiques générales
echo "Step 2: General statistics..."
echo "----------------------------"

TOTAL_PACKETS=$(tshark -r $CAPTURE_FILE 2>/dev/null | wc -l)
echo "Total packets: $TOTAL_PACKETS"

HTTP_REQUESTS=$(tshark -r $CAPTURE_FILE -Y "http.request" 2>/dev/null | wc -l)
echo "HTTP requests: $HTTP_REQUESTS"

HTTP_RESPONSES=$(tshark -r $CAPTURE_FILE -Y "http.response" 2>/dev/null | wc -l)
echo "HTTP responses: $HTTP_RESPONSES"

echo ""

# 3. Endpoints appelés
echo "Step 3: Endpoints called..."
echo "----------------------------"
tshark -r $CAPTURE_FILE -Y "http.request" -T fields -e http.request.uri 2>/dev/null | \
  sort | uniq -c | sort -rn
echo ""

# 4. Vérification single-packet
echo "Step 4: Verifying single-packet requirement..."
echo "-----------------------------------------------"

STREAMS=$(tshark -r $CAPTURE_FILE -Y "http.response" -T fields -e tcp.stream 2>/dev/null | sort -u)
VIOLATIONS=0

for stream in $STREAMS; do
    PACKET_COUNT=$(tshark -r $CAPTURE_FILE \
      -Y "tcp.stream == $stream and tcp.len > 0 and ip.src == $SERVER_IP" \
      2>/dev/null | wc -l)
    
    if [ $PACKET_COUNT -eq 1 ]; then
        echo "✅ Stream $stream: 1 packet"
    else
        echo "❌ Stream $stream: $PACKET_COUNT packets (VIOLATION!)"
        VIOLATIONS=$((VIOLATIONS + 1))
    fi
done

echo ""

# 5. Vérification des headers
echo "Step 5: Verifying HTTP headers..."
echo "----------------------------------"

# Content-Type
CONTENT_TYPE=$(tshark -r $CAPTURE_FILE -Y "http.response" -T fields -e http.content_type 2>/dev/null | head -1)
if echo "$CONTENT_TYPE" | grep -q "json ;charset"; then
    echo "✅ Content-Type: $CONTENT_TYPE (correct format)"
else
    echo "❌ Content-Type: $CONTENT_TYPE (missing space before ;charset)"
fi

# Connection
CONNECTION=$(tshark -r $CAPTURE_FILE -Y "http.response" -T fields -e http.connection 2>/dev/null | sort -u)
if [ "$CONNECTION" = "close" ]; then
    echo "✅ Connection: close"
else
    echo "❌ Connection: $CONNECTION (should be 'close')"
fi

echo ""

# 6. Résumé
echo "=========================================="
echo "DIAGNOSTIC SUMMARY"
echo "=========================================="
echo "HTTP Requests: $HTTP_REQUESTS"
echo "HTTP Responses: $HTTP_RESPONSES"
echo "Single-packet violations: $VIOLATIONS"
echo ""

if [ $VIOLATIONS -eq 0 ] && [ $HTTP_REQUESTS -gt 0 ]; then
    echo "✅ SUCCESS: Client communication looks healthy"
    echo ""
    echo "Next steps:"
    echo "  - Review capture with: wireshark $CAPTURE_FILE"
    echo "  - Check server logs for any errors"
else
    echo "❌ ISSUES DETECTED"
    echo ""
    if [ $VIOLATIONS -gt 0 ]; then
        echo "  - Single-packet requirement violated!"
        echo "    The client will block. Fix the server to send responses in one packet."
    fi
    if [ $HTTP_REQUESTS -eq 0 ]; then
        echo "  - No HTTP requests captured!"
        echo "    Check if client is running and network connectivity."
    fi
    echo ""
    echo "Review capture with: wireshark $CAPTURE_FILE"
fi

echo ""
echo "Capture saved to: $CAPTURE_FILE"
```

**Usage:**

```bash
chmod +x diagnose_client.sh

# Utilisation par défaut (IP 192.168.0.151, 60 secondes)
./diagnose_client.sh

# Spécifier l'IP du client
./diagnose_client.sh 192.168.0.200

# Spécifier IP client et serveur
./diagnose_client.sh 192.168.0.200 192.168.0.50

# Spécifier IP client, serveur et durée
./diagnose_client.sh 192.168.0.200 192.168.0.50 120
```


### Script de Comparaison avec Référence

```bash
#!/bin/bash
# compare_servers.sh - Comparer le comportement de deux serveurs

CLIENT_IP="${1:-192.168.0.151}"
REFERENCE_CAPTURE="$2"
TEST_CAPTURE="$3"

if [ -z "$REFERENCE_CAPTURE" ] || [ -z "$TEST_CAPTURE" ]; then
    echo "Usage: $0 <client_ip> <reference.pcap> <test.pcap>"
    echo ""
    echo "Example:"
    echo "  1. Capture traffic with reference server:"
    echo "     sudo tcpdump -i any -s 0 -w reference.pcap 'tcp port 80 and host 192.168.0.151'"
    echo "  2. Capture traffic with test server:"
    echo "     sudo tcpdump -i any -s 0 -w test.pcap 'tcp port 80 and host 192.168.0.151'"
    echo "  3. Compare:"
    echo "     ./compare_servers.sh 192.168.0.151 reference.pcap test.pcap"
    exit 1
fi

echo "=========================================="
echo "Server Comparison Tool"
echo "=========================================="
echo "Client IP: $CLIENT_IP"
echo "Reference: $REFERENCE_CAPTURE"
echo "Test: $TEST_CAPTURE"
echo ""

# Fonction pour analyser une capture
analyze_capture() {
    local capture=$1
    local label=$2
    
    echo "$label:"
    echo "----------"
    
    # Nombre de requêtes
    local requests=$(tshark -r $capture -Y "http.request" 2>/dev/null | wc -l)
    echo "  Requests: $requests"
    
    # Nombre de réponses
    local responses=$(tshark -r $capture -Y "http.response" 2>/dev/null | wc -l)
    echo "  Responses: $responses"
    
    # Paquets par réponse
    local streams=$(tshark -r $capture -Y "http.response" -T fields -e tcp.stream 2>/dev/null | sort -u)
    local single_packet=0
    local multi_packet=0
    
    for stream in $streams; do
        local count=$(tshark -r $capture \
          -Y "tcp.stream == $stream and tcp.len > 0 and ip.dst == $CLIENT_IP" \
          2>/dev/null | wc -l)
        
        if [ $count -eq 1 ]; then
            single_packet=$((single_packet + 1))
        else
            multi_packet=$((multi_packet + 1))
        fi
    done
    
    echo "  Single-packet responses: $single_packet"
    echo "  Multi-packet responses: $multi_packet"
    
    # Content-Type
    local content_type=$(tshark -r $capture -Y "http.response" -T fields -e http.content_type 2>/dev/null | head -1)
    echo "  Content-Type: $content_type"
    
    echo ""
}

# Analyser les deux captures
analyze_capture "$REFERENCE_CAPTURE" "REFERENCE SERVER"
analyze_capture "$TEST_CAPTURE" "TEST SERVER"

# Comparaison
echo "=========================================="
echo "COMPARISON RESULT"
echo "=========================================="

REF_MULTI=$(tshark -r $REFERENCE_CAPTURE -Y "http.response" -T fields -e tcp.stream 2>/dev/null | sort -u | \
  while read stream; do
    tshark -r $REFERENCE_CAPTURE -Y "tcp.stream == $stream and tcp.len > 0 and ip.dst == $CLIENT_IP" 2>/dev/null | wc -l
  done | grep -v "^1$" | wc -l)

TEST_MULTI=$(tshark -r $TEST_CAPTURE -Y "http.response" -T fields -e tcp.stream 2>/dev/null | sort -u | \
  while read stream; do
    tshark -r $TEST_CAPTURE -Y "tcp.stream == $stream and tcp.len > 0 and ip.dst == $CLIENT_IP" 2>/dev/null | wc -l
  done | grep -v "^1$" | wc -l)

if [ $REF_MULTI -eq 0 ] && [ $TEST_MULTI -eq 0 ]; then
    echo "✅ Both servers send single-packet responses"
elif [ $REF_MULTI -eq 0 ] && [ $TEST_MULTI -gt 0 ]; then
    echo "❌ Test server violates single-packet requirement"
    echo "   Reference server: OK"
    echo "   Test server: $TEST_MULTI violations"
else
    echo "⚠️  Unexpected result - review captures manually"
fi

echo ""
echo "Review captures with:"
echo "  wireshark $REFERENCE_CAPTURE"
echo "  wireshark $TEST_CAPTURE"
```

**Usage:**

```bash
chmod +x compare_servers.sh

# Capturer avec serveur de référence
sudo tcpdump -i any -s 0 -w reference.pcap 'tcp port 80 and host 192.168.0.151'
# (Arrêter après quelques requêtes)

# Capturer avec serveur de test
sudo tcpdump -i any -s 0 -w test.pcap 'tcp port 80 and host 192.168.0.151'
# (Arrêter après quelques requêtes)

# Comparer
./compare_servers.sh 192.168.0.151 reference.pcap test.pcap
```


## Cas d'Usage Courants

### Cas 1: Le Client Se Bloque Après la Première Requête

**Symptômes:**
- Le client appelle `/api/serverinfos` avec succès
- Puis plus aucun appel à `/api/mystatus` ou `/api/myactions`
- Pas de message d'erreur

**Diagnostic:**

```bash
# Capturer le trafic
sudo tcpdump -i any -s 0 -w blocked.pcap 'tcp port 80 and host 192.168.0.151'

# Vérifier le nombre de paquets par réponse
tshark -r blocked.pcap -Y "http.response" -T fields -e tcp.stream | sort -u | \
  while read stream; do
    count=$(tshark -r blocked.pcap \
      -Y "tcp.stream == $stream and tcp.len > 0 and ip.src == 192.168.0.100" | wc -l)
    echo "Stream $stream: $count packets"
  done
```

**Cause probable:** Réponse HTTP fragmentée en plusieurs paquets TCP

**Solution:** Implémenter `legacyResponseWriter` pour bufferiser la réponse complète

### Cas 2: Le Client N'Appelle Aucun Endpoint

**Symptômes:**
- Aucun trafic HTTP capturé
- Le client semble inactif

**Diagnostic:**

```bash
# Vérifier la connectivité réseau
ping 192.168.0.151

# Vérifier si le client écoute sur le réseau
sudo tcpdump -i any 'host 192.168.0.151' -c 10

# Vérifier les connexions TCP
sudo netstat -an | grep 192.168.0.151
```

**Causes probables:**
- Client non démarré ou bloqué
- Problème de configuration réseau (IP, gateway)
- Serveur inaccessible

**Solution:** Vérifier la configuration réseau du client et la connectivité

### Cas 3: Les Actions Ne Sont Pas Exécutées

**Symptômes:**
- Le client appelle `/api/myactions` régulièrement
- Les actions sont renvoyées par le serveur
- Mais les actions ne sont pas exécutées (lumières ne s'allument pas)

**Diagnostic:**

```bash
# Capturer et analyser les actions
sudo tcpdump -i any -s 0 -A 'tcp port 80 and host 192.168.0.151' | \
  grep -A 10 "/api/myactions"

# Vérifier le contenu JSON des actions
tshark -r capture.pcap \
  -Y "http.request.uri contains myactions" \
  -T fields -e http.file_data | jq '.'
```

**Causes probables:**
- Champ `_de67f` manquant ou incorrect
- Index 590 manquant ou différent de "1"
- Format JSON incorrect
- Actions non acknowledgées (pas d'appel à `/api/done/{guid}`)

**Solution:** Vérifier le format de la table d'échange et l'acknowledge


### Cas 4: Erreurs de Parsing JSON

**Symptômes:**
- Le serveur reçoit des requêtes mais ne peut pas parser le JSON
- Erreurs de parsing dans les logs du serveur

**Diagnostic:**

```bash
# Extraire le JSON brut des requêtes
tshark -r capture.pcap -Y "http.request.method == POST" \
  -T fields -e http.file_data

# Vérifier si les clés sont quotées
tshark -r capture.pcap -Y "http.request.method == POST" \
  -T fields -e http.file_data | head -1
```

**Exemple de JSON du client:**
```json
{_de67f:1,605:0,606:0}
```

**Cause:** Le client envoie du JSON avec clés non-quotées (non-standard)

**Solution:** Implémenter un normalisateur JSON côté serveur

### Cas 5: Timeouts Réseau

**Symptômes:**
- Le client se connecte mais les requêtes prennent trop de temps
- Timeouts dans les logs

**Diagnostic:**

```bash
# Analyser les temps de réponse
tshark -r capture.pcap -Y "http" -T fields \
  -e frame.time_relative -e http.request.uri -e http.response.code

# Calculer les délais entre requête et réponse
tshark -r capture.pcap -z http,tree
```

**Causes probables:**
- Serveur surchargé
- Latence réseau élevée
- Problèmes de routage

**Solution:** Optimiser le serveur, vérifier la configuration réseau

### Cas 6: Headers HTTP Incorrects

**Symptômes:**
- Le client reçoit les réponses mais ne les parse pas correctement
- Comportement erratique

**Diagnostic:**

```bash
# Extraire tous les headers des réponses
tshark -r capture.pcap -Y "http.response" -V | \
  grep -A 20 "Hypertext Transfer Protocol"

# Vérifier spécifiquement Content-Type
tshark -r capture.pcap -Y "http.response" -T fields -e http.content_type
```

**Vérifications:**
- ✅ `Content-Type: application/json ;charset=UTF-8` (avec espace)
- ✅ `Connection: close`
- ✅ `Content-Length` correct

**Solution:** Corriger le format des headers côté serveur


## Résolution de Problèmes

### Problème: tcpdump Nécessite sudo

**Erreur:**
```
tcpdump: eth0: You don't have permission to capture on that device
```

**Solutions:**

```bash
# Option 1: Utiliser sudo (recommandé)
sudo tcpdump -i any ...

# Option 2: Ajouter les capacités (Linux uniquement)
sudo setcap cap_net_raw,cap_net_admin=eip /usr/bin/tcpdump

# Option 3: Ajouter l'utilisateur au groupe (Linux)
sudo usermod -a -G wireshark $USER
# Puis se déconnecter/reconnecter
```

### Problème: Interface Réseau Introuvable

**Erreur:**
```
tcpdump: eth0: No such device exists
```

**Solution:**

```bash
# Lister les interfaces disponibles
tcpdump -D
# ou
ip link show  # Linux
ifconfig      # macOS

# Utiliser 'any' pour écouter sur toutes les interfaces
sudo tcpdump -i any ...
```

### Problème: Capture Vide

**Symptômes:** Le fichier `.pcap` est créé mais ne contient aucun paquet

**Diagnostic:**

```bash
# Vérifier le contenu
tcpdump -r capture.pcap
# Si vide: "reading from file capture.pcap, link-type ... (No packet)"

# Vérifier la taille du fichier
ls -lh capture.pcap
```

**Causes probables:**
- Filtre trop restrictif
- Mauvaise interface réseau
- Client non actif pendant la capture

**Solutions:**

```bash
# Tester sans filtre
sudo tcpdump -i any -w test.pcap -c 10

# Vérifier que le client est actif
ping 192.168.0.151

# Élargir le filtre
sudo tcpdump -i any 'host 192.168.0.151'  # Tout le trafic du client
```

### Problème: tshark Non Trouvé

**Erreur:**
```
bash: tshark: command not found
```

**Solution:**

```bash
# macOS
brew install wireshark  # Inclut tshark

# Linux
sudo apt-get install tshark  # Debian/Ubuntu
sudo yum install wireshark   # RedHat/CentOS
```

### Problème: Permission Denied pour Wireshark

**Erreur (Linux):**
```
Couldn't run /usr/bin/dumpcap in child process: Permission denied
```

**Solution:**

```bash
# Reconfigurer Wireshark pour permettre la capture sans root
sudo dpkg-reconfigure wireshark-common
# Sélectionner "Yes" pour permettre aux non-root de capturer

# Ajouter l'utilisateur au groupe wireshark
sudo usermod -a -G wireshark $USER

# Se déconnecter et reconnecter
```


### Problème: Fichier PCAP Corrompu

**Symptômes:**
- Wireshark ou tshark ne peut pas ouvrir le fichier
- Erreur "The file appears to be damaged or corrupt"

**Diagnostic:**

```bash
# Vérifier l'intégrité du fichier
capinfos capture.pcap

# Tenter de réparer
editcap -F pcap capture.pcap repaired.pcap
```

**Prévention:**
- Ne pas interrompre tcpdump brutalement (Ctrl+C est OK)
- Utiliser `timeout` pour arrêter proprement
- Vérifier l'espace disque disponible

### Problème: Trop de Paquets à Analyser

**Symptômes:** La capture contient des milliers de paquets, difficile à analyser

**Solutions:**

```bash
# Filtrer par période de temps
editcap -A "2025-01-08 15:00:00" -B "2025-01-08 15:05:00" \
  capture.pcap filtered.pcap

# Extraire uniquement les paquets HTTP
tshark -r capture.pcap -Y "http" -w http_only.pcap

# Extraire uniquement un stream TCP spécifique
tshark -r capture.pcap -Y "tcp.stream == 5" -w stream5.pcap

# Limiter au nombre de paquets
editcap -c 100 capture.pcap first100.pcap
```

### Problème: Analyse Lente

**Symptômes:** tshark ou Wireshark prend beaucoup de temps pour analyser

**Solutions:**

```bash
# Désactiver la résolution DNS (plus rapide)
tshark -r capture.pcap -n ...

# Utiliser des filtres de capture plutôt que d'affichage
# Mauvais (lent):
tshark -r huge.pcap -Y "ip.addr == 192.168.0.151"

# Bon (rapide):
tshark -r huge.pcap -f "host 192.168.0.151"

# Diviser le fichier en morceaux
editcap -c 1000 huge.pcap chunk.pcap
# Crée chunk_00000_*.pcap, chunk_00001_*.pcap, etc.
```

## Références

### Documentation Externe

- [tcpdump Manual](https://www.tcpdump.org/manpages/tcpdump.1.html) - Documentation officielle tcpdump
- [Wireshark User's Guide](https://www.wireshark.org/docs/wsug_html_chunked/) - Guide complet Wireshark
- [tshark Manual](https://www.wireshark.org/docs/man-pages/tshark.html) - Documentation tshark
- [Berkeley Packet Filter (BPF) Syntax](https://biot.com/capstats/bpf.html) - Syntaxe des filtres tcpdump

### Documentation Interne

- [TCP Single-Packet Requirement](../protocol/tcp-single-packet.md) - Contrainte critique du client
- [HTTP Legacy Protocol](../protocol/http-legacy-protocol.md) - Protocole HTTP non-standard
- [Exchange Table](../protocol/exchange-table.md) - Table d'échange (indices 605-622)
- [Common Issues](./common-issues.md) - Problèmes courants et solutions

### Outils Recommandés

| Outil | Site Web | Description |
|-------|----------|-------------|
| **tcpdump** | [tcpdump.org](https://www.tcpdump.org/) | Capture de paquets en ligne de commande |
| **Wireshark** | [wireshark.org](https://www.wireshark.org/) | Analyseur de protocoles graphique |
| **tshark** | [wireshark.org](https://www.wireshark.org/) | Wireshark en ligne de commande |
| **jq** | [stedolan.github.io/jq](https://stedolan.github.io/jq/) | Processeur JSON en ligne de commande |

---

**Dernière mise à jour**: 2025-01-08  
**Auteur**: Documentation du projet client-essensys-legacy  
**Statut**: Validé avec le client BP_MQX_ETH réel

