# Documentation MkDocs

Cette documentation est générée avec [MkDocs](https://www.mkdocs.org/) et le thème [Material for MkDocs](https://squidfunk.github.io/mkdocs-material/).

## Développement local

### Installation

```bash
pip install -r requirements.txt
```

### Prévisualisation

Pour prévisualiser la documentation localement:

```bash
mkdocs serve
```

La documentation sera accessible à l'adresse: http://127.0.0.1:8000

### Build

Pour générer les fichiers HTML statiques:

```bash
mkdocs build
```

Les fichiers seront générés dans le dossier `site/`.

## Déploiement sur GitHub Pages

### Configuration automatique

La documentation est automatiquement déployée sur GitHub Pages à chaque push sur la branche `main` via GitHub Actions.

Le workflow est défini dans `.github/workflows/deploy-docs.yml`.

### Configuration manuelle

1. Activez GitHub Pages dans les paramètres du repository:
   - Settings → Pages
   - Source: Deploy from a branch
   - Branch: `gh-pages` / `root`

2. Déployez manuellement:
   ```bash
   mkdocs gh-deploy
   ```

### URL de la documentation

Une fois déployée, la documentation sera accessible à:
```
https://VOTRE_USERNAME.github.io/VOTRE_REPO/
```

## Structure

```
docs/
├── index.md                          # Page d'accueil
├── protocol/                         # Documentation du protocole
│   ├── http-legacy-protocol.md
│   ├── tcp-single-packet.md
│   └── exchange-table.md
├── embedded-client/                  # Documentation du client
│   ├── architecture.md
│   ├── modules-reference.md
│   ├── communications.md
│   └── hardware.md
├── server/                           # Documentation du serveur
│   ├── go-implementation.md
│   └── api-endpoints.md
├── diagrams/                         # Diagrammes
│   ├── system-overview.md
│   ├── sequence-diagrams.md
│   └── data-flow.md
└── troubleshooting/                  # Dépannage
    └── network-debugging.md
```

## Personnalisation

### Modifier le thème

Éditez `mkdocs.yml` pour personnaliser:
- Couleurs (`theme.palette`)
- Navigation (`nav`)
- Plugins (`plugins`)
- Extensions Markdown (`markdown_extensions`)

### Ajouter des pages

1. Créez un nouveau fichier `.md` dans `docs/`
2. Ajoutez-le à la navigation dans `mkdocs.yml`:

```yaml
nav:
  - Nouvelle section:
      - Ma page: chemin/vers/page.md
```

## Fonctionnalités

- ✅ Thème Material Design responsive
- ✅ Mode sombre/clair
- ✅ Recherche en français
- ✅ Coloration syntaxique du code
- ✅ Diagrammes Mermaid
- ✅ Navigation par onglets
- ✅ Copie de code en un clic
- ✅ Déploiement automatique sur GitHub Pages
