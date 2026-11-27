#!/usr/bin/env node
// Agent-box harness. One command surface for the three pillars:
//   script — Enforce logic suite (worktree only)
//   world  — screenshot a world position in play mode
//   ui     — screenshot the current menu, after the same inputs a player uses
//
// Player actions are not a side door. `use` calls ScriptedUserAction.PerformAction
// after CanBeShownScript and CanBePerformedScript. `input` pulses an allowlisted
// menu action through InputManager.SetActionValue. `open DebugMenu` toggles the
// same menu the debug key opens.

import "../tools/lib/quiet-gate.mjs";
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { homedir, tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { bmpFileToPng, classifyMesh, classifyMeshFile, decodeBmp, decodePng, encodePng, syntheticBmp } from "./bmp.mjs";
import { checkChains, formatPlan, runChainProbes } from "./chains.mjs";
import { findPad, loadPads, loadPlaced, loadWorld } from "./layers.mjs";
import { claimPlay, decideClaim, rendererFor } from "./play-claim.mjs";
import { readSession, sessionForBox } from "./session.mjs";
import { sessionDirOfBox, takeHoldWarning, touchLease } from "../tools/lib/session-lease.mjs";
import { judgeRoadmapRow, judgeStreet, menuActionFromLook, missingRoadmapNames, presentedMesh, renderRoadmapReport, roadmapSites, rowCamera, runRoadmapReport, runTownReport, selectRoadmapSites, siteLook, siteShotOffset, STREET_Z } from "./report.mjs";
import { expandHandbookTour, selftest as handbookFlowSelftest } from "./handbook-flow.mjs";
import { checkExpect, flowTable, parseExpect, parseState, parseStep, selftest as flowAssertsSelftest } from "./flow-asserts.mjs";
import { TOWN, checkTown, formatTownCheck, judgeTownRow, loadSignCatalog, loadStrings, manifestErrors, parseSignConfig, resolveSite, signCard, signIssues } from "./town.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..");

const MENU_ACTIONS = new Set([
  // Character actions the harness drives for melee proof (pickaxe swing at an ore rock).
  "CharacterMelee",
  "CharacterTurnUp",
  // Absolute view pitch in degrees (#4803): a lost or late relative turn cannot leave the view tilted.
  "CharacterPitch",
  // The clip stamp key (#1173), so the proof presses the real binding.
  "LF_ClipStamp",
  // Vehicle lights, to light the dashboard indicators for the vehicle HUD proof (#921).
  "VehicleLightsToggle",
  "CarLightsHiBeamToggle",
  "Inventory",
  "SwitchWeaponCategory1",
  "SwitchWeaponCategory2",
  "SwitchWeaponCategory3",
  "SwitchCameraType",
  "MenuBack",
  "MenuUp",
  "MenuDown",
  "MenuLeft",
  "MenuRight",
  "MenuSelect",
  "LF_OpenDebugMenu",
  // The digit keys of the menus: a dialogue reply number (0 is Goodbye), an ATM keypad digit.
  ...[0, 1, 2, 3, 4, 5, 6, 7, 8, 9].map((d) => `LF_Keypad${d}`),
]);

// --client <n> (or setBoxes) points every command at one session client's
// profile instead of the Workbench and default game profiles. Commands to a
// session client carry that session's id (s:<id>); the client refuses any
// other id, and sendCommand refuses a session another checkout started.
let boxOverride = null;
let sessionOverride;

export function setBoxes(dirs, sessionId) {
  boxOverride = dirs && dirs.length ? dirs : null;
  sessionOverride = sessionId;
}

// The session a command to these boxes is addressed to: { id } (empty id for
// Workbench and default profiles), or { error } when the live session in the
// shared session dir belongs to another checkout or is not running.
// Three worlds (docs/architecture/three-worlds.md): a proof that walks to an annex fixture (an Annex_* entity) runs
// on GridWorld, so it asks for `session up --mission grid`; town proofs keep the default. Returns "" when the
// running session booted the wanted mission, else the one line the proof prints before it fails.
export function missionProblem(session, want) {
  if (!session || session.stoppedAt) return `no running session: \`tools\\cli session up --mission ${want}\` first`;
  const have = session.mission || "debug";
  if (have === want) return "";
  return `this proof needs \`session up --mission ${want}\` (its fixtures are in that world); the running session is --mission ${have}`;
}

export function addressFor(boxes, root = ROOT, lookup = sessionForBox) {
  if (!boxes) return { id: "" };
  if (sessionOverride !== undefined) return { id: sessionOverride || "" };
  for (const box of boxes) {
    const hit = lookup(box, root);
    if (hit) return hit;
  }
  return { id: "" };
}

export function profileBoxes() {
  if (boxOverride) return boxOverride;
  const docs = join(homedir(), "Documents", "My Games");
  return [
    join(docs, "ArmaReforgerWorkbench", "profile", "agent-box"),
    join(docs, "ArmaReforger", "profile", "agent-box"),
  ];
}

function token(value, label) {
  if (!/^[A-Za-z0-9_]+$/.test(value)) {
    throw new Error(`${label} must be letters, digits, or underscore: ${value}`);
  }
  return value;
}

export function buildLine(id, spec) {
  if (!Number.isInteger(id) || id <= 0) throw new Error("id must be a positive integer");
  if (spec.session) {
    const rest = buildLine(id, { ...spec, session: "" });
    return rest.replace(/^(\d+) /, `$1 s:${token(spec.session, "session")} `);
  }
  if (spec.verb === "use") {
    const cls = token(spec.className, "class");
    if (spec.entityName) return `${id} use ${cls} ${token(spec.entityName, "entity")}`;
    return `${id} use ${cls}`;
  }
  if (spec.verb === "interact") {
    if (spec.step !== "confirm" && spec.step !== "next" && spec.step !== "prev") {
      throw new Error(`interact step must be confirm, next, or prev: ${spec.step}`);
    }
    return `${id} interact ${spec.step}`;
  }
  if (spec.verb === "stand") {
    // Optional side (e default, w, n, s; trailing whole metres 1 to 30 stand that far out, not 1.2, e.g.
    // s9 for a building's street side): stand on that side of the entity, facing it.
    if (spec.side) {
      if (!/^[ewns]([1-9]|[12][0-9]|30)?$/.test(spec.side)) throw new Error(`bad stand side ${spec.side}`);
      return `${id} stand ${token(spec.entityName, "entity")} ${spec.side}`;
    }
    return `${id} stand ${token(spec.entityName, "entity")}`;
  }
  if (spec.verb === "look") {
    return `${id} look`;
  }
  if (spec.verb === "pickup") {
    // pickup [item]: the F-key pickup (vanilla SCR_PickUpItemAction on this client) of the nearest loose item whose prefab name contains item.
    return spec.item ? `${id} pickup ${token(spec.item, "item")}` : `${id} pickup`;
  }
  if (spec.verb === "state") {
    return `${id} state`;
  }
  if (spec.verb === "framegap") {
    // The longest wall time between two harness polls since the last framegap: a hung frame (#4824).
    return `${id} framegap`;
  }
  if (spec.verb === "die") {
    return `${id} die`;
  }
  if (spec.verb === "giveup") {
    return `${id} giveup`;
  }
  if (spec.verb === "resetcrime") {
    return `${id} resetcrime`;
  }
  if (spec.verb === "resetplayer") {
    // spec.keepInventory: leave the inventory (the default empties it and issues the civilian kit).
    if (spec.keepInventory) return `${id} resetplayer ${spec.cash === undefined ? -1 : Number(spec.cash)} keep`;
    return spec.cash === undefined ? `${id} resetplayer` : `${id} resetplayer ${Number(spec.cash)}`;
  }
  if (spec.verb === "gm") {
    return spec.arg === "close" ? `${id} gm close` : `${id} gm`;
  }
  if (spec.verb === "siren") {
    return `${id} siren ${token(spec.entityName, "entity")} ${Number(spec.mode)}`;
  }
  if (spec.verb === "scanner" || spec.verb === "drill" || spec.verb === "keycard" || spec.verb === "charge" || spec.verb === "apples" || spec.verb === "nuggets" || spec.verb === "crate" || spec.verb === "parceldrop" || spec.verb === "pistol" || spec.verb === "moonshine" || spec.verb === "stash" || spec.verb === "park" || spec.verb === "shelf" || spec.verb === "keys" || spec.verb === "spikeverge" || spec.verb === "spikedrop" || spec.verb === "spikecar" || spec.verb === "spikecheck" || spec.verb === "slopecar" || spec.verb === "slopecheck" || spec.verb === "slopedrive" || spec.verb === "spikeseen" || spec.verb === "spikepreview" || spec.verb === "spikepreviewoff" || spec.verb === "spikeplace" || spec.verb === "spikewhere" || spec.verb === "animhold" || spec.verb === "animoff" || spec.verb === "animseen" || spec.verb === "quizclear") {
    return `${id} ${spec.verb}`;
  }
  if (spec.verb === "sound") {
    // sound [SOUND_EVENT]: the held weapon's sound events and the stun buzz on this client (LF_HarnessSound).
    return spec.event ? `${id} sound ${token(spec.event, "sound event")}` : `${id} sound`;
  }
  if (spec.verb === "engine") {
    if (spec.entityName) return `${id} engine ${token(spec.entityName, "entity")}`;
    return `${id} engine`;
  }
  if (spec.verb === "lockpick") {
    return `${id} lockpick`;
  }
  if (spec.verb === "heldxf") {
    return `${id} heldxf`;
  }
  if (spec.verb === "itemgive" || spec.verb === "itemdrop" || spec.verb === "itemon" || spec.verb === "itemoff") {
    // The asset states proof: put one item of a prefab in the inventory, drop it, or switch it on and off (its use state, LF_AssetStatesHarness).
    if (!/^\{[0-9A-F]{16}\}[A-Za-z0-9_/.-]+\.et$/.test(spec.prefab ?? "")) throw new Error(`${spec.verb} needs a {GUID}path prefab: ${spec.prefab}`);
    return `${id} ${spec.verb} ${spec.prefab}`;
  }
  if (spec.verb === "chat") {
    // chat [admin|global|local|ooc] <text>: this client sends one chat line through the real path (LF_HarnessChat);
    // `!handled 3` goes to the Admin channel, which re-checks the sender's role on the server.
    const text = String(spec.text ?? "");
    if (!/^[\x20-\x7E]{1,400}$/.test(text) || text.trim() !== text) throw new Error(`chat text must be 1 to 400 printable ASCII characters with no edge spaces: ${JSON.stringify(spec.text)}`);
    if (spec.channel && !/^(admin|global|local|ooc)$/.test(spec.channel)) throw new Error(`chat channel must be admin, global, local or ooc: ${spec.channel}`);
    return `${id} chat ${spec.channel ? `${spec.channel} ` : ""}${text}`;
  }
  if (spec.verb === "licence") {
    // licence <NAME>: this client asks for one licence by its LF_ELicenseType name, the request the
    // /licence chat command sends (LF_AskLicenceGrant). The server's dev gate decides; the verdict is the
    // server log line `/licence: player N granted NAME`.
    if (!/^[A-Z][A-Z0-9_]{1,63}$/.test(spec.name ?? "")) throw new Error(`licence needs an LF_ELicenseType name such as FIREARM_PERMIT: ${spec.name}`);
    return `${id} licence ${spec.name}`;
  }
  if (spec.verb === "pausemenu") {
    // pausemenu [open|close]: opens the pause menu the way Escape does, or puts it away (LF_HandbookScreen.c, #1773).
    const mode = spec.mode ?? "open";
    if (mode !== "open" && mode !== "close") throw new Error(`pausemenu mode must be open or close: ${mode}`);
    return `${id} pausemenu ${mode}`;
  }
  if (spec.verb === "staffapprove") {
    // staffapprove <uid>: approve that account's open police application through LF_PoliceApplicationStaff.ApproveByUid
    // (LF_HarnessStaff). The uid is the same test as the staff API's (1..64 of letters, digits, - and _).
    if (!/^[A-Za-z0-9_-]{1,64}$/.test(spec.uid ?? "")) throw new Error(`staffapprove needs an account uid (1..64 of A-Za-z0-9_-): ${spec.uid}`);
    return `${id} staffapprove ${spec.uid}`;
  }
  if (spec.verb === "hold") {
    return `${id} hold ${token(spec.item, "item")}`;
  }
  if (spec.verb === "phone" || spec.verb === "radio" || spec.verb === "pager" || spec.verb === "report" || spec.verb === "configreload" || spec.verb === "robhold") {
    return `${id} ${spec.verb}`;
  }
  if (spec.verb === "emote") {
    // emote open|close|seen|<radial command>: the emote radial (LF_EmoteHarness, #982).
    return `${id} emote ${token(spec.arg, "emote")}`;
  }
  if (spec.verb === "reload") {
    // reload [config|dialogue|config,dialogue]: the server reads the owner config and the dev caption file again (#970).
    if (spec.what !== undefined && !/^[a-z]+(,[a-z]+)*$/.test(spec.what)) throw new Error(`reload what must be names joined by commas (config,dialogue): ${spec.what}`);
    return spec.what ? `${id} reload ${spec.what}` : `${id} reload`;
  }
  if (spec.verb === "spikeaim") {
    return `${id} spikeaim ${spec.arg ?? ""} ${spec.value ?? ""}`.trim();
  }
  if (spec.verb === "dialogue") {
    return `${id} dialogue ${spec.arg ?? ""} ${spec.value ?? ""}`.trim();
  }
  if (spec.verb === "peers") {
    return `${id} peers`;
  }
  if (spec.verb === "seen") {
    const radius = Number(spec.radius ?? 0);
    if (!Number.isFinite(radius) || radius < 0) throw new Error(`seen radius must be a number: ${spec.radius}`);
    // seen <radius> <item>: `loose=` counts the loose copies of that prefab file name (a shop shelf read).
    if (spec.item) return `${id} seen ${radius > 0 ? radius : 12} ${token(spec.item, "item")}`;
    return radius > 0 ? `${id} seen ${radius}` : `${id} seen`;
  }
  if (spec.verb === "perf") {
    const seconds = Number(spec.seconds ?? 10);
    if (!Number.isFinite(seconds) || seconds <= 0 || seconds > 60) throw new Error(`perf seconds must be 0..60: ${spec.seconds}`);
    return spec.label ? `${id} perf ${seconds} ${token(spec.label, "label")}` : `${id} perf ${seconds}`;
  }
  if (spec.verb === "shoot") {
    const target = Number(spec.playerId);
    if (!Number.isInteger(target) || target <= 0) throw new Error(`shoot needs a positive player id: ${spec.playerId}`);
    return `${id} shoot ${target}`;
  }
  if (spec.verb === "shootnpc") {
    return `${id} shootnpc ${token(spec.entityName, "entity")}`;
  }
  if (spec.verb === "cpr") {
    const target = Number(spec.playerId);
    if (!Number.isInteger(target) || target <= 0) throw new Error(`cpr needs a positive player id: ${spec.playerId}`);
    if (spec.mode !== "start" && spec.mode !== "stop") throw new Error(`cpr mode must be start or stop: ${spec.mode}`);
    return `${id} cpr ${target} ${spec.mode}`;
  }
  if (spec.verb === "cuffing") {
    const target = Number(spec.playerId);
    if (!Number.isInteger(target) || target <= 0) throw new Error(`cuffing needs a positive player id: ${spec.playerId}`);
    if (spec.kind !== "cuff" && spec.kind !== "release") throw new Error(`cuffing kind must be cuff or release: ${spec.kind}`);
    if (!["start", "cancel", "pin", "finish"].includes(spec.mode)) throw new Error(`cuffing mode must be start, cancel, pin or finish: ${spec.mode}`);
    return `${id} cuffing ${target} ${spec.kind} ${spec.mode}`;
  }
  if (spec.verb === "ui") {
    if (spec.mode === "text") return `${id} ui text`;
    if (spec.mode) throw new Error(`ui mode must be text: ${spec.mode}`);
    return `${id} ui`;
  }
  if (spec.verb === "click") {
    const widget = token(spec.widget, "widget");
    const index = spec.index === undefined ? 0 : Number(spec.index);
    if (!Number.isInteger(index) || index < 0) throw new Error(`click index must be a non-negative integer: ${spec.index}`);
    if (index === 0) return `${id} click ${widget}`;
    return `${id} click ${widget} ${index}`;
  }
  if (spec.verb === "type") {
    // The typed text may carry dashes: an account number is a uid such as 00bbbddd-5e55-0000-0000-000000000002 (#3320).
    if (!/^[A-Za-z0-9_-]+$/.test(spec.text)) throw new Error(`text must be letters, digits, underscore, or dash: ${spec.text}`);
    return `${id} type ${token(spec.widget, "field")} ${spec.text}`;
  }
  if (spec.verb === "input") {
    if (!MENU_ACTIONS.has(spec.action)) {
      throw new Error(`action not allowed: ${spec.action}. Menu actions only.`);
    }
    if (spec.value !== undefined && spec.value !== "") {
      if (!/^-?[0-9]+(.[0-9]+)?$/.test(String(spec.value))) throw new Error(`input value must be a number: ${spec.value}`);
      return `${id} input ${spec.action} ${spec.value}`;
    }
    return `${id} input ${spec.action}`;
  }
  if (spec.verb === "open") {
    // IdCard: Inspect on the carried identity card (#902), the inventory button's request.
    if (spec.menu === "IdCard") return `${id} open IdCard`;
    if (spec.menu !== "DebugMenu") throw new Error("only DebugMenu and IdCard can be opened by name");
    const mode = spec.mode || "toggle";
    if (mode !== "show" && mode !== "hide" && mode !== "toggle") {
      throw new Error(`open mode must be show, hide, or toggle: ${mode}`);
    }
    if (mode === "toggle") return `${id} open DebugMenu`;
    return `${id} open DebugMenu ${mode}`;
  }
  if (spec.verb === "kit") {
    const step = token(spec.step, "kit step");
    if (step === "show" || step === "hide" || step === "terminal" || step === "credits" || step === "policeapps" || step === "policeapps_read" || step === "policeapps_approve") return `${id} kit ${step}`;
    if (step === "theme") return `${id} kit theme ${token(spec.name, "theme")}`;
    if (step === "line") return `${id} kit line ${token(spec.part, "part")} ${token(spec.text, "text")}`;
    if (step === "status") {
      return `${id} kit status ${token(spec.part, "part")} ${token(spec.label, "label")} ${token(spec.statusText, "status")} ${token(spec.action, "action")}`;
    }
    if (step === "quantity") {
      const total = Number(spec.total);
      const take = Number(spec.take);
      if (!Number.isInteger(total) || !Number.isInteger(take)) {
        throw new Error("quantity needs integer total and take");
      }
      return `${id} kit quantity ${token(spec.part, "part")} ${total} ${take}`;
    }
    throw new Error(`kit step must be show, hide, terminal, theme, line, status, or quantity: ${step}`);
  }
  if (spec.verb === "previews") {
    if (spec.mode === "hide") return `${id} previews hide`;
    const list = (spec.prefabs ?? []).map((p) => {
      if (!/^[A-Za-z0-9_{}/.-]+$/.test(p)) throw new Error(`prefab must be a {GUID}path without spaces: ${p}`);
      return p;
    });
    if (!list.length) throw new Error("previews show needs prefabs");
    return `${id} previews show ${list.join(",")}`;
  }
  if (spec.verb === "capture") {
    const name = token(spec.name, "capture name");
    if (spec.mode === "ui") return `${id} capture ui ${name}`;
    if (spec.mode !== "world") throw new Error("capture mode must be ui or world");
    const coords = [spec.x, spec.y, spec.z].map((n) => Number(n));
    if (coords.some((n) => Number.isNaN(n))) throw new Error("world capture needs x y z");
    const line = `${id} capture world ${name} ${coords.map(trimNum).join(" ")}`;
    if (spec.ox === undefined && spec.oy === undefined && spec.oz === undefined) return line;
    const offset = [spec.ox, spec.oy, spec.oz].map((n) => Number(n));
    if (offset.some((n) => Number.isNaN(n))) throw new Error("camera offset needs ox oy oz");
    const off = `${line} ${offset.map(trimNum).join(" ")}`;
    if (spec.hour === undefined) return off;
    const hour = Number(spec.hour);
    if (!(hour >= 0 && hour < 24)) throw new Error(`capture hour must be 0-24: ${spec.hour}`);
    return spec.free ? `${off} ${trimNum(hour)} free` : `${off} ${trimNum(hour)}`;
  }
  if (spec.verb === "clean") return `${id} clean off`;
  if (spec.verb === "time") {
    // `harness.mjs send time 23 [weather]` arrives as words (2026-11-26 round: "time hour must be 0-24: undefined").
    if (spec.hour === undefined && Array.isArray(spec.words)) spec = { ...spec, hour: spec.words[0], weather: spec.weather ?? spec.words[1] };
    const hour = spec.hour === undefined || spec.hour === "" ? NaN : Number(spec.hour);
    if (!(hour >= 0 && hour < 24)) throw new Error(`time hour must be 0-24: ${spec.hour}`);
    return spec.weather ?`${id} time ${trimNum(hour)} ${token(spec.weather, "weather state")}` : `${id} time ${trimNum(hour)}`;
  }
  if (spec.verb === "probe") {
    const nums = (list) => list.map((n) => { const v = Number(n); if (Number.isNaN(v)) throw new Error(`probe needs numbers: ${list}`); return trimNum(v); }).join(" ");
    if (spec.mode === "ground") return `${id} probe ground ${nums([spec.x, spec.z])}`;
    if (spec.mode === "cam") return `${id} probe cam ${nums([spec.x, spec.y, spec.z])}`;
    if (spec.mode === "traceall") return `${id} probe traceall ${nums([spec.x, spec.y, spec.z, spec.x2, spec.y2, spec.z2])}`;
    if (spec.mode === "trace") return `${id} probe trace ${nums([spec.x, spec.y, spec.z, spec.x2, spec.y2, spec.z2])}`;
    if (spec.mode === "ent") return `${id} probe ent ${token(spec.name, "probe entity")} ${nums([spec.x, spec.y, spec.z])}`;
    throw new Error("probe mode must be ground, trace, cam, or ent");
  }
  if (spec.verb === "film") {
    // In-engine recording (LF_FilmRecorder, tools/film/engine.mjs).
    if (spec.mode === "stop" || spec.mode === "status" || spec.mode === "slate") return `${id} film ${spec.mode}`;
    if (spec.mode !== "start") throw new Error("film mode must be start, stop, slate, or status");
    const fps = Number(spec.fps);
    if (!Number.isInteger(fps) || fps < 0 || fps > 30) throw new Error(`film fps must be 0-30: ${spec.fps}`);
    const line = `${id} film start ${token(spec.name, "film name")} ${fps}`;
    if (spec.seconds === undefined) return line;
    const seconds = Number(spec.seconds);
    if (!Number.isInteger(seconds) || seconds <= 0) throw new Error(`film seconds must be a positive integer: ${spec.seconds}`);
    return `${line} ${seconds}`;
  }
  if (spec.verb === "scene" || spec.verb === "camera") return `${id} ${spec.verb} ${spec.args}`;
  // A feature verb (a modded LF_AgentHarnessVerbs): its words after the verb go to the game as they are.
  if (/^[a-z][a-z0-9]*$/.test(spec.verb) && Array.isArray(spec.words)) return `${id} ${spec.verb} ${spec.words.join(" ")}`.trim();
  throw new Error(`unknown verb: ${spec.verb}`);
}

