#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Bundle the built executable and all libraries it needs in a private runtime."""
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

root = Path(sys.argv[1]).resolve()
private = root / 'opt/fbterm'
lib = private / 'lib'
lib.mkdir(parents=True)
listing = subprocess.check_output(['ldd', str(private / 'bin/fbterm')], text=True)
if 'not found' in listing:
    raise SystemExit(listing)
paths = []
for line in listing.splitlines():
    match = re.search(r'(?:=>\s+)?(/\S+)\s+\(', line)
    if match:
        paths.append(Path(match[1]))
if not any(path.name == 'ld-linux.so.2' for path in paths):
    raise SystemExit('Expected native i386 loader')
packages = {}
for path in paths:
    shutil.copyfile(path, lib / path.name)
    (lib / path.name).chmod(0o755)
    # Debian's merged /usr can differ from the path recorded by dpkg.
    candidates = [str(path), str(path.resolve())]
    candidates += [p[4:] for p in candidates if p.startswith('/usr/')]
    owner = None
    for candidate in dict.fromkeys(candidates):
        query = subprocess.run(['dpkg-query', '-S', candidate], text=True, capture_output=True)
        if query.returncode == 0:
            owner = query.stdout.split(': ')[0]
            break
    if not owner:
        raise SystemExit('No Debian package owns runtime library: ' + str(path))
    packages[owner] = subprocess.check_output(
        ['dpkg-query', '-W', '-f=${Version}\t${source:Package}\t${source:Version}', owner], text=True)
    notice = Path('/usr/share/doc') / owner.split(':')[0] / 'copyright'
    destination = private / 'share/runtime-licenses' / owner.replace(':', '_')
    destination.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(notice, destination / 'copyright')
shutil.copytree('/usr/share/common-licenses', private / 'share/runtime-licenses/common')
(private / 'share/runtime-packages.json').write_text(json.dumps(packages, indent=2) + '\n')
launcher = root / 'usr/bin/fbterm'
launcher.parent.mkdir(parents=True)
launcher.write_text('''#!/bin/sh
# Only fbterm uses the private loader. Child shells use their system runtime.
export TERMINFO_DIRS="${TERMINFO_DIRS:+$TERMINFO_DIRS:}/opt/fbterm/share/terminfo:/etc/terminfo:/lib/terminfo:/usr/share/terminfo"
exec /opt/fbterm/lib/ld-linux.so.2 --library-path /opt/fbterm/lib /opt/fbterm/bin/fbterm "$@"
''')
launcher.chmod(0o755)
