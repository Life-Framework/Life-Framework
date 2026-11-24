// red-proof: 2026-11-24 (#4848) `tools/cli test --no-build --red LF_Test_CasualtyTreat`: RED PROOF OK; "a downed resident who is no casualty is not labelled Treat the patient" fails with the hook on (the old label rule marked every downed NPC).
// red-proof: 2026-11-24 (#4852) the board-line checks fail on the old code (a closed call stayed on the medic board until its expiry).
// red-proof: 2026-11-24 `tools/cli test --no-build --red LF_Test_CasualtyTreat`: RED PROOF OK (the revive bonus check: expected 10, got 0; "MEDIC xp rose", "a second treat finds no casualty"). Green with --only.
//! medic/casualty-treat (#4828): an on-duty medic beside a casualty from the generator (#4827) treats them:
//! the patient wakes, the call closes (off the open list), the dressing is used up, one $10 revive bonus is
//! pending and paid at clock-out, and MEDIC xp rose. A second treat on the same casualty pays nothing. An
//! off-duty medic is refused and earns nothing. A casualty the medic downed is treated but pays nothing.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_CasualtyTreat : LF_Test
{
	protected static const string KIND = "test_casualty_treat_site";
	protected static const string MEDIC_UID = "casualty-treat-medic";
	protected static const int MEDIC_ID = 9482;

	protected ref array<IEntity> m_aBodies = {};

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "medic/casualty-treat";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_CasualtyCalls.ClearAll();
		LF_CasualtyCalls.ResetStatics();
		LF_CasualtyTreat.ResetStatics();
		LF_DutyBonus.ResetStatics();
		LF_CasualtyCallConfig config = new LF_CasualtyCallConfig();
		config.m_bEnabled = true;
		config.m_iIntervalSeconds = 1;
		config.m_iPerMedic = 1;
		config.m_iMaxOpen = 2;
		config.m_fSpotRadius = 3;
		config.m_sPrefab = LF_CasualtyCallConfig.DEFAULT_PREFAB;
		config.m_aRows = {};
		config.m_aRows.Insert(LF_CasualtyCallRow.Make(KIND, "test casualty: treat me", 1));
		LF_CasualtyCallConfig.Override(config);
		LF_ServiceBoard.Install();
		LF_MedicDutyBonus.Install();
		LF_JobLevelSources.EnsureSubscribed();

		IEntity medic = LF_TestCharacterFixture.Spawn(ctx);
		if (medic)
		{
			LF_Utils.SetUidForTests(medic, MEDIC_UID);
			bool registered = LF_Places.Register(KIND, "treat-1", medic);
			ctx.True(registered, "the test place registers");
			if (registered)
				RunTreat(ctx, medic);
			LF_Places.Unregister(KIND, "treat-1");
		}

		foreach (IEntity body : m_aBodies)
		{
			if (body)
				SCR_EntityHelper.DeleteEntityAndChildren(body);
		}
		m_aBodies.Clear();

		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(MEDIC_UID);
		account.SetOnDuty(false);
		LF_Utils.SetUidForTests(null, "");
		LF_CasualtyCalls.ClearAll();
		LF_TestCharacterFixture.Delete(medic);
		LF_CasualtyTreat.ResetStatics();
		LF_CasualtyCalls.ResetStatics();
		LF_CasualtyCallConfig.ResetStatics();
		LF_DutyBonus.ResetStatics();
		LF_Downed.ResetStatics();
		LF_CasualtyCalls.Start();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunTreat(LF_TestContext ctx, IEntity medic)
	{
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(MEDIC_UID);
		account.SetFaction(LF_Faction.MEDIC);
		account.SetOnDuty(false);
		LF_MedicConfig medicConfig = LF_MedicConfig.Get();
		int xpBefore = LF_JobLevels.XpOf(MEDIC_UID, "MEDIC");

		int now = System.GetTickCount();
		IEntity body = Raise(ctx, medic, now);
		if (!body)
			return;

		ctx.Equal(1, BoardLines(now), "the casualty's call is on the medic board");

		// The client labels from the replicated flag (#4848): a generator casualty reads "Treat the patient", any other
		// downed resident (here a downed fixture character no player controls) keeps the plain revive.
		SCR_ChimeraCharacter casualtyBody = SCR_ChimeraCharacter.Cast(body);
		ctx.True(casualtyBody && casualtyBody.LF_IsCasualty(), "the generator flags the casualty's body");
		ctx.True(LF_CasualtyTreat.LooksLikeCasualty(body), "a casualty looks like one on the client");
		IEntity bystander = LF_TestCharacterFixture.Spawn(ctx);
		SCR_ChimeraCharacter bystanderCharacter = SCR_ChimeraCharacter.Cast(bystander);
		if (bystanderCharacter)
		{
			bystanderCharacter.LF_SetDowned(true, "");
			if (RedProof())
				bystanderCharacter.LF_SetCasualty(true);
			ctx.False(LF_CasualtyTreat.LooksLikeCasualty(bystander), "a downed resident who is no casualty is not labelled Treat the patient");
			ctx.True(LF_CasualtyTreat.Find(bystander) == null, "the server finds no casualty on that body");
			bystanderCharacter.LF_SetDowned(false, "");
		}
		LF_TestCharacterFixture.Delete(bystander);

		LF_InventoryUtils.AddAmount(medic, medicConfig.m_sReviveItemPrefab, 2);

		// Off duty: refused, nothing changes.
		ctx.Equal(LF_EReviveRefusal.NOT_MEDIC, LF_CasualtyTreat.Perform(medic, body), "an off-duty player is refused");
		ctx.True(LF_Downed.IsDowned(body), "the casualty is still down after the refusal");
		ctx.Equal(1, LF_CasualtyCalls.Open().Count(), "the call stays open after the refusal");
		ctx.Equal(0, LF_DutyBonus.Pending(account), "an off-duty player earns no bonus");

		// On duty, beside the patient, with a dressing.
		account.SetOnDuty(true);
		LF_EReviveRefusal treated = LF_EReviveRefusal.NO_PATIENT;
		if (!RedProof())
			treated = LF_CasualtyTreat.Perform(medic, body);

		ctx.Equal(LF_EReviveRefusal.NONE, treated, "the on-duty medic treats the casualty");
		ctx.False(LF_Downed.IsDowned(body), "the patient is back on their feet");
		ctx.False(casualtyBody && casualtyBody.LF_IsCasualty(), "the casualty flag is off once the call closes");
		ctx.Equal(0, LF_CasualtyCalls.Open().Count(), "the call is closed");
		ctx.Equal(0, BoardLines(now), "the treated casualty's line is off the medic board");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(medic, medicConfig.m_sReviveItemPrefab), "one dressing is used up");
		ctx.Equal(LF_DutyBonus.AmountFor(LF_DutyBonusKind.REVIVE), LF_DutyBonus.Pending(account), "the revive bonus is pending");
		ctx.True(LF_JobLevels.XpOf(MEDIC_UID, "MEDIC") > xpBefore, "MEDIC xp rose");

		// The same casualty again: no casualty, no second bonus.
		ctx.Equal(LF_EReviveRefusal.NO_PATIENT, LF_CasualtyTreat.Perform(medic, body), "a second treat finds no casualty");
		ctx.Equal(LF_DutyBonus.AmountFor(LF_DutyBonusKind.REVIVE), LF_DutyBonus.Pending(account), "a second treat pays nothing");

		// Clock-out books the one bonus.
		ctx.Equal(LF_DutyBonus.AmountFor(LF_DutyBonusKind.REVIVE), LF_DutyPay.TakeBonus(account), "clock-out books one revive bonus");
		ctx.Equal(0, LF_DutyBonus.Pending(account), "nothing is left pending after clock-out");

		// A casualty the medic downed: treated, the call closes, no bonus.
		IEntity second = Raise(ctx, medic, now + 5000);
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(second);
		if (!character)
			return;

		character.LF_SetDowned(true, MEDIC_UID);
		ctx.Equal(LF_EReviveRefusal.NONE, LF_CasualtyTreat.Perform(medic, second), "the medic may treat a casualty they downed");
		ctx.Equal(0, LF_CasualtyCalls.Open().Count(), "that call closes too");
		ctx.Equal(0, LF_DutyBonus.Pending(account), "a casualty the medic downed pays nothing");
		ctx.Equal(1, LF_CasualtyTreat.GetUnpaidCount(), "the unpaid treat is counted");
	}

	//------------------------------------------------------------------------------------------------
	//! Medic board lines with this test's summary.
	protected int BoardLines(int nowMs)
	{
		array<string> entries = {};
		LF_ServiceBoard.BuildEntries("medic", nowMs, 0, entries);
		int count;
		foreach (string entry : entries)
		{
			if (entry.Contains("test casualty: treat me"))
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! One casualty from a generator tick, moved beside the medic. \return Its body, or null (failed).
	protected IEntity Raise(LF_TestContext ctx, IEntity medic, int nowMs)
	{
		array<ref LF_ServiceCandidate> medics = {};
		medics.Insert(LF_ServiceCandidate.Make(MEDIC_ID, false, LF_EJobType.MEDIC));
		LF_Casualty casualty = LF_CasualtyCalls.Tick(nowMs, medics);
		ctx.True(casualty != null, "the generator raises a casualty");
		if (!casualty)
			return null;

		IEntity body = LF_CasualtyCalls.Body(casualty);
		ctx.True(body != null, "the casualty has a body");
		if (!body)
			return null;

		m_aBodies.Insert(body);
		body.SetOrigin(medic.GetOrigin() + "1 0 0");
		ctx.True(LF_Downed.IsDowned(body), "the casualty lies downed");
		return body;
	}
}