function trimNum(n) {
  return String(Math.round(Number(n) * 1000) / 1000);
}

function nextId() {
  return (Date.now() % 2000000000) || 1;
}

let playClaim = null;

// A picture from a lowspec session is 640x360 with the lowest textures: fine for a proof that reads
// logs, useless as evidence. Said once per process; the shot still goes out.
let lowspecWarned = false;
function warnLowspecCapture(dir) {
  if (lowspecWarned) return;
  const s = readSession(dir);
  if (s?.profile !== "lowspec") return;
  lowspecWarned = true;
  console.error("WARN  this session is lowspec (640x360, lowest textures, no post-processing): a capture is not evidence. 'session down', then 'session up --profile full' for shots");
}

// A client answers `stand` once the target has streamed in (up to 20 s after a jump across the island, #4611).
const STAND_TIMEOUT_MS = 25000;

// `hold weapon` answers `ok drew <prefab> false` while the hands are busy (right after a swing, a reload, an
// interaction): the draw was refused. Holster, wait out the animation and draw again, a few times (#4867).
export const HOLD_WEAPON_TRIES = 4;
export const HOLD_WEAPON_WAIT_MS = 900;
export function weaponNotDrawn(result) {
  return result?.status === "ok" && /^drew .* false$/.test(String(result.detail ?? "").trim());
}
export function holdWeapon(send, timeoutMs, wait) {
  let result = send({ verb: "hold", item: "weapon" }, timeoutMs);
  for (let attempt = 1; attempt < HOLD_WEAPON_TRIES && weaponNotDrawn(result); attempt++) {
    send({ verb: "hold", item: "none" }, timeoutMs);
    wait(HOLD_WEAPON_WAIT_MS);
    result = send({ verb: "hold", item: "weapon" }, timeoutMs);
  }
  return result;
}

// `state` answering `no_session` right after the last carried cash leaves the character (an ATM deposit-all, a
// checkpoint to $0) is not a missing session: the client's main thread freezes 25-30 s and answers once it thaws
// (#4867, follow-up issue for the freeze). `state` only reads, so ask again: three more tries outwait the freeze.
export const STATE_TRIES = 4;
export function stateOutwaitingFreeze(send, timeoutMs) {
  let result = send({ verb: "state" }, timeoutMs);
  for (let attempt = 1; attempt < STATE_TRIES && result.status !== "ok" && result.detail === "no_session"; attempt++) result = send({ verb: "state" }, timeoutMs);
  return result;
}

export function sendCommand(spec, timeoutMs = 8000) {
  if (spec.verb === "state" && !spec.retried) return stateOutwaitingFreeze((s, t) => sendCommand({ ...s, retried: true }, t), timeoutMs);
  if (spec.verb === "hold" && spec.item === "weapon" && !spec.retried) return holdWeapon((s, t) => sendCommand({ ...s, retried: true }, t), timeoutMs, pause);
  if (spec.verb === "stand") timeoutMs = Math.max(timeoutMs, STAND_TIMEOUT_MS);
  const id = nextId();
  const address = addressFor(boxOverride);
  // A command to a session client keeps the session's pool lease alive.
  for (const b of profileBoxes()) {
    const d = sessionDirOfBox(b);
    if (d) {
      touchLease(d, spec.verb);
      if (spec.verb === "capture") warnLowspecCapture(d);
      // The pool's hold cap: one line, once, when other sessions are queued behind this one.
      const warn = takeHoldWarning(d);
      if (warn) console.error(warn);
    }
  }
  if (address.error) return { id, status: "error", detail: address.error, line: "", raw: `${id} error ${address.error}` };
  const line = buildLine(id, { ...spec, session: address.id });
  // The machine-wide play claim guards the shared default profile's command file. A session
  // client has its own box and a session id, so concurrent sessions in two checkouts (one per
  // session lane) do not share a channel and need no claim.
  const boxes = profileBoxes();
  // A box set with its own session id (setBoxes(dirs, id): the scratch host client, #1588) is a
  // private channel too.
  const sessionOnly = boxes.length > 0 && (boxes.every((b) => sessionDirOfBox(b)) || (!!boxOverride && !!sessionOverride));
  if (!sessionOnly) playClaim = playClaim ?? claimPlay(ROOT);
  if (!sessionOnly && !playClaim.ok) return { id, status: "error", detail: playClaim.reason, line, raw: `${id} error ${playClaim.reason}` };
  for (const dir of profileBoxes()) {
    mkdirSync(dir, { recursive: true });
    rmSync(join(dir, "result.txt"), { force: true });
    writeFileSync(join(dir, "command.txt"), line + "\n", "utf8");
  }

  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    for (const dir of profileBoxes()) {
      const path = join(dir, "result.txt");
      if (!existsSync(path)) continue;
      const text = readFileSync(path, "utf8").trim();
      const parts = text.split(" ");
      if (Number(parts[0]) !== id) continue;
      const status = parts[1] ?? "";
      const detail = parts.slice(2).join(" ");
      if (status === "pending") continue;
      return { id, status, detail, line, raw: text };
    }
    Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, 200);
  }
  return {
    id,
    status: "error",
    detail: "no_session",
    line,
    raw: "",
  };
}

// `ui text` detail: "button:SignButton=Sign | field:RegAge=30 | line:Balance=Balance: $400".
export function parseUiText(detail) {
  const text = String(detail ?? "").trim();
  if (!text || text === "none") return [];
  return text.split(" | ").map((entry) => {
    const colon = entry.indexOf(":");
    const eq = entry.indexOf("=", colon + 1);
    if (colon <= 0 || eq < 0) return { kind: "?", name: entry, text: "" };
    return { kind: entry.slice(0, colon), name: entry.slice(colon + 1, eq), text: entry.slice(eq + 1) };
  });
}

export function formatUiText(entries) {
  if (!entries.length) return "(no screen: nothing clickable and no UI kit sheet open)";
  return entries.map((e) => `${e.kind.padEnd(6)} ${e.name.padEnd(22)} ${e.text}`).join("\n");
}

// True when any listed name or text contains want (case-insensitive).
export function uiHasText(entries, want) {
  const needle = String(want).toLowerCase();
  return entries.some((e) => e.text.toLowerCase().includes(needle) || e.name.toLowerCase() === needle);
}

// Polls `ui text` until some widget shows want. A menu opened by a server reply
// or a replicated value (ATM balance) appears a few frames after the action.
export function waitForText(want, timeoutMs = 8000, send = sendCommand, sleep = pause) {
  const deadline = Date.now() + timeoutMs;
  let last = null;
  do {
    last = send({ verb: "ui", mode: "text" }, 3000);
    if (last.status === "ok" && uiHasText(parseUiText(last.detail), want)) return { ...last, found: true };
    if (last.status !== "ok" && last.detail === "no_session") return { ...last, found: false };
    sleep(300);
  } while (Date.now() < deadline);
  return { ...last, raw: "", found: false, status: "error", detail: `text_not_found ${want}` };
}

// What `dialogue` says when the scene is in this phase (and on this node): "closed ..." or
// "open node=<id> phase=<line|choosing|waiting> ...".
export function dialogueMatches(detail, phase, node) {
  if (phase === "closed") return /^closed/.test(detail);
  if (!/^open /.test(detail)) return false;
  const got = /phase=(\w+)/.exec(detail);
  if (!got || got[1] !== phase) return false;
  if (!node) return true;
  const at = /node=([\w-]+)/.exec(detail);
  return !!at && at[1] === node;
}

// The scene ignores a fresh press for 350 ms after each change (LF_DialogueScene.INPUT_GUARD_MS), so the key
// that opened a line picks nothing. A key sent right after the wait would land inside that window.
export const DIALOGUE_SETTLE_MS = 500;

// Polls `dialogue` until the scene is in the phase (on the node), then waits out the input guard so the
// next `input` is read. A node's line runs 5 to 15 s and a starved client answers slowly, so the budget is
// long; times out with dialogue_not_<phase>.
export function waitForDialogue(phase, node, timeoutMs = 45000, send = sendCommand, sleep = pause) {
  if (!["line", "choosing", "waiting", "closed"].includes(phase)) throw new Error(`dialogue-wait <line|choosing|waiting|closed> [node]: got ${phase}`);
  const deadline = Date.now() + timeoutMs;
  let last = null;
  do {
    last = send({ verb: "dialogue" }, 5000);
    if (last.status === "ok" && dialogueMatches(last.detail, phase, node)) {
      if (phase !== "closed") sleep(DIALOGUE_SETTLE_MS);
      return { ...last, found: true };
    }
    if (last.status !== "ok" && last.detail === "no_session") return { ...last, found: false };
    sleep(300);
  } while (Date.now() < deadline);
  return { ...last, raw: "", found: false, status: "error", detail: `dialogue_not_${phase}${node ? ` ${node}` : ""} (last: ${last?.detail})` };
}

