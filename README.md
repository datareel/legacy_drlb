# legacy_drlb: final releases of the DataReel-based DRLB packages

This repository holds the last releases of the three DRLB packages that
were built on the DataReel library, each in its own folder with its full
git history:

| Folder | Program | Final release | Tag |
|---|---|---|---|
| `load-balancer/` | `drlb_server`, TCP load balancer | 1.61 | `load-balancer-1.61` |
| `ssl-bridge/` | `drlb_ssl_server`, SSL bridge | 1.61 | `ssl-bridge-1.61` |
| `LDM-load-balancer/` | `drlb_ldm_server`, LDM load balancer | 1.65 | `LDM-load-balancer-1.65` |

Earlier tags from the original repositories are kept with the folder name
in front, for example `load-balancer-1.60-beta`.

## What replaces them

- **load-balancer** and **ssl-bridge** are replaced by **drlb_server 2.0**
  in the `tcp_load_balancer` repository: one program, written without the
  DataReel library, that runs as a plain TCP load balancer or, with an
  `[SSLCONFIG]` section in its config, as the SSL bridge. 1.60 and 1.61
  config files work with 2.0 unchanged. No further releases of these two
  packages are planned.
- **LDM-load-balancer** is not replaced. It is maintained only here.

## Building

Each folder builds on its own, as before. See the README.txt in each
folder (RPM builds are in each folder's `rpm_builder/`).

## TODO: DataReel 6.3 patch releases

When DataReel 6.3 is released, patch all three packages to it:

1. In each package, update the bundled DataReel files in `include/` and
   `src/` from the DataReel 6.3 release. Only the files the package
   already bundles are needed.
2. Keep the local OpenSSL 3.5 changes. These bundled files differ from
   DataReel 6.2 and must be merged by hand, not overwritten:
   - `include/gxssl.h` in all three packages
   - `src/gxssl.cpp` in ssl-bridge
   Check whether 6.3 already has the same changes before merging.
3. Rebuild each package, and run its tests: `tests/drlb_tests.py`,
   `tests/ssl_bridge_tests.py` and `tests/ldm_tests.py`.
4. Release each as a patch version: load-balancer 1.62, ssl-bridge 1.62
   and LDM-load-balancer 1.66. Update `version.txt`, `VersionString` in
   `m_help.cpp`, the man pages, the RPM specs (Version, Release 1 and the
   changelog), the README and the release notes, which must say "Built
   with the DataReel 6.3 library".
5. Tag each one (`load-balancer-1.62`, `ssl-bridge-1.62`,
   `LDM-load-balancer-1.66`) and push to aws.

Bundled DataReel version today: all three packages bundle **DataReel
6.2**. Their release notes say so, and every bundled file matches the
datareel repository's `6.2` tag except the `gxssl` files above.

DataReel 6.3 status on 11/01/2026: it is in progress on the
`6.3-oodb-backport` branch of the datareel repository (based on 6.2,
with `RELEASE_NOTES_6.3.txt`). There is no `6.3` tag yet.
