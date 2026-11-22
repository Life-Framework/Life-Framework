// red-proof: 2026-11-22, cli test --no-build --red LF_Test_MapAlarmRing,LF_Test_MapAlarmPlace: RED-OK (RedProof() skips OnCall, so no place rings).
//------------------------------------------------------------------------------------------------
//! map/alarm-place: a police robbery call rings the nearest map place everyone may see, within the
//! reach; a closer role-only place (the police armoury) is never put on the shared wire, and a robbery
//! with no place in reach rings nothing. Real LF_Places entries on spawned anchors.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_MapAlarmPlace : LF_Test
{
	static const string BANK_ID = "test.alarm.bank";
	static const string ARMOURY_ID = "test.alarm.armoury";

	override string GetName()
	{
		return "map/alarm-place";
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity Anchor(vector at)
	{
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = at;
		return GetGame().SpawnEntity(GenericEntity, GetGame().GetWorld(), params);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MapAlarms.ResetStatics();
		vector site = "3000 0 3000";
		IEntity bank = Anchor(site + "12 0 0");
		IEntity armoury = Anchor(site + "3 0 0");
		LF_Places.Register("Bank", BANK_ID, bank);
		LF_Places.Register("PoliceArmoury", ARMOURY_ID, armoury);

		LF_OnServiceCall call = LF_OnServiceCall.Create("u1", "police", "", LF_WantedReports.OFFENCE_BREAK_IN, "radio", site, null);
		string key;
		if (!RedProof())
			key = LF_MapAlarms.OnCall(call);

		ctx.EqualStr(LF_MapAlarmRules.Key("Bank", BANK_ID), key, "the vault break-in rings the bank, not the closer role-only armoury");
		int raised;
		ctx.Equal(LF_EMapAlarmState.IN_PROGRESS, LF_MapAlarms.StateOf("Bank", BANK_ID, raised), "every client reads the bank in progress");
		ctx.False(LF_MapAlarms.Snapshot().Contains(ARMOURY_ID), "the armoury never goes on the shared wire");

		LF_OnServiceCall far = LF_OnServiceCall.Create("u1", "police", "", LF_WantedReports.OFFENCE_ROBBERY, "radio", site + "200 0 0", null);
		ctx.EqualStr("", LF_MapAlarms.OnCall(far), "a robbery with no place in reach rings nothing");

		LF_MapAlarms.ClearAll();
		LF_MapAlarms.ResetStatics();
		LF_Places.Unregister("Bank", BANK_ID);
		LF_Places.Unregister("PoliceArmoury", ARMOURY_ID);
		SCR_EntityHelper.DeleteEntityAndChildren(bank);
		SCR_EntityHelper.DeleteEntityAndChildren(armoury);
	}
}
