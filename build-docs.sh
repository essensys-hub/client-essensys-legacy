#!/bin/bash

# Script pour construire la documentation MkDocs localement

echo "🔨 Construction de la documentation..."
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

# Nettoyer le dossier site/ s'il existe
if [ -d "site" ]; then
    echo "🧹 Nettoyage du dossier site/ existant..."
    rm -rf site/
fi

# Construire la documentation
echo "🔨 Construction de la documentation..."
mkdocs build

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Documentation construite avec succès!"
    echo "📁 Les fichiers sont dans le dossier: site/"
    echo ""
    echo "Pour prévisualiser localement:"
    echo "  cd site && python3 -m http.server 8000"
    echo "  Puis ouvrez: http://localhost:8000"
else
    echo ""
    echo "❌ Erreur lors de la construction de la documentation"
    exit 1
fi
