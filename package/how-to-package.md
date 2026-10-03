# How to package rcn

Always update the version number!

Compile the program using ``make standard``

Move the packages into ``./releases/x.y.z/``, this directory is ignored by git.

## RHEL and .rpm

Create a rcn-x.y.z.tar.gz file under ``rhel/SOURCES``

```bash
tar -czf  package/rhel/SOURCES/rcn-x.y.z.tar.gz *.c README.md Makefile  include/*.h
```

Inside of the package directory, run this command to package the existing tarball into a .rpm file.

```bash
rpmbuild -bb --define "_topdir $(pwd)/rhel" rhel/SPECS/rcn.spec
```

## Debian and .deb

Inside of ``debian``, create a folder with the correct version and release number.
Install the compiled binary into the mock ``usr/bin/`` folder and the README.md into the mock ``usr/share/doc/<app-name>/`` folder.

Then run this command, where PKG_ROOT is the path to the directory with the current version/release.

```bash
dpkg-deb --build --root-owner-group "$PKG_ROOT"
```

## Arch and .tar.zst

Get the whole project to an arch based system.

Can be a tarball of the whole repo, created using

```bash
tar -czf rcn.tar.gz rcn
```

And unpacked using

```bash
tar -xf rcn.tar.gz
```

Update the ``PKGBUILD`` file to the correct version.

Copy the rcn.tar.gz file into the `package/arch` directory and name it rcn-x.y.z.tar.gz.

Then run these commands to update the sha256, from the `arch/` directory, and build the package

```bash
updpkgsums
makepkg
```
