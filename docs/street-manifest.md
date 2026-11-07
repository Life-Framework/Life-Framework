# Street manifest

DebugWorld terrain is the box from 0 to 512 on X and Z. Ground props sit at Y=0. The release street uses Z=120. Sites stay inside that box. This tool checks a manifest and can write a layer to a path you choose. It does not edit `Roadmap.layer` or any other file under `Worlds/DebugWorld`.

`validate-debugworld-layout.ps1` still checks area-box overlap. This tool does not replace it.

## Manifest

One site per line:

```
Name x y z
```

Blank lines and `#` comments are ignored. `StreetA 210 0 120` is the release street at the first site.

## Check

```
node tools/street/manifest.mjs check <manifest>
```

The check fails when a site is:

- off the terrain (`x` or `z` outside 0..512, inclusive)
- raised (`y` is not 0)
- more than 30 m from the nearest other site (horizontal distance)
- on top of another site (ground positions closer than 0.05 m)

A site with no neighbor does not fail the 30 m rule. Two sites exactly 30 m apart pass. Exit code 0 is a pass.

## Generate

```
node tools/street/manifest.mjs generate <manifest> <out.layer>
```

Writes `GenericEntity` blocks with `coords` to that path only. A failing check writes nothing. A path inside `Worlds/DebugWorld` is refused, so this command cannot change the live street.

## Selftest

```
node tools/street/manifest.mjs selftest
```

The selftest uses a temp directory. A row past x=512 fails. An overlapping pair fails. A short row inside the terrain passes, and generate writes that row only in the temp directory.
