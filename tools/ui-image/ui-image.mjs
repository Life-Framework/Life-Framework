// tools\cli ui-image <svg|png> <name> [--size N] [--no-imageset]
// SVG or PNG -> Assets/UI/Illustrations/<name>.tif -> headless Workbench import
// (LF_ImageImportPlugin) -> <name>.edds + <name>.edds.meta -> optional
// <name>.imageset. Prints the {GUID}path strings a layout needs.
import { createRequire } from "node:module";
import { existsSync, mkdirSync, readFileSync, writeFileSync, copyFileSync } from "node:fs";
import { join, extname, basename } from "node:path";
import { randomBytes } from "node:crypto";

export const ILLUSTRATION_DIR = "Assets/UI/Illustrations";

// The Workbench plugin lives here (tracked); Scripts/WorkbenchGame/ is gitignored,
// so the CLI copies it there before each import.
export function installPlugin(root) {
  const dst = join(root, "addons", "LifeFramework", "Scripts", "WorkbenchGame", "ResourceManager");
  mkdirSync(dst, { recursive: true });
  copyFileSync(new URL("./LF_ImageImportPlugin.c", import.meta.url), join(dst, "LF_ImageImportPlugin.c"));
}

export function newGuid() {
  return randomBytes(8).toString("hex").toUpperCase();
}

// Rasterise an SVG (or copy a PNG) to <addon>/Assets/UI/Illustrations/<name>.png.
export async function writeSource(root, src, name, size) {
  const dir = join(root, "addons", "LifeFramework", ILLUSTRATION_DIR);
  mkdirSync(dir, { recursive: true });
  const out = join(dir, `${name}.tif`);
  const ext = extname(src).toLowerCase();
  if (ext !== ".png" && ext !== ".svg") throw new Error(`source must be .svg or .png: ${src}`);
  const sharp = createRequire(join(root, "website", "package.json"))("sharp");
  // density 72 keeps the SVG's own pixel size; --size resizes to a square.
  let img = sharp(readFileSync(src), { density: 72 });
  if (size) img = img.resize(size, size, { fit: "contain", background: "#00000000" });
  await img.tiff({ compression: "none" }).toFile(out);
  return out;
}

// One-image imageset: the whole texture under the name "full". RefSize is the
// pixel size, so a layout can address it as {GUID}path.imageset:full.
export function writeImageset(root, name, edddsGuid, w, h) {
  const dir = join(root, "addons", "LifeFramework", ILLUSTRATION_DIR);
  const file = join(dir, `${name}.imageset`);
  writeFileSync(file, `ImageSetClass {
 RefSize ${w} ${h}
 Textures {
  ImageSetTextureClass {
   mpix 1
   path "{${edddsGuid}}${ILLUSTRATION_DIR}/${name}.edds"
  }
 }
 Images {
  ImageSetDefClass full {
   Name "full"
   Pos 0 0
   Size ${w} ${h}
   Flags 0
  }
 }
}`);
  // A re-import keeps the imageset's GUID: signs and layouts reference it.
  if (existsSync(`${file}.meta`)) return readMetaGuid(`${file}.meta`);
  const guid = newGuid();
  writeFileSync(`${file}.meta`, `MetaFileClass {
 Name "{${guid}}${ILLUSTRATION_DIR}/${name}.imageset"
 Configurations {
  ImageSetResourceClass PC {
  }
  ImageSetResourceClass XBOX_ONE : PC {
  }
  ImageSetResourceClass XBOX_SERIES : PC {
  }
  ImageSetResourceClass PS4 : PC {
  }
  ImageSetResourceClass HEADLESS : PC {
  }
 }
}
`);
  return guid;
}

export function readMetaGuid(metaFile) {
  const m = /Name "\{([0-9A-F]{16})\}/.exec(readFileSync(metaFile, "utf8"));
  return m ? m[1] : null;
}

export async function imageSize(root, file) {
  const sharp = createRequire(join(root, "website", "package.json"))("sharp");
  const m = await sharp(file).metadata();
  return { w: m.width, h: m.height };
}

// Texture conf GUIDs (Configs/System/ResourceTypes/<platform>/<conf>) (TextureUI.conf, read through the Workbench API).
const CONF = {
  PC: "3549C665CAA9A8EA", XBOX_ONE: "5E6D8E2B87D36B34", XBOX_SERIES: "03194CD39583675A",
  PS4: "A752014F057BCE60", HEADLESS: "E604D15366D985DF",
};
export function writeTextureMeta(metaFile, name) {
  const cls = (p, g) => `  TIFFResourceClass ${p} : "{${g}}Configs/System/ResourceTypes/${p}/TextureUI.conf" {
  }
`;
  writeFileSync(metaFile, `MetaFileClass {
 Name "{${newGuid()}}${ILLUSTRATION_DIR}/${name}.edds"
 Configurations {
${cls("PC", CONF.PC)}${cls("XBOX_ONE", CONF.XBOX_ONE)}${cls("XBOX_SERIES", CONF.XBOX_SERIES)}${cls("PS4", CONF.PS4)}  TIFFResourceClass PS5 : PC {
  }
${cls("HEADLESS", CONF.HEADLESS)} }
}
`);
}
