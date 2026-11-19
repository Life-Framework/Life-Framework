// red-proof: new test, no failing state on the old tree; RedProof() skips the Take act, so every "holds" and "paid" check fails (tools/cli test --no-build --red LF_Test_TrainingDesk).
//------------------------------------------------------------------------------------------------
//! qualifications/training-desk (#3426): an officer on duty finishes a course at the training desk,
//! pays the fee, holds the unlock and is told so. A civilian (off duty) is refused and pays nothing;
//! a short purse is refused; leaving mid-study (the server is never asked) charges and grants
//! nothing. The placed prefab carries one held action per shipped course.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_TrainingDesk : LF_Test
{
	protected static const string OFFICER_UID = "training-desk-officer";
	protected static const string CIVILIAN_UID = "training-desk-civilian";
	protected static const ResourceName DESK_PREFAB = "{F80964F9F2385F4F}Prefabs/Props/Police/LF_TrainingDesk.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "qualifications/training-desk";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_Qualifications.ClearAll();

		IEntity officer = LF_TestCharacterFixture.Spawn(ctx);
		IEntity civilian = LF_TestCharacterFixture.Spawn(ctx);
		if (!officer || !civilian)
		{
			LF_TestCharacterFixture.Delete(officer);
			LF_TestCharacterFixture.Delete(civilian);
			return;
		}

		LF_PlayerAccount officerAccount = LF_PlayerAccountManager.GetOrCreate(OFFICER_UID);
		officerAccount.SetFaction(LF_Faction.POLICE);
		officerAccount.SetPoliceRank(LF_PoliceRanks.SENIOR);
		officerAccount.SetOnDuty(true);
		LF_PoliceRanks.BindOfficer(officer, OFFICER_UID);

		LF_PlayerAccount civilianAccount = LF_PlayerAccountManager.GetOrCreate(CIVILIAN_UID);
		civilianAccount.SetFaction(LF_Faction.CIVILIAN);
		civilianAccount.SetOnDuty(false);
		LF_PoliceRanks.BindOfficer(civilian, CIVILIAN_UID);

		string officerKey = officerAccount.GetActiveCharacterKey();
		string civilianKey = civilianAccount.GetActiveCharacterKey();
		ctx.Equal(1000, LF_MoneyUtils.AddCash(officer, 1000), "the officer carries 1000");
		ctx.Equal(1000, LF_MoneyUtils.AddCash(civilian, 1000), "the civilian carries 1000");

		// Leaving mid-study: the hold never completes, the server is never asked.
		ctx.Equal(1000, LF_MoneyUtils.GetCash(officer), "leaving mid-study takes no fee");
		ctx.False(LF_Qualifications.Has(officerKey, "detective"), "leaving mid-study grants nothing");

		// A civilian, off duty, is refused with no charge and no grant.
		LF_QualificationVerdict refused;
		if (!RedProof())
			refused = LF_TrainingDesk.Take(civilian, "first_aid_course");
		else
			refused = new LF_QualificationVerdict();
		ctx.False(refused.m_bOk, "an off-duty civilian is refused");
		ctx.True(refused.m_sReason == LF_TrainingDesk.MSG_NOT_ON_DUTY, "the refusal says to clock in");
		ctx.Equal(1000, LF_MoneyUtils.GetCash(civilian), "the refused civilian paid nothing");
		ctx.False(LF_Qualifications.Has(civilianKey, "first_aid"), "the refused civilian holds nothing");

		// The officer finishes the detective course: fee taken, unlock held.
		LF_QualificationVerdict done;
		if (!RedProof())
			done = LF_TrainingDesk.Take(officer, "detective_course");
		else
			done = new LF_QualificationVerdict();
		ctx.True(done.m_bOk, "an eligible officer finishes the detective course");
		ctx.Equal(600, LF_MoneyUtils.GetCash(officer), "the 400 fee came out of the officer's cash");
		ctx.True(LF_Qualifications.Has(officerKey, "detective"), "the officer holds the detective unlock");
		ctx.True(LF_Qualifications.HasCourse(officerKey, "detective_course"), "the officer holds the detective course");

		// Taking it again is refused and charges nothing.
		LF_QualificationVerdict again = LF_TrainingDesk.Take(officer, "detective_course");
		ctx.False(again.m_bOk, "a held course is refused");
		ctx.True(again.m_sReason == LF_QualificationRules.REASON_HELD, "the refusal says it is held");
		ctx.Equal(600, LF_MoneyUtils.GetCash(officer), "a held course costs nothing again");

		// A short purse is refused, nothing taken.
		ctx.Equal(450, LF_MoneyUtils.RemoveCash(officer, 450), "the officer spends down to 150");
		LF_QualificationVerdict poor = LF_TrainingDesk.Take(officer, "paramedic_course");
		ctx.False(poor.m_bOk, "a course the officer cannot pay for is refused");
		ctx.Equal(150, LF_MoneyUtils.GetCash(officer), "a refused course takes nothing");

		// The placed prefab carries one held action per shipped course, each with a study hold.
		CheckPrefab(ctx);

		LF_Qualifications.ClearAll();
		officerAccount.SetOnDuty(false);
		officerAccount.SetFaction(LF_Faction.CIVILIAN);
		officerAccount.SetPoliceRank(LF_PoliceRanks.PATROL);
		LF_TestCharacterFixture.Delete(officer);
		LF_TestCharacterFixture.Delete(civilian);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckPrefab(LF_TestContext ctx)
	{
		Resource res = Resource.Load(DESK_PREFAB);
		ctx.True(res.IsValid(), "the training desk prefab loads");
		if (!res.IsValid())
			return;

		IEntity desk = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld());
		ctx.NotNull(desk, "the training desk spawns");
		if (!desk)
			return;

		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(desk.FindComponent(BaseActionsManagerComponent));
		ctx.NotNull(manager, "the training desk has an actions manager");
		if (manager)
		{
			array<BaseUserAction> actions = {};
			manager.GetActionsList(actions);
			int deskActions = 0;
			foreach (BaseUserAction action : actions)
			{
				LF_TrainingDeskAction deskAction = LF_TrainingDeskAction.Cast(action);
				if (!deskAction)
					continue;

				deskActions++;
				ctx.True(deskAction.GetActionDuration() > 0, "a training desk action is a held study");
			}

			ctx.Equal(4, deskActions, "the desk offers one action per shipped course");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(desk);
	}
}
