# RNTuple `hadd` aborts on checksum-disabled inputs

A small, **unpatched ROOT** reproducer for a possible bug in the public `hadd` command.

**Observed with official ROOT 6.42.00-rc1:** RNTuple inputs written with `RNTupleWriteOptions::SetEnablePageChecksums(false)` can cause `hadd` to abort, even though the input files were created by ROOT itself.

| Input A | Input B | `hadd -f505` |
| --- | --- | --- |
| Checksum ON | Checksum ON | **Pass**; all 260 values verified |
| Checksum OFF | Checksum ON | **Abort** (exit 134) |
| Checksum OFF | Checksum OFF | **Abort** (exit 134) |

The failure also reproduces when **both inputs have the same compression settings**. Different compression is not required.

```text
Fatal: sealedPageIt->GetHasChecksum() violated at line 1606 of RPageStorage.cxx
```

The aborted merge leaves an incomplete output file; in our run, the RNTuple could not be opened.

## Reproduce

Use ROOT **6.42.00-rc1** on Linux with `root-config` and `hadd` on your PATH:

```bash
c++ -std=c++17 -O1 reproduce.cxx -o reproduce $(root-config --cflags --libs) -lROOTNTuple
./reproduce generate a.root 1 0 505  # checksum OFF
./reproduce generate b.root 2 1 505  # checksum ON
hadd -f505 merged.root a.root b.root
```

The last command aborts on the tested release candidate. Use `./reproduce merged.root` to verify 260 expected values if a merge succeeds. This helper creates small RNTuples with two fields and 130 entries each.

The [GitHub Actions workflow](.github/workflows/reproduce.yml) runs all five combinations, verifies both successful controls by exact readback, and **passes when the expected aborts are observed**.

**Verified public CI:** [ROOT 6.42.00-rc1 — 2 passing controls + 3 expected aborts (PASS)](https://github.com/Johnny-Kao/root-rntuple-hadd-reproducer/actions/runs/42122803975). It uses the official CERN binary and does not modify ROOT source code.

## What may be happening?

A source page can validly have no checksum. Fast merging preserves the page's checksum state, while the destination's same-page deduplication expects a checksum, triggering an assertion when the page is committed.

This is a **likely contract mismatch**, not a proposed fix. We are asking whether this source/destination combination is intended to be supported and, if not, whether it should fail gracefully rather than aborting.

See [technical notes](docs/technical-notes.md) for the relevant call path and prior upstream discussions.

## Scope

This repository contains **only** a public-CLI reproducer. It does not include the separate L2 recompression investigation, security research, or patches. A crash is demonstrated; **silent data corruption, memory-safety exploitation, and a CVE are not demonstrated**.

Baseline: official ROOT 6.42.00-rc1, Ubuntu 24.04. This is **not** a live test of the latest master commit.
