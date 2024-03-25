# Release

This directory contains some stuff used during preparation of release version of the application and making redistributable packages.

## Update version

Decide [version numbers](https://semver.org) for the new release. Current version is stored in `version.txt` file. Increase at least one of `MAJOR`, `MINOR`, or `PATCH` numbers when creating a new release. Update version info and make a new release tag:

```bash
git commit -am 'Bump version to 0.0.1'
git tag -a v0.0.1 -m 'First demo'
git push origin v0.0.1
```

## Build and package

*TODO*
