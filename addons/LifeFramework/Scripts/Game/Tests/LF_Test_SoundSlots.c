// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_SoundSlots`: making LF_SoundSlots.Unusable skip the Resource.Load check failed `- a missing project is named (expected 'does not load', got '')` and `- pack: a project that does not load is rejected; core default (expected '{2BC9B5C5F72B71E7}Sounds/Atmosphere/Atmosphere_PubRadio.acp', got '{F00DF00DF00DF00D}Sounds/Atmosphere/NoSuch_Fixture.acp')`. Restored; green.

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
//! Sound slots (#323) on the real core file. Core alone: every slot gives the sound the prop played
//! before the slots existed. With the fixture pack: a slot patched to a sound that loads swaps; a
//! slot patched to a missing file falls back to the core default; a removed slot keeps the core
//! default; an unknown slot is empty and logs once, so only that sound is off.
// tier: WORLD
class LF_Test_SoundSlots : LF_Test
{
	static const ResourceName FURNACE = "{9D9DC60799E815A4}Sounds/Atmosphere/Atmosphere_Furnace.acp";

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
		array<string> ids = {};
		array<ResourceName> expected = {};
		ids.Insert("atmo_pub_radio"); expected.Insert("{2BC9B5C5F72B71E7}Sounds/Atmosphere/Atmosphere_PubRadio.acp");
		ids.Insert("atmo_store_radio"); expected.Insert("{F81D983989C40DCD}Sounds/Atmosphere/Atmosphere_StoreRadio.acp");
		ids.Insert("atmo_fuel_pump"); expected.Insert("{EC2B286A4A5D758A}Sounds/Atmosphere/Atmosphere_FuelPump.acp");
		ids.Insert("atmo_still"); expected.Insert("{371B46E27DAC90B8}Sounds/Atmosphere/Atmosphere_Still.acp");
		ids.Insert("atmo_payphone_line"); expected.Insert("{4F67E8DF29241102}Sounds/Atmosphere/Atmosphere_PayphoneLine.acp");
		ids.Insert("atmo_chop_shop"); expected.Insert("{C992ED4AD7C05169}Sounds/Atmosphere/Atmosphere_ChopShop.acp");
		ids.Insert("atmo_hospital_equipment"); expected.Insert("{ED0EA6116B7FA7AF}Sounds/Atmosphere/Atmosphere_HospitalEquipment.acp");
		ids.Insert("atmo_hospital_ward"); expected.Insert("{EB0DB92516AB336C}Sounds/Atmosphere/Atmosphere_HospitalWard.acp");
		ids.Insert("atmo_police_desk"); expected.Insert("{78589B4D585BFBA4}Sounds/Atmosphere/Atmosphere_PoliceDesk.acp");
		ids.Insert("atmo_furnace"); expected.Insert("{9D9DC60799E815A4}Sounds/Atmosphere/Atmosphere_Furnace.acp");
		ids.Insert("atmo_sawmill"); expected.Insert("{7D137D89A4BD8A95}Sounds/Atmosphere/Atmosphere_Sawmill.acp");
		ids.Insert("atmo_farm"); expected.Insert("{64724EE52851E9CA}Sounds/Atmosphere/Atmosphere_Farm.acp");
		ids.Insert("atmo_bank_till"); expected.Insert("{6539B71BA4BFA1FC}Sounds/Atmosphere/Atmosphere_BankTill.acp");
		ids.Insert("atmo_bank_murmur"); expected.Insert("{9E627437CDE85DF1}Sounds/Atmosphere/Atmosphere_BankMurmur.acp");
		ids.Insert("atmo_quarry"); expected.Insert("{A795096DB7F9F55D}Sounds/Atmosphere/Atmosphere_Quarry.acp");
		ids.Insert("atmo_van_idle"); expected.Insert("{676F8D61F536FDD2}Sounds/Atmosphere/Atmosphere_VanIdle.acp");
		foreach (int i, string id : ids)
			ctx.EqualStr(expected[i], LF_SoundSlots.Get(id), "core: slot " + id + " is the sound the prop played before slots");

		ctx.True(LF_SoundSlots.Get("no_such_slot").IsEmpty(), "an unknown slot is empty, so that one sound is off");
		ctx.EqualStr("no sound project", LF_SoundSlots.Unusable(ResourceName.Empty), "an empty project is named");
		ctx.EqualStr("does not load", LF_SoundSlots.Unusable("{F00DF00DF00DF00D}Sounds/Atmosphere/NoSuch_Fixture.acp"), "a missing project is named");
		ctx.EqualStr("", LF_SoundSlots.Unusable(FURNACE), "a real project is usable");

		// The fixture pack over core.
		LF_ConfigService.SetPackSource(new LF_Test_SoundSlotsPackStub());
		LF_SoundSlots.ResetStatics();
		ctx.EqualStr(FURNACE, LF_SoundSlots.Get("atmo_store_radio"), "pack: the store radio swaps to a project that loads");
		ctx.EqualStr(expected[ids.Find("atmo_pub_radio")], LF_SoundSlots.Get("atmo_pub_radio"), "pack: a project that does not load is rejected; core default");
		ctx.EqualStr(expected[ids.Find("atmo_still")], LF_SoundSlots.Get("atmo_still"), "pack: a removed slot keeps the core default");
		ctx.EqualStr(expected[ids.Find("atmo_sawmill")], LF_SoundSlots.Get("atmo_sawmill"), "pack: a slot the pack leaves alone stays core");

		LF_ConfigService.Reset();
		LF_SoundSlots.ResetStatics();
	}
}
