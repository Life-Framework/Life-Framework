// red-proof: observed 2026-11-18 `tools/cli test --no-build --red LF_Test_GarageBuySlot,LF_Test_GarageSlotsPersist`: RED PROOF OK, first `one bought slot is back (expected 1, got 0)`. Before the fix the JSON key `slotRows` collided with the characters payload and the same line failed; green with `--only`. Restart: `tools/cli wt e2e slice-2656`.
// tier: PERSISTENCE
//! Bought garage slots ride the real save path (#2656, the "garageslots" payload, save version 55):
//! rows are written per character, an emptied store reads the same counts back, loading twice gives
//! the same store, a count past the config's max clamps, and a save from before the payload loads
//! every character with 0 bought slots. The restart itself is `tools\cli wt e2e` (SeedE2E / VerifyE2E).
[LF_TestInfo(LF_TestTier.PERSISTENCE)]
class LF_Test_GarageSlotsPersist : LF_Test
{
	static const string ONE = "test-garage-slots-one";
	static const string THREE = "test-garage-slots-three";
	//! The e2e character (LF_Test_E2E_SaveState pass 1 seeds it, pass 2 reads it after the restart).
	static const string E2E_KEY = "e2e-garage-slots";

	override string GetName()
	{
		return "vehicles/garage-slots-persist";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		LF_PersistenceComponent persistence;
		if (gameMode)
			persistence = LF_PersistenceComponent.Cast(gameMode.FindComponent(LF_PersistenceComponent));

		ctx.NotNull(persistence, "the game mode carries LF_PersistenceComponent");
		if (!persistence)
			return;

		array<ref LF_GarageSlotRow> before = LF_GarageSlotStore.ExportAll();
		array<ref LF_GarageSlotRow> seed = {};
		seed.Insert(LF_GarageSlotRow.Create(ONE, 1));
		seed.Insert(LF_GarageSlotRow.Create(THREE, 3));
		ctx.Equal(2, LF_GarageSlotStore.ApplyAll(seed), "two characters hold bought slots before the save");

		JsonSaveContext saveContext = new JsonSaveContext(false);
		ctx.True(LF_PersistenceComponentSerializer.SerializeStore(persistence, saveContext) == ESerializeResult.OK, "the store serializes");
		string saved = saveContext.SaveToString();
		ctx.True(saved.Contains(THREE), "the save carries the character key");

		LF_GarageSlotStore.ApplyAll(null);
		ctx.Equal(0, LF_GarageSlotStore.Bought(THREE), "the restarted session has an empty store");

		JsonLoadContext loadContext = new JsonLoadContext(false);
		ctx.True(loadContext.LoadFromString(saved), "the save parses");
		int version;
		loadContext.ReadValue("version", version);
		ctx.Equal(LF_TestSaveVersion.CURRENT, version, "the save is written at the current version");
		ctx.Equal(55, LF_PersistenceComponentSerializer.PayloadSinceVersion("garageslots"), "garageslots is payload 55");
		// RedProof(): the load is skipped, so every read-back assertion fails (`--red`).
		if (!RedProof())
			LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, loadContext, "garageslots");

		ctx.Equal(1, LF_GarageSlotStore.Bought(ONE), "one bought slot is back");
		ctx.Equal(3, LF_GarageSlotStore.Bought(THREE), "three bought slots are back");
		ctx.Equal(LF_GarageConfig.BaseSlots() + 3, LF_GarageSlots.CapOf(THREE), "the cap after the load counts the bought slots");

		JsonLoadContext again = new JsonLoadContext(false);
		again.LoadFromString(saved);
		LF_PersistenceComponentSerializer.ReadPersistencePayload(persistence, again, "garageslots");
		ctx.Equal(3, LF_GarageSlotStore.Bought(THREE), "loading twice does not add slots");
		ctx.Equal(2, LF_GarageSlotStore.ExportAll().Count(), "loading twice gives the same store");

		// Bad rows: no key and 0 slots are skipped; a count past the max clamps.
		array<ref LF_GarageSlotRow> rows = {};
		rows.Insert(LF_GarageSlotRow.Create("", 2));
		rows.Insert(LF_GarageSlotRow.Create(ONE, 0));
		rows.Insert(LF_GarageSlotRow.Create(THREE, 99));
		ctx.Equal(1, LF_GarageSlotStore.ApplyAll(rows), "a row with no key or no slot is skipped");
		ctx.Equal(LF_GarageConfig.MaxSlots() - LF_GarageConfig.BaseSlots(), LF_GarageSlotStore.Bought(THREE), "a count past the max clamps on load");
		ctx.Equal(LF_GarageConfig.MaxSlots(), LF_GarageSlots.CapOf(THREE), "the clamped cap is the max");

		// A save from before this payload: a restarted session loads every character with 0 bought slots.
		LF_GarageSlotStore.ApplyAll(seed);
		JsonSaveContext oldContext = new JsonSaveContext(false);
		LF_PersistenceComponentSerializer.SerializeStoreAsVersion(persistence, oldContext, 54);
		string oldSave = oldContext.SaveToString();
		ctx.False(oldSave.Contains(THREE), "the version 54 save has no slot rows");
		LF_GarageSlotStore.ApplyAll(null);
		JsonLoadContext oldLoad = new JsonLoadContext(false);
		ctx.True(oldLoad.LoadFromString(oldSave), "the old save parses");
		LF_PersistenceComponentSerializer.DeserializeStore(persistence, oldLoad);
		ctx.Equal(0, LF_GarageSlotStore.Bought(THREE), "a pre-payload save loads with 0 bought slots");
		ctx.Equal(LF_GarageConfig.BaseSlots(), LF_GarageSlots.CapOf(THREE), "a pre-payload save leaves the base cap");

		LF_GarageSlotStore.ApplyAll(before);
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e save boot: one character holds two bought slots.
	static void SeedE2E(LF_TestContext ctx)
	{
		array<ref LF_GarageSlotRow> rows = LF_GarageSlotStore.ExportAll();
		rows.Insert(LF_GarageSlotRow.Create(E2E_KEY, 2));
		LF_GarageSlotStore.ApplyAll(rows);
		ctx.Equal(2, LF_GarageSlotStore.Bought(E2E_KEY), "e2e character holds 2 bought garage slots before the save");
	}

	//------------------------------------------------------------------------------------------------
	//! wt e2e reload boot: the same bought slots after the restart.
	static void VerifyE2E(LF_TestContext ctx)
	{
		ctx.Equal(2, LF_GarageSlotStore.Bought(E2E_KEY), "e2e bought garage slots survive the restart");
		LF_Debug.Info(LF_GarageSlotStore.FEATURE, string.Format("e2e restart: %1 holds %2 bought slots, cap %3", E2E_KEY, LF_GarageSlotStore.Bought(E2E_KEY), LF_GarageSlots.CapOf(E2E_KEY)));
	}
}
