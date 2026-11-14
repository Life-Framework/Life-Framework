// red-proof: 2026-11-14, `tools\cli test --no-build --only LF_Test_DownedPatientWorld` with the `LF_Jail.MoveBody(patient, slot.m_aHome[3])` line removed from LF_DownedPatient.Reset: `the patient is at its spot again`; restored, green.
// tier: WORLD
//! The practice patient in a world: the prefab is the patient, lethal damage downs it as it would a
//! player, it does not bleed out, the medic actions are on it, a reset puts a treated one back
//! healed at its spot and downs it again, and the world reset has a step for it.
class LF_Test_DownedPatientWorld : LF_Test
{
	static const ResourceName PATIENT_PREFAB = "{686A77317FE9AD1B}Prefabs/Medic/LF_DownedPatient.et";

	override string GetName()
	{
		return "medic/downed-patient-world";
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity Spawn(LF_TestContext ctx, vector position)
	{
		Resource resource = Resource.Load(PATIENT_PREFAB);
		if (!resource || !resource.IsValid())
		{
			ctx.Fail("the patient prefab does not load: " + PATIENT_PREFAB);
			return null;
		}

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		IEntity patient = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		if (!patient)
			ctx.Fail("the patient prefab did not spawn");

		return patient;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MedicConfig config = LF_MedicConfig.Get();
		bool wasEnabled = config.m_bDownedEnabled;
		config.m_bDownedEnabled = true;

		bool registered = false;
		foreach (LF_WorldResetStep step : LF_WorldReset.Steps())
		{
			if (step.GetName() == "patient")
				registered = true;
		}

		ctx.True(registered, "the patient step is registered with the world reset by its own file");

		IEntity car = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.ESCORT_CAR);
		ctx.NotNull(car, "DebugWorld has Annex_Escort_Car");
		if (car)
		{
			vector home = car.GetOrigin() + "4 0 0";
			IEntity patient = Spawn(ctx, home);
			if (patient)
				RunPatient(ctx, patient, home);

			if (patient)
				SCR_EntityHelper.DeleteEntityAndChildren(patient);
		}

		config.m_bDownedEnabled = wasEnabled;
	}

	//------------------------------------------------------------------------------------------------
	protected void RunPatient(LF_TestContext ctx, IEntity patient, vector home)
	{
		SCR_ChimeraCharacter body = SCR_ChimeraCharacter.Cast(patient);
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(patient.FindComponent(SCR_CharacterDamageManagerComponent));
		ctx.NotNull(body, "the patient is a character");
		ctx.NotNull(damage, "the patient has the character damage manager");
		if (!body || !damage)
			return;

		ctx.True(LF_DownedPatient.IsPatient(patient), "the prefab carries the patient component");
		ctx.True(LF_Downed.IsEligible(patient), "the patient can be downed though it is not a player");

		string actions;
		array<BaseUserAction> list = {};
		ActionsManagerComponent manager = ActionsManagerComponent.Cast(patient.FindComponent(ActionsManagerComponent));
		ctx.NotNull(manager, "the patient has an actions manager");
		if (manager)
		{
			manager.GetActionsList(list);
			foreach (BaseUserAction action : list)
				actions += action.Type().ToString() + ";";
		}

		ctx.True(actions.Contains("LF_ReviveAction"), "Revive is on the patient: " + actions);
		ctx.True(actions.Contains("LF_DragAction"), "Drag is on the patient");
		ctx.True(actions.Contains("LF_LoadPatientAction"), "Load into vehicle is on the patient");

		// The town's own patient may already be tracked (DebugWorld is a playtest mission).
		int before = LF_DownedPatient.GetSlotCount();
		int resetsBefore = LF_DownedPatient.GetResetCount();
		LF_PatientSlot slot = LF_DownedPatient.AddSlot(patient);
		ctx.Equal(before + 1, LF_DownedPatient.GetSlotCount(), "the patient is tracked");
		LF_DownedPatient.Arm(slot, patient);
		ctx.True(body.LF_IsDowned(), "lethal damage leaves the patient downed, not dead");

		LF_BleedOut.Advance(100000);
		ctx.True(body.LF_IsDowned(), "the patient does not bleed out");
		ctx.True(damage.GetState() != EDamageState.DESTROYED, "the patient is still alive after the bleed-out clock would have run out");

		// A medic clears it, and a stray step takes it away from its spot.
		LF_Downed.Clear(patient);
		ctx.False(body.LF_IsDowned(), "a revive clears the downed state");
		patient.SetOrigin(home + "9 0 0");
		ctx.True(LF_DownedPatient.Reset(slot, patient), "a reset puts the patient back");
		ctx.True(vector.Distance(patient.GetOrigin(), home) < 1.0, "the patient is at its spot again");
		ctx.EqualFloat(1.0, damage.GetDefaultHitZone().GetHealthScaled(), 0.01, "the patient is healed");
		ctx.Equal(resetsBefore + 1, LF_DownedPatient.GetResetCount(), "the reset is counted");

		LF_DownedPatient.Arm(slot, patient);
		ctx.True(body.LF_IsDowned(), "the next tick downs it again");
		ctx.Equal(before + 1, LF_DownedPatient.ResetAll(), "the world reset puts every tracked patient back");
		ctx.False(body.LF_IsDowned(), "after the world reset the patient is not downed until the tick arms it");
		LF_DownedPatient.Forget(slot);
		ctx.Equal(before, LF_DownedPatient.GetSlotCount(), "a forgotten patient is not tracked");
	}
}