// The node a `dialogue` answer is on ("closed" when no scene is open, "" when it is not a scene line).
export function dialogueNodeOf(detail) {
  if (/^closed/.test(detail)) return "closed";
  const at = /^open node=([\w-]+)/.exec(detail);
  return at ? at[1] : "";
}

export const CHOOSE_KEY_TRIES = 2;

// `dialogue-choose <node> <digit> <next>` answers a reply the way a player does and proves it landed.
// It waits until the scene is choosing on <node>, presses LF_Keypad<digit> (1 is the first reply; the
// Goodbye key 0 is refused) and polls `dialogue` for the scene to leave that choice. A harness key is
// dropped now and then on a loaded client (the clerk talk picked "bye" or stalled on its greeting, #4953);
// after CHOOSE_KEY_TRIES dropped presses it answers with `dialogue pick <digit - 1>` (the scene's own pick,
// the documented fallback) and says so in the detail. It then reads the scene again: the node must be
// <next> ("closed" is a valid next), else the step fails with `dialogue_chose_wrong`.
export function chooseDialogue(node, digit, next, send = sendCommand, sleep = pause, now = Date.now) {
  if (!node || !/^[1-9]$/.test(String(digit ?? "")) || !next) throw new Error(`dialogue-choose <node> <digit 1-9> <next node|closed>: got ${node} ${digit} ${next}`);
  const here = waitForDialogue("choosing", node, 45000, send, sleep);
  if (!here.found) return here;
  const left = (budgetMs) => {
    const deadline = now() + budgetMs;
    let last = null;
    do {
      last = send({ verb: "dialogue" }, 5000);
      if (last.status === "ok" && !dialogueMatches(last.detail, "choosing", node)) return last;
      if (last.status !== "ok" && last.detail === "no_session") return last;
      sleep(300);
    } while (now() < deadline);
    return null;
  };
  let via = "key";
  let answer = null;
  for (let attempt = 0; attempt < CHOOSE_KEY_TRIES && !answer; attempt++) {
    send({ verb: "input", action: `LF_Keypad${digit}` }, 8000);
    answer = left(4000);
  }
  if (!answer) {
    via = "pick (keys dropped)";
    const picked = send({ verb: "dialogue", arg: "pick", value: String(Number(digit) - 1) }, 10000);
    if (picked.status !== "ok") return { ...picked, found: false };
    answer = left(8000);
  }
  if (!answer) return { status: "error", detail: `dialogue_choice_not_taken ${node} digit ${digit}`, found: false };
  // The pick is sent to the server, which answers with the next node: the scene reads phase=waiting on the old
  // node meanwhile. Wait for it to reach another node (or close) before judging where it went.
  const settleBy = now() + 15000;
  while (dialogueNodeOf(answer.detail) === node && now() < settleBy) {
    sleep(300);
    const again = send({ verb: "dialogue" }, 5000);
    if (again.status !== "ok") break;
    answer = again;
  }
  const got = dialogueNodeOf(answer.detail);
  if (got !== next) return { ...answer, status: "error", detail: `dialogue_chose_wrong wanted ${next}, scene says ${answer.detail}`, found: false };
  return { ...answer, found: true, detail: `${node} -> ${got} via ${via}` };
}

export function toolchainPaths() {
  const programFiles = process.env["ProgramFiles(x86)"] || "C:\\Program Files (x86)";
  const steam = join(programFiles, "Steam", "steamapps", "common");
  return {
    workbench: process.env.ENFUSION_WORKBENCH_PATH || join(steam, "Arma Reforger Tools"),
    game: process.env.ENFUSION_GAME_PATH || join(steam, "Arma Reforger"),
    server: process.env.ENFUSION_SERVER_PATH || join(steam, "Arma Reforger Server"),
    project: process.env.ENFUSION_PROJECT_PATH || join(ROOT, "addons"),
  };
}

function mcpEntry(distRelative) {
  const dist = join(ROOT, distRelative);
  if (!existsSync(dist)) return null;
  const paths = toolchainPaths();
  return {
    command: "node",
    args: [dist],
    env: {
      ENFUSION_WORKBENCH_PATH: paths.workbench,
      ENFUSION_GAME_PATH: paths.game,
      ENFUSION_SERVER_PATH: paths.server,
      ENFUSION_PROJECT_PATH: paths.project,
    },
  };
}

export function cursorMcpConfig() {
  const servers = {};
  const primary = mcpEntry("tools/mcp/enfusion-mcp-bk/dist/index.js");
  const workbench = mcpEntry("tools/mcp/enfusion-workbench-mcp/dist/index.js");
  if (primary) servers["enfusion-mcp"] = primary;
  if (workbench) servers["enfusion-workbench"] = workbench;
  servers["life-agent-box"] = {
    command: "node",
    args: [join(ROOT, "agent-box", "mcp-server.mjs")],
  };
  const windowsMcp = join(homedir(), ".local", "bin", "windows-mcp.exe");
  if (existsSync(windowsMcp)) {
    servers["windows-mcp"] = {
      command: windowsMcp,
      args: ["serve", "--tools", "Screenshot,Click,Shortcut,Wait,Move,Type,DisplayInventory"],
      env: {
        WINDOWS_MCP_SCREENSHOT_SCALE: "0.5",
        ANONYMIZED_TELEMETRY: "false",
        WINDOWS_MCP_DISABLE_FLASH: "1",
      },
    };
  }
  return { mcpServers: servers };
}

function mergeMcp(path, config) {
  let current = { mcpServers: {} };
  if (existsSync(path)) {
    try {
      current = JSON.parse(readFileSync(path, "utf8"));
    } catch {
      current = { mcpServers: {} };
    }
  }
  current.mcpServers = { ...(current.mcpServers ?? {}), ...config.mcpServers };
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, JSON.stringify(current, null, 2) + "\n", "utf8");
  return path;
}

function runNode(args, extraEnv) {
  const res = spawnSync("node", args, {
    windowsHide: true,
    cwd: ROOT,
    encoding: "utf8",
    env: extraEnv ? { ...process.env, ...extraEnv } : process.env,
  });
  if (res.stdout) process.stdout.write(res.stdout);
  if (res.stderr) process.stderr.write(res.stderr);
  return res.status ?? 1;
}

const DEBUG_WORLD = "Worlds/DebugWorld/DebugWorld.ent";

// The Workbench bridge caches editor mode per process and starts it as unknown.
// wb_play and wb_stop refuse until wb_state has run in that same process.
export function playSteps(mode) {
  const state = { tool: "wb_state", args: {} };
  const enter = [
    state,
    { tool: "wb_open_resource", args: { path: DEBUG_WORLD } },
    state,
    { tool: "wb_play", args: { debugMode: false, fullScreen: false } },
  ];
  if (mode === "play") return [[state, { tool: "wb_stop", args: {} }], enter];
  return [enter];
}

function workbenchEnv() {
  const paths = toolchainPaths();
  return {
    ENFUSION_WORKBENCH_PATH: paths.workbench,
    ENFUSION_GAME_PATH: paths.game,
    ENFUSION_SERVER_PATH: paths.server,
    ENFUSION_PROJECT_PATH: paths.project,
  };
}

function mcpBatch(steps) {
  const dir = join(ROOT, "tmp");
  mkdirSync(dir, { recursive: true });
  const file = join(dir, "agent-box-batch.json");
  writeFileSync(file, JSON.stringify(steps));
  const res = spawnSync("node", ["tools/mcp-call.mjs", "--batch", `@${file}`, "enfusion-workbench"], {
    windowsHide: true,
    cwd: ROOT,
    encoding: "utf8",
    env: { ...process.env, ...workbenchEnv() },
  });
  const text = `${res.stdout || ""}${res.stderr || ""}`;
  if (res.stdout) process.stdout.write(res.stdout);
  if (res.stderr) process.stderr.write(res.stderr);
  return { code: res.status ?? 1, text };
}

function workbenchMode(text) {
  const match = text.match(/\*\*Mode:\*\*\s*(\w+)/);
  return match ? match[1] : "";
}

// The one place the harness starts Workbench, on this checkout's project with
// DebugWorld open. wb_launch is a no-op when a Workbench already answers on
// the bridge port. Swap the launcher here, nowhere else.
export function launchWorkbench() {
  const gprojPath = join(ROOT, "addons", "LifeFramework", "LifeFramework.gproj").replaceAll("\\", "/");
  return mcpBatch([{ tool: "wb_launch", args: { gprojPath, world: DEBUG_WORLD } }]).code;
}

export function enterPlay() {
  const state = mcpBatch([{ tool: "wb_state", args: {} }]);
  if (state.code !== 0) return state.code;
  const mode = workbenchMode(state.text);
  const batches = playSteps(mode);
  for (const steps of batches) {
    const result = mcpBatch(steps);
    if (result.code !== 0) return result.code;
  }
  return 0;
}

export function runScriptPillar(shouldRun) {
  if (!shouldRun) {
    return {
      code: 0,
      text: "script pillar: node tools/cli.mjs test --tier fast\nPass --run from a worktree. The main checkout refuses the suite.\n",
    };
  }
  if (isMainCheckout()) {
    return {
      code: 1,
      text: "refusing script pillar on the main checkout. Use a worktree: tools\\cli wt new <feature>\n",
    };
  }
  const res = spawnSync("node", ["tools/cli.mjs", "test", "--tier", "fast", "--verbose"], { windowsHide: true, cwd: ROOT, encoding: "utf8" });
  return { code: res.status ?? 1, text: `${res.stdout || ""}${res.stderr || ""}` };
}

export function isMainCheckout() {
  const res = spawnSync("git", ["rev-parse", "--git-dir"], { windowsHide: true, cwd: ROOT, encoding: "utf8" });
  const dir = (res.stdout || "").trim().replaceAll("/", "\\");
  if (!dir) return false;
  return !dir.toLowerCase().includes("\\worktrees\\");
}

export function setup() {
  const install = runNode(["tools/cli.mjs", "mcp", "install"]);
  const verify = runNode(["tools/cli.mjs", "mcp", "verify"]);
  const config = cursorMcpConfig();
  const projectConfig = mergeMcp(join(ROOT, ".cursor", "mcp.json"), config);
  const userConfig = mergeMcp(join(homedir(), ".cursor", "mcp.json"), config);
  const names = Object.keys(config.mcpServers).join(", ");
  console.log(`cursor mcp: ${projectConfig}`);
  console.log(`user mcp:   ${userConfig}`);
  console.log(`servers:    ${names}`);
  if (!config.mcpServers["enfusion-mcp"] || !config.mcpServers["enfusion-workbench"]) {
    console.log("WARN  an Enfusion MCP dist/index.js is missing. Re-run setup after install succeeds.");
  }
  return install === 0 && verify === 0 ? 0 : 1;
}

export function statusText() {
  const paths = toolchainPaths();
  const lines = [
    `repo: ${ROOT}`,
    `main checkout: ${isMainCheckout()}`,
    `workbench: ${paths.workbench} (${existsSync(paths.workbench) ? "ok" : "missing"})`,
    `game: ${paths.game} (${existsSync(paths.game) ? "ok" : "missing"})`,
    `server: ${paths.server} (${existsSync(paths.server) ? "ok" : "missing"})`,
    `enfusion-mcp dist: ${existsSync(join(ROOT, "tools/mcp/enfusion-mcp-bk/dist/index.js"))}`,
    `enfusion-workbench dist: ${existsSync(join(ROOT, "tools/mcp/enfusion-workbench-mcp/dist/index.js"))}`,
    `profiles: ${profileBoxes().join(" | ")}`,
  ];
  return lines.join("\n");
}

function pause(ms) {
  Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
}

export function logRoots() {
  // A session client's box is <profile>\profile\agent-box; its logs are <profile>\logs.
  if (boxOverride) return boxOverride.map((box) => join(dirname(dirname(box)), "logs"));
  const docs = join(homedir(), "Documents", "My Games");
  return [join(docs, "ArmaReforgerWorkbench", "logs"), join(docs, "ArmaReforger", "logs")];
}

function captureFile(full) {
  if (existsSync(full)) return full;
  if (full.toLowerCase().endsWith(".png")) {
    const bmp = full.slice(0, -4) + ".bmp";
    if (existsSync(bmp)) return bmpFileToPng(bmp, full);
  }
  return "";
}

// A session client at 1280x720 or larger answers warn:file_not_found because the
// engine is still writing the BMP when it checks. Wait for the file before a
// report judges the shot as missing.
function settleCapture(result) {
  if (!result || result.status !== "warn:file_not_found") return result;
  for (let i = 0; i < 20; i++) {
    if (resolveCapture(result.detail)) return { ...result, status: "ok" };
    pause(250);
  }
  return result;
}

export function resolveCapture(detail) {
  if (!detail || !detail.includes("$logs:")) return "";
  const alias = detail.split(" ").find((part) => part.startsWith("$logs:"));
  if (!alias) return "";
  const rel = alias.slice("$logs:".length).replaceAll("/", "\\");
  for (const root of logRoots()) {
    const direct = captureFile(join(root, rel));
    if (direct) return direct;
    if (!existsSync(root)) continue;
    const sessions = readdirSync(root)
      .filter((name) => name.startsWith("logs_"))
      .sort()
      .reverse();
    for (const session of sessions) {
      const hit = captureFile(join(root, session, rel));
      if (hit) return hit;
    }
  }
  return "";
}

const FLOW_DIR = join(ROOT, "agent-box", "flows");

export function readFlow(name) {
  if (!/^[A-Za-z0-9_-]+$/.test(name)) throw new Error(`flow name must be letters, digits, hyphen, or underscore: ${name}`);
  const safe = name;
  const path = join(FLOW_DIR, `${safe}.txt`);
  if (!existsSync(path)) {
    const names = existsSync(FLOW_DIR)
      ? readdirSync(FLOW_DIR)
          .filter((file) => file.endsWith(".txt"))
          .map((file) => file.slice(0, -4))
      : [];
    throw new Error(`unknown flow ${name}. Known: ${names.join(", ") || "(none)"}`);
  }
  // `@handbook-tour <pack> ...` expands to the page-by-page read from the handbook configs (handbook-flow.mjs).
  return readFileSync(path, "utf8")
    .split(/\r?\n/)
    .flatMap((line) => (line.startsWith("@handbook-tour") ? expandHandbookTour(line) : [line]));
}

// A `state` that failed because nothing is there to answer (not a game-side error): sendCommand's timeout
// (`no_session`) or addressFor's refusal (`no_live_session`, a session of another checkout, no id).
export function flowUnanswered(result) {
  return result?.status !== "ok" && /^(no_session|no_live_session|session_has_no_id|session_owned_by_other_checkout)/.test(String(result?.detail ?? ""));
}

