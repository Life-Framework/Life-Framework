// red-proof: observed 2026-11-23 (#4454) with `toolscli test --no-build --red LF_Test_EveronOutpostNpcs`: RedProof() skips the spawn, so no NPC exists (RED-OK everon/outpost-npcs; `red: the mechanic spawns`, `red: gate guard two spawns` fail). Green without the hook: PASS everon/outpost-npcs (52 assertions, pack leg).
//! #4454 (gangs O11, part 1): the north outpost's five people. Under the Everon 1989 header (the pack leg) each of
//! the quartermaster, the contraband buyer, the two gate guards and the mechanic spawns wearing exactly the hat,
//! torso, legs and boots of its own outfit preset (`LF_Outfit_Outpost*.conf`). The guards carry a VZ-58 in the weapon slot
//! (held at the low ready, never fired); nobody else carries a weapon. The quartermaster holds the crate (`LF_OutpostStockComponent`
//! and one Buy row per default stock row), the buyer holds the contraband Sell row, nobody else has either.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronOutpostNpcs : LF_Test
{
	static const string RIFLE_FILE = "Rifle_VZ58P.et";
	static const int DEFAULT_STOCK_ROWS = 3;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/outpost-npcs";
	}

	//------------------------------------------------------------------------------------------------
	//! The path after the {GUID} of a resource name, so a worn item compares with a preset slot by file.
	protected static string PathOf(string resourceName)
	{
		int close = resourceName.IndexOf("}");
		if (close < 0)
			return resourceName;

		return resourceName.Substring(close + 1, resourceName.Length() - close - 1);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool Wears(IEntity npc, ResourceName item)
	{
		string want = PathOf(item);
		IEntity child = npc.GetChildren();
		while (child)
		{
			EntityPrefabData data = child.GetPrefabData();
			if (data && child.FindComponent(BaseLoadoutClothComponent) && PathOf(data.GetPrefabName()) == want)
				return true;

			child = child.GetSibling();
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static int CountActions(IEntity npc, typename actionType)
	{
		ActionsManagerComponent manager = ActionsManagerComponent.Cast(npc.FindComponent(ActionsManagerComponent));
		if (!manager)
			return 0;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		int count;
		foreach (BaseUserAction action : actions)
		{
			if (action.IsInherited(actionType))
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CarriesRifle(IEntity npc)
	{
		CharacterControllerComponent controller = CharacterControllerComponent.Cast(npc.FindComponent(CharacterControllerComponent));
		BaseWeaponManagerComponent weapons;
		if (controller)
			weapons = controller.GetWeaponManagerComponent();

		if (!weapons)
			return false;

		array<WeaponSlotComponent> slots = {};
		weapons.GetWeaponsSlots(slots);
		foreach (WeaponSlotComponent slot : slots)
		{
			IEntity weapon = slot.GetWeaponEntity();
			EntityPrefabData data;
			if (weapon)
				data = weapon.GetPrefabData();

			if (data && PathOf(data.GetPrefabName()).EndsWith(RIFLE_FILE))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckNpc(LF_TestContext ctx, string label, ResourceName prefab, ResourceName outfit, bool guard, int buyRows, int sellRows)
	{
		// The act: the NPC as the pack ships it. Red proof: nothing spawns.
		IEntity npc;
		if (!RedProof())
			npc = LF_Utils.SpawnEntityPrefab(prefab, "0 -50 0");

		ctx.NotNull(npc, label + " spawns");
		if (!npc)
			return;

		Resource container = BaseContainerTools.LoadContainer(outfit);
		LF_NpcOutfitPreset preset;
		if (container && container.IsValid() && container.GetResource())
			preset = LF_NpcOutfitPreset.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));

		ctx.NotNull(preset, label + " has its outfit preset");
		if (preset)
		{
			ctx.True(Wears(npc, preset.m_Head), label + " wears the preset's hat");
			ctx.True(Wears(npc, preset.m_Torso), label + " wears the preset's torso piece");
			ctx.True(Wears(npc, preset.m_Legs), label + " wears the preset's trousers");
			ctx.True(Wears(npc, preset.m_Feet), label + " wears the preset's boots");
			if (!preset.m_Vest.IsEmpty())
				ctx.True(Wears(npc, preset.m_Vest), label + " wears the preset's vest");
		}

		ctx.Equal(guard, CarriesRifle(npc), label + " carries a rifle exactly when it is a guard");
		ctx.Equal(buyRows, CountActions(npc, LF_OutpostStockBuyAction), label + " has its crate Buy rows");
		ctx.Equal(sellRows, CountActions(npc, LF_OutpostBuyerSellAction), label + " has its contraband Sell rows");

		LF_OutpostStockComponent crate = LF_Component<LF_OutpostStockComponent>.Find(npc);
		if (buyRows > 0)
		{
			ctx.NotNull(crate, label + " holds the crate stock");
			if (crate)
				ctx.Equal(buyRows, crate.GetStock().Count(), label + " lists one Buy row per stock row");
		}
		else
		{
			ctx.True(crate == null, label + " holds no crate");
		}

		LF_TestCharacterFixture.Delete(npc);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.NotNull(LF_MissionHeader.GetCurrent(), "the pack leg runs under an LF mission header");

		CheckNpc(ctx, "the quartermaster", "{D9C9D205CD9D4A61}Prefabs/Characters/Outposts/LF_Outpost_Quartermaster.et", "{7D77CC710FC9A870}Configs/NpcOutfits/LF_Outfit_OutpostQuartermaster.conf", false, DEFAULT_STOCK_ROWS, 0);
		CheckNpc(ctx, "the contraband buyer", "{ABD45E880BD19544}Prefabs/Characters/Outposts/LF_Outpost_Buyer.et", "{AA18814E9798F975}Configs/NpcOutfits/LF_Outfit_OutpostBuyer.conf", false, 0, 1);
		CheckNpc(ctx, "gate guard one", "{C974EBD0B66A34F4}Prefabs/Characters/Outposts/LF_Outpost_GuardGate1.et", "{61996C6AC0CC6BAC}Configs/NpcOutfits/LF_Outfit_OutpostGuardGate1.conf", true, 0, 0);
		CheckNpc(ctx, "gate guard two", "{6B2180250B983E0F}Prefabs/Characters/Outposts/LF_Outpost_GuardGate2.et", "{535AA215AD1ADCBD}Configs/NpcOutfits/LF_Outfit_OutpostGuardGate2.conf", true, 0, 0);
		CheckNpc(ctx, "the mechanic", "{D9E78AC61656C5CA}Prefabs/Characters/Outposts/LF_Outpost_Mechanic.et", "{57A00B3A5D06DCC7}Configs/NpcOutfits/LF_Outfit_OutpostMechanic.conf", false, 0, 0);
	}
}
