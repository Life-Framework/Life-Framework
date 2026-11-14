// red-proof: observed 2026-11-14 with `tools/cli test --no-build --only LF_Test_SystemsServices`: removing each system's gate (the Refuses/IsEnabled call at its entry point, one per system) failed one line each: `- Comms off: a service call is refused`, `- Chat off: the base game's chat panels are left alone`, `- Medic off: nobody is downed`, `- Crime off: a crime is not recorded`, `- Police off: a crime is not dispatched`, `- Investigation off: no witness scene opens`, `- Vehicles off: with the off caption (expected '#LF-Systems_Off', got '#EL-Vehicle_CannotAfford')`, `- VehicleLock off: the doors are open`, `- LicensePlate off: no plate is issued`, `- SirenLights off: the siren does not start`, `- Quests off: a quest does not progress`. Restored; green.
// tier: WORLD
//! Systems (#1076): the service systems read the switch. With each of Comms, Chat, Medic, Police,
//! Crime, Investigation, Vehicles, VehicleLock, LicensePlate, SirenLights and Quests off in the
//! owner config, one server entry point of it refuses (or one prompt hides) and what it saved stays;
//! with it on again the same call goes through. Police and Investigation are turned off with what
//! they need turned off too: a config that cut Crime under Police would be refused (#279).
class LF_Test_SystemsServices : LF_Test
{
	static const ResourceName POLICE_CAR = "{EFEB161F9BC2031D}Prefabs/Vehicles/Wheeled/UAZ469/LF_UAZ469_Police.et";
	static const ResourceName HANDCUFFS = "{DE0B6FEBDCECC6FF}Prefabs/Items/Roleplay/Handcuffs.et";
	protected static const string UID = "systems-services-uid";

	override string GetName()
	{
		return "systems/services";
	}

