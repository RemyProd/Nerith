[← Index de la documentation](README.md)

# Environnement de développement (Windows)

## Objectif
Préparer un environnement C++ fiable. Ne pas installer toutes les bibliothèques graphiques et dépendances dès le premier jour.

## 1. Visual Studio Community
https://visualstudio.microsoft.com/vs/community/

Dans Visual Studio Installer, sélectionner **Développement Desktop en C++**.

Vérifier les composants :
- MSVC C++ x64/x86 Build Tools.
- Windows 10 ou Windows 11 SDK.
- C++ CMake tools for Windows.
- Outils de diagnostic et de débogage C++.

## 2. Git
https://git-scm.com/downloads/win

Gestion de versions du code.

## 3. GitHub Desktop (facultatif)
https://desktop.github.com/

Interface graphique facultative pour Git.

## 4. CMake
https://cmake.org/download/

Outil de configuration du système de build. Visual Studio inclut aussi des outils CMake ; vérifier avant d'installer une seconde version.

## 5. Visual Studio Code (facultatif)
https://code.visualstudio.com/

Éditeur complémentaire. Visual Studio Community reste l'environnement principal au départ.

## À ne pas installer immédiatement
- Qt.
- Vulkan SDK.
- GoogleTest.
- Bibliothèques audio.
- Bibliothèques USB/HID.
- Outils de profilage avancés.

Les installer uniquement quand une étape du projet en a besoin.

## Vérifications
Dans « Developer PowerShell for Visual Studio », tester :

```powershell
cl
cmake --version
git --version
```

`cl` doit afficher les informations du compilateur Microsoft. CMake et Git doivent afficher leur version. Si une commande n'est pas reconnue, vérifier l'installation et utiliser le terminal développeur.

`cl` n'existe que dans « Developer PowerShell for Visual Studio », pas dans un PowerShell normal. Ce terminal affiche « for x86 » : il cible x86 par défaut. Le projet visera du 64 bits (x64), à régler plus tard dans la configuration CMake.

## Configuration utilisée
| Élément | Version |
|---|---|
| Système | Windows 11 Professionnel, 64 bits |
| Visual Studio | Community 2026, version 18.12.0 |
| MSVC | 19.52.37029 (affichée par `cl`) |
| Windows SDK | 10.0.26100.0 |
| CMake | 4.5.0 |
| Git | 2.56.0 (Windows) |
