# Updating from upstream PCSX2

This fork rebases onto `PCSX2/pcsx2` master. All RetroNest-specific code
lives in `pcsx2-libretro/` (sibling of `pcsx2-qt/` and `pcsx2-gsrunner/`).
There is exactly one modification to a top-level upstream file: a 4-line
block in `CMakeLists.txt` that conditionally includes `pcsx2-libretro/`.
This is the only intentional source of rebase friction.

## Rebase + release process

```sh
# 1. Fetch upstream
git fetch upstream

# 2. Rebase onto upstream master
git checkout main
git rebase upstream/master

# 3. Resolve conflicts. Typical: 0–1 anchor lines in top-level CMakeLists.txt
#    (~11/12 months trivial, 30s fix). The fork-notice block at the top of
#    README.md may also drift if upstream restructures their README.

# 4. Local sanity build
cmake -S . -B build -DENABLE_LIBRETRO=ON -DENABLE_QT_UI=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --target pcsx2_libretro -j

# 5. (Optional) local smoke test before publishing
cp build/pcsx2-libretro/pcsx2_libretro.dylib \
   ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro.dylib
rsync -a --delete bin/resources/ \
   ~/Documents/RetroNest/emulators/libretro/cores/pcsx2_libretro_resources/

# 6. Push the rebased main to your fork
git push origin main --force-with-lease

# 7. Cut a release. The GitHub Actions workflow does the rest.
TAG="v$(date -u +%Y.%m.%d)"
git tag "$TAG"
git push origin "$TAG"
```

That last `git push origin <tag>` triggers `.github/workflows/libretro_release.yml`.
The workflow builds the core for BOTH arches on `macos-14` runners (x86_64
under Rosetta + native arm64/ARMSX2), lipos them into one UNIVERSAL
`pcsx2_libretro.dylib.zip` (see CLAUDE.md → Releases), and creates a GitHub
Release with the zip attached. RetroNest can then download it.

If you need to cut a second release on the same day, suffix the tag with
`.1`, `.2`, etc — e.g. `v2026.05.21.1`.

## Discipline: keep the rebase surface tiny

Never edit upstream **source** files outside the single 4-line block in
top-level `CMakeLists.txt` and the small fork-notice block at the top of
`README.md`. All RetroNest-specific code lives in `pcsx2-libretro/` (a
new sibling directory, not an edit to upstream). There are two narrow
exceptions already in place for libretro-specific dispatch tables (audio
backend + input source enums), each comment-flagged
`// pcsx2-libretro… (SP4)` / `(SP5)` (grep for `pcsx2-libretro` in
`pcsx2/Host/AudioStream*` and `pcsx2/Input/InputManager.{h,cpp}`) for
future rebase reviewers — do not widen these.

### `.github/` is a wider exception

Upstream PCSX2's CI configuration is deleted on our fork. CI files are
not code — they're configuration meant for upstream's release pipeline.
Keeping them just to satisfy the source-file discipline rule meant the
fork's Actions tab fired noisy upstream workflows on every dependabot
PR and competed with `libretro_release` for the shared macOS runner
pool. Deleted on this fork:

- All `.github/workflows/*.yml` except `libretro_release.yml`
- `.github/workflows/scripts/` and `.github/workflows/architecture/`
- `.github/dependabot.yml`
- `.github/labeler.yml`

Kept (harmless, fork-friendly): `.github/FUNDING.yml`,
`.github/ISSUE_TEMPLATE/`, `.github/PULL_REQUEST_TEMPLATE.md`.

When rebasing onto upstream master, expect upstream's workflows to come
back via the rebase and need re-deleting. Quick recipe:

```sh
git ls-files .github/workflows/ \
  | grep -v '^\.github/workflows/libretro_release\.yml$' \
  | xargs -I{} git rm {} 2>/dev/null
git rm -f .github/dependabot.yml .github/labeler.yml 2>/dev/null
git rm -rf .github/workflows/scripts .github/workflows/architecture 2>/dev/null
git commit -m "ci: drop upstream PCSX2 workflows reintroduced by rebase"
```

## What can go wrong on rebase

Three patterns, in rough order of frequency:

1. **CMakeLists.txt anchor-line conflict** (~11/12 months trivial). The
   conditional `add_subdirectory(pcsx2-libretro)` block has an anchor line
   above it that occasionally drifts when upstream restructures
   directories. 30 seconds to manually re-align.

2. **PCSX2 internal API drift** (2–4 times per year). An upstream commit
   renames a function or restructures a class the libretro shim depends
   on. The build fails inside `pcsx2-libretro/` with a clear error
   message. Fix is minutes-to-hour inside the shim — never propagate the
   change upward.

3. **New upstream Homebrew dep**. The cmake configure step in CI fails
   with a missing-package error. Edit `.github/workflows/libretro_release.yml`'s
   `Install Homebrew dependencies` step, add the new brew package, commit,
   re-cut the tag.

## What can go wrong in CI

The workflow logs live at `https://github.com/<your-user>/pcsx2-libretro/actions`.

**Most common failure:** missing brew dep. The fix is above.

**Note: CI is already on macos-14 + Rosetta.** We migrated off `macos-13`
on 2026-05-21 because GitHub's Intel runner pool became starved (30-90 min
queue waits even with no other workflows competing). The workflow now uses
`macos-14` (Apple Silicon), installs x86_64 Homebrew at `/usr/local`, and
wraps cmake configure + build in `arch -x86_64` so the produced dylib is
still x86_64. Apple Silicon runners dequeue in seconds. Per-build cost is
~30 min slower due to Rosetta translation, but end-to-end time (queue +
build) is meaningfully faster.

If a future builds fails inside the `Install x86_64 Homebrew at /usr/local`
or `Install Homebrew dependencies (x86_64)` steps, the official Homebrew
installer's URL or the cask layout may have drifted. Check the Homebrew
docs and update the install command in the workflow.

**Tag race.** If two release tags are pushed within seconds of each other,
the workflow's "previous release" lookup for the auto-generated commit
list could produce an odd diff. Trivial to ignore in practice — date tags
don't collide except by deliberate same-day re-cuts (which use the `.N`
suffix and produce sensible diffs).
