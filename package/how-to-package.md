# How to package rcn

Always update the version number!

Compile the program using ``make standard``

Move the package into ``./releases/x.y.z/``, this directory is ignored by git.

## RHEL and .rpm

Create a rcn-x.y.z.tar.gz file under ``rhel/SOURCES``

```bash
tar -czf  package/rhel/SOURCES/rcn-0.0.1.tar.gz *.c README.md Makefile  include/*.h
```

Inside of the package directory, run this command to package the existing tarball into a .rpm file.

```bash
rpmbuild -bb --define "_topdir $(pwd)/rhel" rhel/SPECS/rcn.spec
```

## Debian and .deb

Inside of ``deb``, create a folder with the correct version and release number.
Install the compiled binary into mock ``usr/bin/`` folder and the README.md into the mock ``usr/share/doc/<app-name>`` folder.

Then run this command, where PKG_ROOT is the path to the directory with the current version/release.

```bash
dpkg-deb --build --root-owner-group "$PKG_ROOT"
```

## Arch

Get the whole project to an arch based system.

Update the ``PKGBUILD`` file to the correct version.

Then run these commands to update the sha256 and build the package

```bash
updpkgsums
makepkg
```
