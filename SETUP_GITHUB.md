# Configuration GitHub pour la Documentation MkDocs

Ce guide vous explique comment configurer votre repository GitHub pour publier automatiquement la documentation avec MkDocs.

## Étape 1: Créer le Repository sur GitHub

1. Allez sur [GitHub](https://github.com) et connectez-vous
2. Cliquez sur le bouton "+" en haut à droite → "New repository"
3. Remplissez les informations:
   - **Repository name**: `client-essensys-legacy` (ou le nom de votre choix)
   - **Description**: "Système domotique embarqué basé sur MQX RTOS"
   - **Visibility**: Public ou Private (selon vos besoins)
   - **Ne cochez PAS** "Initialize this repository with a README" (vous en avez déjà un)
4. Cliquez sur "Create repository"

## Étape 2: Pousser le Code vers GitHub

Dans votre terminal, exécutez les commandes suivantes:

```bash
# Ajouter le remote GitHub (remplacez VOTRE_USERNAME et VOTRE_REPO)
git remote add origin https://github.com/VOTRE_USERNAME/VOTRE_REPO.git

# Vérifier que le remote est bien configuré
git remote -v

# Pousser le code vers GitHub
git push -u origin main
```

## Étape 3: Activer GitHub Pages

1. Allez dans votre repository sur GitHub
2. Cliquez sur "Settings" (⚙️)
3. Dans le menu de gauche, cliquez sur "Pages"
4. Sous "Build and deployment":
   - **Source**: Sélectionnez "Deploy from a branch"
   - **Branch**: Sélectionnez `gh-pages` et `/root`
   - Cliquez sur "Save"

**Note**: La branche `gh-pages` sera créée automatiquement lors du premier déploiement par GitHub Actions.

## Étape 4: Activer GitHub Actions

GitHub Actions devrait être activé par défaut, mais vérifiez:

1. Allez dans "Settings" → "Actions" → "General"
2. Sous "Actions permissions", assurez-vous que "Allow all actions and reusable workflows" est sélectionné
3. Sous "Workflow permissions", sélectionnez "Read and write permissions"
4. Cochez "Allow GitHub Actions to create and approve pull requests"
5. Cliquez sur "Save"

## Étape 5: Déclencher le Premier Déploiement

Le workflow se déclenche automatiquement à chaque push sur `main`, mais vous pouvez aussi le déclencher manuellement:

1. Allez dans l'onglet "Actions" de votre repository
2. Cliquez sur "Deploy Documentation" dans la liste des workflows
3. Cliquez sur "Run workflow" → "Run workflow"

Le déploiement prend environ 1-2 minutes.

## Étape 6: Mettre à Jour les URLs

Une fois le repository créé, mettez à jour les URLs dans les fichiers suivants:

### 1. `mkdocs.yml`

Remplacez:
```yaml
repo_url: https://github.com/VOTRE_USERNAME/VOTRE_REPO
```

Par:
```yaml
repo_url: https://github.com/votre-username-reel/client-essensys-legacy
```

### 2. `README.md`

Remplacez:
```markdown
[![Documentation](https://img.shields.io/badge/docs-MkDocs-blue.svg)](https://VOTRE_USERNAME.github.io/VOTRE_REPO/)
```

Par:
```markdown
[![Documentation](https://img.shields.io/badge/docs-MkDocs-blue.svg)](https://votre-username-reel.github.io/client-essensys-legacy/)
```

Et:
```markdown
📚 **[Documentation complète en ligne](https://VOTRE_USERNAME.github.io/VOTRE_REPO/)**
```

Par:
```markdown
📚 **[Documentation complète en ligne](https://votre-username-reel.github.io/client-essensys-legacy/)**
```

### 3. `docs/index.md`

Remplacez les deux occurrences de:
```markdown
https://github.com/VOTRE_USERNAME/VOTRE_REPO
```

Par:
```markdown
https://github.com/votre-username-reel/client-essensys-legacy
```

### 4. Commitez et poussez les changements

```bash
git add mkdocs.yml README.md docs/index.md
git commit -m "Update repository URLs"
git push
```

## Étape 7: Vérifier le Déploiement

1. Allez dans l'onglet "Actions" de votre repository
2. Vous devriez voir un workflow "Deploy Documentation" en cours d'exécution
3. Attendez qu'il se termine (✅ vert)
4. Allez dans "Settings" → "Pages"
5. Vous verrez l'URL de votre documentation: `https://votre-username.github.io/client-essensys-legacy/`
6. Cliquez sur le lien pour voir votre documentation en ligne!

## Développement Local

Pour prévisualiser la documentation localement avant de pousser:

```bash
# Installer les dépendances
pip install -r requirements.txt

# Lancer le serveur de développement
mkdocs serve

# Ouvrir http://127.0.0.1:8000 dans votre navigateur
```

## Mise à Jour de la Documentation

Pour mettre à jour la documentation:

1. Modifiez les fichiers `.md` dans le dossier `docs/`
2. Prévisualisez localement avec `mkdocs serve`
3. Commitez et poussez:
   ```bash
   git add docs/
   git commit -m "Update documentation"
   git push
   ```
4. GitHub Actions déploiera automatiquement les changements

## Dépannage

### Le workflow échoue

- Vérifiez que les permissions GitHub Actions sont correctes (Étape 4)
- Vérifiez les logs dans l'onglet "Actions"

### La page GitHub Pages affiche une erreur 404

- Assurez-vous que la branche `gh-pages` existe
- Vérifiez que GitHub Pages est configuré pour utiliser la branche `gh-pages`
- Attendez quelques minutes après le premier déploiement

### Les diagrammes Mermaid ne s'affichent pas

- Vérifiez que `mkdocs-mermaid2-plugin` est installé
- Vérifiez la syntaxe de vos diagrammes Mermaid

### La recherche ne fonctionne pas

- La recherche est générée lors du build
- Videz le cache de votre navigateur
- Vérifiez que le plugin `search` est activé dans `mkdocs.yml`

## Personnalisation

### Changer les couleurs

Éditez `mkdocs.yml`:

```yaml
theme:
  palette:
    - scheme: default
      primary: blue  # Changez ici
      accent: blue   # Et ici
```

Couleurs disponibles: red, pink, purple, deep purple, indigo, blue, light blue, cyan, teal, green, light green, lime, yellow, amber, orange, deep orange

### Ajouter une page

1. Créez un fichier `.md` dans `docs/`
2. Ajoutez-le à la navigation dans `mkdocs.yml`:

```yaml
nav:
  - Ma nouvelle section:
      - Ma page: chemin/vers/ma-page.md
```

### Ajouter un logo

1. Ajoutez votre logo dans `docs/assets/logo.png`
2. Éditez `mkdocs.yml`:

```yaml
theme:
  logo: assets/logo.png
  favicon: assets/favicon.ico
```

## Support

Pour toute question:
- Consultez la [documentation MkDocs](https://www.mkdocs.org/)
- Consultez la [documentation Material for MkDocs](https://squidfunk.github.io/mkdocs-material/)
- Ouvrez une issue sur GitHub
