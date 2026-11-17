// red-proof: 2026-11-17 `tools\cli test --no-build --red LF_Test_EveronSafeTypes`: RED-OK everon/safe-types (12 assertions; RedProof skips the overlay apply); failed "the pack patches the store_box haul to 120 (expected 120, got 150)". Green without the hook (pack leg).
// tier: WORLD
//! Safe types in game: under the Everon 1989 header (the pack leg of `tools\cli test`), the pack's safe
//! type overlay patches the core store_box row (1989 till takings) and leaves the other tiers as
//! the core ships them. The vault-room door kit names bank_room, a type the table has, and the town
//! bank's own prefab sets no type, so its numbers do not move.
class LF_Test_EveronSafeTypes : LF_Test
{
	static const ResourceName OVERLAY = "{CB77B848FEFA13A3}Configs/Crime/LF_SafeTypes_Everon1989.conf";
	static const ResourceName ROOM_DOOR = "{B17A72DCB10A840C}Prefabs/Props/Roleplay/BankVault/LF_BankVault_RoomDoor.et";
	static const ResourceName TOWN_VAULT = "{6177818988438527}Prefabs/Props/Roleplay/BankVault/LF_BankVault.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/safe-types";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header)
			ctx.True(header.m_aLF_ConfigOverlays && header.m_aLF_ConfigOverlays.Find(OVERLAY) != -1, "the header lists the Everon safe type overlay");

		Resource container = BaseContainerTools.LoadContainer(LF_SafeTypeConfig.CONFIG_PATH);
		if (!container || !container.IsValid() || !container.GetResource())
		{
			ctx.Fail("the core safe type table does not load");
			return;
		}

		LF_SafeTypeConfig table = LF_SafeTypeConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		ctx.NotNull(table, "the core table reads");
		if (!table)
			return;

		LF_SafeTypeRow coreBox = table.Find(LF_SafeTypeConfig.TYPE_STORE_BOX);
		ctx.True(coreBox != null && coreBox.m_iCashPerBag == 150, "the core store_box row alone pays 150");
		// The act: the running header's overlays layered over the core table.
		if (!RedProof())
			table.ApplyLayers();

		LF_SafeTypeRow box = table.Find(LF_SafeTypeConfig.TYPE_STORE_BOX);
		ctx.NotNull(box, "the store_box row survives the overlay");
		if (box)
		{
			ctx.Equal(120, box.m_iCashPerBag, "the pack patches the store_box haul to 120");
			ctx.EqualFloat(10, box.m_fFuseSeconds, 0.001, "the patch leaves the store_box timer alone");
		}

		LF_SafeTypeRow bank = table.Find(LF_SafeTypeConfig.TYPE_BANK_ROOM);
		ctx.True(bank != null && bank.m_iCashPerBag == 500, "bank_room is untouched by the pack");

		string doorType = LF_Test_SafeTypes.SafeTypeOf(ROOM_DOOR);
		ctx.True(doorType == LF_SafeTypeConfig.TYPE_BANK_ROOM, "the vault-room door kit names bank_room");
		ctx.NotNull(table.Find(doorType), "the type the door kit names is in the table");
		ctx.True(LF_Test_SafeTypes.SafeTypeOf(TOWN_VAULT).IsEmpty(), "the town bank's own prefab sets no type, so its numbers do not move");
	}
}