// `step` and `expect` lines judge a flow (flow-asserts.mjs): a failed line or assert fails its step, and
// the flow ends with a PASS/FAIL table and exit 1 when any step failed.
export function runFlow(lines, name = "flow") {
  const out = [];
  let code = 0;
  const steps = [];
  let current = null;
  const failStep = (why) => {
    if (current) current.fails.push(why);
  };
  for (const raw of lines) {
    const line = raw.trim();
    if (!line || line.startsWith("#")) continue;
    const [verb, ...rest] = line.split(/\s+/);
    let result = null;
    if (verb === "step") {
      current = parseStep(rest);
      steps.push(current);
      const base = sendCommand({ verb: "state" }, 10000);
      current.before = base.status === "ok" ? parseState(base.detail) : null;
      if (base.status !== "ok") failStep(`state at step start: ${base.detail}`);
      out.push(`== step ${current.id} ${current.title}${current.pending ? ` [pending ${current.pending}]` : ""}`);
      // Nobody answers (no session, or a closed one): every later line would wait out its own timeout and the flow
      // would print nothing for minutes. Stop now and say which session the commands were aimed at.
      if (flowUnanswered(base)) {
        out.push(`flow ${name} stopped at step ${current.id}: no session client answers (${base.detail}). Is a session up for this checkout (\`tools\\cli session status\`)? A flow on a session with more than one client takes \`--client <n>\` in front of \`flow\`.`);
        code = 1;
        break;
      }
      continue;
    }
    if (verb === "expect") {
      if (!current) throw new Error(`expect outside a step: ${line}`);
      const spec = parseExpect(rest);
      let why = "";
      if (spec.field === "item") {
        const carried = sendCommand({ verb: "carries", words: [spec.want] }, 10000);
        if (carried.status !== "ok") why = `expect item ${spec.want}: ${carried.detail}`;
      } else {
        const now = sendCommand({ verb: "state" }, 10000);
        why = now.status === "ok" ? checkExpect(spec, current.before, parseState(now.detail)) : `expect ${rest.join(" ")}: state ${now.detail}`;
      }
      // Lower case: the step's table row is the failure line the compact runner shows.
      out.push(why ? `expect-miss ${why}` : `ok expect ${rest.join(" ")}`);
      if (why) failStep(why);
      continue;
    }
    if (verb === "wait") {
      const ms = Number(rest[0]);
      if (!Number.isFinite(ms) || ms < 0 || ms > 30000) throw new Error(`bad wait: ${line}`);
      pause(ms);
      out.push(`wait ${ms}`);
      continue;
    }
    if (verb === "stand") result = sendCommand({ verb: "stand", entityName: rest[0], side: rest[1] });
    else if (verb === "look") result = sendCommand({ verb: "look" });
    else if (verb === "state") result = sendCommand({ verb: "state" }, 10000);
    else if (verb === "ui" && rest[0] === "text") result = sendCommand({ verb: "ui", mode: "text" });
    else if (verb === "ui") result = sendCommand({ verb: "ui" });
    else if (verb === "wait-text") result = waitForText(rest.join(" "));
    // dialogue-wait <line|choosing|waiting|closed> [node]: poll `dialogue` until the scene is in that phase
    // (on that node): the player's "read, then answer". dialogue <verb> [value]: a scene verb, e.g. `dialogue pick 0`.
    else if (verb === "dialogue-wait") result = waitForDialogue(rest[0], rest[1]);
    // dialogue-choose <node> <digit> <next|closed>: wait for the reply list on <node>, press the digit key, fall back to
    // `dialogue pick` when keys are dropped, and fail unless the scene is on <next> afterwards.
    else if (verb === "dialogue-choose") result = chooseDialogue(rest[0], rest[1], rest[2]);
    else if (verb === "dialogue") result = sendCommand({ verb: "dialogue", arg: rest[0], value: rest[1] }, 10000);
    else if (verb === "ui-shot") result = sendCommand({ verb: "capture", mode: "ui", name: rest[0] }, 25000);
    else if (verb === "click") result = sendCommand({ verb: "click", widget: rest[0], index: rest[1] });
    else if (verb === "type") result = sendCommand({ verb: "type", widget: rest[0], text: rest[1] });
    else if (verb === "press") {
      const spec = PRESS[rest[0]];
      if (!spec) throw new Error(`press one of F, WheelUp, WheelDown, F10: ${line}`);
      result = sendCommand(spec);
    } else if (verb === "use") result = sendCommand({ verb: "use", className: rest[0], entityName: rest[1] });
    else if (verb === "input") result = sendCommand({ verb: "input", action: rest[0], value: rest[1] });
    else if (verb === "open") result = sendCommand({ verb: "open", menu: rest[0], mode: rest[1] });
    else if (verb === "pausemenu") result = sendCommand({ verb: "pausemenu", mode: rest[0] });
    else if (verb === "kit") result = sendKit(rest);
    else if (verb === "licence") result = sendCommand({ verb: "licence", name: rest[0] }, 10000);
    // send <verb> [words]: a feature verb (a modded LF_AgentHarnessVerbs), its words passed as they are.
    else if (verb === "send" && /^[a-z][a-z0-9]*$/.test(rest[0] ?? "")) result = sendCommand({ verb: rest[0], words: rest.slice(1) }, 10000);
    else if (verb === "layer") {
      const played = playLayer(rest[0], rest[1], rest[2]);
      out.push(...played.lines);
      if (played.code !== 0) {
        code = played.code;
        failStep(`layer ${rest[0]}: ${played.lines[played.lines.length - 1] ?? "failed"}`);
      }
      continue;
    } else if (verb === "shot" && rest[0] === "ui") {
      result = sendCommand({ verb: "capture", mode: "ui", name: rest[1] }, 25000);
    } else if (verb === "shot" && rest[0] === "world") {
      const target = resolveWorldTarget(rest[1], rest[2], rest[3], rest[4]);
      result = sendCommand(worldCapture(target, rest.slice(5, 8)), 25000);
    } else {
      throw new Error(`unknown flow step: ${line}`);
    }
    const text = result.raw || `${result.id} ${result.status} ${result.detail}`;
    out.push(text);
    const image = resolveCapture(result.detail);
    if (image) out.push(`image: ${image}`);
    if (result.status !== "ok") {
      code = 1;
      failStep(`${verb}${rest[0] ? ` ${rest[0]}` : ""}: ${result.detail}`);
    }
  }
  const table = flowTable(name, steps);
  out.push(...table.lines);
  return { code: code || table.code, lines: out };
}

export function pads() {
  return loadPads(ROOT);
}

export function formatPads(list = pads()) {
  if (!list.length) return "no interactable pads in DebugWorld layers";
  return list
    .map((pad) => `${pad.name}  layer=${pad.layer}  actions=${pad.actions.join(",")}  at ${pad.x} ${pad.y} ${pad.z}`)
    .join("\n");
}

// `shot world <name> x y z [ox oy oz]`: the optional camera offset is where
// the character stands relative to the target (default 10 3 10). A close
// offset reads printed text, such as a sign face.
function worldCapture(target, offset) {
  const spec = { verb: "capture", mode: "world", name: target.name, x: target.x, y: target.y, z: target.z };
  if (offset.length === 0) return spec;
  if (offset.length !== 3) throw new Error("camera offset needs ox oy oz");
  return { ...spec, ox: offset[0], oy: offset[1], oz: offset[2] };
}

function resolveWorldTarget(name, x, y, z) {
  if (x !== undefined && y !== undefined && z !== undefined) {
    return { name, x, y, z };
  }
  const pad = findPad(pads(), name);
  if (!pad) {
    throw new Error(`unknown world target ${name}. Pass x y z, or use an entity from 'layers'.`);
  }
  return { name: pad.name, x: pad.x, y: pad.y, z: pad.z };
}

const PRESS = {
  F: { verb: "interact", step: "confirm" },
  WheelUp: { verb: "interact", step: "next" },
  WheelDown: { verb: "interact", step: "prev" },
  F10: { verb: "open", menu: "DebugMenu" },
};

function sendKit(rest) {
  const step = rest[0];
  if (step === "show" || step === "hide" || step === "terminal" || step === "credits" || step === "policeapps" || step === "policeapps_read" || step === "policeapps_approve") return sendCommand({ verb: "kit", step });
  if (step === "theme") return sendCommand({ verb: "kit", step, name: rest[1] });
  if (step === "line") return sendCommand({ verb: "kit", step, part: rest[1], text: rest[2] });
  if (step === "status") {
    return sendCommand({
      verb: "kit",
      step,
      part: rest[1],
      label: rest[2],
      statusText: rest[3],
      action: rest[4],
    });
  }
  if (step === "quantity") {
    return sendCommand({ verb: "kit", step, part: rest[1], total: rest[2], take: rest[3] });
  }
  throw new Error(`kit step must be show, hide, terminal, theme, line, status, or quantity: ${step}`);
}

const KIT_SHOTS = ["kit-sheet", "kit-balance", "kit-status", "kit-keep", "kit-take", "kit-focus"];

function waitForSession(timeoutMs = 120000) {
  const deadline = Date.now() + timeoutMs;
  let last = null;
  while (Date.now() < deadline) {
    last = sendCommand({ verb: "ui" }, 3000);
    if (last.status === "ok") return last;
    pause(1000);
  }
  return last;
}

export function runKit() {
  const state = mcpBatch([{ tool: "wb_state", args: {} }]);
  if (state.code !== 0) return { code: state.code, lines: ["play failed"] };
  if (workbenchMode(state.text) !== "play") {
    const entered = enterPlay();
    if (entered !== 0) return { code: entered, lines: ["play failed"] };
    const ready = waitForSession();
    if (!ready || ready.status !== "ok") {
      const line = ready && ready.raw ? ready.raw : "no_session";
      return { code: 1, lines: [line] };
    }
  }
  const played = runFlow(readFlow("ui-kit"));
  const images = played.lines.filter((line) => line.startsWith("image:"));
  for (const name of KIT_SHOTS) {
    if (!images.some((line) => line.includes(name))) {
      played.lines.push(`missing image: ${name}`);
      played.code = 1;
    }
  }
  const listed = played.lines.some((line) => line.split(" ").slice(2).join(" ").split(",").includes("Button"));
  if (!listed) {
    played.lines.push("missing widget: Button");
    played.code = 1;
  }
  return played;
}

function waitForAction(className, entityName, timeoutMs = 5000) {
  const deadline = Date.now() + timeoutMs;
  let last = null;
  while (Date.now() < deadline) {
    last = sendCommand({ verb: "look" }, 2500);
    if (last.status !== "ok") return last;
    const hit = last.detail.includes(`${className}@${entityName}`) || last.detail.split(",").includes(className);
    if (hit) return last;
    pause(300);
  }
  return last;
}

// Sides `layer` tries when the action is not offered where it first stood (#4677). `stand` defaults to
// the east side, which is the back of a counter clerk who faces west (the playtest town registry
// clerk): her customer-side-only prompt is never shown there. A named side is tried alone.
export const LAYER_SIDE_FALLBACK = ["", "w", "n", "s"];

export function layerSideOrder(side) {
  if (side) return [side];
  return LAYER_SIDE_FALLBACK;
}

export function playLayer(name, actionName, side) {
  const pad = findPad(pads(), name);
  if (!pad) {
    console.error(`unknown layer entity ${name}`);
    console.error(formatPads());
    return { code: 1, lines: [`unknown layer entity ${name}`] };
  }
  const action = actionName || pad.actions[0];
  const lines = [];
  const sides = layerSideOrder(side);
  for (let i = 0; i < sides.length; i++) {
    const spec = { verb: "stand", entityName: pad.name };
    if (sides[i]) spec.side = sides[i];
    const stood = sendCommand(spec);
    lines.push(stood.raw || `${stood.id} ${stood.status} ${stood.detail}`);
    if (stood.status !== "ok") return { code: 1, lines, pad };
    if (!action) break;
    const seen = waitForAction(action, pad.name, sides.length > 1 ? 2500 : 5000);
    if (seen.status === "ok" && seen.detail.includes(action)) break;
    lines.push(seen.raw || `${seen.id} ${seen.status} ${seen.detail}`);
    if (i === sides.length - 1) return { code: 1, lines, pad };
    lines.push(`layer: ${action} not offered on the ${sides[i] || "default (east)"} side of ${pad.name}; trying ${sides[i + 1]}`);
  }
  if (action) {
    const used = sendCommand({ verb: "use", className: action, entityName: pad.name });
    lines.push(used.raw || `${used.id} ${used.status} ${used.detail}`);
    if (used.status !== "ok") return { code: 1, lines, pad };
    pause(400);
  }
  const shot = sendCommand({ verb: "capture", mode: "ui", name: pad.name }, 25000);
  lines.push(shot.raw || `${shot.id} ${shot.status} ${shot.detail}`);
  const image = resolveCapture(shot.detail);
  if (image) lines.push(`image: ${image}`);
  return { code: shot.status === "ok" ? 0 : 1, lines, pad };
}

function printResult(result) {
  console.log(result.raw || `${result.id} ${result.status} ${result.detail}`);
  console.log(`command: ${result.line}`);
  const image = resolveCapture(result.detail);
  if (image) console.log(`image: ${image}`);
  return result.status === "ok" ? 0 : 1;
}

