dpkg-buildpackage -us -uc -b
dpkg-buildpackage -us -uc -b -aarm64

mv ../*.deb ../*.buildinfo ../*.changes dist