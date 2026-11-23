// red-proof: observed 2026-11-16 (#1775): before the overlays and header lines, `tools\cli test --no-build --only LF_Test_EveronRulesOverlays` failed `- the header lists the Everon handbook overlay`, `- the header lists the Everon prefab-slot overlay`, `- the handbook loader picks up its overlay` and `- the prefab-slot loader picks up its overlay` (passed=0 failed=1); with them it passes and the boot logs `changed 0 page(s)` and `changed 0 slots`.
//! #1775 in game: under the Everon 1989 header (the pack leg of `tools\cli test`), the header lists
//! the handbook overlay and the prefab-slot overlay, both resolve, `LF_ConfigOverlay` picks each up
//! for its own config class, and neither changes the core: the handbook keeps its core pages, every
//! prefab slot keeps its core prefab. The stubs are empty; the flagship cards (#1007) fill them.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronRulesOverlays : LF_Test
{
	static const ResourceName HANDBOOK_OVERLAY = "{7829FAC0964C207C}Configs/Rules/LF_Handbook_Everon1989.conf";
	static const ResourceName SLOTS_OVERLAY = "{54BDF6E63A12AC53}Configs/PrefabSlots/LF_PrefabSlots_Everon1989.conf";
	static const string GANG_RADIO_SLOT = "gang_radio";
	static const ResourceName GANG_RADIO_PREFAB = "{42646EECFBC540D9}Prefabs/Items/Comms/LF_GangRadio.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/rules-overlays";
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Picked(ResourceName coreConf, typename type, string section, ResourceName overlay)
	{
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(coreConf, type, section, resources, names);
		return names.Find(overlay) != -1;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header && header.m_aLF_ConfigOverlays)
		{
			ctx.True(header.m_aLF_ConfigOverlays.Find(HANDBOOK_OVERLAY) != -1, "the header lists the Everon handbook overlay");
			ctx.True(header.m_aLF_ConfigOverlays.Find(SLOTS_OVERLAY) != -1, "the header lists the Everon prefab-slot overlay");
		}
		else
		{
			ctx.Fail("the header lists no config overlays");
		}

		// The act: each loader asks the header for the overlays of its own class.
		ctx.True(Picked(LF_HandbookConfig.CONFIG_PATH, LF_HandbookConfig, "handbook", HANDBOOK_OVERLAY), "the handbook loader picks up its overlay");
		ctx.True(Picked(LF_PrefabSlotsConfig.CONFIG_PATH, LF_PrefabSlotsConfig, LF_PrefabSlotsConfig.LIST_NAME, SLOTS_OVERLAY), "the prefab-slot loader picks up its overlay");
		ctx.False(Picked(LF_PrefabSlotsConfig.CONFIG_PATH, LF_PrefabSlotsConfig, LF_PrefabSlotsConfig.LIST_NAME, HANDBOOK_OVERLAY), "the handbook overlay is not read as a prefab-slot overlay");

		// The overlays keep the core pages and slots; the only slot the pack swaps is the gang radio.
		LF_HandbookConfig handbook = LF_Handbook.Get();
		ctx.NotNull(handbook, "the handbook loads under the pack");
		if (handbook)
		{
			array<LF_HandbookPage> pages = {};
			int skipped = LF_Handbook.Collect(handbook, pages);
			ctx.Equal(0, skipped, "no handbook page is skipped under the pack");
			ctx.True(pages.Count() >= 2, "the handbook keeps its core pages");
		}

		LF_PrefabSlotsConfig core = LF_PrefabSlotsConfig.LoadCore();
		ctx.NotNull(core, "the core prefab slots load");
		if (core)
		{
			LF_PrefabSlotsConfig layered = LF_PrefabSlotsConfig.LoadCore();
			layered.ApplyLayers();
			// The pack adds the 20 gang flag slots (#4449): the core file lists none, LF_OrgFlagWall reads them layered.
			ctx.Equal(core.m_aSlots.Count() + LF_OrgFlags.COUNT, layered.m_aSlots.Count(), "the pack overlay removes no slot and adds only the 20 gang flag slots");
			foreach (LF_PrefabSlot slot : core.m_aSlots)
			{
				LF_PrefabSlot got = layered.Find(slot.m_sSlot);
				if (!got)
					ctx.Fail(string.Format("slot '%1' is missing under the pack overlay", slot.m_sSlot));
				else if (slot.m_sSlot == GANG_RADIO_SLOT)
					ctx.True(got.m_Prefab == GANG_RADIO_PREFAB, "the pack swaps the gang_radio slot to its own radio (#4223)");
				else if (got.m_Prefab != slot.m_Prefab)
					ctx.Fail(string.Format("slot '%1' changed under the pack overlay", slot.m_sSlot));
			}
		}
	}
}
