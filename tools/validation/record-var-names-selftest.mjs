// tools/validation/record-var-names-selftest.mjs
// inputs: tools/validation/validate-record-var-names.mjs, tools/validation/record-var-names-selftest.mjs
//
// Proves validate-record-var-names catches a save-record variable name shared by two payloads
// (#2799: 'slotRows' reused made every bought garage slot load as 0), a name written or read
// twice, and a name with no read. A clean fixture passes.
// red-proof: with the `payloads.length > 1` branch in check() (validate-record-var-names.mjs)
// set to `if (false)`, this printed "FAIL name shared by two payloads: expected an error matching
// /slotRows.*2 payloads (characters, garageslots)/, got record variable 'slotRows' in payload
// 'characters' has 2 write(s) and 2 read(s) ..." and exited 1 (2026-11-18).

import { check, collectSites } from "./validate-record-var-names.mjs";

// slotRows is the second payload's variable unless `sharedName` says to reuse the first's.
const SERIALIZER = ({ secondWriteName = "garageRows", secondReadName = "garageRows", extraWrite = "", extraRead = "", dropRead = false } = {}) => `
class LF_PersistenceComponentSerializer
{
	protected static void WritePrefix(notnull SaveContext context)
	{
		array<ref LF_AccountRecord> accountRecords = LF_Accounts.ExportAll();
		context.Write(accountRecords);
	}

	static void WritePersistencePayload(LF_PersistenceComponent persistence, SaveContext context, string payload, int saveVersion = 0)
	{
		if (payload == "characters")
		{
			array<ref LF_SlotRow> slotRows = LF_Slots.ExportAll();
			context.Write(slotRows);${extraWrite}
		}
		else if (payload == "garageslots")
		{
			array<ref LF_GarageSlotRow> ${secondWriteName} = LF_Garage.ExportAll();
			// context.Write(slotRows) in a comment is not a site
			context.Write(${secondWriteName});
		}
	}

	static bool DeserializeStore(LF_PersistenceComponent persistence, LoadContext context)
	{
		array<ref LF_AccountRecord> accountRecords = new array<ref LF_AccountRecord>();
		context.Read(accountRecords);
		return true;
	}

	static void ReadPersistencePayload(LF_PersistenceComponent persistence, LoadContext context, string payload)
	{
		if (payload == "characters")
		{
			array<ref LF_SlotRow> slotRows = new array<ref LF_SlotRow>();
			if (context.Read(slotRows))
			{
			}${extraRead}
		}
		else if (payload == "garageslots")
		{
			array<ref LF_GarageSlotRow> ${secondReadName} = new array<ref LF_GarageSlotRow>();
${dropRead ? "" : `			if (context.Read(${secondReadName}))
			{
			}`}
		}
	}
}
`;

let failed = 0;
function expect(label, source, pattern) {
  const errors = check(collectSites(source));
  const ok = pattern ? errors.some((e) => pattern.test(e)) : errors.length === 0;
  if (!ok) {
    failed++;
    const want = pattern ? `an error matching ${pattern}` : "no errors";
    console.log(`FAIL ${label}: expected ${want}, got ${errors.length ? errors.join(" | ") : "none"}`);
  }
}

expect("clean fixture", SERIALIZER());
expect("name shared by two payloads", SERIALIZER({ secondWriteName: "slotRows", secondReadName: "slotRows" }), /slotRows.*2 payloads \(characters, garageslots\)/);
expect("shared name only on the read side", SERIALIZER({ secondReadName: "slotRows" }), /slotRows.*2 payloads/);
expect("written twice in one payload", SERIALIZER({ extraWrite: "\n\t\t\tcontext.Write(slotRows);" }), /slotRows.*2 write\(s\) and 1 read/);
expect("read twice in one payload", SERIALIZER({ extraRead: "\n\t\t\tcontext.Read(slotRows);" }), /slotRows.*1 write\(s\) and 2 read/);
expect("written but never read", SERIALIZER({ dropRead: true }), /garageRows.*1 write\(s\) and 0 read/);
expect("prefix name reused by a payload", SERIALIZER({ secondWriteName: "accountRecords", secondReadName: "accountRecords" }), /accountRecords.*prefix/);

if (failed) {
  console.log(`record-var-names selftest: ${failed} case(s) failed`);
  process.exit(1);
}
console.log("record-var-names selftest: 7 cases ok");
