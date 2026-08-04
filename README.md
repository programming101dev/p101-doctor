# p101-doctor

`p101-doctor` is the source preflight conductor for the Programming 101 quality
tools. It runs a project through:

1. `p101-wrapper-audit`, which checks whether source code bypasses available
   p101 wrappers;
2. `p101-error-contract`, which checks whether p101 wrapper/error use has a
   visible `env`/`err` contract and rejects silently discarded wrapper errors;
3. `p101-module-map`, which checks module shape, public API surface, include
   relationships, and likely split/static-scope opportunities.

The lower-level tools remain the source of truth. `p101-doctor` gives students
and reviewers one command that leaves behind a readable source-quality index.
Runtime capture, fault campaigns, and replay analysis are separate
`p101 run`, `p101 walk`, and `p101 analyze` operations.
Use `-x` when you want the module check without the static p101 source-contract
checks.

## Usage

```sh
p101-doctor [-h] [-v] [-x] [-o <doctor-dir>] [-s <source-path>]... [-C <compile_commands.json>] \
    [-A <p101-wrapper-audit>] [-E <p101-error-contract>] [-M <p101-module-map>] \
    -- <command> [args...]
```

Examples:

```sh
p101-doctor -- ./my-program
p101-doctor -x -s src -s include -- ./my-program
p101-doctor -o doctor -s src -s include -- ./my-program
p101-doctor -C build-clang/compile_commands.json -s src -s include -- ./my-program
p101-doctor \
    -A ../p101-wrapper-audit/p101-wrapper-audit \
    -E ../p101-error-contract/build-clang/p101-error-contract \
    -M ../p101-module-map/build-clang/p101-module-map \
    -- ./my-program
```

With no `-o`, the doctor directory is `p101-doctor-<pid>` in the current
directory. The directory must not already exist.

## Doctor contents

`p101-doctor` writes:

```text
command.txt
manifest.txt
summary.md
doctor.json
receipt.txt
tool-receipt.json
wrapper-audit.stdout.txt
wrapper-audit.stderr.txt
source-facts.tsv
source-inputs.json
error-contract.stdout.txt
error-contract.stderr.txt
module-map.md
module-map.json
module-map.stdout.txt
module-map.stderr.txt
```

The source-contract files are produced by `p101-wrapper-audit` and
`p101-error-contract`; they are omitted when `-x` is used. `module-map.md` is
produced by `p101-module-map`; it is always run because module/API shape is
useful even when static p101 source-contract checks are skipped.

`summary.md` starts with a quick grade for wrapper usage, error contracts,
and module shape. `manifest.txt` records the exact tool paths, source paths, and
target command used for the run.

## Boundaries

`p101-doctor` is a conductor, not a separate proof engine. Its findings are only
as complete as the delegated tools and the admitted inputs they receive. Direct
non-p101 calls, third-party code outside the wrapper/event stream, skipped
source-contract checks with `-x`, and source paths that do not cover the real
project can all hide issues from the final summary.

For source checks, the doctor runs one Clang AST pass through
`p101-wrapper-audit`, writes `source-facts.tsv` plus `source-inputs.json`, and
feeds that immutable P101FACT v4 snapshot to both `p101-error-contract` and
`p101-module-map`. The wrapper pass also enables its portability-header rule
pack, so known platform-only headers are reported at the wrapper boundary
rather than as module-structure findings. Use `-C` to pin the compilation
database; otherwise the
doctor uses `lib_c_facts` to discover the current project database. The input
manifest records inactive and unparsed files so a green policy result cannot
hide an incomplete admitted-input set.
If the project root contains `.p101-wrapper-audit-allow`, doctor passes that
scoped boundary ledger to wrapper-audit; the same input manifest records its
path and hash.

## Exit status

| Status | Meaning |
| --- | --- |
| `0` | All delegated checks completed cleanly with no findings |
| `1` | At least one delegated tool found a wrapper, error-contract, or module issue |
| `2` | `p101-doctor` could not create/run the doctor workflow |

## Build and check

Configure a compiler once, then run the gate:

```sh
./change-compiler.sh -c clang
./check.sh
```
