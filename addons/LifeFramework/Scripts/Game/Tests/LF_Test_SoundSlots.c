// red-proof: observed 2026-11-14 with `toolscli test --no-build --only LF_Test_SoundSlots`: making LF_SoundSlots.Resolve keep the core default instead of the pack project failed `- pack: the store radio is filled with a project that loads (expected '{FD5E9674E51B9D13}Sounds/Props/Military/Radios/Props_Radios_Signals.acp', got '')`. Restored; green.

//------------------------------------------------------------------------------------------------
//! A stand-in pack source that overlays the sound slots core file with a test fixture.
class LF_Test_SoundSlotsPackStub : LF_ConfigPackSource
{
	static const ResourceName OVERLAY = "{D27BB673CB82D568}Configs/Tests/LF_Test_SoundSlotsOverlay.conf";

	override void GetOverlays(ResourceName coreConf, notnull array<ResourceName> overlays)
	{
		if (coreConf == LF_SoundSlotsConfig.CONFIG_PATH)
			overlays.Insert(OVERLAY);
	}

	override string GetName()
	{
		return "sound-slots-test-stub";
	}
}

//------------------------------------------------------------------------------------------------
//! Sound slots (#323) on the real core file. Core alone: the town ambience slots have no core sound
//! (they moved to the flagship pack in batch 4), so each is empty and silent, never an error. With the
//! fixture pack: a slot patched to a base game sound that loads swaps; a slot patched to a missing
//! file is rejected and stays empty; a removed slot keeps the (empty) core default; an unknown slot is
//! empty. The pack overlay itself is LF_Test_SoundSlotsEveron.
// tier: WORLD
class LF_Test_SoundSlots : LF_Test
{
	//! A base game sound project, so the test needs no pack.
	static const ResourceName BASE = "{FD5E9674E51B9D13}Sounds/Props/Military/Radios/Props_Radios_Signals.acp";

	override string GetName()
	{
		return "sounds/slots";
	}

	override void Run(LF_TestContext ctx)
	{
		// Core only: an empty pack source.
		LF_ConfigService.Reset();
		LF_ConfigService.SetPackSource(new LF_ConfigPackSource());
		LF_SoundSlots.ResetStatics();
		array<string> ids = {"atmo_pub_radio", "atmo_store_radio", "atmo_fuel_pump", "atmo_still", "atmo_payphone_line", "atmo_chop_shop", "atmo_hospital_equipment", "atmo_hospital_ward", "atmo_police_desk", "atmo_furnace", "atmo_sawmill", "atmo_farm", "atmo_bank_till", "atmo_bank_murmur", "atmo_quarry", "atmo_van_idle"};
		foreach (string id : ids)
			ctx.True(LF_SoundSlots.Get(id).IsEmpty(), "core: slot " + id + " has no core sound, so it is silent");

		ctx.True(LF_SoundSlots.Get("no_such_slot").IsEmpty(), "an unknown slot is empty, so that one sound is off");
		ctx.EqualStr("no sound project", LF_SoundSlots.Unusable(ResourceName.Empty), "an empty project is named");
		ctx.EqualStr("does not load", LF_SoundSlots.Unusable("{F00DF00DF00DF00D}Sounds/LF_Test/NoSuch_Fixture.acp"), "a missing project is named");
		ctx.EqualStr("", LF_SoundSlots.Unusable(BASE), "a real project is usable");

		// The fixture pack over core.
		LF_ConfigService.SetPackSource(new LF_Test_SoundSlotsPackStub());
		LF_SoundSlots.ResetStatics();
		ctx.EqualStr(BASE, LF_SoundSlots.Get("atmo_store_radio"), "pack: the store radio is filled with a project that loads");
		ctx.True(LF_SoundSlots.Get("atmo_pub_radio").IsEmpty(), "pack: a project that does not load is rejected; the slot stays empty");
		ctx.True(LF_SoundSlots.Get("atmo_still").IsEmpty(), "pack: a removed slot keeps the empty core default");
		ctx.True(LF_SoundSlots.Get("atmo_sawmill").IsEmpty(), "pack: a slot the pack leaves alone stays core (empty)");

		LF_ConfigService.Reset();
		LF_SoundSlots.ResetStatics();
	}
}
