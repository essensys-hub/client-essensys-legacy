# Design Document - Documentation GitHub pour client-essensys-legacy

## Overview

Ce document décrit la conception de la documentation complète pour le projet client-essensys-legacy. La documentation sera structurée en plusieurs fichiers markdown organisés dans un dossier `docs/` avec un README.md principal à la racine. L'objectif est de fournir une documentation claire, accessible et complète pour les développeurs qui découvrent ce projet de système domotique embarqué avec ses contraintes uniques.

## Architecture

### Structure de la Documentation

```
/
├── README.md                          # Point d'entrée principal
├── LICENSE                            # Licence MIT
├── docs/
│   ├── protocol/
│   │   ├── http-legacy-protocol.md   # Protocole HTTP non-standard
│   │   ├── tcp-single-packet.md      # Contrainte critique TCP
│   │   └── exchange-table.md         # Table d'échange (indices 605-622)
│   ├── embedded-client/
│   │   ├── architecture.md           # Architecture MQX et tâches
│   │   ├── modules-reference.md      # Référence des modules C
│   │   ├── communications.md         # I2C, SPI, UART, Ethernet
│   │   └── hardware.md               # MCF52259, périphériques
│   ├── server/
│   │   ├── go-implementation.md      # Implémentation serveur Go
│   │   └── api-endpoints.md          # Documentation des endpoints
│   ├── diagrams/
│   │   ├── system-overview.md        # Diagramme système complet
│   │   ├── sequence-diagrams.md      # Diagrammes de séquence
│   │   └── data-flow.md              # Flux de données
│   └── troubleshooting/
│       ├── network-debugging.md      # Utilisation de tcpdump
│       └── common-issues.md          # Problèmes courants et solutions
```

### Principes de Design

1. **Progressive Disclosure**: Le README donne une vue d'ensemble, les docs/ approfondissent
2. **Code Examples**: Chaque concept important illustré par du code
3. **Visual Aids**: Diagrammes Mermaid pour l'architecture et les flux
4. **Searchability**: Structure claire avec titres descriptifs
5. **Bilingual**: Documentation en français (langue du projet original)

## Components and Interfaces

### README.md Principal

**Sections:**
- Badges (licence, langage)
- Description courte du projet
- Table des matières
- Contexte et motivation
- Architecture système (diagramme)
- Contraintes critiques (résumé)
- Getting Started
- Documentation complète (liens vers docs/)
- Licence
- Remerciements

**Format:**
- Markdown standard GitHub
- Diagrammes Mermaid intégrés
- Liens relatifs vers docs/
- Code snippets avec syntax highlighting

### LICENSE

**Contenu:**
- Texte complet de la licence MIT
- Copyright holder: À déterminer avec l'utilisateur
- Année: 2025

### docs/protocol/http-legacy-protocol.md

**Sections:**
1. Vue d'ensemble du protocole
2. JSON malformé (clés non-quotées)
3. Normalisation du JSON
4. Headers HTTP requis
5. Endpoints API
6. Format des réponses
7. Codes de statut
8. Exemples complets

**Exemples de code:**
- JSON brut du client
- JSON normalisé
- Fonction Go de normalisation
- Exemples de requêtes/réponses complètes

### docs/protocol/tcp-single-packet.md

**Sections:**
1. Découverte du problème
2. Symptômes observés
3. Analyse avec tcpdump
4. Explication technique (mémoire limitée, parser simple)
5. Solution implémentée (bufferisation)
6. Code Go du legacyResponseWriter
7. Validation
8. Outils de diagnostic

**Diagrammes:**
- Comparaison paquets TCP (avant/après)
- Flow chart du parser HTTP client

### docs/protocol/exchange-table.md

**Sections:**
1. Concept de la table d'échange
2. Indices 605-622 (liste complète)
3. Index spéciaux (590, 613, 607, 615, etc.)
4. Logique de merge (bitwise OR)
5. Champ _de67f
6. Exemples d'actions
7. Historique des valeurs

**Tableaux:**
- Mapping complet des indices
- Exemples de commandes

### docs/embedded-client/architecture.md

