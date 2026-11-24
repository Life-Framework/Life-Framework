// red-proof: 2026-11-24, the unfixed HasToolIn read the right hand and left gadget only; the live session (#4833 round 5) logged `fellhold aim -> no_axe right=none left=none weapon=LF_Tool_Axe_Standard.et`. `--red LF_Test_FellAxeWeapon` leaves the axe out of the weapon source, so "a drawn axe counts as held" fails.
//! tool/fell-axe-weapon (#4854): a drawn axe is the character's current weapon, in neither the right-hand
//! slot nor the left gadget slot. Every tool check reads the hands through LF_GatherTools.HeldItems (right
//! hand, left gadget, current weapon), so the drawn axe counts: the Fell row shows and the server half
//! accepts it; no axe still means no row. The engine's draw animation outlasts a one-frame test, so the
//! weapon source is handed to CollectHeld directly; the live draw is proved in the island session.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_FellAxeWeapon : LF_Test
{
	protected static const ResourceName PICKAXE = "{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et";

	override string GetName()
	{
		return "tool/fell-axe-weapon";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		if (!body)
			return;

		ctx.False(LF_FellAction.HoldsAxe(body), "an unarmed character does not hold the axe");
		array<IEntity> hands = {};
		LF_GatherTools.HeldItems(body, hands);
		ctx.Equal(0, hands.Count(), "an unarmed character holds nothing");

		ctx.Equal(1, LF_InventoryUtils.AddAmount(body, LF_FellAction.AXE, 1), "the axe is given");
		ctx.False(LF_FellAction.HoldsAxe(body), "an axe in the pack is not held");

		IEntity axe;
		CharacterControllerComponent controller = LF_Component<CharacterControllerComponent>.Find(body);
		BaseWeaponManagerComponent weapons;
		if (controller)
			weapons = controller.GetWeaponManagerComponent();
		ctx.NotNull(weapons, "the character has a weapon manager");
		if (weapons)
		{
			array<WeaponSlotComponent> slots = {};
			weapons.GetWeaponsSlots(slots);
			foreach (WeaponSlotComponent slot : slots)
			{
				IEntity candidate = slot.GetWeaponEntity();
				if (candidate && LF_Utils.EntityInherits(candidate, LF_FellAction.AXE))
					axe = candidate;
			}
		}
		ctx.NotNull(axe, "the axe sits in a weapon slot");

		// The axe as the current weapon, with both hand slots empty: what a player who drew it has.
		IEntity drawn = axe;
		if (RedProof())
			drawn = null;
		array<IEntity> held = {};
		LF_GatherTools.CollectHeld(null, null, drawn, held);
		ctx.Equal(1, held.Count(), "the drawn axe is in the held list");
		ctx.True(LF_GatherTools.AnyIsTool(held, LF_FellAction.AXE), "a drawn axe counts as held");
		ctx.False(LF_GatherTools.AnyIsTool(held, PICKAXE), "an axe is not a pickaxe");

		array<IEntity> empty = {};
		LF_GatherTools.CollectHeld(null, null, null, empty);
		ctx.False(LF_GatherTools.AnyIsTool(empty, LF_FellAction.AXE), "no axe, no row");

		// The same entity in two sources is listed once.
		array<IEntity> twice = {};
		LF_GatherTools.CollectHeld(axe, null, axe, twice);
		ctx.True(twice.Count() <= 1, "an item in two sources is listed once");

		LF_TestCharacterFixture.Delete(body);
	}
}
