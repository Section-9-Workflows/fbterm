# Debian package and Depot build

Based on Debian's fbterm 1.7-5 packaging by Nobuhiro Iwamatsu:
https://salsa.debian.org/debian/fbterm
https://deb.debian.org/debian/pool/main/f/fbterm/fbterm_1.7-5.debian.tar.xz
SHA256: a473a1aeda3d62ca3c22fad9396140a7f2fa3672768e47ec590c0f8d24d853ef
The Debian changelog and packaging copyright are retained. The native debhelper
workflow is adapted to this fork; old Debian patches against FbTerm 1.7 are
not reapplied over the 1.8 fork.

`dpkg-buildpackage -b -us -uc` builds package **fbterm**, executable
**/usr/bin/fbterm**. The actual ELF and its unmodified Debian shared libraries
live under /opt/fbterm. The launcher invokes the private dynamic loader without
setting LD_LIBRARY_PATH, so executed child shells use their normal system
libraries. This allows a modern native i386 build to run on older userspace.
System libraries and font configuration are not replaced. VESA support is
omitted from this targeted artifact; existing fbdev and GPM support remain.
The terminfo installation respects DESTDIR and installs under the private prefix.

Runtime package/source versions and copyright notices are packaged under
/opt/fbterm/share. Obtain corresponding unmodified Debian library sources by
those versions from Debian's archive (https://snapshot.debian.org/ if retired).
The artifact includes the fbterm source and build manifest. No setuid bit or
capabilities are added; console/DRM permissions must be provided by the owner
workflow. A manually installed /usr/local/bin/fbterm shadows /usr/bin/fbterm;
back it and its launch configuration up before changing that path.

## Remote pipeline

Depot project qk39vv1js0 (fbterm), native AMD64 builder with i386 userspace.
Build from the exact pushed Git revision, never a moving branch:

```
revision=$(git rev-parse HEAD)
depot build --project qk39vv1js0 --build-platform linux/amd64 --platform linux/386 \
  --file tools/Dockerfile --build-arg SOURCE_REVISION="$revision" --save \
  --metadata-file build-metadata.json --output type=local,dest=artifacts \
  "https://github.com/Section-9-Workflows/fbterm.git#$revision"
```

Use the existing protected host Depot credential or Depot login. Never commit
credentials, pass them as build arguments, or upload them as build context.
Artifacts stay in Depot Registry and export directly; no GitHub artifact upload.
The dependency layer caches separately from source. Compilation uses native
32-bit GCC with -O2 -march=bonnell -mtune=bonnell. No host -march=native.
Unit tests and synthetic probes are not run. Validate the actual package and
application on Icarus before replacing its working login terminal.
