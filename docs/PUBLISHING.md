# Publishing on GitHub

This folder is a clean source export, not the private development repository or its Git history. Publish the contents as a new repository. Do not publish the original working tree, its `work/`, cached builds or device data.

Create an empty GitHub repository named `COD4iOS`, then run these commands from this folder, replacing the account placeholder:

```sh
python3 tools/check-public-source.py
git init -b main
git add .
git status --short
git commit -m "Publish COD4iOS 1.0.3 source"
git remote add origin https://github.com/YOUR-GITHUB-USERNAME/COD4iOS.git
git push -u origin main
```

Git uses the name/email configured by you for commits; this export includes no commit history or signing identity. The `.gitignore` excludes builds, signing credentials, retail archives, runtime profiles and caches. The check tool also scans tracked files if the folder becomes a Git repository.

## GitHub Release

Create a release for tag `v1.0.3` and attach the separately provided unsigned device IPA, source ZIP, release notes and checksums. An IPA is an attachment to a Release, not a file to commit into the source repository. Preserve the supplied licenses and corresponding source.

Suggested release title: **COD4iOS 1.0.3 — SP/MP playtest**.

Describe the included fixes and the default browser filter. State that the IPA requires signing and the user's own game files. Do not claim universal server compatibility: Steam/official-client authentication and untested gameplay remain limitations.
