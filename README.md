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
