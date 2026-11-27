// A prefab with user actions needs an RplComponent, or the actions only work for a host (#5110).
// inputs: addons/**/Prefabs/**, addons/**/Scripts/**/*.c, tools/lib/rpl-actions.mjs
//
// The engine sends a performed user action to the server over the entity's replication. A prefab whose
// root has an ActionsManagerComponent with actions and no RplComponent on its addon ancestry drops every
// request ("Attempting to send an RPC through unregistred item", docs/architecture/rpc-routing.md): the
// action works for a host and does nothing for a client on a dedicated server. A prefab that really is a
// local mesh (the plant stage meshes, #3177) names action classes that override
// `HasLocalEffectOnlyScript()` and send their request some other way; those are not counted.
// A parent outside the addons is a base-game prefab (CI cannot read it) and is assumed to carry what the
// child inherits.
//
// LF_RPLACTIONS_ROOT points the scan at another tree (the selftest uses a scratch one).
//
// red-proof: tools/validation/actions-have-rpl-selftest.mjs (a rootless prefab with a vanilla action
// fails; the same prefab with an RplComponent, with a local-only action class, or under a vanilla parent
// passes). Observed 2026-11-27 on the tree before the fix: five FAIL lines (LF_JailKitchenSink, LF_JailLaundryTub,
// LF_JailYardBroom with LF_JailWorkAction, LF_HospitalBed with LF_BedDeliveryAction, LF_FellHelper with
// LF_FellAction) and exit 1; after the fix it prints OK.
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { findUnreachableActions, localOnlyActionClasses, readPrefabs } from "../lib/rpl-actions.mjs";

const root = process.env.LF_RPLACTIONS_ROOT || join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const prefabs = readPrefabs(root);
const bad = findUnreachableActions(prefabs, localOnlyActionClasses(root));

for (const b of bad) {
  console.log(`FAIL ${b.path}: has user actions (${b.actions.join(", ")}) and no RplComponent on its prefab chain, so a client's action is dropped; add the RplComponent (inherit the base's GUID), or make the action class override HasLocalEffectOnlyScript if it runs on the acting machine only`);
}
if (bad.length) {
  console.log(`validate-actions-have-rpl: FAILED (${bad.length} prefab(s) of ${prefabs.length})`);
  process.exit(1);
}
console.log(`validate-actions-have-rpl: OK (${prefabs.length} prefab(s) checked)`);