function selftest() {
  const yes = () => true;
  // #4677: layer tries every side when none is named (the registry clerk shows her prompt on the west only); a named side is tried alone.
  if (layerSideOrder("").join(",") !== ",w,n,s" || layerSideOrder("w2").join(",") !== "w2") throw new Error("layer side order");
  if (decideClaim(null, "C:/a", yes).claim !== true) throw new Error("free channel is claimed");
  if (decideClaim({ pid: 7, root: "C:/a" }, "C:\\a", yes).ok !== true) throw new Error("own claim drives");
  if (decideClaim({ pid: 7, root: "C:/b" }, "C:/a", yes).ok !== false) throw new Error("other checkout refuses");
  if (decideClaim({ pid: 7, root: "C:/b" }, "C:/a", () => false).claim !== true) throw new Error("dead owner is stale");
  const procs = [
    { pid: 1, cmd: "Workbench.exe -gproj C:/x/Life-Framework/addons/LifeFramework/LifeFramework.gproj" },
    { pid: 2, cmd: "Game.exe -addonsDir C:\\x\\Life-Framework-ws-a\\addons -addons LifeFramework" },
  ];
  if (rendererFor("C:/x/Life-Framework-ws-a", procs) !== 2) throw new Error("renderer by addonsDir");
  if (rendererFor("C:/x/Life-Framework", procs) !== 1) throw new Error("renderer by gproj");
  const check = spawnSync(process.execPath, ["--check", join(ROOT, "agent-box", "mcp-server.mjs")], { windowsHide: true, encoding: "utf8" });
  if ((check.status ?? 1) !== 0) throw new Error(check.stderr || "life-agent-box MCP server failed to parse");
  const use = buildLine(4, { verb: "use", className: "LF_ShopAction", entityName: "Trader_01" });
  if (use !== "4 use LF_ShopAction Trader_01") throw new Error(use);
  const approve = buildLine(6, { verb: "staffapprove", uid: "00bbbddd-5e55-0000-0000-000000000002" });
  if (approve !== "6 staffapprove 00bbbddd-5e55-0000-0000-000000000002") throw new Error(approve);
  for (const bad of ["", "a b", "a|b", "x".repeat(65), undefined]) {
    let refused = false;
    try {
      buildLine(6, { verb: "staffapprove", uid: bad });
    } catch {
      refused = true;
    }
    if (!refused) throw new Error(`staffapprove accepted the uid ${JSON.stringify(bad)}`);
  }
  if (buildLine(7, { verb: "chat", text: "!handled 3" }) !== "7 chat !handled 3") throw new Error("chat line");
  if (buildLine(7, { verb: "chat", channel: "admin", text: "!wadd POLICE abc" }) !== "7 chat admin !wadd POLICE abc") throw new Error("chat line with a channel");
  for (const bad of [{ text: "" }, { text: " lead" }, { text: "line\nbreak" }, { text: "x".repeat(401) }, { text: "hi", channel: "radio" }, {}]) {
    let refused = false;
    try {
      buildLine(7, { verb: "chat", ...bad });
    } catch {
      refused = true;
    }
    if (!refused) throw new Error(`chat accepted ${JSON.stringify(bad)}`);
  }
  {
    // red-proof 2026-11-24 (#4867): with holdWeapon sending once, the "retried" check below failed.
    const calls = [];
    const answers = ["drew AK false", "holstered", "drew AK false", "holstered", "drew AK true"];
    const fake = (spec) => { calls.push(`${spec.verb} ${spec.item}`); return { status: "ok", detail: answers.shift() }; };
    const got = holdWeapon(fake, 1, () => {});
    if (got.detail !== "drew AK true" || calls.join() !== "hold weapon,hold none,hold weapon,hold none,hold weapon") throw new Error(`hold weapon retry: ${calls.join()} -> ${got.detail}`);
    let n = 0;
    const never = () => { n++; return { status: "ok", detail: "drew AK false" }; };
    holdWeapon(never, 1, () => {});
    if (n !== 2 * HOLD_WEAPON_TRIES - 1) throw new Error(`hold weapon gave up after ${n} sends`);
    const once = [];
    holdWeapon((spec) => { once.push(spec.item); return { status: "ok", detail: "drew AK true" }; }, 1, () => { throw new Error("waited after a draw that worked"); });
    if (once.join() !== "weapon") throw new Error("hold weapon retried a draw that worked");
    if (weaponNotDrawn({ status: "error", detail: "no_character" }) || weaponNotDrawn({ status: "ok", detail: "no_weapon_in_slots" })) throw new Error("weaponNotDrawn matched a non-draw answer");
  }
  {
    // red-proof 2026-11-24 (#4867): with stateOutwaitingFreeze sending once, the first check below failed.
    const answers = [{ status: "error", detail: "no_session" }, { status: "error", detail: "no_session" }, { status: "ok", detail: "cash=0" }];
    let sent = 0;
    const got = stateOutwaitingFreeze(() => { sent++; return answers.shift(); }, 1);
    if (got.status !== "ok" || sent !== 3) throw new Error(`state retry: ${sent} sends -> ${got.status}`);
    sent = 0;
    stateOutwaitingFreeze(() => { sent++; return { status: "error", detail: "no_character" }; }, 1);
    if (sent !== 1) throw new Error("state retried an answer that was not a timeout");
    sent = 0;
    stateOutwaitingFreeze(() => { sent++; return { status: "error", detail: "no_session" }; }, 1);
    if (sent !== STATE_TRIES) throw new Error(`state gave up after ${sent} sends`);
  }
  const quizClear = buildLine(9, { verb: "quizclear" });
  if (quizClear !== "9 quizclear") throw new Error(quizClear);
  const input = buildLine(5, { verb: "input", action: "MenuSelect" });
  if (input !== "5 input MenuSelect") throw new Error(input);
  const world = buildLine(8, { verb: "capture", mode: "world", name: "atm_pad", x: 128, y: 1, z: 133 });
  if (world !== "8 capture world atm_pad 128 1 133") throw new Error(world);
  const clean = buildLine(8, { verb: "capture", mode: "world", name: "s", x: 1, y: 2, z: 3, ox: 4, oy: 5, oz: 6, hour: 18.5 });
  if (clean !== "8 capture world s 1 2 3 4 5 6 18.5") throw new Error(clean);
  if (buildLine(3, { verb: "time", hour: 18.5 }) !== "3 time 18.5") throw new Error("time line");
  // resetplayer empties the inventory by default; `keep` is the opt-out and sits after the cash argument.
  // red-proof 2026-11-14: with the keepInventory branch of buildLine removed, "resetplayer keep line" failed.
  if (buildLine(6, { verb: "resetplayer" }) !== "6 resetplayer" || buildLine(6, { verb: "resetplayer", cash: 50 }) !== "6 resetplayer 50") throw new Error("resetplayer line");
  if (buildLine(6, { verb: "resetplayer", keepInventory: true }) !== "6 resetplayer -1 keep" || buildLine(6, { verb: "resetplayer", cash: 50, keepInventory: true }) !== "6 resetplayer 50 keep") throw new Error("resetplayer keep line");
  if (buildLine(3, { verb: "time", hour: 12, weather: "Cloudy" }) !== "3 time 12 Cloudy") throw new Error("time weather line");
  const probeEnt = buildLine(8, { verb: "probe", mode: "ent", name: "Hospital_Bed_1", x: 1, y: 0.2, z: 3 });
  if (probeEnt !== "8 probe ent Hospital_Bed_1 1 0.2 3") throw new Error(probeEnt);
  if (buildLine(8, { verb: "probe", mode: "ground", x: 5, z: 6 }) !== "8 probe ground 5 6") throw new Error("probe ground line");
  const interact = buildLine(9, { verb: "interact", step: "confirm" });
  if (interact !== "9 interact confirm") throw new Error(interact);
  const stand = buildLine(10, { verb: "stand", entityName: "Town_ATM_Bank" });
  if (stand !== "10 stand Town_ATM_Bank") throw new Error(stand);
  const state = buildLine(11, { verb: "state" });
  if (state !== "11 state") throw new Error(state);
  if (buildLine(12, { verb: "perf", seconds: 8, label: "boards_4" }) !== "12 perf 8 boards_4") throw new Error("perf line");
  if (buildLine(12, { verb: "perf" }) !== "12 perf 10") throw new Error("perf default line");
  if (buildLine(13, { verb: "framegap" }) !== "13 framegap") throw new Error("framegap line");
  setBoxes(["X"]);
  const boxes = profileBoxes();
  setBoxes(null);
  if (boxes.length !== 1 || boxes[0] !== "X") throw new Error("setBoxes did not target one client box");
  if (profileBoxes().length !== 2) throw new Error("setBoxes(null) did not restore the default boxes");
  if (buildLine(30, { verb: "click", widget: "SignButton", session: "ab12" }) !== "30 s:ab12 click SignButton") throw new Error("session token goes after the id");
  if (buildLine(31, { verb: "look", session: "" }) !== "31 look") throw new Error("no session, no token");
  if (buildLine(32, { verb: "pickup", item: "LF_Crackers" }) !== "32 pickup LF_Crackers" || buildLine(33, { verb: "pickup" }) !== "33 pickup") throw new Error("pickup [item]");
  const mine = (box) => (box === "B1" ? { id: "ab12" } : null);
  const theirs = () => ({ error: "session_owned_by_other_checkout C:/other" });
  if (addressFor(["B1"], "R", mine).id !== "ab12") throw new Error("a box in the caller's session is addressed to it");
  if (!addressFor(["B1"], "R", theirs).error) throw new Error("another checkout's session must refuse");
  if (addressFor(null, "R", theirs).id !== "") throw new Error("Workbench boxes carry no session");
  const list = pads();
  const atm = findPad(list, "Town_ATM_Bank");
  if (!atm || !atm.actions.includes("LF_ATMAction")) throw new Error("Town_ATM_Bank missing from layer catalog");
  const shop = findPad(list, "Shop_GeneralStore_Debug");
  if (!shop || shop.actions[0] !== "LF_ShopAction") throw new Error("Shop_GeneralStore_Debug missing from layer catalog");
  // $grp members carry world coords; children add their parent's (village layout,
  // docs/roadmap/m1-25-village.md). The general store counter stands inside the village shop house (general-store).
  if (shop.x !== 180.57 || shop.z !== 233.627) throw new Error(`shop coords ${shop.x} ${shop.z}`);
  const apple = findPad(list, "AppleTradeContainer");
  // AppleTrader 206.359 0 233.467 (the fruit stall, #924) plus the container's -0.29 1.027 -0.478.
  if (!apple || Math.abs(apple.x - 206.069) > 0.001 || Math.abs(apple.z - 232.989) > 0.001) throw new Error(`apple coords ${apple && apple.x} ${apple && apple.z}`);
  const police = findPad(list, "Roadmap_Police_Desk");
  if (!police || police.x !== 232.9 || police.z !== 239.6 || !police.actions.includes("LF_DutyAction")) throw new Error(`police coords ${police && police.x} ${police && police.z}`);
  const pile = findPad(list, "CementPile_Debug");
  if (!pile || !pile.actions.includes("LF_GatherAction")) throw new Error("CementPile_Debug gather missing");
  const ore = findPad(list, "IronOre_Debug");
  // Ore rocks are mined by a pickaxe swing, so they carry no action and are not pads.
  if (ore && ore.actions.includes("LF_GatherAction")) throw new Error("IronOre_Debug still has an F-key gather; it is mined by hitting it");
  const counter = findPad(list, "TraderChainCounter");
  if (!counter || !counter.actions.includes("LF_TraderSellAction")) throw new Error("TraderChainCounter missing");
  const saw = findPad(list, "Sawmill_Debug");
  if (!saw || !saw.actions.includes("LF_ProcessAction")) throw new Error("Sawmill_Debug process missing");
  if (buildLine(1, { verb: "input", action: "LF_Keypad1" }) !== "1 input LF_Keypad1") throw new Error("the dialogue digit key is allowed");
  for (const bad of ["LF_Keypad", "LF_Keypad10", "LF_KeypadBack"]) {
    let refused = false;
    try {
      buildLine(1, { verb: "input", action: bad });
    } catch {
      refused = true;
    }
    if (!refused) throw new Error(`${bad} was accepted`);
  }
  const phases = [
    ["open node=greet phase=choosing replies=1 camera=1 shown=1", "choosing", "greet", true],
    ["open node=greet phase=line replies=0 camera=1", "choosing", undefined, false],
    ["open node=menu phase=choosing replies=3", "choosing", "greet", false],
    ["closed lines=0 mouthframes=0", "closed", undefined, true],
    ["closed lines=0", "choosing", undefined, false],
  ];
  for (const [detail, phase, node, want] of phases) if (dialogueMatches(detail, phase, node) !== want) throw new Error(`dialogueMatches ${detail} ${phase} ${node}`);
  const seen = [];
  const polled = waitForDialogue("choosing", "greet", 5000, () => { seen.push(1); return { status: "ok", detail: seen.length < 3 ? "open node=greet phase=line replies=0" : "open node=greet phase=choosing replies=1" }; }, () => {});
  if (!polled.found || seen.length !== 3) throw new Error(`waitForDialogue polled ${seen.length} times`);
  // dialogue-choose: a key that lands, a key dropped twice (the pick fallback), a pick onto the wrong node, and the refused Goodbye digit.
  const sceneAt = { node: "greet", log: [] };
  const fake = (landKey) => (spec) => {
    sceneAt.log.push(spec.verb === "input" ? `key ${spec.action}` : spec.verb === "dialogue" && spec.arg ? `pick ${spec.value}` : "poll");
    if (spec.verb === "input" && landKey) sceneAt.node = "welcome_papers";
    if (spec.verb === "dialogue" && spec.arg === "pick") sceneAt.node = "welcome_papers";
    return { status: "ok", detail: spec.arg ? "picked=true" : `open node=${sceneAt.node} phase=choosing replies=1` };
  };
  let clock = 0;
  const tick = () => clock;
  const idle = (ms) => { clock += ms; };
  let chose = chooseDialogue("greet", "1", "welcome_papers", fake(true), idle, tick);
  if (!chose.found || !/greet -> welcome_papers via key/.test(chose.detail)) throw new Error(`dialogue-choose key: ${JSON.stringify(chose)}`);
  sceneAt.node = "greet";
  sceneAt.log = [];
  chose = chooseDialogue("greet", "1", "welcome_papers", fake(false), idle, tick);
  const keys = sceneAt.log.filter((l) => l.startsWith("key")).length;
  if (!chose.found || !/via pick \(keys dropped\)/.test(chose.detail) || keys !== CHOOSE_KEY_TRIES || !sceneAt.log.includes("pick 0")) throw new Error(`dialogue-choose fallback: ${JSON.stringify(chose)} ${sceneAt.log.join(",")}`);
  // The scene reads phase=waiting on the old node while the server answers: the choice settles on the next node.
  let waitingPolls = 0;
  sceneAt.node = "greet";
  const slowServer = (spec) => {
    if (spec.verb === "input") return { status: "ok", detail: "LF_Keypad1" };
    waitingPolls++;
    return { status: "ok", detail: waitingPolls < 4 ? "open node=greet phase=waiting replies=1" : "open node=welcome_papers phase=line replies=0" };
  };
  chose = chooseDialogue("greet", "1", "welcome_papers", (spec) => (waitingPolls === 0 && spec.verb === "dialogue" ? (waitingPolls++, { status: "ok", detail: "open node=greet phase=choosing replies=1" }) : slowServer(spec)), idle, tick);
  if (!chose.found || !/greet -> welcome_papers via key/.test(chose.detail)) throw new Error(`dialogue-choose waiting: ${JSON.stringify(chose)}`);
  sceneAt.node = "greet";
  chose = chooseDialogue("greet", "1", "welcome_room", fake(true), idle, tick);
  if (chose.found || !/dialogue_chose_wrong wanted welcome_room/.test(chose.detail)) throw new Error(`dialogue-choose wrong node: ${JSON.stringify(chose)}`);
  let refusedBye = false;
  try {
    chooseDialogue("greet", "0", "bye", fake(true), idle, tick);
  } catch {
    refusedBye = true;
  }
  if (!refusedBye) throw new Error("dialogue-choose took the Goodbye digit 0");
  let rejected = false;
  try {
    buildLine(1, { verb: "input", action: "MoveForward" });
  } catch {
    rejected = true;
  }
  if (!rejected) throw new Error("movement action was accepted");
  const bmp = decodeBmp(syntheticBmp());
  if (bmp.width !== 2 || bmp.height !== 2) throw new Error("bmp size");
  if (bmp.pixels[0] !== 255 || bmp.pixels[1] !== 0 || bmp.pixels[2] !== 0) throw new Error("bmp top pixel");
  if (buildLine(12, { verb: "open", menu: "IdCard" }) !== "12 open IdCard") throw new Error("open IdCard");
  const shown = buildLine(11, { verb: "open", menu: "DebugMenu", mode: "show" });
  if (shown !== "11 open DebugMenu show") throw new Error(shown);
  const click = buildLine(12, { verb: "click", widget: "CivilianButton" });
  if (click !== "12 click CivilianButton") throw new Error(click);
  const typed = buildLine(13, { verb: "type", widget: "RegFirstName", text: "Ada" });
  if (typed !== "13 type RegFirstName Ada") throw new Error(typed);
  if (buildLine(14, { verb: "ui", mode: "text" }) !== "14 ui text") throw new Error("ui text line");
  const sheet = parseUiText("button:SignButton=Sign here | field:RegAge=30 | line:Balance=Balance: $400 / cash | button:IdClose=");
  if (sheet.length !== 4 || sheet[1].kind !== "field" || sheet[1].name !== "RegAge" || sheet[1].text !== "30") throw new Error(`parseUiText: ${JSON.stringify(sheet)}`);
  if (sheet[2].text !== "Balance: $400 / cash" || sheet[3].name !== "IdClose" || sheet[3].text !== "") throw new Error(`parseUiText text: ${JSON.stringify(sheet)}`);
  if (!uiHasText(sheet, "balance: $400") || !uiHasText(sheet, "IdClose") || uiHasText(sheet, "Withdraw")) throw new Error("uiHasText");
  if (parseUiText("none").length !== 0) throw new Error("ui text none is no entries");
  let polls = 0;
  const late = waitForText("Deposited", 5000, () => ({ status: "ok", detail: ++polls < 3 ? "line:Status=Enter an amount" : "line:Status=Deposited $50" }), () => {});
  if (!late.found || polls !== 3) throw new Error(`waitForText polls until the text shows: ${polls}`);
  const never = waitForText("Deposited", 1, () => ({ status: "ok", detail: "line:Status=Enter an amount" }), () => {});
  if (never.found || !never.detail.startsWith("text_not_found")) throw new Error("waitForText times out with text_not_found");
  const indexed = buildLine(13, { verb: "click", widget: "BuyButton", index: 1 });
  if (indexed !== "13 click BuyButton 1") throw new Error(indexed);
  const flow = readFlow("ui-debug-menu").filter((line) => line.trim() && !line.trim().startsWith("#"));
  if (!flow.includes("open DebugMenu show") || !flow.includes("shot ui debug_menu")) throw new Error(flow.join("|"));
  const kitLine = buildLine(21, { verb: "kit", step: "line", part: "Balance", text: "400" });
  if (kitLine !== "21 kit line Balance 400") throw new Error(kitLine);
  const kitStatus = buildLine(22, {
    verb: "kit",
    step: "status",
    part: "Status",
    label: "Warrant",
    statusText: "Closed",
    action: "none",
  });
  if (kitStatus !== "22 kit status Status Warrant Closed none") throw new Error(kitStatus);
  const kitTake = buildLine(23, { verb: "kit", step: "quantity", part: "Quantity", total: 5, take: 4 });
  if (kitTake !== "23 kit quantity Quantity 5 4") throw new Error(kitTake);
  const kitFlow = readFlow("ui-kit").filter((line) => line.trim() && !line.trim().startsWith("#"));
  for (const name of ["kit-sheet", "kit-balance", "kit-status", "kit-keep", "kit-take", "kit-focus"]) {
    if (!kitFlow.some((line) => line.includes(name))) throw new Error(`ui-kit flow missing ${name}`);
  }
  if (!kitFlow.includes("kit show") || !kitFlow.includes("kit hide")) throw new Error(kitFlow.join("|"));
  const intake = readFlow("ui-intake").filter((line) => line.trim() && !line.trim().startsWith("#"));
  if (intake.includes("click CivilianButton") || !intake.includes("click SignButton")) throw new Error(intake.join("|"));
  const chain = checkChains(ROOT);
  if (!chain.ok) throw new Error(chain.errors.join("; "));
  const appleFlow = readFlow("chain-apple").filter((line) => line.trim() && !line.trim().startsWith("#"));
  if (!appleFlow.includes("use LF_GatherAction AppleGatherTree") || !appleFlow.includes("use LF_OpenTraderAction AppleTradeContainer")) {
    throw new Error(appleFlow.join("|"));
  }
  const editPlay = playSteps("edit");
  if (editPlay.length !== 1 || editPlay[0][0].tool !== "wb_state" || editPlay[0].at(-1).tool !== "wb_play") {
    throw new Error("edit play batch must read state before play");
  }
  const wideLine = buildLine(15, { verb: "capture", mode: "world", name: "roadmap_row", x: 300, y: 1, z: 121, ox: 0, oy: 40, oz: 90 });
  if (wideLine !== "15 capture world roadmap_row 300 1 121 0 40 90") throw new Error(wideLine);
  const street = judgeStreet([
    { name: "Roadmap_A", x: 210, y: 0, z: 120, look: "LF_BreachArmAction@Roadmap_A", mesh: "ok", menu: null, src: "shots/Roadmap_A.png" },
    { name: "Roadmap_B", x: 228, y: 0, z: 120, look: "none", mesh: "ok", menu: null, src: "shots/Roadmap_B.png" },
    { name: "Roadmap_Far", x: 228, y: 0, z: 200, look: "LF_RobAction@Roadmap_Far", mesh: "ok", menu: null, src: "shots/Roadmap_Far.png" },
  ]);
  const byName = Object.fromEntries(street.map((row) => [row.name, row]));
  if (byName.Roadmap_B.ok || !byName.Roadmap_B.failures.includes("look offers no interaction")) {
    throw new Error("empty look must fail");
  }
  if (byName.Roadmap_Far.ok || byName.Roadmap_Far.distanceM !== 80 || !byName.Roadmap_Far.failures.includes("more than 30 m from the rest of the street")) {
    throw new Error("entity 80 m away must fail");
  }
  if (!byName.Roadmap_A.ok) throw new Error("a close site with an interaction must pass");
  const selfActions = judgeRoadmapRow({
    name: "Roadmap_Garage",
    look: "SCR_LootDeadBodyAction,LF_ConfiscateAction",
    mesh: "ok",
    distanceM: 18,
    menu: null,
  });
  if (selfActions.ok || !selfActions.failures.includes("look offers no interaction")) {
    throw new Error("the character's own actions are not the site");
  }
  if (siteLook("Roadmap_Garage", "LF_VehicleStoreAction@Roadmap_Garage,SCR_LootDeadBodyAction") !== "LF_VehicleStoreAction@Roadmap_Garage") {
    throw new Error("a named site keeps only its own action");
  }
  const northShot = siteShotOffset("Roadmap_Garage", { x: 238, y: 0, z: STREET_Z + 6.6 });
  const southShot = siteShotOffset("Roadmap_Escort_Car", { x: 247.5, y: 0, z: STREET_Z - 12 });
  if (!northShot || northShot.oz >= 0 || !southShot || southShot.oz <= 0) {
    throw new Error("a street site is photographed from the street: north side from the south, south side from the north");
  }
  if (siteShotOffset("Roadmap_Convoy_Start", { x: 466, y: 0, z: 331 }) !== null) {
    throw new Error("an off-street site keeps the default camera");
  }
  if (!judgeRoadmapRow({ name: "Roadmap_Convoy_Start", look: "LF_ConvoyStartAction@Roadmap_Convoy_Start", mesh: "ok", distanceM: 170, menu: null }).ok) {
    throw new Error("the contraband pickup is out of town by design");
  }
  if (!judgeRoadmapRow({ look: "LF_RobAction@X", mesh: "ok", distanceM: 30, menu: null }).ok) {
    throw new Error("30 m is still with the street");
  }
  const whiteRow = judgeRoadmapRow({ look: "LF_RobAction@X", mesh: "white", distanceM: 18, menu: null });
  if (whiteRow.ok || !whiteRow.failures.includes("mesh is white or missing")) throw new Error("white mesh must fail");
  const blankMenu = judgeRoadmapRow({
    look: "LF_OrganizationDeskAction@Roadmap_Org_Desk",
    mesh: "ok",
    distanceM: 18,
    menu: "none",
  });
  if (blankMenu.ok || !blankMenu.failures.includes("menu is blank")) throw new Error("blank menu must fail");
  if (menuActionFromLook("none") || menuActionFromLook("LF_BreachArmAction@Roadmap_Breach_Door")) {
    throw new Error("world actions are not menus");
  }
  if (menuActionFromLook("LF_OrganizationDeskAction@Roadmap_Org_Desk") !== "LF_OrganizationDeskAction@Roadmap_Org_Desk") {
    throw new Error("desk action opens a menu");
  }
  const whitePixels = Buffer.alloc(2 * 2 * 3, 255);
  if (classifyMesh({ width: 2, height: 2, pixels: whitePixels }) !== "white") throw new Error("white pixels");
  if (classifyMesh(decodePng(encodePng(2, 2, whitePixels))) !== "white") throw new Error("white png");
  if (classifyMesh(decodeBmp(syntheticBmp())) !== "ok") throw new Error("colored bmp must not be white");
  if (classifyMeshFile("") !== "missing") throw new Error("missing shot");
  const camera = rowCamera([
    { x: 228, y: 0, z: 120 },
    { x: 372, y: 0, z: 122 },
  ]);
  if (camera.oz < 40 || camera.oy < 18) throw new Error("row camera must step back and up");
  const placed = loadPlaced(ROOT).filter((site) => site.name.startsWith("Roadmap_"));
  if (!placed.some((site) => site.name === "Roadmap_Breach_Charge")) throw new Error("charge is part of the walk");
  const cash = placed.find((site) => site.name === "Roadmap_Bank_Bag_Cash");
  if (!cash || cash.x !== 194.302 || cash.z !== 261.587) throw new Error("bag cash world position (Roadmap_Bank_Bag plus its child offset)");
  if (roadmapSites(ROOT).length < 12) throw new Error("roadmap walk is short");
  const page = renderRoadmapReport({
    wide: { src: "shots/roadmap-row.png", mesh: "ok" },
    rows: street,
    note: "selftest",
  });
  if (!page.includes("shots/roadmap-row.png") || !page.includes("Roadmap_Far") || !page.includes("look offers no interaction")) {
    throw new Error("report page must put the picture beside the text");
  }
  const farArticle = page.split("<article").find((part) => part.includes("Roadmap_Far"));
  const emptyArticle = page.split("<article").find((part) => part.includes("Roadmap_B"));
  if (!farArticle || !farArticle.includes("shots/Roadmap_Far.png") || !farArticle.includes("more than 30 m from the rest of the street")) {
    throw new Error("80 m picture must sit beside its failure");
  }
  if (!emptyArticle || !emptyArticle.includes("shots/Roadmap_B.png") || !emptyArticle.includes("look offers no interaction")) {
    throw new Error("empty look picture must sit beside its failure");
  }
  const painted = presentedMesh("Roadmap_Jail_Door", "white", "LF_BreachArmAction@Roadmap_Jail_Door");
  if (painted !== "painted-light") throw new Error("prison door with an action is painted-light");
  const paintedCell = presentedMesh("Roadmap_Jail_Cell", "white", "LF_JailArrestAction@Roadmap_Jail_Cell");
  if (paintedCell !== "painted-light") throw new Error("prison cell with an action is painted-light");
  if (presentedMesh("Roadmap_Jail_Door", "white", "none") !== "white") throw new Error("a white door with no action stays white");
  if (presentedMesh("Roadmap_Garage", "white", "LF_VehicleStoreAction@Roadmap_Garage") !== "white") {
    throw new Error("a white mesh on any other site stays a failure");
  }
  const paintedRow = judgeRoadmapRow({
    name: "Roadmap_Jail_Door",
    look: "LF_BreachArmAction@Roadmap_Jail_Door",
    mesh: painted,
    distanceM: 3,
    menu: null,
  });
  if (!paintedRow.ok) throw new Error("painted-light prison door must pass");
  const whiteMenu = judgeRoadmapRow({
    look: "LF_OrganizationDeskAction@Roadmap_Org_Desk",
    mesh: "ok",
    distanceM: 3,
    menu: "CloseButton",
    menuMesh: "white",
  });
  if (whiteMenu.ok || !whiteMenu.failures.includes("menu is blank")) throw new Error("a white menu shot is blank");
  const listedMenu = judgeRoadmapRow({
    look: "LF_OrganizationDeskAction@Roadmap_Org_Desk",
    mesh: "ok",
    distanceM: 3,
    menu: "CloseButton",
    menuMesh: "ok",
  });
  if (!listedMenu.ok) throw new Error("a menu with widgets and a picture passes");
  const picked = selectRoadmapSites(
    [
      { name: "Roadmap_Garage" },
      { name: "Roadmap_Jail_Door" },
      { name: "Roadmap_Escort_Car" },
    ],
    ["Roadmap_Garage", "Roadmap_Escort_Car"],
  );
  if (picked.length !== 2 || picked.some((site) => site.name === "Roadmap_Jail_Door")) {
    throw new Error("name filter must keep only the named row");
  }
  if (missingRoadmapNames(picked, ["Roadmap_Garage", "Roadmap_Missing"]).join(",") !== "Roadmap_Missing") {
    throw new Error("a missing name must be reported");
  }
  const scratch = mkdtempSync(join(tmpdir(), "roadmap-"));
  const samePng = join(scratch, "same.png");
  writeFileSync(samePng, encodePng(2, 2, Buffer.alloc(2 * 2 * 3, 40)));
  const stuckLogs = [];
  let stuckShots = 0;
  const stuckCode = runRoadmapReport({
    root: scratch,
    names: ["Roadmap_Garage", "Roadmap_Escort_Car", "Roadmap_Jail_Door"],
    sites: () => [
      { name: "Roadmap_Garage", layer: "Roadmap.layer", x: 186, y: 0, z: 108 },
      { name: "Roadmap_Escort_Car", layer: "Roadmap.layer", x: 204, y: 0, z: 108 },
      { name: "Roadmap_Jail_Door", layer: "Roadmap.layer", x: 192, y: 0, z: 120 },
    ],
    stand: () => ({ status: "ok", detail: "stood" }),
    look: () => ({ status: "ok", detail: "LF_VehicleStoreAction@Roadmap_Garage" }),
    ui: () => ({ status: "ok", detail: "CloseButton" }),
    use: () => ({ status: "ok" }),
    input: () => ({ status: "ok" }),
    captureWorld: () => {
      stuckShots++;
      return { status: "ok", detail: "$logs:el-captures/same.png" };
    },
    captureUi: () => ({ status: "ok", detail: "" }),
    resolve: () => samePng,
    pause: () => {},
    log: (line) => stuckLogs.push(line),
  });
  if (stuckShots !== 2) throw new Error(`camera_stuck must stop after the repeated shot, saw ${stuckShots}`);
  if (stuckLogs.filter((line) => line === "camera_stuck").length !== 1) throw new Error(stuckLogs.join(" | "));
  if (stuckCode !== 1) throw new Error("camera_stuck must fail the report");
  if (stuckLogs.some((line) => line.startsWith("pass Roadmap_Jail_Door") || line.startsWith("fail Roadmap_Jail_Door"))) {
    throw new Error("camera_stuck must not walk the next site");
  }
  const filterLogs = [];
  const filterSeen = [];
  runRoadmapReport({
    root: join(scratch, "filter"),
    names: ["Roadmap_Garage"],
    sites: () => [
      { name: "Roadmap_Garage", layer: "Roadmap.layer", x: 186, y: 0, z: 108 },
      { name: "Roadmap_Jail_Door", layer: "Roadmap.layer", x: 192, y: 0, z: 120 },
    ],
    stand: () => ({ status: "ok", detail: "stood" }),
    look: () => ({ status: "ok", detail: "LF_VehicleStoreAction@Roadmap_Garage" }),
    ui: () => ({ status: "ok", detail: "none" }),
    use: () => ({ status: "ok" }),
    input: () => ({ status: "ok" }),
    captureWorld: (spec) => {
      filterSeen.push(spec.name);
      return { status: "error", detail: "skipped" };
    },
    captureUi: () => ({ status: "ok", detail: "" }),
    resolve: () => "",
    pause: () => {},
    log: (line) => filterLogs.push(line),
  });
  if (filterSeen.includes("Roadmap_Jail_Door") || !filterSeen.includes("Roadmap_Garage") || !filterSeen.includes("roadmap_row")) {
    throw new Error(`filter walked ${filterSeen.join(",")}`);
  }
  const wide108 = rowCamera(
    [
      { name: "Roadmap_Garage", x: 186, y: 0, z: 108 },
      { name: "Roadmap_Escort_Car", x: 204, y: 0, z: 108 },
    ],
    [
      { name: "Roadmap_Garage", x: 186, y: 0, z: 108 },
      { name: "Roadmap_Escort_Car", x: 204, y: 0, z: 108 },
      { name: "Roadmap_Breach_Door", x: 186, y: 0, z: 120 },
    ],
  );
  if (wide108.oz <= 0 || wide108.z + wide108.oz <= 108) {
    throw new Error("Z=108 wide shot stands north of that row, the way the player camera faces");
  }
  const wideStreet = rowCamera([
    { name: "Roadmap_Breach_Door", x: 186, y: 0, z: 120 },
    { name: "Roadmap_Vehicle_Lot", x: 243, y: 0, z: 120 },
  ]);
  if (wideStreet.oz <= 0) throw new Error("the release street wide shot stays on +Z");
  // The live village is not asserted here: the maintainer places the town by hand, so where a site
  // stands is his call (validate-no-town-layout-asserts). judgeStreet is covered on fixture rows above.
  // Three worlds (W5, #1807): a proof that walks to an annex fixture asks for the grid mission and says so when the session booted another world.
  // red-proof: 2026-11-16 written before missionProblem existed: the selftest threw "missionProblem is not defined", then these cases passed.
  if (missionProblem({ mission: "grid" }, "grid") !== "") throw new Error("a grid session was refused for a grid proof");
  if (!/session up --mission grid/.test(missionProblem({ mission: "debug" }, "grid"))) throw new Error("a town session was accepted for a grid proof");
  if (!/session up --mission grid/.test(missionProblem({}, "grid"))) throw new Error("a session record with no mission (the town default) was accepted for a grid proof");
  if (!/no running session/.test(missionProblem(null, "grid"))) throw new Error("no session was accepted");
  if (!/no running session/.test(missionProblem({ mission: "grid", stoppedAt: "2026-11-16T00:00:00Z" }, "grid"))) throw new Error("a stopped session was accepted");
  townSelftest();
  console.log(flowAssertsSelftest());
  console.log(handbookFlowSelftest());
  console.log("selftest ok");
  return 0;
}

