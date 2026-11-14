// The vfx hook of the anim stage: particle effects written as text. A .ptc is a text file the engine
// reads as it is (no Workbench build), so an effect made of vanilla particle materials needs only this
// file and its .meta. One template so far: "sparks", a looping spray of hot streaks with a flicker of
// light, for a tool that cuts or grinds. The emitter layout follows the vanilla
// Particles/Enviroment/electrical_short_small_loop.ptc and this addon's Particles/Gather/OreHit.ptc.
const fmt = (n) => String(+Number(n).toFixed(4));

export const VFX_TEMPLATES = ["sparks"];

// Vanilla particle materials (game 1.9, named in the two files above).
const MAT = {
  streak: "{D9FC42124DBB8845}Particles/_Materials/Fire/Base_Sparks_06.emat",
  flash: "{EF5B1079929283A6}Particles/_Materials/Fire/Base_Sparks_05.emat",
};

export const SPARK_DEFAULTS = { rate: 60, speed: 2.5, life: 0.45, size: 0.025, spread: 70, flash: true };

// Everything wrong with a sparks description, as sentences.
export function sparksProblems(s) {
  const o = { ...SPARK_DEFAULTS, ...s };
  const out = [];
  if (!(o.rate >= 1 && o.rate <= 400)) out.push("sparks.rate is streaks per second, 1 to 400");
  if (!(o.speed >= 0.1 && o.speed <= 30)) out.push("sparks.speed is metres per second, 0.1 to 30");
  if (!(o.life >= 0.05 && o.life <= 5)) out.push("sparks.life is seconds, 0.05 to 5");
  if (!(o.size >= 0.002 && o.size <= 0.5)) out.push("sparks.size is the streak width in metres, 0.002 to 0.5");
  if (!(o.spread >= 0 && o.spread <= 180)) out.push("sparks.spread is the cone half-angle in degrees, 0 to 180");
  return out;
}

// The .ptc text. No Repeat line: an emitter repeats unless told not to, so the effect runs until stopped.
export function sparksPtc(s = {}) {
  const o = { ...SPARK_DEFAULTS, ...s };
  // As many live streaks as the rate and the lifetime need, with headroom for the random lifetime.
  const maxNum = Math.max(4, Math.ceil(o.rate * o.life * 1.6));
  const flash = o.flash ? `  EmitterDef flash {
   BirthRate 10
   LodMin 0
   LodMax 1
   Material "${MAT.flash}"
   StretchMultiplier 1
   SizeMultiplier ${fmt(o.size * 4)}
   SizeRND 0.5
   VelAngle 1
   Velocity 0
   RandomFrame 1
   Lifetime 0.06
   Size {
    0 0.6 0.5 1 1 0
   }
   EmittingTime 1
   EnableLight 1
   MaxLV 4
   LightColorMast {
    0 1 0.72 0.35
   }
   LightIntensityMast {
    0 0.5 0.25 1 0.5 0.4 0.75 0.9 1 0.5
   }
  }
` : "";
  return `EffectDef {
 Version 2
 Emitters {
${flash}  EmitterDef streaks {
   ConeAngle 360 0 ${fmt(o.spread)}
   MaxNum ${maxNum}
   BirthRate ${fmt(o.rate)}
   BirthRateRND ${fmt(o.rate * 0.5)}
   LodMin 0
   LodMax 1
   Material "${MAT.streak}"
   StretchMultiplier 1.1
   StreakFullUV 1
   SizeMultiplier ${fmt(o.size)}
   SizeRND 0.3
   VelAngle 1
   Velocity ${fmt(o.speed)}
   VelocityRND ${fmt(o.speed * 0.6)}
   VelAffect 0.1
   AirResistance 0.3
   AirResistanceRND 0.6
   GravityMultiply 0.5
   GravityMultiplyRND 0.5
   RandomFrame 1
   Lifetime ${fmt(o.life)}
   LifetimeRND ${fmt(o.life * 0.5)}
   Color {
    0 1 0.95 0.8 0.25 1 0.68 0.3 1 0.9 0.3 0.06
   }
   Alpha {
    0 1 0.8 1 1 0
   }
   Size {
    0 1 0.7 0.9 1 0
   }
   EmittingTime 1
  }
 }
 LightRadius 6
}
`;
}

const PTC_CONF = { PC: "4B4500E061CCD624", XBOX_ONE: "73FE8562A95EBA36", PS4: "B94602D6C3160C98", HEADLESS: "EED8908A9A55ED1C" };
export function ptcMeta(resPath, guid) {
  const row = (p) => (PTC_CONF[p]
    ? `  PTCResourceClass ${p} : "{${PTC_CONF[p]}}Configs/System/ResourceTypes/${p}/ParticleEffect.conf" {\n  }\n`
    : `  PTCResourceClass ${p} : PC {\n  }\n`);
  return `MetaFileClass {\n Name "{${guid}}${resPath}"\n Configurations {\n${["PC", "XBOX_ONE", "XBOX_SERIES", "PS4", "HEADLESS"].map(row).join("")} }\n}\n`;
}