**Sections:**
1. Vue d'ensemble MQX RTOS
2. Microcontrôleur MCF52259
3. Tâches MQX
   - Main_task (priorité, stack, rôle)
   - Ecran_task
   - Boitiers_task (I2C)
   - TeleInfo_task
   - Ethernet_task
4. Synchronisation (mutex, semaphores)
5. Message pools
6. Timers
7. Cycle de vie du système

**Diagrammes:**
- Architecture des tâches
- Diagramme de priorités
- Flow chart du démarrage

### docs/embedded-client/modules-reference.md

**Sections:**
1. Organisation des fichiers (C/, H/, Ethernet/)
2. Modules principaux:
   - Alarme (détection, sirènes)
   - Chauffage (fil pilote, zones)
   - Ecran (interface utilisateur)
   - TableEchange (données partagées)
   - TeleInfo (compteur Linky)
   - Hard (initialisation hardware)
   - Scenario (automatisations)
   - GestionSocket (Ethernet)
   - Json (parsing)
   - Cryptage (sécurité)
3. Dépendances entre modules
4. Points d'extension

**Tableaux:**
- Liste des modules avec description
- Matrice de dépendances

### docs/embedded-client/communications.md

**Sections:**
1. I2C (boîtiers auxiliaires)
   - Configuration
   - Protocole
   - Gestion des erreurs
   - Timeouts
2. SPI (EEPROM)
   - Adresse MAC
   - Clé serveur
   - Configuration
3. UART (TeleInfo, debug)
   - Format des trames
   - Parsing
4. Ethernet (TCP/IP)
   - Stack RTCS
   - Configuration réseau
   - Sockets

**Exemples de code:**
- Lecture I2C
- Écriture SPI
- Parsing TeleInfo

### docs/embedded-client/hardware.md

**Sections:**
1. MCF52259 Coldfire
2. Mémoire (Flash, RAM, MRAM externe)
3. Périphériques
   - GPIO (entrées/sorties)
   - ADC (détection fuites)
   - PWM (fil pilote)
   - Timers
4. Alimentation et batterie
5. Schéma de principe

### docs/server/go-implementation.md

**Sections:**
1. Architecture du serveur Go
2. legacyResponseWriter
3. Handlers HTTP
4. Store (état du système)
5. Services
6. Configuration
7. Déploiement

### docs/server/api-endpoints.md

**Sections:**
1. GET /api/serverinfos
2. POST /api/mystatus
3. GET /api/myactions
4. POST /api/done/{guid}
5. POST /api/admin/inject
6. Format des requêtes/réponses
7. Codes d'erreur

### docs/diagrams/system-overview.md

**Contenu:**
- Diagramme Mermaid du système complet
- Client embarqué ↔ Serveur ↔ Interface web
- Flux de données
- Composants externes (compteur Linky, boîtiers I2C)

### docs/diagrams/sequence-diagrams.md

**Diagrammes:**
1. Séquence de démarrage
2. Polling mystatus/myactions
3. Exécution d'une action
4. Acknowledge d'action
5. Injection de commande admin

### docs/diagrams/data-flow.md

**Diagrammes:**
- Flow des données de la table d'échange
- Cycle de vie d'une action
- Mise à jour de l'état

### docs/troubleshooting/network-debugging.md

**Sections:**
1. Utilisation de tcpdump
2. Analyse des paquets TCP
3. Vérification du single-packet
4. Inspection des headers HTTP
5. Debugging du JSON
6. Outils recommandés (Wireshark, tshark)

### docs/troubleshooting/common-issues.md

**Sections:**
1. Client ne répond plus après première requête
2. Actions non exécutées
3. JSON parsing errors
4. Timeouts réseau
5. Problèmes I2C
6. Erreurs de synchronisation

## Data Models

### Documentation Metadata

```markdown
---
title: "Titre du document"
description: "Description courte"
category: "protocol|embedded-client|server|diagrams|troubleshooting"
tags: ["http", "tcp", "json", "i2c", etc.]
last_updated: "2025-01-XX"
---
```

### Code Example Format

```markdown
### Exemple: [Titre]

**Contexte:** [Explication du contexte]

```language
// Code avec commentaires
```

**Résultat:** [Ce que produit le code]
```

### Diagram Format

