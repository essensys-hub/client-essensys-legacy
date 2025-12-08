#!/bin/bash

# Script pour prévisualiser la documentation MkDocs localement

echo "🚀 Démarrage de la prévisualisation de la documentation..."
echo ""

# Vérifier si Python est installé
if ! command -v python3 &> /dev/null; then
    echo "❌ Python 3 n'est pas installé. Veuillez l'installer d'abord."
    exit 1
fi

# Vérifier si pip est installé
if ! command -v pip3 &> /dev/null; then
    echo "❌ pip3 n'est pas installé. Veuillez l'installer d'abord."
    exit 1
fi

# Installer les dépendances si nécessaire
if ! python3 -c "import mkdocs" &> /dev/null; then
    echo "📦 Installation des dépendances MkDocs..."
    pip3 install -r requirements.txt
    echo ""
fi

# Lancer le serveur de développement
echo "✅ Lancement du serveur de développement..."
echo "📖 La documentation sera accessible à: http://127.0.0.1:8000"
echo ""
echo "Appuyez sur Ctrl+C pour arrêter le serveur"
echo ""

mkdocs serve
