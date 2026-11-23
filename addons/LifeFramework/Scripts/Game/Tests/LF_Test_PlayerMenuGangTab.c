// red-proof: observed 2026-11-23 `tools/cli test --no-build --red LF_Test_PlayerMenuGangTab`: RED PROOF OK (with the hook on the gang flies no flag: the flag key, the flag on the wire and the printed flag name all fail). Green with `--only`.
//! The F2 Gang tab (#4456, gangs C13), no world: a member's tab carries the gang's name, flag name, the
//! member's own rank and the gang's members who are online, one answer that round-trips on its wire; a
//! player in no gang has no tab and no answer; another gang's name, flag and members are never on a
//! member's wire; and no standing (number or word) is printed anywhere on the tab. The test puts back
//! the organizations and flags it found.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_PlayerMenuGangTab : LF_Test
{
	protected static const string ORG_A = "gangtab-test-a";
	protected static const string ORG_B = "gangtab-test-b";
	protected static const string CHAR_FOUNDER = "gangtab-founder";
	protected static const string CHAR_MEMBER = "gangtab-member";
	protected static const string CHAR_OFFLINE = "gangtab-offline";
	protected static const string CHAR_RIVAL = "gangtab-rival";
	protected static const string CHAR_LONER = "gangtab-loner";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "playermenu/gang-tab";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_TestLegacySnapshot kept = LF_TestLegacySnapshot.Take();
		array<ref LF_OrgFlagRow> keptFlags = LF_OrgFlags.ExportAll();

		Register();
		// RedProof(): the gang flies no flag (`--red`).
		if (!RedProof())
			LF_OrgFlags.Set(ORG_A, 5);

		LF_OrgFlags.Set(ORG_B, 9);

		CheckMember(ctx);
		CheckNoGang(ctx);

		LF_OrganizationManager.Reset();
		kept.Restore();
		LF_OrgFlags.ResetStatics();
		LF_OrgFlags.ApplyAll(keptFlags);
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
	//! Gang A: a founder, an online member and an offline member. Gang B: its own founder and a rival.
	protected static void Register()
	{
		array<ref LF_OrganizationRecord> records = {};
		LF_OrganizationRecord a = Record(ORG_A, "North Dock", CHAR_FOUNDER, CHAR_MEMBER);
		a.m_aMembers.Insert(LF_OrganizationMemberRecord.Create(CHAR_OFFLINE, LF_OrganizationConfig.RANK_MEMBER));
		records.Insert(a);
		records.Insert(Record(ORG_B, "Quarry Kings", CHAR_RIVAL, ""));
		LF_OrganizationManager.Reset();
		LF_OrganizationManager.ApplyAll(records);
		LF_OrgFlags.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_UIPart Find(notnull array<ref LF_UIPart> parts, string name)
	{
		foreach (LF_UIPart part : parts)
		{
			if (part.GetName() == name)
				return part;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckMember(LF_TestContext ctx)
	{
		array<string> online = {CHAR_FOUNDER, CHAR_MEMBER, CHAR_RIVAL};
		map<string, string> names = new map<string, string>();
		names.Set(CHAR_FOUNDER, "Ada Quill");
		names.Set(CHAR_MEMBER, "Bea Marsh");
		names.Set(CHAR_RIVAL, "Rex Rival");

		LF_PlayerMenuGangData data = LF_PlayerMenuGangTab.Gather(CHAR_MEMBER, online, names, LF_EPlayerMenuGangRadio.ON);
		ctx.NotNull(data, "a member of a gang gets crew facts");
		if (!data)
			return;

		ctx.EqualStr("North Dock", data.m_sName, "the gang's name");
		ctx.Equal(5, data.m_iFlag, "the gang's flag");
		ctx.EqualStr("#LF-OrgFlag_Name_05", LF_PlayerMenuGangTab.FlagKey(data.m_iFlag), "the flag prints through its name key");
		ctx.EqualStr("Member", data.m_sRank, "the member's own rank");
		ctx.Equal(2, data.m_aOnlineNames.Count(), "two of the gang are online; the offline member and the rival are not listed");
		ctx.True(data.m_aOnlineNames.Find("Ada Quill") >= 0 && data.m_aOnlineNames.Find("Bea Marsh") >= 0, "the online members are named");
		ctx.True(data.m_aOnlineNames.Find("Rex Rival") < 0, "another gang's member is never listed");

		// The wire carries the same facts and none of the other gang's.
		string wire = data.Encode();
		ctx.False(wire.Contains("Quarry Kings") || wire.Contains("Rex Rival"), "another gang's name and members are not on the wire");
		LF_PlayerMenuGangData back = LF_PlayerMenuGangData.Decode(wire);
		ctx.NotNull(back, "the wire decodes");
		if (!back)
			return;

		ctx.EqualStr("North Dock", back.m_sName, "name survives the wire");
		ctx.Equal(5, back.m_iFlag, "flag survives the wire");
		ctx.EqualStr("Member", back.m_sRank, "rank survives the wire");
		ctx.Equal(LF_EPlayerMenuGangRadio.ON, back.m_eRadio, "radio state survives the wire");
		ctx.Equal(2, back.m_aOnlineNames.Count(), "online members survive the wire");

		array<ref LF_UIPart> parts = {};
		LF_PlayerMenuGangTab.Build(parts, back);
		LF_UIPart name = Find(parts, LF_PlayerMenuGangTab.PART_NAME);
		LF_UIPart flag = Find(parts, LF_PlayerMenuGangTab.PART_FLAG);
		LF_UIPart rank = Find(parts, LF_PlayerMenuGangTab.PART_RANK);
		LF_UIPart radio = Find(parts, LF_PlayerMenuGangTab.PART_RADIO);
		ctx.True(name && name.GetStatus() == "North Dock", "the tab prints the gang's name");
		ctx.True(flag && flag.GetStatus() == "#LF-OrgFlag_Name_05", "the tab prints the flag's name");
		ctx.True(rank && rank.GetStatus() == "Member", "the tab prints the member's rank");
		ctx.True(radio && radio.GetStatus() == "#LF-PlayerMenuGang_RadioOn", "the tab says the radio is on the gang channel");
		ctx.True(Find(parts, LF_PlayerMenuGangTab.ROW_ONLINE + "0") != null && Find(parts, LF_PlayerMenuGangTab.ROW_ONLINE + "1") != null, "one row per online member");
		ctx.True(Find(parts, LF_PlayerMenuGangTab.ROW_SITE + "none") != null, "a gang with no site says so");

		// No standing anywhere: no standing row, no standing icon, no standing word on the tab.
		foreach (LF_UIPart part : parts)
		{
			string text = part.GetName() + " " + part.GetText() + " " + part.GetStatus() + " " + part.GetIcon();
			text.ToLower();
			ctx.False(text.Contains("standing"), "no standing on the tab: " + part.GetName());
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckNoGang(LF_TestContext ctx)
	{
		array<string> online = {CHAR_LONER, CHAR_FOUNDER};
		map<string, string> names = new map<string, string>();
		ctx.True(LF_PlayerMenuGangTab.Gather(CHAR_LONER, online, names, LF_EPlayerMenuGangRadio.NONE) == null, "a player in no gang gets no crew facts");
		ctx.True(LF_PlayerMenuGangTab.Gather("", online, names, LF_EPlayerMenuGangRadio.NONE) == null, "no character, no crew facts");
		ctx.True(LF_PlayerMenuGangData.Decode("") == null, "an empty answer decodes to no tab data");
		ctx.True(LF_PlayerMenuGangData.Decode("V|1;G||0||0") == null, "a gang record with no name decodes to no tab data");

		ctx.False(LF_PlayerMenuGangTab.HasTab(null), "no view, no tab");
		ctx.False(LF_PlayerMenuGangTab.HasTab(LF_OrgView.Create("", "", 0, "", 0)), "a player in no gang has no Gang tab");
		ctx.True(LF_PlayerMenuGangTab.HasTab(LF_OrgView.Create(ORG_A, "North Dock", 0, "", 0)), "a member has the Gang tab");

		array<ref LF_UIPart> waiting = {};
		LF_PlayerMenuGangTab.Build(waiting, null);
		ctx.True(Find(waiting, LF_PlayerMenuGangTab.PART_WAITING) != null, "before the answer the tab prints a waiting line");
		ctx.True(Find(waiting, LF_PlayerMenuGangTab.PART_NAME) == null, "and no facts");
	}
}
