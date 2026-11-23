// red-proof: observed 2026-11-23 with `tools\cli test --no-build --red LF_Test_OrgFlagWallActions` (batched with the pack's flag-slot test): RedProof() skips the wall spawn and the leader pick, so `the flag wall spawns` and `the leader's pick is taken` fail, RED-OK; green with the hook off (--only, 4 passed over both legs).
//! The registry flag wall (#4449, gangs C4): twenty actions, one per preset flag, "Fly the <name> flag". A
//! leader's pick changes the organization's flag; a member who is not a leader gets no pick; a flag held by
//! a gang with a member online is refused with the holder named. The test puts back the organizations and
//! flags it found.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_OrgFlagWallActions : LF_Test
{
	protected static const ResourceName WALL = "{E0EE8DD048907D65}Prefabs/Organizations/LF_OrgFlagWall.et";
	protected static const string ORG_A = "flagwall-test-a";
	protected static const string ORG_B = "flagwall-test-b";
	protected static const string LEADER_A = "flagwall-leader-a";
	protected static const string MEMBER_A = "flagwall-member-a";
	protected static const string LEADER_B = "flagwall-leader-b";
	protected static const string NOBODY = "flagwall-nobody";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "org/flag-wall-actions";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		CheckWallActions(ctx);

		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		array<ref LF_OrgFlagRow> keptFlags = LF_OrgFlags.ExportAll();

		CheckPicks(ctx);

		LF_OrganizationManager.Reset();
		kept.Restore();
		LF_OrgFlags.ResetStatics();
		LF_OrgFlagRules.ResetStatics();
		LF_OrgFlags.ApplyAll(keptFlags);
	}

	//------------------------------------------------------------------------------------------------
	//! The wall prefab carries 20 flag actions, flags 1 to 20 once each.
	protected void CheckWallActions(LF_TestContext ctx)
	{
		IEntity wall;
		if (!RedProof())
			wall = LF_Utils.SpawnEntityPrefab(WALL, "0 -50 0");

		ctx.True(wall != null, "the flag wall spawns");
		if (!wall)
			return;

		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(wall.FindComponent(BaseActionsManagerComponent));
		ctx.True(manager != null, "the wall has an actions manager");
		if (manager)
		{
			array<BaseUserAction> actions = {};
			manager.GetActionsList(actions);
			array<int> seen = {};
			foreach (BaseUserAction action : actions)
			{
				LF_OrgFlagWallAction flagAction = LF_OrgFlagWallAction.Cast(action);
				if (flagAction)
					seen.Insert(flagAction.GetFlag());
			}

			ctx.Equal(LF_OrgFlags.COUNT, seen.Count(), "the wall offers one action per preset flag");
			for (int flag = 1; flag <= LF_OrgFlags.COUNT; flag++)
			{
				ctx.Equal(1, CountOf(seen, flag), string.Format("flag %1 has exactly one action", flag));
			}
		}

		SCR_EntityHelper.DeleteEntityAndChildren(wall);
	}

	//------------------------------------------------------------------------------------------------
	protected static int CountOf(notnull array<int> values, int value)
	{
		int count;
		foreach (int each : values)
		{
			if (each == value)
				count++;
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_OrganizationRecord Record(string id, string name, string founder, string member)
	{
		LF_OrganizationRecord record = LF_OrganizationRecord.Create(id, name, 0, false, false, false);
		record.m_aRanks.Insert(LF_OrganizationRankRecord.Create(LF_OrganizationConfig.RANK_FOUNDER, "Founder", true, true, true, true));
		record.m_aRanks.Insert(LF_OrganizationRankRecord.Create(LF_OrganizationConfig.RANK_MEMBER, "Member", false, false, false, false));
		record.m_aMembers.Insert(LF_OrganizationMemberRecord.Create(founder, LF_OrganizationConfig.RANK_FOUNDER));
		if (!member.IsEmpty())
			record.m_aMembers.Insert(LF_OrganizationMemberRecord.Create(member, LF_OrganizationConfig.RANK_MEMBER));

		return record;
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckPicks(LF_TestContext ctx)
	{
		array<ref LF_OrganizationRecord> records = {};
		records.Insert(Record(ORG_A, "Wall Gang A", LEADER_A, MEMBER_A));
		records.Insert(Record(ORG_B, "Wall Gang B", LEADER_B, string.Empty));
		LF_OrganizationManager.Reset();
		LF_OrganizationManager.ApplyAll(records);
		LF_OrgFlags.ResetStatics();
		LF_OrgFlagRules.ResetStatics();

		array<string> nobodyOnline = {};
		array<string> bOnline = {LEADER_B};
		string holder;

		// Who is offered the pick.
		ctx.True(LF_OrgFlagWall.Offers(LF_OrgView.Create(ORG_A, "Wall Gang A", LF_OrgRules.RightsOf(LF_OrganizationManager.Get(ORG_A), LEADER_A), "", 0)), "the leader is offered the pick");
		ctx.False(LF_OrgFlagWall.Offers(LF_OrgView.Create(ORG_A, "Wall Gang A", LF_OrgRules.RightsOf(LF_OrganizationManager.Get(ORG_A), MEMBER_A), "", 0)), "a member who is not a leader gets no pick");
		ctx.False(LF_OrgFlagWall.Offers(LF_OrgView.Create("", "", 0, "", 0)), "a player in no gang gets no pick");

		// A leader's pick changes the organization's flag.
		int before = LF_OrgFlags.FlagOf(ORG_A);
		int wanted = 7;
		if (before == wanted)
			wanted = 8;

		LF_EOrgFlagWallResult result;
		if (!RedProof())
			result = LF_OrgFlagWall.Pick(LEADER_A, wanted, nobodyOnline, holder);

		ctx.True(result == LF_EOrgFlagWallResult.PICKED, "the leader's pick is taken");
		ctx.Equal(wanted, LF_OrgFlags.FlagOf(ORG_A), "the gang's flag changed to the picked flag");
		ctx.True(LF_OrgFlagWall.Pick(LEADER_A, wanted, nobodyOnline, holder) == LF_EOrgFlagWallResult.KEPT, "picking the flag the gang already flies changes nothing");

		// A non-leader changes nothing.
		int flagBefore = LF_OrgFlags.FlagOf(ORG_A);
		ctx.True(LF_OrgFlagWall.Pick(MEMBER_A, 12, nobodyOnline, holder) == LF_EOrgFlagWallResult.NOT_LEADER, "a member who is not a leader is refused");
		ctx.Equal(flagBefore, LF_OrgFlags.FlagOf(ORG_A), "the refused member did not change the flag");
		ctx.True(LF_OrgFlagWall.Pick(NOBODY, 12, nobodyOnline, holder) == LF_EOrgFlagWallResult.NO_ORG, "a player in no gang is refused");
		ctx.True(LF_OrgFlagWall.Pick(LEADER_A, 0, nobodyOnline, holder) == LF_EOrgFlagWallResult.BAD_FLAG, "flag 0 is not a preset");
		ctx.True(LF_OrgFlagWall.Pick(LEADER_A, LF_OrgFlags.COUNT + 1, nobodyOnline, holder) == LF_EOrgFlagWallResult.BAD_FLAG, "a flag past the last preset is refused");

		// The lock: B may not take A's flag while A is online, and the holder is named.
		array<string> aOnline = {LEADER_A};
		ctx.True(LF_OrgFlagWall.Pick(LEADER_B, wanted, aOnline, holder) == LF_EOrgFlagWallResult.HELD, "a flag held by a gang with a member online is refused");
		ctx.EqualStr("Wall Gang A", holder, "the refusal names the holder");
		ctx.True(LF_OrgFlags.FlagOf(ORG_B) != wanted, "the refused gang did not get the flag");
		ctx.EqualStr("#LF-OrgFlagWall_Held", LF_OrgFlagWall.MessageKey(LF_EOrgFlagWallResult.HELD), "the refusal has its own words");
		ctx.True(LF_OrgFlagWall.Pick(LEADER_B, wanted, bOnline, holder) == LF_EOrgFlagWallResult.PICKED, "the same flag is open once the holder has nobody online");
	}
}
