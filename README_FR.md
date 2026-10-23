# GaëlRack Fx — projet Windows x64

Plugin d'effets vocaux JUCE, formats VST3 et Standalone. Le code source et les scripts sont fournis dans cette archive. **Cette archive contient les sources, pas un VST3 déjà compilé.**

## Prérequis

- Windows 10/11 64 bits
- Visual Studio 2022 avec la charge de travail **Développement Desktop en C++** et un SDK Windows
- CMake 3.22 ou plus récent, ajouté au `PATH`
- Connexion Internet lors de la première configuration, car CMake télécharge JUCE 8.0.6 depuis GitHub

## Compiler

1. Décompresser l'archive.
2. Ouvrir PowerShell dans le dossier `GaelRackFx`.
3. Lancer :

   ```powershell
   Set-ExecutionPolicy -Scope Process Bypass
   .\build.ps1
   ```

Le script arrête l'opération si la configuration CMake ou la compilation échoue. Les résultats se trouvent dans `build`. Le nom et le chemin précis dépendent de la génération CMake/JUCE. Rechercher le bundle `GaelRack Fx.vst3` et l'application Standalone.

## Installer le VST3

Copier le dossier complet `GaelRack Fx.vst3` vers :

`C:\Program Files\Common Files\VST3\`

Relancer le logiciel audio et rescanner les plugins VST3. Il faut copier le dossier `.vst3` complet, pas seulement le fichier qu'il contient.

## Créer un ZIP de distribution après compilation

Après une compilation réussie, lancer :

```powershell
.\package.ps1
```

Le ZIP est créé dans `dist\GaelRackFx-Windows-x64.zip`.

## État de validation et limites connues

- Le fichier C++ fourni est complet jusqu'à la fonction de création du plugin (`createPluginFilter`).
- La configuration/compilation n'a pas pu être validée dans l'environnement de préparation : l'accès à GitHub était indisponible pour télécharger JUCE. Il faut donc effectuer la compilation sur Windows avec les prérequis ci-dessus avant de considérer le plugin comme prêt à distribuer.
- Les potentiomètres sont exposés comme paramètres automatisables par l'hôte.
- Les presets sont des points de départ ; les modifications manuelles ne sont pas enregistrées comme un nouveau preset nommé.
- Le menu Mic applique des corrections tonales indicatives, pas une émulation mesurée de microphones précis.
- L'EQ est adaptatif et simplifié ; ce n'est pas une reproduction de soothe2. La saturation n'utilise pas de suréchantillonnage.
- Avant une distribution publique, tester dans l'application Standalone et au moins un hôte VST3, notamment l'automatisation, les sessions mono/stéréo et la sauvegarde/restauration des paramètres.