// Town manifest: missing sign, blank card, 80 m outlier, empty look, and the
// requireSigns flag that M1-09 turns on. The real manifest must resolve offline.
function townSelftest() {
  const real = checkTown(ROOT);
  if (!real.ok) throw new Error(`town manifest does not resolve against the layers: ${real.errors.join("; ")}`);
  if (manifestErrors({ ...TOWN, sites: [{ id: "a", entity: "A", actions: [], sign: null, step: 1, x: 3 }] }).length !== 1) {
    throw new Error("a manifest site with a position must be rejected");
  }

  const world = [
    { name: "Site_A", layer: "T.layer", x: 0, y: 0, z: 0, actions: ["LF_A"], prefab: "" },
    { name: "Site_B", layer: "T.layer", x: 10, y: 0, z: 0, actions: ["LF_B"], prefab: "" },
    { name: "Site_Far", layer: "T.layer", x: 10, y: 0, z: 80, actions: ["LF_Far"], prefab: "" },
    { name: "Sign_A", layer: "T.layer", x: 3, y: 0, z: 0, actions: ["LF_SignReadAction"], prefab: "", signId: "a" },
    { name: "Sign_B", layer: "T.layer", x: 10, y: 0, z: 9, actions: ["LF_SignReadAction"], prefab: "", signId: "b" },
    { name: "Sign_Blank", layer: "T.layer", x: 10, y: 0, z: 81, actions: ["LF_SignReadAction"], prefab: "", signId: "blank" },
  ];
  const catalog = parseSignConfig(`LF_SignConfig {
 m_aSigns {
  LF_SignDef "{0000000000000001}" {
   m_sId "a"
   m_sTitle "#T-A_Title"
   m_sWhat "A place."
  }
  LF_SignDef "{0000000000000002}" {
   m_sId "b"
   m_sTitle "B"
   m_sHow "Press it."
  }
  LF_SignDef "{0000000000000003}" {
   m_sId "blank"
   m_sTitle "Blank"
   m_sWhat "#T-Missing"
  }
 }
}`);
  const strings = new Map([["T-A_Title", "Site A"]]);
  if (signCard("a", catalog, strings).blank || signCard("a", catalog, strings).title !== "Site A") throw new Error("a #key title resolves");
  if (!signCard("blank", catalog, strings).blank) throw new Error("a card whose only section is an unknown key is blank");
  // The pack's signs load beside the core's: an added sign is there, a patch keeps the core fields it does not name.
  // red-proof 2026-11-16: with the core-only loader, "registry" was missing and this threw.
  const packCatalog = loadSignCatalog(ROOT);
  if (!packCatalog.has("registry")) throw new Error("the catalog holds the pack's added sign (registry)");
  if (!packCatalog.get("hospital")?.m_sTitle || !packCatalog.get("hospital")?.m_sImage) throw new Error("a pack patch keeps the core title and adds its field (hospital)");
  const manifest = {
    requireSigns: false,
    signRadiusM: 8,
    neighbourM: 30,
    sites: [
      { id: "a", entity: "Site_A", actions: ["LF_A"], sign: "a", step: 1 },
      { id: "b", entity: "Site_B", actions: ["LF_B"], sign: "b", step: 2 },
      { id: "far", entity: "Site_Far", actions: ["LF_Far"], sign: "blank", step: 3 },
      { id: "nosign", entity: "Site_B", actions: [], sign: "missing", step: 4 },
    ],
  };
  const soft = checkTown(ROOT, { manifest, world, catalog, strings });
  // Distances are the maintainer's layout: a site 80 m away warns and never fails.
  // red-proof 2026-11-16: before the warn-only change this threw "a site 80 m away only warns".
  if (soft.errors.some((error) => error.includes("nearest other site")) || !soft.warnings.some((w) => w.startsWith("far: layout:") && w.includes("80 m from the nearest other site"))) {
    throw new Error(`a site 80 m away only warns: ${soft.errors.join("; ")}`);
  }
  if (soft.errors.some((error) => error.includes("sign"))) throw new Error("requireSigns false: sign problems are warnings");
  const outskirts = checkTown(ROOT, { manifest: { ...manifest, sites: manifest.sites.map((site) => (site.id === "far" ? { ...site, outskirts: true } : site)) }, world, catalog, strings });
  if (outskirts.warnings.some((warning) => warning.startsWith("far:") && warning.includes("nearest other site"))) {
    throw new Error("an outskirts site is exempt from the neighbour rule");
  }
  if (!judgeTownRow({ entity: "Site_Far", actions: ["LF_Far"], look: "LF_Far@Site_Far", mesh: "ok", neighbourM: 80 }, manifest).ok) throw new Error("a play-mode row 80 m out only warns");
  if (!judgeTownRow({ entity: "Site_Far", actions: ["LF_Far"], look: "LF_Far@Site_Far", mesh: "ok", neighbourM: 80, outskirts: true }, manifest).ok) {
    throw new Error("an outskirts row passes at 80 m");
  }
  const pending = (id, text) => soft.warnings.some((warning) => warning.startsWith(`${id}: sign: pending`) && warning.includes(text));
  if (!pending("nosign", "no missing sign placed")) throw new Error(`missing sign must be sign: pending: ${soft.warnings.join("; ")}`);
  if (!soft.warnings.some((w) => w.startsWith("b: layout:") && w.includes("9 m away"))) throw new Error("a sign 9 m away is a layout warning");
  if (!pending("far", "Read card is blank")) throw new Error("a blank card must be reported");
  if (soft.warnings.some((warning) => warning.startsWith("a:"))) throw new Error("a sign 3 m away with a card passes");
  const strict = checkTown(ROOT, { manifest: { ...manifest, requireSigns: true }, world, catalog, strings });
  for (const [id, text] of [["nosign", "no missing sign placed"], ["far", "Read card is blank"]]) {
    if (!strict.errors.some((error) => error.startsWith(`${id}:`) && error.includes(text))) throw new Error(`requireSigns true must fail ${id}`);
  }
  if (strict.errors.some((error) => error.includes("m away"))) throw new Error("a far sign never fails, even with requireSigns");
  // A renamed town entity resolves by the actions it carries, with a warning (the maintainer renames by hand).
  const renamed = resolveSite({ id: "x", entity: "Site_Renamed", actions: ["LF_B"] }, world);
  if (!renamed.ok || renamed.name !== "Site_B" || !renamed.warnings?.[0]?.includes("resolved by its actions")) throw new Error("a renamed site resolves by its actions");
  const gone = resolveSite({ id: "x", entity: "Site_Gone", actions: ["LF_Nobody"] }, world);
  if (gone.ok) throw new Error("a site whose actions nothing carries fails");
  const twice = resolveSite({ id: "x", entity: "Site_A", actions: [] }, [...world, world[0]]);
  if (twice.ok || !twice.errors[0].includes("placed 2 times")) throw new Error("a site must resolve to exactly one entity");
  const lacking = resolveSite({ id: "x", entity: "Site_A", actions: ["LF_Other"] }, world);
  if (lacking.ok || !lacking.errors[0].includes("does not carry LF_Other")) throw new Error("the layer must carry the expected action");

  const emptyLook = judgeTownRow({ entity: "Site_A", actions: ["LF_A"], look: "none", mesh: "ok", neighbourM: 3 }, manifest);
  if (emptyLook.ok || !emptyLook.failures.includes("look offers no interaction")) throw new Error("empty look must fail");
  const otherLook = judgeTownRow({ entity: "Site_A", actions: ["LF_A"], look: "LF_A@Site_B,SCR_LootDeadBodyAction", mesh: "ok", neighbourM: 3 }, manifest);
  if (otherLook.ok || !otherLook.failures.includes("look lacks LF_A")) throw new Error("an action on another entity is not this site");
  const goodLook = judgeTownRow({ entity: "Site_A", actions: ["LF_A"], look: "LF_A@Site_A", mesh: "ok", neighbourM: 3, signIssues: ["x"] }, manifest);
  if (!goodLook.ok || goodLook.warnings[0] !== "sign: pending (x)") throw new Error("pending sign is a warning while requireSigns is false");
  if (judgeTownRow({ entity: "Site_A", actions: ["LF_A"], look: "LF_A@Site_A", mesh: "ok", neighbourM: 3, signIssues: ["x"] }, { ...manifest, requireSigns: true }).ok) {
    throw new Error("requireSigns true fails a sign issue");
  }
  if (judgeTownRow({ entity: "Site_A", actions: ["LF_A"], look: "LF_A@Site_A", mesh: "white", neighbourM: 3 }, manifest).ok) throw new Error("a white site fails");
  if (!judgeTownRow({ entity: "Site_A", actions: ["LF_A"], look: "LF_A", mesh: "ok", neighbourM: 3 }, manifest).ok) throw new Error("an action on an unnamed prefab child of the site counts");
  const stateful = judgeTownRow({ entity: "Site_A", actions: ["LF_A"], look: "none", mesh: "ok", neighbourM: 3, when: "an on-duty officer" }, manifest);
  if (!stateful.ok || stateful.warnings[0] !== "look: LF_A needs an on-duty officer") throw new Error("a site whose actions need a player state warns instead of failing");
  if (!manifestErrors({ ...manifest, sites: [{ ...manifest.sites[0], when: "" }] }).some((e) => e.includes("when"))) throw new Error("an empty when is a manifest error");
  const liveBlank = signIssues(manifest.sites[0], world[0], world, catalog, strings, 8, { read: true, card: "blank" });
  if (!liveBlank.issues.some((issue) => issue.includes("shows blank in play"))) throw new Error("a card blank in play is a sign issue");

  const scratch = mkdtempSync(join(tmpdir(), "town-"));
  const png = (shade) => {
    const file = join(scratch, `shot-${shade}.png`);
    writeFileSync(file, encodePng(2, 2, Buffer.from([shade, 30, 90, 30, shade, 90, 90, 30, shade, 60, 60, shade])));
    return file;
  };
  let shade = 10;
  const logs = [];
  const used = [];
  const code = runTownReport({
    root: scratch,
    manifest,
    world: () => world,
    catalog,
    strings,
    stand: () => ({ status: "ok", detail: "stood" }),
    look: () => ({ status: "ok", detail: "LF_A@Site_A,LF_SignReadAction@Sign_A,LF_B@Site_B" }),
    ui: () => ({ status: "ok", detail: "CloseButton" }),
    use: (className, entityName) => {
      used.push(`${className}@${entityName}`);
      return { status: "ok" };
    },
    input: () => ({ status: "ok" }),
    captureWorld: () => ({ status: "ok", detail: "x" }),
    captureUi: () => ({ status: "ok", detail: "x" }),
    resolve: () => png((shade += 7) % 250),
    pause: () => {},
    log: (line) => logs.push(line),
  });
  if (code !== 1) throw new Error("the 80 m site and the empty look must fail the town report");
  if (!used.includes("LF_SignReadAction@Sign_A")) throw new Error(`report town must open Read on a sign within 8 m: ${used.join(",")}`);
  if (!logs.includes("pass a: ok") || !logs.some((line) => line.startsWith("fail far:") && line.includes("look lacks LF_Far"))) {
    throw new Error(logs.join(" | "));
  }
  if (!logs.some((line) => line.startsWith("warn nosign: sign: pending"))) throw new Error("missing sign is a warning in the run");
  const page = readFileSync(join(scratch, "agent-box", "reports", "town.html"), "utf8");
  if (!page.includes("<h1>Town</h1>") || !page.includes("shots/town-far.png") || !page.includes("sign: pending")) {
    throw new Error("town.html must put each picture beside its verdict and pending signs");
  }
}

