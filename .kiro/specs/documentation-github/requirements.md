# Requirements Document

## Introduction

Ce document définit les exigences pour créer une documentation complète du projet client-essensys-legacy pour publication sur GitHub. Le projet comprend un client embarqué BP_MQX_ETH (basé sur MQX RTOS) qui communique avec un serveur via un protocole HTTP non-standard avec des contraintes critiques découvertes lors du développement.

## Glossary

- **BP_MQX_ETH**: Boîtier Principal utilisant le système d'exploitation temps réel MQX sur microcontrôleur Coldfire MCF52259
- **Client Embarqué**: Le système embarqué qui exécute le firmware C et communique avec le serveur
- **Serveur Legacy**: Le serveur Go qui implémente le protocole HTTP adapté aux contraintes du client
- **Table d'Échange**: Structure de données partagée entre le client et le serveur (indices 605-622)
- **Protocole Legacy**: Le protocole HTTP non-standard avec JSON malformé et contraintes TCP
- **Single-Packet Requirement**: Contrainte critique où toute la réponse HTTP doit être envoyée en un seul paquet TCP
- **I2C**: Bus de communication série utilisé pour communiquer avec les boîtiers auxiliaires
- **Fil Pilote**: Système de contrôle du chauffage électrique français
- **MQX RTOS**: Real-Time Operating System de Freescale utilisé par le client embarqué

## Requirements

### Requirement 1

**User Story:** En tant que développeur découvrant le projet, je veux un README.md clair et structuré, afin de comprendre rapidement le but du projet et comment démarrer.

#### Acceptance Criteria

1. WHEN un développeur ouvre le dépôt GitHub THEN le système SHALL afficher un README.md avec une description du projet
2. WHEN le README est lu THEN le système SHALL présenter la licence MIT de manière visible
3. WHEN un développeur lit le README THEN le système SHALL fournir une section "Getting Started" avec les prérequis
4. WHEN le README décrit l'architecture THEN le système SHALL inclure un diagramme montrant client embarqué et serveur
5. WHEN le README liste les fonctionnalités THEN le système SHALL mentionner les contraintes critiques du protocole

### Requirement 2

**User Story:** En tant que développeur, je veux une documentation détaillée du protocole HTTP legacy, afin de comprendre les contraintes non-standard et comment les gérer.

#### Acceptance Criteria

1. WHEN un développeur consulte la documentation du protocole THEN le système SHALL expliquer la contrainte single-packet TCP
2. WHEN la documentation décrit le JSON THEN le système SHALL montrer des exemples de JSON malformé et sa normalisation
3. WHEN la documentation liste les endpoints THEN le système SHALL décrire chaque endpoint avec méthode HTTP et format de réponse
4. WHEN la documentation explique les headers THEN le système SHALL spécifier les headers obligatoires avec leur format exact
5. WHEN la documentation présente la table d'échange THEN le système SHALL expliquer pourquoi tous les indices 605-622 doivent être envoyés

### Requirement 3

**User Story:** En tant que développeur, je veux une documentation de l'architecture du client embarqué, afin de comprendre comment le firmware C est organisé et fonctionne.

#### Acceptance Criteria

1. WHEN un développeur lit la documentation client THEN le système SHALL décrire l'architecture MQX avec ses tâches
2. WHEN la documentation liste les tâches THEN le système SHALL expliquer le rôle de chaque tâche (Main, Ecran, BA, TeleInfo, Ethernet)
3. WHEN la documentation décrit les communications THEN le système SHALL expliquer le bus I2C vers les boîtiers auxiliaires
4. WHEN la documentation présente les modules THEN le système SHALL lister les modules C avec leur responsabilité
5. WHEN la documentation explique le parser HTTP THEN le système SHALL décrire pourquoi il ne supporte qu'un seul paquet TCP

### Requirement 4

**User Story:** En tant que développeur, je veux une documentation des contraintes critiques découvertes, afin d'éviter les pièges lors de modifications futures.

#### Acceptance Criteria

1. WHEN un développeur lit les contraintes THEN le système SHALL expliquer la découverte du single-packet requirement avec tcpdump
2. WHEN la documentation présente les symptômes THEN le système SHALL décrire le comportement de blocage du client
3. WHEN la documentation montre la solution THEN le système SHALL fournir des exemples de code Go pour bufferiser la réponse
4. WHEN la documentation liste les outils THEN le système SHALL recommander tcpdump pour le diagnostic réseau
5. WHEN la documentation explique le Content-Type THEN le système SHALL préciser l'espace requis avant ";charset"

### Requirement 5

**User Story:** En tant que développeur, je veux une documentation de la table d'échange, afin de comprendre comment contrôler les lumières et autres équipements.

#### Acceptance Criteria

1. WHEN un développeur consulte la table d'échange THEN le système SHALL lister tous les indices spéciaux avec leur signification
2. WHEN la documentation explique le contrôle THEN le système SHALL montrer un exemple complet d'allumage d'une lumière
3. WHEN la documentation décrit le merge THEN le système SHALL expliquer la logique bitwise OR pour combiner les commandes
4. WHEN la documentation présente l'index 590 THEN le système SHALL préciser qu'il doit toujours valoir "1"
5. WHEN la documentation montre le champ _de67f THEN le système SHALL expliquer qu'il doit être en premier dans le JSON

### Requirement 6

**User Story:** En tant que développeur, je veux des exemples de code et diagrammes, afin de visualiser rapidement comment implémenter ou modifier le système.

#### Acceptance Criteria

1. WHEN la documentation présente un concept THEN le système SHALL inclure des exemples de code commentés
2. WHEN la documentation explique un flux THEN le système SHALL fournir un diagramme de séquence
3. WHEN la documentation montre le polling THEN le système SHALL illustrer la fréquence des appels API
4. WHEN la documentation présente les réponses HTTP THEN le système SHALL montrer des exemples complets avec headers
5. WHEN la documentation décrit l'architecture THEN le système SHALL inclure un diagramme des composants

### Requirement 7

**User Story:** En tant que développeur, je veux une documentation de référence du firmware C, afin de comprendre les modules et leurs interactions.

#### Acceptance Criteria

1. WHEN un développeur consulte la référence C THEN le système SHALL documenter les modules principaux (Alarme, Chauffage, Ecran, etc.)
2. WHEN la documentation décrit un module THEN le système SHALL expliquer sa responsabilité et ses dépendances
3. WHEN la documentation présente le fil pilote THEN le système SHALL expliquer le système de chauffage français
4. WHEN la documentation liste les communications THEN le système SHALL décrire I2C, SPI, UART et Ethernet
5. WHEN la documentation montre la structure THEN le système SHALL présenter l'organisation des dossiers C/ H/ Ethernet/

### Requirement 8

**User Story:** En tant que développeur, je veux une licence MIT claire, afin de savoir comment je peux utiliser et modifier le code.

#### Acceptance Criteria

1. WHEN un développeur consulte le dépôt THEN le système SHALL inclure un fichier LICENSE avec le texte MIT complet
2. WHEN le README mentionne la licence THEN le système SHALL inclure un badge de licence visible
3. WHEN la documentation est créée THEN le système SHALL dater la licence avec l'année actuelle
4. WHEN le fichier LICENSE est lu THEN le système SHALL identifier le détenteur du copyright
5. WHEN la licence est affichée THEN le système SHALL utiliser le format standard OSI pour MIT
