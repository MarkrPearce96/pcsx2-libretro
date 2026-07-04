# Core-option style guide

Binding for NEW options in all RetroNest core forks. Existing options are
grandfathered — migrating their values is a schema-breaking release and is
explicitly out of scope until one is planned.

- **Keys:** `<coreid>_<snake_case>` (`pcsx2_fast_forward_speed`). Never
  rename a shipped key (persisted in every user's options.json).
- **Booleans:** values `"enabled"` / `"disabled"`, default explicit.
  (duckstation's existing `"true"/"false"` options are grandfathered.)
- **Floats:** shortest round-trip text, no trailing zero padding: `"1.5"`,
  `"2"` — never `"1.500000"`.
- **Ints:** plain decimal, no unit suffix in the value (put units in the
  display label: `"Rewind granularity (frames)"`).
- **Value labels:** every non-obvious value gets a display label in the v2
  definition; raw enum ints (e.g. EXI device numbers) must never surface
  as user-visible values.
- **Naming across cores:** one concept, one suffix — check the other forks'
  option tables before inventing a new name for an existing concept
  (existing divergence like `cdrom_preload`/`cdvd_precache` is
  grandfathered; don't add a third name).
- **Categories:** cores emit `categories = nullptr`
  (see `emit_core_options_v2.h`) — grouping is host-side curation.
- **Wording:** `desc` = short sentence-case label; `info` = one-to-three
  tooltip sentences, no trailing period on `desc`, period(s) on `info`.
