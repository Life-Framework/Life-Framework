# Asset Licenses & Attribution

This file documents all third-party assets used in the Life Framework project, their licenses, and proper attribution.

## Our Assets

All original assets created by the Life Framework team are licensed under the MIT License, matching the project license.

## Third-Party Assets

### Example Format:
```
Asset Name: [Name of the asset]
Author: [Creator name]
Source: [URL to original]
License: [License type]
Usage: [How we use it in the mod]
Attribution Required: [Yes/No and how]
```

---

## Audio Assets

*No third-party audio assets currently used.*

---

## 3D Models

*No third-party 3D models currently used.*

---

## Textures

```
Asset Name: ambientCG sets Metal046B, Metal047B, Metal048B, Metal058C, Rust009, Rock035, Rock058
Author: ambientCG
Source: https://ambientcg.com (one page per set: https://ambientcg.com/view?id=<set>)
License: CC0 1.0 Universal (public domain dedication)
Usage: baked into the mining ingot, nugget and ore-vein maps (Assets/Resources/Mining/{Ingot,Nugget,OreVein}/Data);
       the downloaded sets are not in the repository (tools/assets/<slug>/spec.json lists them)
Attribution Required: No (credited in docs/credits.md)
```

---

## Fonts

*No third-party fonts currently used.*

---

## Other Resources

*No other third-party resources currently used.*

---

## Notes

- All assets must be compatible with the MIT License
- Always verify license terms before adding new assets
- Update this file whenever adding third-party content
- Keep source files in their respective directories with clear naming

## Adding New Assets

Which sources and licenses are allowed: `docs/ecosystem.md` (section 3 and the
import policy). Record each imported asset as one line under its section:

```
- <asset> | <source URL> | <author> | <license> | <our resource path>
```

CC-BY assets are also credited in the README.

When adding a new third-party asset:

1. Verify the license is compatible with MIT
2. Download and save the original source file
3. Document it in this file with full attribution
4. Include any required license files in the same directory
5. Note any specific attribution requirements

## Questions?

If you're unsure about an asset's licensing, ask in Discord before adding it to the project.
