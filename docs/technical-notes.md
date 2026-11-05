# Technical notes

## Actual failure and call path

Observed with **official, unmodified ROOT 6.42.00-rc1**, Ubuntu 24.04:

```text
hadd -f505 merged.root a.root b.root
  → TFileMerger
  → RNTuple::Merge
  → RNTupleMerger
  → RPageStorage.cxx:1606
Fatal: sealedPageIt->GetHasChecksum() violated
```

The public `RNTupleWriter` generated both input files; the source with checksum disabled is not a deliberately malformed RNTuple. The process terminates with code **134 (SIGABRT)** and leaves a partially created target; the observed output was 413 bytes and contained no readable `ntuple`.

## Why it appears to happen

- `SetEnablePageChecksums(false)` is a supported source-file option.
- The sink's same-page merging optimization expects checksums on committed sealed pages.
- When the merger copies a source page directly, it retains that source page's `HasChecksum` state.
- A checksum-free page can therefore reach a destination invariant demanding checksums.

**Evidence:** checksum-disabled input crashes even when both sources use compression `505`, whereas two checksum-enabled inputs also succeed when their compression differs. This isolates checksum state from the need for recompression.

This is a likely **source/destination invariant mismatch**, not proof of a specific preferred fix. The correct behavior (supporting mixed inputs versus gracefully rejecting them) needs upstream clarification.

## Prior upstream design discussions

- [PR #15961](https://github.com/root-project/root/pull/15961): same-page deduplication is coupled to checksums.
- [PR #18118](https://github.com/root-project/root/pull/18118): writer option behavior when checksums or same-page merging are disabled.
- [PR #20112](https://github.com/root-project/root/pull/20112): merger strategies and source-page versus destination-write-option checksum semantics. [Reviewer discussion](https://github.com/root-project/root/pull/20112#discussion_r2504319608) notes that L1 fast merging does not fully respect the destination write options.

These discussions demonstrate that maintainers considered the relevant design constraints. They do **not** demonstrate that the specific public-`hadd` abort is accepted behavior or was previously recognized.

## Test matrix

The public workflow generates **130 rows per source**, `id` and `value` fields, and verifies **260 exact rows** for successful merges. Output compression is `505`.

| Case | A checksum / compression | B checksum / compression | Expected on 6.42 RC1 |
| --- | --- | --- | --- |
| Control, same compression | ON / 505 | ON / 505 | Success; 260 exact rows |
| Control, different compression | ON / 0 | ON / 505 | Success; 260 exact rows |
| Mixed, same compression | OFF / 505 | ON / 505 | Abort; checksum assertion |
| Mixed, different compression | OFF / 0 | ON / 505 | Abort; checksum assertion |
| Both disabled | OFF / 505 | OFF / 505 | Abort; checksum assertion |

The workflow intentionally treats a **matching abort signature** as a reproduced observation and reports CI success. If ROOT changes behavior (e.g., supports the merge or reports a controlled error), the workflow will flag the behavior change for review rather than falsely claiming the bug is still present.

## Limitations

- No version-identical execution on current `master` has been completed.
- No silent numerical corruption or memory-safety exploit has been shown.
- This is a public-CLI availability/reliability issue; a separate L2 recompression checksum investigation is **out of scope**.
- The repository has an independent Git history and contains no private research repo history or other security investigations.
