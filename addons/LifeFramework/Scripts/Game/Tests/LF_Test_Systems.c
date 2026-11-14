// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_Systems`: dropping the parent
// check in LF_Systems.IsOn failed `- a part of a system that is off is off` and `- and says why: `. Restored; green.
// tier: LOGIC
//! Systems (#960): each system's on or off read through the config service. A catalog built in
//! script and an owner `systems` section: an off switch turns the system and its parts off; a
//! framework (LOCKED) system, a system with no switch yet (NOT_YET) and an unknown id are refused;
//! turning off a system an enabled one needs is refused and it goes back to its shipped value; an
//! opt-in system stays off and what needs it goes off; a run-time switch-off logs once and holds;
//! a loadout item of a system that is off is left out.
class LF_Test_Systems : LF_Test
{
	static const ResourceName ITEM = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";

	override string GetName()
	{
		return "systems/toggle";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_SystemCatalog MakeCatalog()
	{
		LF_SystemCatalog catalog = new LF_SystemCatalog();
		LF_SystemDef parent = catalog.Add("TParent", LF_ESystemSwitch.SWITCHABLE);
		parent.m_aItems.Insert(ITEM);
		catalog.Add("TPart", LF_ESystemSwitch.SWITCHABLE, "TParent");
		catalog.Add("TFramework", LF_ESystemSwitch.LOCKED);
		catalog.Add("TUngated", LF_ESystemSwitch.NOT_YET);
		LF_SystemDef bank = catalog.Add("TBank", LF_ESystemSwitch.SWITCHABLE);
		LF_SystemDef org = catalog.Add("TOrg", LF_ESystemSwitch.SWITCHABLE);
		org.m_aRequires.Insert("TBank");
		catalog.Add("TOptIn", LF_ESystemSwitch.SWITCHABLE, string.Empty, true);
		LF_SystemDef needsOptIn = catalog.Add("TNeedsOptIn", LF_ESystemSwitch.SWITCHABLE);
		needsOptIn.m_aRequires.Insert("TOptIn");
		catalog.Add("TOther", LF_ESystemSwitch.SWITCHABLE);
		return catalog;
	}

	//------------------------------------------------------------------------------------------------
	//! Installs the catalog with an owner `systems` section. \return The owner reader's rejected count.
	protected int InstallOwner(LF_SystemCatalog catalog, string switchesJson)
	{
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"systems\":{\"switches\":" + switchesJson + "}}", "test");
		LF_SystemsConfig layers = new LF_SystemsConfig();
		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(LF_SystemsConfig.SECTION);
		int rejected = -1;
		if (reader)
		{
			layers.ApplyOwner(reader, catalog);
			rejected = reader.GetRejectedCount();
		}
		LF_Systems.Install(catalog, layers);
		return rejected;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_Systems.ResetStatics();
		LF_SystemCatalog catalog = MakeCatalog();

		// No owner switches: every system on except the opt-in one and what needs it.
		InstallOwner(catalog, "{}");
		ctx.True(LF_Systems.IsEnabled("TParent"), "a system with no switch is on");
		ctx.True(LF_Systems.IsEnabled("TOrg"), "a system whose requirement is on is on");
		ctx.False(LF_Systems.IsEnabled("TOptIn"), "an opt-in system is off until something turns it on");
		ctx.False(LF_Systems.IsEnabled("TNeedsOptIn"), "a system that needs an opt-in system that is off is off");
		ctx.True(LF_Systems.IsItemAllowed(ITEM), "the item of a system that is on is given");
		ctx.True(LF_Systems.IsEnabled("TNotListed"), "an id the catalog does not list reads as on (and logs once)");

		// The owner turns systems off, some of which the catalog does not allow.
		int rejected = InstallOwner(catalog, "{\"TParent\":{\"enabled\":false},\"TFramework\":{\"enabled\":false},\"TUngated\":{\"enabled\":false},"
			+ "\"TBank\":{\"enabled\":false},\"TOptIn\":{\"enabled\":true},\"TMissing\":{\"enabled\":false},\"TOther\":{\"enabled\":\"no\"}}");
		ctx.Equal(2, rejected, "the reader refuses the unknown id and the value that is not true or false");
		ctx.False(LF_Systems.IsEnabled("TParent"), "the owner's off switch turns the system off");
		ctx.False(LF_Systems.IsEnabled("TPart"), "a part of a system that is off is off");
		ctx.True(LF_Systems.WhyOff("TPart").Contains("part of TParent"), "and says why: " + LF_Systems.WhyOff("TPart"));
		ctx.True(LF_Systems.WhyOff("TParent").Contains("owner config"), "the system names the layer that turned it off: " + LF_Systems.WhyOff("TParent"));
		ctx.False(LF_Systems.IsItemAllowed(ITEM), "the item of a system that is off is left out of a loadout");
		ctx.True(LF_Systems.IsEnabled("TFramework"), "a framework system cannot be turned off");
		ctx.True(LF_Systems.IsEnabled("TUngated"), "a system with no switch yet stays on");
		ctx.True(LF_Systems.IsEnabled("TBank"), "turning off a system an enabled one needs is refused: it goes back to its shipped value");
		ctx.True(LF_Systems.IsEnabled("TOrg"), "and the system that needs it stays on");
		ctx.True(LF_Systems.IsEnabled("TOptIn"), "the owner turns an opt-in system on");
		ctx.True(LF_Systems.IsEnabled("TNeedsOptIn"), "and what needs it comes on with it");
		ctx.True(LF_Systems.IsEnabled("TOther"), "a bad value keeps the shipped value");

		// Both off together is allowed: nothing enabled needs the bank then.
		InstallOwner(catalog, "{\"TBank\":{\"enabled\":false},\"TOrg\":{\"enabled\":false}}");
		ctx.False(LF_Systems.IsEnabled("TBank"), "a needed system goes off when what needs it goes off too");
		ctx.False(LF_Systems.IsEnabled("TOrg"), "the system that needs it is off");
		ctx.True(LF_Systems.Refuses("TOrg", "test request"), "a server entry point of a system that is off refuses");
		ctx.False(LF_Systems.Refuses("TParent", "test request"), "a system that is on does not refuse");

		// A run-time switch-off (#1047): logs once, holds over a reload of the layers, can be cleared.
		InstallOwner(catalog, "{}");
		ctx.True(LF_Systems.SwitchOff("TOther", "no test place on this map"), "a system switches itself off");
		ctx.False(LF_Systems.IsEnabled("TOther"), "it is off");
		ctx.EqualStr("no test place on this map", LF_Systems.WhyOff("TOther"), "with its reason");
		ctx.True(LF_Systems.SwitchOff("TOther", "again"), "a second switch-off is accepted quietly");
		ctx.EqualStr("no test place on this map", LF_Systems.WhyOff("TOther"), "and keeps the first reason");
		InstallOwner(catalog, "{\"TOther\":{\"enabled\":true}}");
		ctx.False(LF_Systems.IsEnabled("TOther"), "a config reload does not undo a run-time switch-off");
		ctx.False(LF_Systems.SwitchOff("TFramework", "no place"), "a framework system cannot switch itself off");
		ctx.True(LF_Systems.IsEnabled("TFramework"), "it stays on");
		LF_Systems.SwitchOff("TBank", "no bank on this map");
		ctx.False(LF_Systems.IsEnabled("TOrg"), "a system whose requirement switched itself off is off too");
		ctx.True(LF_Systems.WhyOff("TOrg").Contains("needs TBank"), "and says why: " + LF_Systems.WhyOff("TOrg"));
		LF_Systems.ClearRuntimeOff();
		ctx.True(LF_Systems.IsEnabled("TOther") && LF_Systems.IsEnabled("TOrg"), "clearing the run-time offs brings them back");

		LF_ConfigService.Reset();
		LF_Systems.ResetStatics();
	}
}