// The one-line answer to `harness.mjs <word> ...` when <word> is not a harness command: feature verbs go through send.
export function unknownCommandHint(cmd, rest = []) {
  const words = rest.filter((a) => !a.startsWith("--")).join(" ");
  const send = `node agent-box/harness.mjs --client <n> send ${cmd}${words ? ` ${words}` : ""}`;
  const cli = cmd === "checkpoint" ? ` (or: tools\\cli checkpoint ${words || "<hour>"} [--client <n>])` : "";
  return `harness: '${cmd}' is not a harness command. A feature verb goes through send: ${send}${cli}. 'node agent-box/harness.mjs help' lists the commands.`;
}

function usage() {
  console.log(`agent-box harness

  node agent-box/harness.mjs setup
  node agent-box/harness.mjs status
  node agent-box/harness.mjs selftest
  node agent-box/harness.mjs launch
  node agent-box/harness.mjs play
  node agent-box/harness.mjs layers
  node agent-box/harness.mjs layer <EntityName> [ActionClass] [side]
  node agent-box/harness.mjs stand <EntityName> [side]
  node agent-box/harness.mjs look
  node agent-box/harness.mjs ui [text]
  node agent-box/harness.mjs wait-text <text> [--timeout ms]
  node agent-box/harness.mjs ui-shot <name>
  node agent-box/harness.mjs click <WidgetName> [index]
  node agent-box/harness.mjs type <FieldName> <Text>
  node agent-box/harness.mjs press F|WheelUp|WheelDown|F10
  node agent-box/harness.mjs use <ClassName> [EntityName]
  node agent-box/harness.mjs reload [config|dialogue|config,dialogue]
  node agent-box/harness.mjs chat [admin|global|local|ooc] <text>   (one chat line as this client, e.g. chat !handled 3)
  node agent-box/harness.mjs staffapprove <account uid>   (server, harness gate; the staff API still needs an administrator)
  node agent-box/harness.mjs quizclear   (server, LF_HARNESS build only: forget this client's rules-quiz pass)
  node agent-box/harness.mjs dialogue [caption <#key>|shown]
  node agent-box/harness.mjs input <MenuAction>
  node agent-box/harness.mjs open DebugMenu
  node agent-box/harness.mjs kit
  node agent-box/harness.mjs shot ui <name>
  node agent-box/harness.mjs shot world <name> [x y z]
  node agent-box/harness.mjs flow <name>
  node agent-box/harness.mjs chains [--run]
  node agent-box/harness.mjs report roadmap [EntityName...]
  node agent-box/harness.mjs town
  node agent-box/harness.mjs report town [site-id...]
  node agent-box/harness.mjs journey [step...] [--list]     (also: report journey)
  node agent-box/harness.mjs pillar script [--run]

Pillars
  script  Logic suite. --run refuses the main checkout (world-editor copy).
  world   Teleport the character to the camera offset and screenshot the player view.
  ui      Screenshot whatever menu is on screen. Open and navigate it first.
  kit     Photograph every UI kit state. Reuse a play session that is already up.

Play mode has to be running. Start it with: node agent-box/harness.mjs play
kit enters play only when a session is not already up.
`);
}

// Commands that never send to a client: no session to default to.
const NO_CLIENT_COMMANDS = new Set(["selftest", "status", "setup", "launch", "play", "town", "pillar", "layers"]);
const samePath = (a, b) => resolve(String(a)).replaceAll("\\", "/").toLowerCase() === resolve(String(b)).replaceAll("\\", "/").toLowerCase();

// A command with no --client, run while this checkout has a live session, goes to that session's client 1.
// Without it the command file of the default profile (the Workbench's) was written and nothing read it: every step
// of `flow` waited out its timeout and the run printed nothing for 10 minutes (2026-11-27, twice), while the
// session, which saw no command, was closed by the pool as idle. Pure: `session` is the recorded session.json,
// `alive(pid)` says whether a process runs. Returns { argv, note }: argv with `--client 1` in front when it applies,
// note a line for stderr when the session has more than one client ("" otherwise).
export function defaultClientArgs(argv, session, root, alive, env = process.env) {
  if (argv.includes("--client") || env.LF_HARNESS_NO_DEFAULT_CLIENT === "1") return { argv, note: "" };
  const first = argv.find((a) => !a.startsWith("--"));
  if (!first || NO_CLIENT_COMMANDS.has(first) || (first === "chains" && !argv.includes("--run"))) return { argv, note: "" };
  if (!session || session.stoppedAt || !session.id) return { argv, note: "" };
  if (!samePath(session.root ?? "", root)) return { argv, note: "" };
  const pids = [session.serverPid, ...(session.clients ?? []).map((c) => c.pid)];
  if (!pids.some((p) => p && alive(p))) return { argv, note: "" };
  const n = (session.clients ?? []).length || Number(session.expectClients) || 0;
  if (n < 1) return { argv, note: "" };
  const note = n > 1 ? `harness: this session has ${n} clients and no --client was given; using client 1 (pass --client <n> for another)` : "";
  return { argv: ["--client", "1", ...argv], note };
}