	//------------------------------------------------------------------------------------------------
	//! The owner turns these systems off (comma separated ids; empty: none).
	protected void Off(string ids)
	{
		LF_ConfigService.Reset();
		LF_ConfigStream.Reset();
		if (!ids.IsEmpty())
		{
			array<string> list = {};
			ids.Split(",", list, true);
			string json = "";
			foreach (string id : list)
			{
				if (!json.IsEmpty())
					json += ",";
				json += "\"" + id + "\":{\"enabled\":false}";
			}
			LF_ConfigService.LoadOwnerString("{\"schemaVersion\":1,\"systems\":{\"switches\":{" + json + "}}}", "test");
		}
		LF_Systems.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected bool Shown(ScriptedUserAction action, IEntity user)
	{
		return action.CanBeShownScript(user);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Off(string.Empty);
		IEntity user = LF_TestCharacterFixture.Spawn(ctx);
		ctx.NotNull(user, "a test character spawns");
		if (!user)
			return;

		RunComms(ctx, user);
		RunChat(ctx);
		RunMedic(ctx);
		RunCrime(ctx);
		RunPolice(ctx, user);
		RunInvestigation(ctx, user);
		RunVehicles(ctx, user);
		RunVehicleLock(ctx);
		RunLicensePlate(ctx);
		RunSirenLights(ctx);
		RunQuests(ctx, user);

		Off(string.Empty);
		LF_TestCharacterFixture.Delete(user);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunComms(LF_TestContext ctx, IEntity user)
	{
		array<ref LF_ServiceCandidate> nobody = {};
		Off(string.Empty);
		ctx.NotNull(LF_ServiceCalls.Place(UID, "police", "test place", "test", vector.Zero, nobody), "Comms on: a service call is placed");
		ctx.True(Shown(new LF_PayphoneAction(), user), "Comms on: the payphone offers its action");
		Off("Comms");
		ctx.True(LF_ServiceCalls.Place(UID, "police", "test place", "test", vector.Zero, nobody) == null, "Comms off: a service call is refused");
		ctx.False(Shown(new LF_PayphoneAction(), user), "Comms off: the payphone offers nothing");
		Off(string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunChat(LF_TestContext ctx)
	{
		Off(string.Empty);
		ctx.True(LF_ChatManagerComponent.HidesPanels(), "Chat on: the chat panels are kept hidden until opened");
		Off("Chat");
		ctx.False(LF_ChatManagerComponent.HidesPanels(), "Chat off: the base game's chat panels are left alone");
		Off(string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunMedic(LF_TestContext ctx)
	{
		Off(string.Empty);
		ctx.True(LF_Downed.Enabled(), "Medic on: a lethal hit downs a player");
		Off("Medic");
		ctx.False(LF_Downed.Enabled(), "Medic off: nobody is downed");
		Off(string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunCrime(LF_TestContext ctx)
	{
		array<ref LF_CrimeRecordRow> before = LF_CrimeRecord.ExportAll();
		LF_CrimeRecord.ApplyAll(null);
		Off(string.Empty);
		ctx.True(LF_CrimeRecord.Record(LF_OnCrimeCommitted.Create(UID, LF_WantedReports.OFFENCE_ROBBERY)), "Crime on: a crime is recorded");
		Off("Crime,Police,Investigation");
		ctx.False(LF_CrimeRecord.Record(LF_OnCrimeCommitted.Create(UID, LF_WantedReports.OFFENCE_ROBBERY)), "Crime off: a crime is not recorded");
		ctx.Equal(1, LF_CrimeRecord.Total(UID), "Crime off: the record that was saved stays");
		Off(string.Empty);
		LF_CrimeRecord.ApplyAll(before);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunPolice(LF_TestContext ctx, IEntity user)
	{
		Off(string.Empty);
		ctx.True(LF_Systems.IsItemAllowed(HANDCUFFS), "Police on: the kit has handcuffs");
		ctx.NotNull(LF_Dispatch.Report(UID, null, LF_WantedReports.OFFENCE_ROBBERY, vector.Zero, 1), "Police on: a crime is dispatched");
		ctx.True(Shown(new LF_DutyAction(), user), "Police on: the duty locker offers its action");
		Off("Police,Investigation");
		ctx.True(LF_Dispatch.Report(UID, null, LF_WantedReports.OFFENCE_ROBBERY, vector.Zero, 1) == null, "Police off: a crime is not dispatched");
		ctx.False(LF_Systems.IsItemAllowed(HANDCUFFS), "Police off: the kit leaves handcuffs out");
		ctx.False(Shown(new LF_DutyAction(), user), "Police off: the duty locker offers nothing");
		Off("Crime");
		ctx.True(LF_Systems.IsEnabled("Crime"), "Crime cannot be turned off under Police: it keeps its shipped value");
		Off(string.Empty);
		SCR_PlayerController.LF_ResetDispatchCount();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunInvestigation(LF_TestContext ctx, IEntity user)
	{
		Off(string.Empty);
		ctx.NotNull(LF_Witness.RecordCrime(1, UID, LF_WantedReports.OFFENCE_ROBBERY, user, "", "", "test place"), "Investigation on: witnesses of a crime are collected");
		Off("Investigation");
		ctx.True(LF_Witness.RecordCrime(1, UID, LF_WantedReports.OFFENCE_ROBBERY, user, "", "", "test place") == null, "Investigation off: no witness scene opens");
		Off("Police");
		ctx.True(LF_Systems.IsEnabled("Police") || !LF_Systems.IsEnabled("Investigation"), "Investigation needs Police: it is never on while Police is off");
		Off(string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunVehicles(LF_TestContext ctx, IEntity user)
	{
		array<ref LF_VehicleRecord> before = LF_VehicleOwnership.ExportAll();
		string id = "systems-services-car";
		Off(string.Empty);
		ctx.True(LF_VehicleOwnership.Register(id, UID, id, POLICE_CAR, POLICE_CAR, "shop", "AB 12 CD", "#EL-Vehicle_CannotAfford", true), "Vehicles on: a record registers");
		ctx.True(Shown(new LF_GarageLedgerAction(), user), "Vehicles on: the garage ledger offers its action");
		Off("Vehicles");
		string vehicleId;
		int cash;
		string reason;
		ctx.False(LF_VehicleOwnership.Buy(UID, POLICE_CAR, POLICE_CAR, 1, 1, "shop", "#EL-Vehicle_CannotAfford", vector.Zero, user, vehicleId, cash, reason), "Vehicles off: a purchase is refused");
		ctx.EqualStr("#LF-Systems_Off", reason, "Vehicles off: with the off caption");
		ctx.False(LF_VehicleOwnership.Store(id, UID), "Vehicles off: a store is refused");
		ctx.False(LF_VehicleOwnership.RestoreRecord(id, UID), "Vehicles off: a retrieve is refused");
		ctx.False(Shown(new LF_GarageLedgerAction(), user), "Vehicles off: the garage ledger offers nothing");
		LF_VehicleRecord kept = LF_VehicleOwnership.Export(id);
		ctx.NotNull(kept, "Vehicles off: the vehicle record stays");
		if (kept)
			ctx.EqualStr("AB 12 CD", kept.m_sPlate, "Vehicles off: and keeps its plate");
		Off(string.Empty);
		LF_VehicleOwnership.ApplyAll(before);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunVehicleLock(LF_TestContext ctx)
	{
		IEntity car = GetGame().GetWorld().FindEntityByName("Vehicle_LockTest");
		LF_VehicleLockComponent vehicleLock = LF_Component<LF_VehicleLockComponent>.Find(car);
		ctx.NotNull(vehicleLock, "the lock test vehicle is in DebugWorld");
		if (!vehicleLock)
			return;

		Off(string.Empty);
		vehicleLock.SetLocked(true);
		ctx.True(vehicleLock.IsLocked(null, null), "VehicleLock on: a locked vehicle is shut");
		Off("VehicleLock");
		ctx.False(vehicleLock.IsLocked(null, null), "VehicleLock off: the doors are open");
		ctx.True(vehicleLock.IsVehicleLocked(), "VehicleLock off: the saved lock state stays");
		Off(string.Empty);
		ctx.True(vehicleLock.IsLocked(null, null), "VehicleLock on again: the vehicle is shut as it was saved");
		vehicleLock.SetLocked(false);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunLicensePlate(LF_TestContext ctx)
	{
		Off(string.Empty);
		string plate = LF_LicensePlateRegistry.Issue(new LF_LicensePlateGeneratorGeneric(), "systems-services-plate");
		ctx.False(plate.IsEmpty(), "LicensePlate on: a plate is issued");
		LF_LicensePlateRegistry.Release(plate);
		Off("LicensePlate");
		ctx.True(LF_LicensePlateRegistry.Issue(new LF_LicensePlateGeneratorGeneric(), "systems-services-plate").IsEmpty(), "LicensePlate off: no plate is issued");
		Off(string.Empty);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunSirenLights(LF_TestContext ctx)
	{
		Resource res = Resource.Load(POLICE_CAR);
		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		IEntity car = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		LF_SirenManagerComponent siren = LF_Component<LF_SirenManagerComponent>.Find(car);
		ctx.NotNull(siren, "the police car carries a siren");
		if (!siren)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(car);
			return;
		}

		Off("SirenLights");
		ctx.False(siren.ApplyRequest(1, 1000), "SirenLights off: the siren does not start");
		ctx.False(siren.IsOn(), "SirenLights off: and stays dark");
		Off(string.Empty);
		ctx.True(siren.ApplyRequest(1, 5000), "SirenLights on: the siren starts");
		SCR_EntityHelper.DeleteEntityAndChildren(car);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunQuests(LF_TestContext ctx, IEntity user)
	{
		LF_QuestManager manager = LF_QuestManager.GetInstance();
		LF_QuestTrackerComponent tracker = LF_Component<LF_QuestTrackerComponent>.Find(user);
		ctx.NotNull(manager, "the quest manager exists");
		ctx.NotNull(tracker, "the test character tracks quests");
		if (!manager || !tracker)
			return;

		Off(string.Empty);
		tracker.AcceptQuest("intro_talk");
		Off("Quests");
		manager.ReportTalk(user, "Start");
		ctx.True(tracker.GetTracker().GetStatus("intro_talk") == LF_EQuestStatus.ACTIVE, "Quests off: a quest does not progress");
		Off(string.Empty);
		manager.ReportTalk(user, "Start");
		ctx.True(tracker.GetTracker().GetStatus("intro_talk") == LF_EQuestStatus.COMPLETED, "Quests on again: the saved quest was still active and completes");
	}
}
