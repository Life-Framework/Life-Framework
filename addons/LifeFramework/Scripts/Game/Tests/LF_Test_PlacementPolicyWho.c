// tier: LOGIC
// red-proof: 2026-11-18, `tools/cli test --no-build --red LF_Test_PlacementPolicyWho` printed RED PROOF OK (the RedProof hook skips filling the organization facts, so the officer, founder and unknown-rank assertions failed with 'is not in an organization (rule ORG_MEMBER)'); with the hook off it passes, and PlacementConfirm, PlacementComposition, SpikeStripPreview and Roadblock stay green.
//! placement/policy-who (#2446): each `who` rule of the placement policy refuses the wrong player with
//! its own reason and toast and lets the right one place. The spike strip and the roadblock still need an
//! on-duty officer, now from Configs/Placement/LF_PlacementPolicy.conf. Pure: the rules judge
//! LF_PlacementWhoFacts, so no world, account or entity is needed.
class LF_Test_PlacementPolicyWho : LF_Test
{
	protected static const string ORG = "policy-org";
	protected static const string FOUNDER = "policy-founder";
	protected static const string OFFICER = "policy-officer";
	protected static const string MEMBER = "policy-member";
	protected static const string STRANGER = "policy-stranger";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "placement/policy-who";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_PlacementPolicyConfig.Reload();
		RunPolice(ctx);
		RunHouse(ctx);
		RunOrg(ctx);
		RunAnyone(ctx);
		RunShippedTable(ctx);
		RunTableOverrides(ctx);
		LF_PlacementPolicyConfig.Reload();
	}

	//------------------------------------------------------------------------------------------------
	protected void RunPolice(LF_TestContext ctx)
	{
		string toast;
		LF_PlacementWhoFacts civilian = new LF_PlacementWhoFacts();
		string why = LF_PlacementPolicy.Judge(LF_EPlacementWho.ON_DUTY_POLICE, civilian, toast);
		ctx.True(why.IndexOf("on-duty police") != -1 && why.IndexOf("ON_DUTY_POLICE") != -1, "an off-duty player is refused, the reason names the rule: " + why);
		ctx.EqualStr(LF_PlacementPolicy.TOAST_POLICE, toast, "and the toast names it");

		LF_PlacementWhoFacts officer = new LF_PlacementWhoFacts();
		officer.m_bOnDuty = true;
		ctx.EqualStr("", LF_PlacementPolicy.Judge(LF_EPlacementWho.ON_DUTY_POLICE, officer, toast), "an on-duty officer places");
		ctx.EqualStr("", toast, "with no toast");

		// A house key or an organization does not make a police officer.
		LF_PlacementWhoFacts keyHolder = new LF_PlacementWhoFacts();
		keyHolder.m_bKeyHolder = true;
		keyHolder.m_sHouseId = "h1";
		ctx.True(!LF_PlacementPolicy.Judge(LF_EPlacementWho.ON_DUTY_POLICE, keyHolder, toast).IsEmpty(), "a key holder is not on-duty police");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunHouse(LF_TestContext ctx)
	{
		string toast;
		array<string> noKeys = {};
		array<string> keys = {"house-7"};

		LF_PlacementWhoFacts none = new LF_PlacementWhoFacts();
		LF_PlacementPolicy.FillHouse("", "", "char-a", noKeys, none);
		string why = LF_PlacementPolicy.Judge(LF_EPlacementWho.HOUSE_OWNER_OR_KEY, none, toast);
		ctx.True(why.IndexOf("no house") != -1 && why.IndexOf("HOUSE_OWNER_OR_KEY") != -1, "away from every house is refused, the reason says so: " + why);
		ctx.EqualStr(LF_PlacementPolicy.TOAST_HOUSE, toast, "with the house toast");

		LF_PlacementWhoFacts stranger = new LF_PlacementWhoFacts();
		LF_PlacementPolicy.FillHouse("house-7", "char-owner", "char-a", noKeys, stranger);
		why = LF_PlacementPolicy.Judge(LF_EPlacementWho.HOUSE_OWNER_OR_KEY, stranger, toast);
		ctx.True(why.IndexOf("neither the owner nor a key holder") != -1 && why.IndexOf("house-7") != -1, "a stranger at the house is refused, the reason names the house: " + why);

		LF_PlacementWhoFacts wrongKey = new LF_PlacementWhoFacts();
		LF_PlacementPolicy.FillHouse("house-8", "char-owner", "char-a", keys, wrongKey);
		ctx.True(!LF_PlacementPolicy.Judge(LF_EPlacementWho.HOUSE_OWNER_OR_KEY, wrongKey, toast).IsEmpty(), "a key to another house does not open this one");

		LF_PlacementWhoFacts owner = new LF_PlacementWhoFacts();
		LF_PlacementPolicy.FillHouse("house-7", "char-owner", "char-owner", noKeys, owner);
		ctx.EqualStr("", LF_PlacementPolicy.Judge(LF_EPlacementWho.HOUSE_OWNER_OR_KEY, owner, toast), "the owner places");

		LF_PlacementWhoFacts holder = new LF_PlacementWhoFacts();
		LF_PlacementPolicy.FillHouse("house-7", "char-owner", "char-a", keys, holder);
		ctx.EqualStr("", LF_PlacementPolicy.Judge(LF_EPlacementWho.HOUSE_OWNER_OR_KEY, holder, toast), "a key holder places");

		// A house nobody owns lets nobody in by ownership: an empty owner key matches no one.
		LF_PlacementWhoFacts unowned = new LF_PlacementWhoFacts();
		LF_PlacementPolicy.FillHouse("house-7", "", "", noKeys, unowned);
		ctx.True(!LF_PlacementPolicy.Judge(LF_EPlacementWho.HOUSE_OWNER_OR_KEY, unowned, toast).IsEmpty(), "an empty character key does not own an unowned house");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunOrg(LF_TestContext ctx)
	{
		LF_Organization org = LF_Organization.CreateNew(ORG, "Policy Crew", FOUNDER);
		ctx.True(org.AddMember(OFFICER, LF_OrganizationConfig.RANK_OFFICER), "the officer joins the fixture organization");
		ctx.True(org.AddMember(MEMBER, LF_OrganizationConfig.RANK_MEMBER), "the member joins the fixture organization");

		string toast;

		// Any member.
		LF_PlacementWhoFacts outsider = new LF_PlacementWhoFacts();
		if (!RedProof())
			LF_PlacementPolicy.FillOrg(org, STRANGER, "", outsider);
		string why = LF_PlacementPolicy.Judge(LF_EPlacementWho.ORG_MEMBER, outsider, toast);
		ctx.True(why.IndexOf("not in an organization") != -1 && why.IndexOf("ORG_MEMBER") != -1, "a non-member is refused, the reason names the rule: " + why);
		ctx.EqualStr(LF_PlacementPolicy.TOAST_ORG, toast, "with the organization toast");

		LF_PlacementWhoFacts member = Org(org, MEMBER, "");
		ctx.EqualStr("", LF_PlacementPolicy.Judge(LF_EPlacementWho.ORG_MEMBER, member, toast), "with no minimum rank any member places");

		// A minimum rank: officer and above.
		why = LF_PlacementPolicy.Judge(LF_EPlacementWho.ORG_MEMBER, Org(org, MEMBER, LF_OrganizationConfig.RANK_OFFICER), toast);
		ctx.True(why.IndexOf("ranks below officer") != -1, "a member below the minimum rank is refused, the reason names the rank: " + why);
		ctx.EqualStr(LF_PlacementPolicy.TOAST_RANK, toast, "with the rank toast");
		ctx.EqualStr("", LF_PlacementPolicy.Judge(LF_EPlacementWho.ORG_MEMBER, Org(org, OFFICER, LF_OrganizationConfig.RANK_OFFICER), toast), "an officer meets an officer minimum");
		ctx.EqualStr("", LF_PlacementPolicy.Judge(LF_EPlacementWho.ORG_MEMBER, Org(org, FOUNDER, LF_OrganizationConfig.RANK_OFFICER), toast), "the founder, above it, places");

		// A minimum rank the organization does not have refuses everyone and says why.
		why = LF_PlacementPolicy.Judge(LF_EPlacementWho.ORG_MEMBER, Org(org, FOUNDER, "warlord"), toast);
		ctx.True(why.IndexOf("warlord") != -1, "a minimum rank the organization lacks refuses even the founder, naming it: " + why);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_PlacementWhoFacts Org(LF_Organization org, string characterId, string minRank)
	{
		LF_PlacementWhoFacts facts = new LF_PlacementWhoFacts();
		if (!RedProof())
			LF_PlacementPolicy.FillOrg(org, characterId, minRank, facts);

		return facts;
	}

	//------------------------------------------------------------------------------------------------
	protected void RunAnyone(LF_TestContext ctx)
	{
		string toast;
		ctx.EqualStr("", LF_PlacementPolicy.Judge(LF_EPlacementWho.ANYONE, new LF_PlacementWhoFacts(), toast), "anyone places an ANYONE kind");
		ctx.EqualStr("", toast, "with no toast");
	}

	//------------------------------------------------------------------------------------------------
	//! The shipped conf: the spike strip and the roadblock (the kit's prefab and the set's name) need an
	//! on-duty officer, whatever rule the prefab itself names; a kind with no entry keeps its own.
	protected void RunShippedTable(LF_TestContext ctx)
	{
		string minRank;
		ctx.True(LF_PlacementPolicyConfig.Get() != null, "the shipped placement policy loads");
		ctx.True(LF_PlacementPolicy.RuleFor("SpikeStrip", LF_EPlacementWho.ANYONE, minRank) == LF_EPlacementWho.ON_DUTY_POLICE, "the spike strip needs an on-duty officer");
		ctx.True(LF_PlacementPolicy.RuleFor("Roadblock_Barrier", LF_EPlacementWho.ANYONE, minRank) == LF_EPlacementWho.ON_DUTY_POLICE, "the roadblock kit needs an on-duty officer");
		ctx.True(LF_PlacementPolicy.RuleFor("roadblock", LF_EPlacementWho.ANYONE, minRank) == LF_EPlacementWho.ON_DUTY_POLICE, "the roadblock set needs an on-duty officer");
		ctx.True(LF_PlacementPolicy.RuleFor("Chair", LF_EPlacementWho.ANYONE, minRank) == LF_EPlacementWho.ANYONE, "a kind with no entry keeps its own rule (anyone)");
		ctx.True(LF_PlacementPolicy.RuleFor("Chair", LF_EPlacementWho.ON_DUTY_POLICE, minRank) == LF_EPlacementWho.ON_DUTY_POLICE, "a kind with no entry keeps its own rule (police)");

		ctx.EqualStr("SpikeStrip", LF_PlacementPolicy.KindOfPrefab("{8FEC063EE8B0100B}Prefabs/Props/Roleplay/SpikeStrip/SpikeStrip.et"), "a placed prefab's kind is its file name");

		// The whole path with no player: an absent player is not an officer.
		string toast;
		string why = LF_PlacementPolicy.Check(null, "SpikeStrip", LF_EPlacementWho.ANYONE, toast);
		ctx.True(why.IndexOf("on-duty police") != -1, "Check refuses a spike strip with nobody to place it: " + why);
	}

	//------------------------------------------------------------------------------------------------
	//! A table entry replaces the prefab's own rule and carries the minimum rank.
	protected void RunTableOverrides(LF_TestContext ctx)
	{
		LF_PlacementPolicyConfig config = new LF_PlacementPolicyConfig();
		config.m_aEntries = new array<ref LF_PlacementPolicyEntry>();
		LF_PlacementPolicyEntry entry = new LF_PlacementPolicyEntry();
		entry.m_sKind = "Stall";
		entry.m_eWho = LF_EPlacementWho.ORG_MEMBER;
		entry.m_sMinRank = LF_OrganizationConfig.RANK_OFFICER;
		config.m_aEntries.Insert(entry);
		LF_PlacementPolicyConfig.UseForTest(config);

		string minRank;
		ctx.True(LF_PlacementPolicy.RuleFor("Stall", LF_EPlacementWho.ANYONE, minRank) == LF_EPlacementWho.ORG_MEMBER, "the table's entry replaces the prefab's own rule");
		ctx.EqualStr(LF_OrganizationConfig.RANK_OFFICER, minRank, "and carries its minimum rank");
		ctx.True(LF_PlacementPolicy.RuleFor("Other", LF_EPlacementWho.ANYONE, minRank) == LF_EPlacementWho.ANYONE, "another kind keeps its own rule");
		ctx.EqualStr("", minRank, "with no minimum rank");

		string toast;
		string why = LF_PlacementPolicy.Check(null, "Stall", LF_EPlacementWho.ANYONE, toast);
		ctx.True(why.IndexOf("ORG_MEMBER") != -1, "Check follows the table, not the prefab: " + why);
	}
}