function pidAlive(pid) {
  try {
    process.kill(pid, 0);
    return true;
  } catch (e) {
    return e.code === "EPERM";
  }
}

// Pulls --client <n> out of argv and points the command files at that
// session client (agent-box/session.mjs).
export async function applyClientFlag(argv) {
  if (!argv.includes("--client")) {
    const { readSession, sessionDir } = await import("./session.mjs");
    const dflt = defaultClientArgs(argv, readSession(sessionDir()), ROOT, pidAlive);
    if (dflt.note) console.error(dflt.note);
    argv = dflt.argv;
  }
  const i = argv.indexOf("--client");
  if (i === -1) return argv;
  const n = Number(argv[i + 1]);
  if (!Number.isInteger(n) || n < 1) throw new Error("--client needs a client number");
  const { clientBox, sessionDir } = await import("./session.mjs");
  // The session id comes from session.json at send time (addressFor).
  setBoxes([clientBox(sessionDir(), n)]);
  return [...argv.slice(0, i), ...argv.slice(i + 2)];
}

function main(argv) {
  const [cmd, ...rest] = argv;
  try {
    if (cmd === "setup") return setup();
    if (cmd === "status") {
      console.log(statusText());
      return 0;
    }
    if (cmd === "selftest") return selftest();
    if (cmd === "launch") return launchWorkbench();
    if (cmd === "play") return enterPlay();
    if (cmd === "kit") {
      const played = runKit();
      for (const line of played.lines) console.log(line);
      return played.code;
    }
    if (cmd === "chains") {
      if (!rest.includes("--run")) {
        const check = checkChains(ROOT);
        console.log(formatPlan());
        if (!check.ok) {
          for (const error of check.errors) console.error(error);
          return 1;
        }
        console.log("chains ok (static). Pass --run in a play session to stand, look, and use.");
        return 0;
      }
      const played = runChainProbes({
        send: (spec) => sendCommand(spec),
        pause,
        onLine: (line) => console.log(line),
      });
      return played.code;
    }
    if (cmd === "layers") {
      console.log(formatPads());
      return 0;
    }
    if (cmd === "layer") {
      const played = playLayer(rest[0], rest[1], rest[2]);
      for (const line of played.lines) console.log(line);
      return played.code;
    }
    if (cmd === "stand") {
      return printResult(sendCommand({ verb: "stand", entityName: rest[0], side: rest[1] }));
    }
    if (cmd === "look") {
      return printResult(sendCommand({ verb: "look" }));
    }
    if (cmd === "pickup") {
      return printResult(sendCommand({ verb: "pickup", item: rest[0] }, 15000));
    }
    if (cmd === "scene" || cmd === "camera") {
      // scene start <name> [hour|-] [weather] | stop | reset | status; camera orbit|follow x y z size seconds (LF_FilmDirector, LF_FilmCamera)
      return printResult(sendCommand({ verb: cmd, args: rest.join(" ") }));
    }
    if (cmd === "state") {
      return printResult(sendCommand({ verb: "state" }, 10000));
    }
    if (cmd === "die") {
      return printResult(sendCommand({ verb: "die" }, 10000));
    }
    if (cmd === "resetcrime") {
      return printResult(sendCommand({ verb: "resetcrime" }, 10000));
    }
    if (cmd === "resetplayer") {
      // `resetplayer [cash] [keep]`: `keep` leaves the inventory, which is emptied (civilian kit issued) by default.
      const keepInventory = rest.includes("keep");
      const cashArg = rest.find((a) => a !== "keep");
      return printResult(sendCommand({ verb: "resetplayer", ...(cashArg === undefined ? {} : { cash: Number(cashArg) }), ...(keepInventory ? { keepInventory: true } : {}) }, 10000));
    }
    if (cmd === "giveup") {
      return printResult(sendCommand({ verb: "giveup" }, 10000));
    }
    if (cmd === "spikeaim") {
      return printResult(sendCommand({ verb: "spikeaim", arg: rest[0], value: rest[1] }, 10000));
    }
    if (cmd === "dialogue") {
      return printResult(sendCommand({ verb: "dialogue", arg: rest[0], value: rest[1] }, 15000));
    }
    if (cmd === "gm") {
      return printResult(sendCommand({ verb: "gm", arg: rest[0] }, 15000));
    }
    if (cmd === "scanner" || cmd === "drill" || cmd === "keycard" || cmd === "charge" || cmd === "apples" || cmd === "nuggets" || cmd === "crate" || cmd === "parceldrop" || cmd === "pistol" || cmd === "moonshine" || cmd === "stash" || cmd === "park" || cmd === "shelf" || cmd === "keys" || cmd === "spikeverge" || cmd === "spikedrop" || cmd === "spikecar" || cmd === "spikecheck" || cmd === "slopecar" || cmd === "slopecheck" || cmd === "slopedrive" || cmd === "spikeseen" || cmd === "spikepreview" || cmd === "spikepreviewoff" || cmd === "spikeplace" || cmd === "spikewhere" || cmd === "animhold" || cmd === "animoff" || cmd === "animseen" || cmd === "quizclear") {
      return printResult(sendCommand({ verb: cmd }, 10000));
    }
    if (cmd === "chat") {
      // chat [admin|global|local|ooc] <text...>
      const named = /^(admin|global|local|ooc)$/.test(rest[0] ?? "") && rest.length > 1;
      return printResult(sendCommand({ verb: "chat", channel: named ? rest[0] : undefined, text: rest.slice(named ? 1 : 0).join(" ") }, 10000));
    }
    if (cmd === "staffapprove") {
      return printResult(sendCommand({ verb: "staffapprove", uid: rest[0] }, 10000));
    }
    if (cmd === "engine") {
      return printResult(sendCommand({ verb: "engine", entityName: rest[0] }, 10000));
    }
    if (cmd === "lockpick") {
      return printResult(sendCommand({ verb: "lockpick" }, 10000));
    }
    if (cmd === "phone" || cmd === "radio" || cmd === "pager" || cmd === "report" || cmd === "robhold") {
      return printResult(sendCommand({ verb: cmd }, 10000));
    }
    if (cmd === "peers") {
      return printResult(sendCommand({ verb: "peers" }, 10000));
    }
    if (cmd === "seen") {
      return printResult(sendCommand({ verb: "seen", radius: rest[0] }, 10000));
    }
    if (cmd === "perf") {
      const seconds = Number(rest[0] ?? 10);
      return printResult(sendCommand({ verb: "perf", seconds, label: rest[1] }, (seconds + 15) * 1000));
    }
    if (cmd === "shoot") {
      return printResult(sendCommand({ verb: "shoot", playerId: rest[0] }, 10000));
    }
    if (cmd === "shootnpc") {
      return printResult(sendCommand({ verb: "shootnpc", entityName: rest[0] }, 10000));
    }
    if (cmd === "cpr") {
      return printResult(sendCommand({ verb: "cpr", playerId: rest[0], mode: rest[1] }, 10000));
    }
    if (cmd === "cuffing") {
      return printResult(sendCommand({ verb: "cuffing", playerId: rest[0], kind: rest[1], mode: rest[2] }, 10000));
    }
    if (cmd === "ui") {
      if (rest[0] !== "text" && rest[0] !== "--text") return printResult(sendCommand({ verb: "ui" }));
      const result = sendCommand({ verb: "ui", mode: "text" });
      if (result.status !== "ok") return printResult(result);
      console.log(formatUiText(parseUiText(result.detail)));
      return 0;
    }
    if (cmd === "wait-text") {
      const ti = rest.indexOf("--timeout");
      const timeout = ti === -1 ? 8000 : Number(rest[ti + 1]);
      const words = ti === -1 ? rest : [...rest.slice(0, ti), ...rest.slice(ti + 2)];
      if (!words.length) throw new Error("wait-text <text> [--timeout ms]");
      const result = waitForText(words.join(" "), timeout);
      console.log(result.found ? `found: ${words.join(" ")}` : `${result.status} ${result.detail}`);
      return result.found ? 0 : 1;
    }
    if (cmd === "ui-shot") {
      if (!rest[0]) throw new Error("ui-shot <name>");
      return printResult(sendCommand({ verb: "capture", mode: "ui", name: rest[0] }, 25000));
    }
    if (cmd === "click") {
      return printResult(sendCommand({ verb: "click", widget: rest[0], index: rest[1] }));
    }
    if (cmd === "type") {
      return printResult(sendCommand({ verb: "type", widget: rest[0], text: rest[1] }));
    }
    if (cmd === "press") {
      const spec = PRESS[rest[0]];
      if (!spec) throw new Error("press one of F, WheelUp, WheelDown, F10");
      return printResult(sendCommand(spec));
    }
    if (cmd === "use") {
      return printResult(sendCommand({ verb: "use", className: rest[0], entityName: rest[1] }));
    }
    if (cmd === "reload") {
      // reload [config|dialogue|config,dialogue]: the server reads config.json and the dev caption file again (#970).
      return printResult(sendCommand({ verb: "reload", what: rest[0] }, 10000));
    }
    if (cmd === "dialogue") {
      // dialogue [caption <#key> | shown | pick <row> | skip | ...]: the conversation scene on this client.
      return printResult(sendCommand({ verb: "dialogue", arg: rest[0], value: rest[1] }, 10000));
    }
    if (cmd === "input") {
      return printResult(sendCommand({ verb: "input", action: rest[0], value: rest[1] }));
    }
    if (cmd === "open") {
      return printResult(sendCommand({ verb: "open", menu: rest[0], mode: rest[1] }));
    }
    if (cmd === "shot" && rest[0] === "ui") {
      if (rest[1] === "debug_menu") {
        const opened = sendCommand({ verb: "open", menu: "DebugMenu", mode: "show" });
        if (opened.status !== "ok") return printResult(opened);
        pause(500);
      }
      return printResult(
        sendCommand({ verb: "capture", mode: "ui", name: rest[1] }, 25000),
      );
    }
    if (cmd === "shot" && rest[0] === "world") {
      const target = resolveWorldTarget(rest[1], rest[2], rest[3], rest[4]);
      return printResult(sendCommand(worldCapture(target, rest.slice(5, 8)), 25000));
    }
    if (cmd === "send" && /^[a-z][a-z0-9]*$/.test(rest[0] ?? "")) {
      // send <verb> [words]: a feature verb (a modded LF_AgentHarnessVerbs), its words passed as they are.
      return printResult(sendCommand({ verb: rest[0], words: rest.slice(1) }, 10000));
    }
    if (cmd === "flow") {
      const played = runFlow(readFlow(rest[0]), rest[0]);
      for (const line of played.lines) console.log(line);
      return played.code;
    }
    if (cmd === "town") {
      const result = checkTown(ROOT);
      console.log(formatTownCheck(result));
      console.log(`town ${result.ok ? "ok" : "FAILED"} (static): sites=${result.rows.length} errors=${result.errors.length} warnings=${result.warnings.length} requireSigns=${TOWN.requireSigns}`);
      return result.ok ? 0 : 1;
    }
    if (cmd === "journey" || (cmd === "report" && rest[0] === "journey")) {
      // agent-box/journey.mjs: the M1 journey on a running session, report in agent-box/reports/journey.html.
      const args = cmd === "journey" ? rest : rest.slice(1);
      const r = spawnSync(process.execPath, [join(ROOT, "agent-box", "journey.mjs"), ...args], { windowsHide: true, stdio: "inherit" });
      return r.status ?? 1;
    }
    if (cmd === "report" && rest[0] === "town") {
      return runTownReport({
        root: ROOT,
        manifest: TOWN,
        ids: rest.slice(1).filter((arg) => !arg.startsWith("--")),
        world: () => loadWorld(ROOT),
        catalog: loadSignCatalog(ROOT),
        strings: loadStrings(ROOT),
        stand: (entityName) => sendCommand({ verb: "stand", entityName }),
        look: () => sendCommand({ verb: "look" }),
        ui: () => sendCommand({ verb: "ui" }),
        use: (className, entityName) => sendCommand({ verb: "use", className, entityName }),
        input: (action) => sendCommand({ verb: "input", action }),
        captureWorld: (spec) => settleCapture(sendCommand({ verb: "capture", mode: "world", ...spec }, 25000)),
        captureUi: (name) => settleCapture(sendCommand({ verb: "capture", mode: "ui", name }, 25000)),
        resolve: (result) => resolveCapture(result.detail),
        pause,
        log: (line) => console.log(line),
      });
    }
    if (cmd === "report") {
      if (rest[0] !== "roadmap") throw new Error("report roadmap|town|journey");
      const names = [];
      for (let i = 1; i < rest.length; i++) {
        if (rest[i] === "--name") {
          i++;
          if (rest[i]) names.push(rest[i]);
        } else if (!rest[i].startsWith("--")) {
          names.push(rest[i]);
        }
      }
      return runRoadmapReport({
        root: ROOT,
        names,
        sites: () => roadmapSites(ROOT),
        stand: (entityName) => sendCommand({ verb: "stand", entityName }),
        look: () => sendCommand({ verb: "look" }),
        ui: () => sendCommand({ verb: "ui" }),
        use: (className, entityName) => sendCommand({ verb: "use", className, entityName }),
        input: (action) => sendCommand({ verb: "input", action }),
        captureWorld: (spec) => settleCapture(sendCommand({ verb: "capture", mode: "world", ...spec }, 25000)),
        captureUi: (name) => settleCapture(sendCommand({ verb: "capture", mode: "ui", name }, 25000)),
        resolve: (result) => resolveCapture(result.detail),
        pause,
        log: (line) => console.log(line),
      });
    }
    if (cmd === "pillar" && rest[0] === "script") {
      const result = runScriptPillar(rest.includes("--run"));
      if (result.text) process.stdout.write(result.text.endsWith("\n") ? result.text : result.text + "\n");
      return result.code;
    }
    if (cmd && /^[a-z][a-z0-9]*$/.test(cmd)) {
      // A feature verb typed as a command (`harness.mjs --client 1 checkpoint 5`) used to print the whole usage, whose last
      // line "Play mode has to be running" sent agents looking for a dead session (2026-11-25 round, tooling note 1).
      console.log(unknownCommandHint(cmd, rest));
      return 1;
    }
    usage();
    return cmd ? 1 : 0;
  } catch (err) {
    console.error(err.message);
    return 1;
  }
}

// A session that `up --machine any` put on a pool machine has no command files on this PC:
// run the same command there (session.mjs run) against client 1, or the one named by --client.
// --local keeps it on this PC. selftest/status/setup never relay.
async function relayIfRemote(argv) {
  if (argv.includes("--local")) return { argv: argv.filter((a) => a !== "--local"), code: null };
  const first = argv.find((a) => !a.startsWith("--"));
  if (!first || ["selftest", "status", "setup", "town", "pillar", "report"].includes(first)) return { argv, code: null };
  const { readRemotePointer } = await import("./session.mjs");
  const ptr = readRemotePointer();
  if (!ptr) return { argv, code: null };
  const args = argv.includes("--client") ? argv : [...argv, "--client", "1"];
  const r = spawnSync(process.execPath, [join(ROOT, "agent-box", "session.mjs"), "run", "agent-box/harness.mjs", ...args, "--local"], { stdio: "inherit", windowsHide: true, cwd: ROOT });
  if (first === "shot" || first === "flow") console.log(`(the images are on ${ptr.machine}: node agent-box/session.mjs pull)`);
  return { argv, code: r.status ?? 1 };
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  relayIfRemote(process.argv.slice(2)).then(async (relay) => (relay.code !== null ? relay.code : main(await applyClientFlag(relay.argv)))).then(
    (code) => process.exit(code),
    (err) => {
      console.error(err.message);
      process.exit(1);
    },
  );
}
