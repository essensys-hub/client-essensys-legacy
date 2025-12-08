# Diagrammes de Séquence

Ce document présente les diagrammes de séquence pour les principaux flux d'interaction du système client-essensys-legacy. Ces diagrammes illustrent comment le client embarqué, le serveur, et les autres composants communiquent au fil du temps.

## Table des Matières

- [Séquence de Démarrage](#séquence-de-démarrage)
- [Polling mystatus/myactions](#polling-statusmyactions)
- [Exécution d'une Action](#exécution-dune-action)
- [Acknowledgment d'Action](#acknowledgment-daction)
- [Injection de Commande Admin](#injection-de-commande-admin)

---

## Séquence de Démarrage

Ce diagramme montre le processus de démarrage du client embarqué et son initialisation avec le serveur.

```mermaid
sequenceDiagram
    participant HW as Hardware
    participant MQX as MQX RTOS
    participant Main as Main_task
    participant Eth as Ethernet_task
    participant Server as Serveur Legacy
    
    HW->>MQX: Power On / Reset
    MQX->>MQX: Initialiser RTOS
    MQX->>Main: Créer Main_task (priorité 8)
    
    Main->>Main: Initialiser hardware (Hard_Init)
    Main->>Main: Charger config EEPROM
    Main->>Main: Initialiser table d'échange
    
    Main->>MQX: Créer Ecran_task (priorité 9)
    Main->>MQX: Créer Boitiers_task (priorité 10)
    Main->>MQX: Créer TeleInfo_task (priorité 11)
    Main->>MQX: Créer Ethernet_task (priorité 12)
    
    MQX->>Eth: Démarrer Ethernet_task
    Eth->>Eth: Initialiser stack TCP/IP (RTCS)
    Eth->>Eth: Configurer adresse IP
    Eth->>Eth: Ouvrir socket TCP
    
    Eth->>Server: GET /api/serverinfos
    Server-->>Eth: 200 OK + infos serveur
    
    Eth->>Eth: Démarrer polling loop
    
    Note over Main,Server: Système prêt - polling actif
```

### Explication

1. **Power On**: Le microcontrôleur MCF52259 démarre suite à une mise sous tension ou un reset
2. **Initialisation MQX**: Le RTOS MQX s'initialise et configure le scheduler
3. **Main_task**: La tâche principale démarre et initialise le hardware (GPIO, ADC, PWM, timers)
4. **Configuration**: Lecture de la configuration depuis l'EEPROM SPI (adresse MAC, clé serveur)
5. **Création des tâches**: Main_task crée toutes les autres tâches avec leurs priorités respectives
6. **Stack réseau**: Ethernet_task initialise la stack TCP/IP RTCS et configure l'interface réseau
7. **Premier contact**: Le client contacte le serveur via GET /api/serverinfos pour vérifier la connectivité
8. **Polling actif**: Le système entre dans sa boucle principale de polling

**Points critiques:**
- L'ordre de création des tâches est important pour les dépendances
- La stack TCP/IP doit être complètement initialisée avant toute communication
- Le premier appel à /api/serverinfos valide la configuration réseau

---

## Polling mystatus/myactions

Ce diagramme illustre la boucle de polling continue entre le client et le serveur, qui constitue le cœur du protocole de communication.

```mermaid
sequenceDiagram
    participant Eth as Ethernet_task
    participant Server as Serveur Legacy
    participant Store as Store (État)
    participant TE as Table d'Échange
    
    loop Toutes les 1-2 secondes
        Note over Eth: Préparer mystatus
        Eth->>TE: Lire état actuel
        TE-->>Eth: Valeurs indices 605-622
        
        Eth->>Eth: Construire JSON mystatus
        Note over Eth: JSON avec clés non-quotées
        
        Eth->>Server: POST /api/mystatus
        Note over Eth,Server: Body: {_de67f:1, i605:val, ...}
        
        Server->>Server: Normaliser JSON
        Server->>Store: Mettre à jour état client
        
        Server->>Server: Vérifier actions pendantes
        
        alt Actions disponibles
            Server->>Server: Construire myactions
            Server->>Server: Bufferiser réponse complète
            Note over Server: legacyResponseWriter
            Server-->>Eth: 201 Created + myactions JSON
            Note over Server,Eth: UN SEUL paquet TCP!
            
            Eth->>Eth: Parser réponse HTTP
            Eth->>Eth: Parser JSON myactions
            Eth->>TE: Merger actions (bitwise OR)
            
            Note over Eth: Actions prêtes pour exécution
        else Pas d'actions
            Server->>Server: Bufferiser réponse vide
            Server-->>Eth: 200 OK (pas d'actions)
            Note over Server,Eth: UN SEUL paquet TCP!
            
            Eth->>Eth: Parser réponse HTTP
            Note over Eth: Continuer polling
        end
        
        Eth->>Eth: Attendre 1-2 secondes
    end
```

### Explication

1. **Fréquence**: Le client envoie une requête mystatus toutes les 1-2 secondes
2. **Préparation mystatus**: 
   - Lecture de l'état actuel de la table d'échange (indices 605-622)
   - Construction du JSON avec clés non-quotées (format legacy)
   - Le champ `_de67f` doit toujours être en premier et valoir 1
3. **Envoi au serveur**: POST /api/mystatus avec le JSON malformé
4. **Traitement serveur**:
   - Normalisation du JSON (ajout des quotes aux clés)
   - Mise à jour de l'état du client dans le store
   - Vérification des actions pendantes pour ce client
5. **Réponse avec actions** (201 Created):
   - Le serveur construit le JSON myactions avec toutes les actions
   - **CRITIQUE**: Utilisation du legacyResponseWriter pour bufferiser la réponse complète
   - Envoi en UN SEUL paquet TCP (contrainte hardware du client)
6. **Réponse sans actions** (200 OK):
   - Réponse vide mais toujours en un seul paquet TCP
7. **Traitement client**:
   - Parsing HTTP simple (attend tout en un paquet)
   - Parsing JSON des actions
   - Merge des actions dans la table d'échange avec bitwise OR
8. **Attente**: Délai de 1-2 secondes avant le prochain cycle

**Points critiques:**
- La contrainte single-packet TCP est ESSENTIELLE - sans elle, le client se bloque
- Le merge bitwise OR permet d'accumuler plusieurs commandes
- Le code 201 signale des actions disponibles, 200 signale aucune action
- Le polling continu assure une latence faible pour les commandes

---

## Exécution d'une Action

Ce diagramme montre comment une action reçue du serveur est exécutée par le client embarqué.

```mermaid
sequenceDiagram
    participant Eth as Ethernet_task
    participant TE as Table d'Échange
    participant Main as Main_task
    participant Module as Module Concerné
    participant HW as Hardware
    
    Note over Eth: Actions reçues via myactions
    
    Eth->>TE: Merger actions (bitwise OR)
    Note over TE: Indices 605-622 mis à jour
    
    Main->>TE: Lire table d'échange
    TE-->>Main: Nouvelles valeurs
    
    Main->>Main: Détecter changements
    
    alt Action sur lumière (index 607)
        Main->>Module: Appeler module concerné
        Note over Main,Module: Ex: Chauffage, Alarme, etc.
        
        Module->>HW: Commander GPIO/I2C
        HW-->>Module: Confirmation
        
        Module-->>Main: Action exécutée
        Main->>TE: Mettre à jour état (index 605)
        
    else Action sur chauffage (index 613)
        Main->>Module: Chauffage_SetMode()
        Module->>HW: Commander fil pilote (PWM)
        HW-->>Module: Confirmation
        Module-->>Main: Action exécutée
        Main->>TE: Mettre à jour état
        
    else Action sur scénario (index 615)
        Main->>Module: Scenario_Execute()
        Module->>Module: Exécuter séquence
        Module->>HW: Multiples commandes
        HW-->>Module: Confirmations
        Module-->>Main: Scénario terminé
        Main->>TE: Mettre à jour état
    end
    
    Note over Main,TE: État mis à jour pour prochain mystatus
```

### Explication

1. **Réception**: Les actions arrivent via le polling myactions et sont mergées dans la table d'échange
2. **Détection**: Main_task lit périodiquement la table d'échange et détecte les changements
3. **Dispatch**: Selon l'index modifié, Main_task appelle le module approprié:
   - **Index 607**: Contrôle des lumières (via I2C vers boîtiers auxiliaires)
   - **Index 613**: Contrôle du chauffage (fil pilote via PWM)
   - **Index 615**: Exécution de scénarios (séquences d'actions)
   - **Index 590**: Toujours à 1 (validation)
4. **Exécution hardware**: Le module concerné commande le hardware:
   - GPIO pour sorties directes
   - I2C pour boîtiers auxiliaires
   - PWM pour fil pilote
   - ADC pour lectures de capteurs
5. **Mise à jour état**: Une fois l'action exécutée, l'état est mis à jour dans la table d'échange
6. **Feedback**: Le prochain mystatus enverra l'état mis à jour au serveur

**Points critiques:**
- Le merge bitwise OR permet d'accumuler plusieurs commandes sans perte
- Main_task orchestre l'exécution mais délègue aux modules spécialisés
- La mise à jour de l'état dans la table d'échange assure le feedback au serveur
- Les actions peuvent être simples (une lumière) ou complexes (scénario complet)

---

## Acknowledgment d'Action

Ce diagramme montre comment le client acknowledge l'exécution d'une action au serveur.

```mermaid
sequenceDiagram
    participant Eth as Ethernet_task
    participant Server as Serveur Legacy
    participant Store as Store (État)
    
    Note over Eth: Action exécutée, état mis à jour
    
    Eth->>Eth: Préparer mystatus avec nouvel état
    Eth->>Server: POST /api/mystatus
    Note over Eth,Server: Contient état mis à jour (ex: lumière ON)
    
    Server->>Server: Normaliser JSON
    Server->>Store: Mettre à jour état client
    Server->>Store: Comparer avec état attendu
    
    alt État correspond à l'action
        Server->>Server: Marquer action comme exécutée
        Server->>Server: Générer GUID pour done
        Server-->>Eth: 200 OK
        
        Note over Server: Action confirmée implicitement
        
    else Action avait un GUID explicite
        Server->>Server: Préparer done dans myactions
        Server-->>Eth: 201 Created + done command
        
        Eth->>Eth: Parser done command
        Eth->>Server: POST /api/done/{guid}
        Note over Eth,Server: Acknowledge explicite
        
        Server->>Store: Supprimer action de la queue
        Server-->>Eth: 200 OK
    end
    
    Note over Server,Store: Action complètement traitée
```

### Explication

1. **Exécution complète**: Une fois l'action exécutée, l'état du client est mis à jour dans la table d'échange
2. **Mystatus avec nouvel état**: Le prochain mystatus contient l'état mis à jour
3. **Vérification serveur**: Le serveur compare l'état reçu avec l'état attendu après l'action
4. **Acknowledgment implicite**:
   - Si l'état correspond, l'action est considérée comme exécutée
   - Le serveur marque l'action comme complète dans sa queue
   - Pas de communication supplémentaire nécessaire
5. **Acknowledgment explicite** (optionnel):
   - Pour certaines actions critiques, le serveur peut demander un acknowledge explicite
   - Il envoie une commande "done" avec un GUID dans myactions
   - Le client répond avec POST /api/done/{guid}
   - Le serveur supprime définitivement l'action de sa queue
6. **Nettoyage**: L'action est retirée de la queue et ne sera plus renvoyée

**Points critiques:**
- L'acknowledgment implicite via mystatus est le mécanisme principal
- L'acknowledgment explicite via /api/done est utilisé pour les actions critiques
- Le serveur maintient un historique des 25 dernières valeurs pour chaque index
- Si une action n'est pas acknowledgée, elle peut être renvoyée (retry)

---

## Injection de Commande Admin

Ce diagramme montre comment un administrateur peut injecter une commande directement via l'interface web.

```mermaid
sequenceDiagram
    participant Admin as Administrateur
    participant Web as Interface Web
    participant Server as Serveur Legacy
    participant Store as Store (État)
    participant Eth as Client (Ethernet_task)
    participant TE as Table d'Échange
    
    Admin->>Web: Saisir commande
    Note over Admin,Web: Ex: Allumer lumière salon
    
    Web->>Web: Construire action JSON
    Note over Web: {i607: 1, i613: 0, ...}
    
    Web->>Server: POST /api/admin/inject
    Note over Web,Server: Body: action JSON + client ID
    
    Server->>Server: Valider action
    Server->>Store: Ajouter action à la queue client
    Server-->>Web: 200 OK
    
    Web-->>Admin: Confirmation
    
    Note over Store: Action en attente
    
    Eth->>Server: POST /api/mystatus (polling normal)
    Server->>Store: Vérifier queue client
    Store-->>Server: Action pendante trouvée
    
    Server->>Server: Construire myactions avec action
    Server->>Server: Bufferiser réponse (legacyResponseWriter)
    Server-->>Eth: 201 Created + myactions
    Note over Server,Eth: UN SEUL paquet TCP!
    
    Eth->>Eth: Parser myactions
    Eth->>TE: Merger action (bitwise OR)
    
    Note over Eth,TE: Action sera exécutée par Main_task
    
    Eth->>Server: POST /api/mystatus (avec nouvel état)
    Server->>Store: Vérifier exécution
    Server-->>Eth: 200 OK
    
    Server->>Store: Marquer action comme exécutée
    Server->>Web: Notification (WebSocket/SSE)
    Web-->>Admin: Action exécutée ✓
```

### Explication

1. **Saisie admin**: L'administrateur utilise l'interface web pour créer une commande
2. **Construction action**: L'interface web construit le JSON d'action avec les bons indices
3. **Injection**: POST /api/admin/inject envoie l'action au serveur avec l'ID du client cible
4. **Validation**: Le serveur valide l'action (indices corrects, valeurs valides)
5. **Queue**: L'action est ajoutée à la queue des actions pendantes pour ce client
6. **Confirmation web**: L'interface web confirme que l'action a été enregistrée
7. **Polling normal**: Le client continue son polling mystatus normal
8. **Détection action**: Le serveur détecte l'action pendante lors du prochain mystatus
9. **Envoi myactions**: L'action est envoyée au client via myactions (201 Created)
10. **Exécution**: Le client merge l'action et l'exécute normalement
11. **Feedback**: Le mystatus suivant contient l'état mis à jour
12. **Notification**: Le serveur peut notifier l'interface web que l'action a été exécutée

**Points critiques:**
- L'injection admin utilise la même queue que les actions automatiques
- Le délai d'exécution dépend de la fréquence de polling (1-2 secondes)
- L'interface web peut utiliser WebSocket ou SSE pour recevoir les notifications en temps réel
- La validation côté serveur empêche les actions invalides
- Le mécanisme est identique pour les actions automatiques (scénarios, programmations)

---

## Notes Générales

### Contraintes Critiques

Tous ces diagrammes doivent respecter les contraintes suivantes:

1. **Single-Packet TCP**: Toutes les réponses HTTP doivent être envoyées en UN SEUL paquet TCP
2. **JSON malformé**: Le client envoie du JSON avec clés non-quotées, le serveur doit normaliser
3. **Headers HTTP**: Format exact requis, notamment `Content-Type: application/json; charset=utf-8` (avec espace)
4. **Polling continu**: Le client poll toutes les 1-2 secondes, c'est le cœur du protocole
5. **Bitwise OR merge**: Les actions sont mergées avec OR, pas écrasées
6. **Champ _de67f**: Doit toujours être en premier dans le JSON et valoir 1

### Outils de Diagnostic

Pour observer ces séquences en pratique:

- **tcpdump**: Capturer les paquets TCP et vérifier le single-packet
- **Wireshark**: Analyser les séquences complètes avec timeline
- **Logs serveur**: Tracer les requêtes mystatus/myactions
- **Debug UART**: Observer les logs du client embarqué

### Références

- [Protocole HTTP Legacy](../protocol/http-legacy-protocol.md)
- [Contrainte TCP Single-Packet](../protocol/tcp-single-packet.md)
- [Table d'Échange](../protocol/exchange-table.md)
- [Architecture Client](../embedded-client/architecture.md)
- [API Endpoints](../server/api-endpoints.md)
