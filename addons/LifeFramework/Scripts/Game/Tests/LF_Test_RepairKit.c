// red-proof: 2026-11-19 `tools/cli test --no-build --red LF_Test_RepairKit` RED PROOF OK: with the RedProof() hook skipping the repair, `the first kit repairs the car`, `the last kit is gone (expected 0, got 1)` and `with no kit the car refuses` failed. Green without the hook (2 of 2).
//! vehicles/repair-kit (#3332): a car at 40 % health and a kit: after the repair it reads 40 % plus the kit's
//! configured share and the kit is gone; a second kit inside the car's cooldown is refused and nothing is
//! consumed; a refused repair (no kit, full health, out of reach) leaves the kit and the car alone.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_RepairKit : LF_Test
{
	protected static const ResourceName UAZ = "{C8B16ABCA57679A2}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_covered_CIV_blue.et";
	protected static const ResourceName PACK = "{06B68C58B72EAAC6}Prefabs/Items/Equipment/Backpacks/Backpack_ALICE_Medium.et";
	protected static const vector SPOT = "160 0 80";
	protected static const int T0 = 200000;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "vehicles/repair-kit";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_RepairKit.ResetStatics();

		// The pure rule.
		int cooldown = 90000;
		ctx.True(LF_RepairKit.Rules(0.4, false, true, 2, -1, T0, cooldown, false) == LF_ERepairKitCheck.NONE, "a damaged car, a kit, in reach, never repaired: allowed");
		ctx.True(LF_RepairKit.Rules(1, false, true, 2, -1, T0, cooldown, false) == LF_ERepairKitCheck.NOT_DAMAGED, "a car at full health refuses");
		ctx.True(LF_RepairKit.Rules(0.4, true, true, 2, -1, T0, cooldown, false) == LF_ERepairKitCheck.WRECKED, "a wreck refuses");
		ctx.True(LF_RepairKit.Rules(0.4, false, false, 2, -1, T0, cooldown, false) == LF_ERepairKitCheck.NO_KIT, "no kit refuses");
		ctx.True(LF_RepairKit.Rules(0.4, false, true, 30, -1, T0, cooldown, false) == LF_ERepairKitCheck.TOO_FAR, "out of reach refuses");
		ctx.True(LF_RepairKit.Rules(0.4, false, true, 2, -1, T0, cooldown, true) == LF_ERepairKitCheck.BUSY, "a car already being worked on refuses");
		ctx.True(LF_RepairKit.Rules(0.4, false, true, 2, T0, T0 + 1000, cooldown, false) == LF_ERepairKitCheck.COOLDOWN, "a second kit inside the cooldown refuses");
		ctx.True(LF_RepairKit.Rules(0.4, false, true, 2, T0, T0 + cooldown, cooldown, false) == LF_ERepairKitCheck.NONE, "after the cooldown it is allowed again");
		ctx.EqualFloat(1, LF_RepairKit.Restored(0.9, 0.4), 0.001, "a repair never goes above full health");

		// The world: a real car at 40 % and a character with two kits.
		vector at = SPOT;
		at[1] = GetGame().GetWorld().GetSurfaceY(at[0], at[2]) + 1;
		IEntity car = LF_Utils.SpawnEntityPrefab(UAZ, at);
		ctx.NotNull(car, "a UAZ469 spawns");
		IEntity worker = LF_TestCharacterFixture.Spawn(ctx);
		if (!car || !worker)
		{
			LF_TestCharacterFixture.Delete(worker);
			if (car)
				SCR_EntityHelper.DeleteEntityAndChildren(car);

			return;
		}

		vector beside = at;
		beside[0] = beside[0] + 2;
		worker.SetOrigin(beside);

		DamageManagerComponent damage = LF_Component<DamageManagerComponent>.Find(car);
		ctx.NotNull(damage, "the car has a damage manager");
		if (!damage)
		{
			LF_TestCharacterFixture.Delete(worker);
			SCR_EntityHelper.DeleteEntityAndChildren(car);
			return;
		}

		damage.SetHealthScaled(0.4);
		ctx.EqualFloat(0.4, LF_RepairKit.HealthOf(car), 0.02, "the car starts at 40 % health");

		// The fixture's jacket is too small for a toolbox; a field pack carries the kits.
		InventoryStorageManagerComponent inventory = LF_InventoryUtils.GetResponsibleStorageManager(worker);
		IEntity pack = LF_Utils.SpawnEntityPrefab(PACK, worker.GetOrigin());
		ctx.True(pack != null && inventory.TryInsertItem(pack, EStoragePurpose.PURPOSE_LOADOUT_PROXY), "the worker wears a pack");
		ctx.True(LF_RepairKit.Check(car, worker, T0) == LF_ERepairKitCheck.NO_KIT, "before the kit the car refuses the worker");
		ctx.Equal(2, LF_InventoryUtils.AddAmount(worker, LF_RepairKit.KIT_PREFAB, 2), "two kits are handed over");
		ctx.Equal(2, LF_InventoryUtils.GetAmount(worker, LF_RepairKit.KIT_PREFAB), "the worker holds two kits");

		LF_RepairKitComponent kit = LF_Component<LF_RepairKitComponent>.Find(LF_RepairKit.KitOf(worker));
		ctx.NotNull(kit, "the kit carries LF_RepairKitComponent");
		if (!kit)
		{
			LF_TestCharacterFixture.Delete(worker);
			SCR_EntityHelper.DeleteEntityAndChildren(car);
			return;
		}

		float restore = kit.GetRestoreScaled();
		ctx.True(restore > 0.05 && restore < 1, "the kit restores a configured share");
		int cooldownMs = kit.GetCooldownMs();
		ctx.True(cooldownMs > 0, "the kit sets a cooldown");

		// First repair.
		LF_ERepairKitCheck first = LF_ERepairKitCheck.NO_KIT;
		if (!RedProof())
			first = LF_RepairKit.Try(car, worker, T0);

		ctx.True(first == LF_ERepairKitCheck.NONE, "the first kit repairs the car");
		ctx.EqualFloat(0.4 + restore, LF_RepairKit.HealthOf(car), 0.02, "the car reads 40 % plus the kit's share");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(worker, LF_RepairKit.KIT_PREFAB), "one kit is used up");

		// The second kit, inside the cooldown.
		float healthAfterFirst = LF_RepairKit.HealthOf(car);
		ctx.True(LF_RepairKit.Try(car, worker, T0 + 1000) == LF_ERepairKitCheck.COOLDOWN, "a second kit inside the cooldown is refused");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(worker, LF_RepairKit.KIT_PREFAB), "the refused repair keeps the kit");
		ctx.EqualFloat(healthAfterFirst, LF_RepairKit.HealthOf(car), 0.001, "the refused repair leaves the car alone");

		// After the cooldown the second kit works and the last kit is gone.
		ctx.True(LF_RepairKit.Try(car, worker, T0 + cooldownMs) == LF_ERepairKitCheck.NONE, "after the cooldown the second kit repairs");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(worker, LF_RepairKit.KIT_PREFAB), "the last kit is gone");
		ctx.True(LF_RepairKit.HealthOf(car) > healthAfterFirst, "the second repair raised the car again");

		// No kit left: refused, nothing changes.
		damage.SetHealthScaled(0.4);
		ctx.True(LF_RepairKit.Try(car, worker, T0 + cooldownMs * 3) == LF_ERepairKitCheck.NO_KIT, "with no kit the car refuses");

		LF_TestCharacterFixture.Delete(worker);
		SCR_EntityHelper.DeleteEntityAndChildren(car);
		LF_RepairKit.ResetStatics();
	}
}