```markdown
### Diagramme: [Titre]

**Description:** [Explication du diagramme]

```mermaid
[Code Mermaid]
```

**Légende:**
- [Explication des éléments]
```

## Error Handling

### Documentation Errors

1. **Liens brisés**: Vérifier tous les liens relatifs
2. **Code invalide**: Tester les exemples de code
3. **Diagrammes cassés**: Valider la syntaxe Mermaid
4. **Informations obsolètes**: Dater les documents

### Maintenance

1. **Versioning**: Utiliser les dates de mise à jour
2. **Changelog**: Documenter les modifications importantes
3. **Issues**: Encourager les contributions via GitHub issues
4. **Pull Requests**: Template pour les contributions

## Testing Strategy

### Documentation Quality

1. **Lisibilité**: Relecture humaine
2. **Exactitude technique**: Vérification avec le code source
3. **Complétude**: Checklist des exigences
4. **Liens**: Validation automatique des liens
5. **Markdown**: Linting avec markdownlint

### Validation des Exemples

1. **Code C**: Compilation des snippets
2. **Code Go**: Tests unitaires
3. **JSON**: Validation de la syntaxe
4. **Diagrammes**: Rendu Mermaid

### User Testing

1. **Développeur novice**: Peut-il démarrer?
2. **Développeur expérimenté**: Trouve-t-il les détails?
3. **Mainteneur**: Peut-il modifier le système?

## Implementation Notes

### Ordre de Création

1. LICENSE
2. README.md (structure de base)
3. docs/protocol/ (fondamental pour comprendre)
4. docs/embedded-client/ (architecture du client)
5. docs/server/ (implémentation serveur)
6. docs/diagrams/ (visualisations)
7. docs/troubleshooting/ (guide pratique)
8. README.md (finalisation avec liens)

### Style Guide

- **Langue**: Français (projet français)
- **Ton**: Technique mais accessible
- **Code**: Commentaires en français
- **Termes techniques**: Glossaire dans chaque doc si nécessaire
- **Formatting**: 
  - Titres: # ## ### (max 3 niveaux)
  - Code blocks: Toujours avec langage spécifié
  - Listes: - pour unordered, 1. pour ordered
  - Emphasis: **bold** pour important, *italic* pour termes

### Tools

- **Editor**: Tout éditeur markdown
- **Preview**: GitHub markdown preview ou VSCode
- **Diagrams**: Mermaid live editor pour validation
- **Linting**: markdownlint-cli
- **Link checking**: markdown-link-check

## Correctness Properties

*A property is a characteristic or behavior that should hold true across all valid executions of a system-essentially, a formal statement about what the system should do. Properties serve as the bridge between human-readable specifications and machine-verifiable correctness guarantees.*

### Property 1: Documentation Completeness

*For any* requirement in requirements.md, there should exist at least one documentation file that addresses that requirement with concrete content.

**Validates: Requirements 1.1, 2.1, 3.1, 4.1, 5.1, 6.1, 7.1, 8.1**

### Property 2: Link Validity

*For any* markdown link in the documentation, the target file or anchor should exist in the repository.

**Validates: Requirements 1.3, 6.1**

### Property 3: Code Example Validity

*For any* code example in the documentation, the syntax should be valid for the specified language.

**Validates: Requirements 6.1, 6.4**

### Property 4: Diagram Renderability

*For any* Mermaid diagram in the documentation, the syntax should be valid and renderable.

**Validates: Requirements 1.4, 6.2, 6.5**

### Property 5: License Presence

*For any* documentation file created, the LICENSE file should exist at the repository root with valid MIT license text.

**Validates: Requirements 8.1, 8.2, 8.3, 8.4, 8.5**

### Property 6: Structural Consistency

*For any* documentation file in docs/, it should follow the defined structure with title, sections, and proper markdown formatting.

**Validates: Requirements 1.1, 2.1, 3.1, 4.1, 5.1, 6.1, 7.1**

### Property 7: Cross-Reference Accuracy

*For any* reference to a code file or line number in the documentation, that file and line should exist in the repository.

**Validates: Requirements 3.4, 7.1, 7.2**

### Property 8: Example Completeness

*For any* protocol endpoint documented, there should exist a complete request and response example.

**Validates: Requirements 2.3, 6.4**
