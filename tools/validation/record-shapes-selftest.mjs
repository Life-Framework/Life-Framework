// tools/validation/record-shapes-selftest.mjs
// inputs: tools/validation/validate-record-shapes.mjs, tools/validation/record-shapes-selftest.mjs
//
// Proves validate-record-shapes enforces the append-only rule (#761). A fixture
// serializer writes two payloads; the lock is built from the clean sources.
// Each case changes one record and asserts the check passes or names the
// violation, and whether --update may record it.
// red-proof: with the retype branch in compare() (validate-record-shapes.mjs)
// made to `continue` instead of reporting, this printed
// "FAIL retyped field: expected a hard error matching /retyped/, got none"
// and exited 1 (2026-11-14).

import { buildLock, compare, readCurrent } from "./validate-record-shapes.mjs";

const SERIALIZER = (payloads, version) => `
class LF_PersistenceComponentSerializer
{
	protected static void RegisterPersistencePayloads(notnull array<string> order)
	{
${payloads.map((p) => `		order.Insert("${p}");`).join("\n")}
	}

	static ESerializeResult SerializeStore(LF_PersistenceComponent persistence, SaveContext context)
	{
		context.WriteValue("version", ${version});
	}

	protected static void WritePrefix(notnull SaveContext context)
	{
		array<ref LF_AccountRecord> accountRecords = LF_Accounts.ExportAll();
		context.Write(accountRecords);
	}

	static void WritePersistencePayload(LF_PersistenceComponent persistence, SaveContext context, string payload, int saveVersion = 0)
	{
		if (payload == "garage")
		{
			array<ref LF_GarageRecord> garageRows = LF_Garage.ExportAll();
			if (!garageRows)
				garageRows = new array<ref LF_GarageRecord>();
			context.Write(garageRows);
		}
${payloads.includes("tow") ? `		else if (payload == "tow")
		{
			array<ref LF_TowRow> towRows = LF_Tow.ExportAll();
			context.Write(towRows);
		}` : ""}
	}
}
`;

const ACCOUNT = `
class LF_RecordBase : Managed
{
	int m_iVersion;
}

class LF_AccountRecord : LF_RecordBase
{
	string m_sId;
	ref array<ref LF_CharacterRecord> m_aCharacters = {};
	static const int MAX = 3;

	void Copy(LF_AccountRecord other)
	{
		m_sId = other.m_sId;
	}
}

class LF_CharacterRecord
{
	string m_sName;
	int m_iAge;
}
`;

const GARAGE = `
//! A garage slot. "{ not a brace }"
class LF_GarageRecord : Managed
{
	protected string m_sVehicleId;
	bool m_bStored;
	[NonSerialized()]
	int m_iLiveOnly;
	float m_fFuel, m_fHealth;

	bool IsStored()
	{
		if (m_bStored) { return true; }
		return false;
	}
}
`;

const TOW = `
class LF_TowRow : Managed
{
	string m_sVehicleId;
	int m_iState;
}
`;

function current({ account = ACCOUNT, garage = GARAGE, payloads = ["garage"], version = 1 } = {}) {
  const files = new Map([
    ["addons/X/Scripts/Game/LF_Account.c", account],
    ["addons/X/Scripts/Game/LF_Garage.c", garage],
    ["addons/X/Scripts/Game/LF_Tow.c", TOW],
  ]);
  return readCurrent(files, SERIALIZER(payloads, version));
}

const clean = current();
const lock = buildLock(clean, null);
let failed = 0;

function expect(label, cur, { hard = null, soft = null, pass = false, theLock = lock }) {
  const errors = compare(theLock, cur);
  const hardErrors = errors.filter((e) => e.hard);
  const softErrors = errors.filter((e) => !e.hard);
  let problem = null;
  if (pass && errors.length) problem = `expected a pass, got ${errors.map((e) => e.msg).join(" | ")}`;
  if (hard && !hardErrors.some((e) => hard.test(e.msg))) {
    problem = `expected a hard error matching ${hard}, got ${hardErrors.length ? hardErrors.map((e) => e.msg).join(" | ") : "none"}`;
  }
  if (soft && (hardErrors.length || !softErrors.some((e) => soft.test(e.msg)))) {
    problem = `expected only an --update error matching ${soft}, got ${errors.length ? errors.map((e) => (e.hard ? "HARD " : "") + e.msg).join(" | ") : "none"}`;
  }
  if (problem) {
    failed++;
    console.log(`FAIL ${label}: ${problem}`);
  }
}

