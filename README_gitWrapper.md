# git_wrapper

An error-proof front-end for `git commit` and `git push`, built to stop two
mistakes that already cost a history rewrite:

1. **Foreign contributors on commits.** Every commit is forced to
   `nava <nava@noreply.com>` for *both* author and committer, and any
   `Co-Authored-By:` / "Generated with … Claude" / 🤖 line is stripped from the
   message. No trailer, no stray identity, ever.
2. **Wrong push order with submodules.** `push` pushes each submodule that has
   unpushed commits **first**, then the parent — so a fresh clone never points
   at a submodule commit the remote doesn't have.

## Build

```bat
Build.bat            :: vswhere -> vcvars64 -> Ninja -> cl, produces build\git_wrapper.exe
```

Then put it on PATH (or copy `build\git_wrapper.exe` somewhere on PATH) and run
it from anywhere inside a repository.

## Usage

```
git_wrapper commit "<message>"            stage everything + commit as nava
git_wrapper push  [--force]               push submodules first, then this repo
git_wrapper save  "<message>" [--force]   commit, then push
```

- `commit`/`save` run `git add -A` first, then commit. If nothing is staged it
  says so and exits cleanly.
- `--force` uses `--force-with-lease` (refuses to clobber remote work you
  haven't fetched) — the safe kind of force, needed e.g. after a history rewrite.
- `push` skips pinned/detached submodules (third-party deps) and submodules that
  are already up to date; it aborts *before* touching the parent if a submodule
  push fails, or if an un-pushed commit still carries a `Co-Authored-By` trailer.

## Notes

- The identity is the two constants `kName` / `kEmail` at the top of `main.cc`.
- Commit messages are passed to git via a temp file, so spaces and shell
  metacharacters (`&`, `%`, quotes, newlines) in the message are always safe.
# git_Wrapper