// The parser itself: base class first, nested record, multi-declarators,
// [NonSerialized()], static const and methods skipped.
const want = {
  LF_AccountRecord: ["int m_iVersion", "string m_sId", "array<ref LF_CharacterRecord> m_aCharacters"],
  LF_CharacterRecord: ["string m_sName", "int m_iAge"],
  LF_GarageRecord: ["string m_sVehicleId", "bool m_bStored", "float m_fFuel", "float m_fHealth"],
};
for (const [name, fields] of Object.entries(want)) {
  const got = clean.shapes.get(name) ?? [];
  if (got.join("|") !== fields.join("|")) {
    failed++;
    console.log(`FAIL parse ${name}: expected ${fields.join(", ")}, got ${got.join(", ")}`);
  }
}
if (lock.records.LF_GarageRecord?.since !== 1 || lock.records.LF_CharacterRecord?.since !== 1) {
  failed++;
  console.log(`FAIL since: expected garage and the nested character record at version 1, got ${JSON.stringify(lock.records)}`);
}

expect("clean sources", clean, { pass: true });
expect("reordered field", current({ garage: GARAGE.replace("protected string m_sVehicleId;\n\tbool m_bStored;", "bool m_bStored;\n\tprotected string m_sVehicleId;") }), { hard: /moved from position 1/ });
expect("removed field", current({ garage: GARAGE.replace("\tbool m_bStored;\n", "") }), { hard: /removed/ });
expect("retyped field", current({ garage: GARAGE.replace("bool m_bStored;", "int m_bStored;") }), { hard: /retyped/ });
expect("retyped field in a multi-declaration", current({ garage: GARAGE.replace("float m_fFuel, m_fHealth;", "float m_fFuel;\n\tint m_fHealth;") }), { hard: /retyped/ });
expect("renamed field", current({ garage: GARAGE.replace("m_bStored;", "m_bParked;") }), { hard: /removed or renamed/ });
expect("nested record field reordered", current({ account: ACCOUNT.replace("string m_sName;\n\tint m_iAge;", "int m_iAge;\n\tstring m_sName;") }), { hard: /LF_CharacterRecord: field "string m_sName" moved/ });
expect("base class field retyped", current({ account: ACCOUNT.replace("int m_iVersion;", "float m_iVersion;") }), { hard: /LF_AccountRecord: field 1 .* retyped/ });
expect("payload record class swapped", current({ garage: GARAGE.replaceAll("LF_GarageRecord", "LF_GarageRecord2") }), { hard: /no longer written|writes LF_GarageRecord2/ });

const appended = GARAGE.replace("float m_fFuel, m_fHealth;", "float m_fFuel, m_fHealth;\n\tint m_iTowState;");
expect("field appended without a version bump", current({ garage: appended }), { hard: /appended without a save version bump/ });
expect("field appended with a version bump", current({ garage: appended, version: 2 }), { soft: /appended at save version 2.*--update/ });
expect("field appended to a launched record", current({ garage: appended, version: 2 }), { hard: /launched record/, theLock: { ...lock, launched: true } });
const bumpedLock = buildLock(current({ garage: appended, version: 2 }), lock);
expect("field appended with a bump, lock refreshed", current({ garage: appended, version: 2 }), { pass: true, theLock: bumpedLock });
if (bumpedLock.records.LF_GarageRecord.since !== 1) {
  failed++;
  console.log(`FAIL --update kept since: expected 1, got ${bumpedLock.records.LF_GarageRecord.since}`);
}
expect("field appended after the lock moved on, no new bump", current({ garage: appended.replace("int m_iTowState;", "int m_iTowState;\n\tint m_iTowedAt;"), version: 2 }), { hard: /appended without a save version bump/, theLock: bumpedLock });

expect("[NonSerialized()] field added (sidecar pattern)", current({ garage: GARAGE.replace("float m_fFuel, m_fHealth;", "float m_fFuel, m_fHealth;\n\t[NonSerialized()]\n\tint m_iTowState;") }), { pass: true });
expect("sidecar payload without a version bump", current({ payloads: ["garage", "tow"] }), { hard: /new (record class LF_TowRow|payload "tow") is written without a save version bump/ });
expect("sidecar payload with a version bump", current({ payloads: ["garage", "tow"], version: 2 }), { soft: /new record class LF_TowRow at save version 2/ });
expect("version bumped, lock not refreshed", current({ version: 2 }), { soft: /save version moved 1 -> 2/ });
expect("version lowered", current({ version: 0 }), { hard: /save version went down/, theLock: { ...lock, saveVersion: 1 } });

if (failed) {
  console.log(`record-shapes selftest: ${failed} case(s) failed`);
  process.exit(1);
}
console.log("record-shapes selftest: every planted change is reported");
