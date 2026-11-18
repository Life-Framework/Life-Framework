// red-proof: 2026-11-18 `tools\cli test --no-build --red LF_Test_OrgWarKillTag` (RedProof() skips the war acceptance): RED PROOF OK, "the murder war kill is a second war_kill incident (expected 2, got 0)".
// tier: LOGIC
//! org/war-kill-tag (#2670, M7 gang war W5): a kill between members of two organizations at war is a
//! war kill. LF_Murder's war rule skips it (no murder, no wanted) and the subscriber files one `war_kill`
//! incident; members of two organizations not at war are murder and file nothing; with
//! `warKillsAreMurder` on the kill is murder and still tagged; `tagWarKills` off files nothing.
class LF_Test_OrgWarKillTag : LF_Test
{
	protected const string UID_A = "war-kill-uid-a";
	protected const string UID_B = "war-kill-uid-b";
	protected const string UID_C = "war-kill-uid-c";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "org/war-kill-tag";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_OrganizationManager.Reset();
		LF_OrgWars.ResetStatics();

		RunCases(ctx);

		LF_OrgWarKills.ResetStatics();
		LF_OrgWars.ResetStatics();
		LF_OrganizationManager.Reset();
	}

	//------------------------------------------------------------------------------------------------
	protected string MakeMember(string uid, string orgName, out LF_Organization org)
	{
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(uid);
		LF_PlayerCharacter character = LF_PlayerCharacter.Create("Prefabs/Characters/Core/Character_Roleplay.et", orgName, "Fighter", 30);
		account.AddCharacter(character, true);
		org = LF_OrganizationManager.Create(character.GetId(), orgName);
		return uid;
	}

	//------------------------------------------------------------------------------------------------
	protected void RunCases(LF_TestContext ctx)
	{
		LF_Organization orgA;
		LF_Organization orgB;
		LF_Organization orgC;
		MakeMember(UID_A, "War Kill Alpha", orgA);
		MakeMember(UID_B, "War Kill Bravo", orgB);
		MakeMember(UID_C, "War Kill Charlie", orgC);
		ctx.NotNull(orgA, "org A is registered");
		ctx.NotNull(orgB, "org B is registered");
		ctx.NotNull(orgC, "org C is registered");
		if (!orgA || !orgB || !orgC)
			return;

		LF_OrgWarRules rules = new LF_OrgWarRules();
		ctx.False(rules.m_bWarKillsAreMurder, "the default rules do not make a war kill murder");
		ctx.True(rules.m_bTagWarKills, "the default rules tag war kills");

		// A and B go to war; C stays out of it.
		int now = System.GetUnixTime();
		LF_OrgWarRecord war;
		ctx.EqualStr("", LF_OrgWars.Declare(orgA.GetId(), orgB.GetId(), now, rules, war), "A declares on B");
		if (!war)
			return;

		if (!RedProof())
			ctx.EqualStr("", LF_OrgWars.Accept(war.m_sWarId, now, rules), "B accepts: the two are at war");

		// The rule LF_Murder applies after its own.
		ctx.EqualStr(LF_Murder.SKIP_ORG_WAR, LF_OrgWarKills.SkipReasonFor(UID_A, UID_B, rules), "warring members: the kill is a war kill, not murder");
		ctx.EqualStr(LF_Murder.SKIP_ORG_WAR, LF_OrgWarKills.SkipReasonFor(UID_B, UID_A, rules), "it holds in both directions");
		ctx.EqualStr("", LF_OrgWarKills.SkipReasonFor(UID_A, UID_C, rules), "members of two organizations not at war: murder");
		ctx.EqualStr("", LF_OrgWarKills.SkipReasonFor(UID_C, UID_B, rules), "an outsider killing a member of a warring org: murder");
		ctx.EqualStr("", LF_OrgWarKills.SkipReasonFor(UID_A, "war-kill-uid-nobody", rules), "a victim with no account is never a war kill");
		ctx.EqualStr("", LF_OrgWarKills.SkipReasonFor("", UID_B, rules), "no killer uid: not a war kill");

		LF_OrgWarRules strict = new LF_OrgWarRules();
		strict.m_bWarKillsAreMurder = true;
		ctx.EqualStr("", LF_OrgWarKills.SkipReasonFor(UID_A, UID_B, strict), "warKillsAreMurder on: murder");

		// The tag: one war_kill incident for a war pair, none otherwise.
		vector where = Vector(100, 0, 100);
		int before = LF_IncidentLog.Query(vector.Zero, -1, 0, -1, LF_OrgWarKills.KIND_WAR_KILL).Count();

		LF_OnPlayerKilled warKill = LF_OnPlayerKilled.Create(UID_B, UID_A);
		warKill.m_vPosition = where;
		warKill.m_sWeapon = "war-kill-weapon";
		warKill.m_bMurder = false;
		LF_OrgWarKillsSubscriber subscriber = new LF_OrgWarKillsSubscriber();
		ctx.True(subscriber.Handle(warKill), "the subscriber handles a kill event");
		array<ref LF_IncidentRecord> tagged = LF_IncidentLog.Query(vector.Zero, -1, 0, -1, LF_OrgWarKills.KIND_WAR_KILL);
		ctx.Equal(before + 1, tagged.Count(), "a war kill files one war_kill incident");
		if (tagged.Count() > before)
		{
			LF_IncidentRecord record = tagged.Get(tagged.Count() - 1);
			ctx.EqualStr(UID_B, record.m_sSubjectUid, "the victim is the subject");
			ctx.EqualStr(UID_A, record.m_sOtherUid, "the killer is the other party");
			ctx.EqualStr("war-kill-weapon", record.m_sHow, "the weapon is carried");
			ctx.False(record.m_bMurder, "the war kill is not murder");
		}

		LF_OnPlayerKilled plain = LF_OnPlayerKilled.Create(UID_C, UID_A);
		plain.m_bMurder = true;
		subscriber.Handle(plain);
		ctx.Equal(before + 1, LF_IncidentLog.Query(vector.Zero, -1, 0, -1, LF_OrgWarKills.KIND_WAR_KILL).Count(), "a kill outside the war files no war_kill");

		// warKillsAreMurder on: murder, and still tagged.
		LF_OnPlayerKilled murder = LF_OnPlayerKilled.Create(UID_A, UID_B);
		murder.m_bMurder = true;
		ctx.True(LF_OrgWarKills.Tag(murder, strict), "with warKillsAreMurder on the kill is still tagged");
		array<ref LF_IncidentRecord> both = LF_IncidentLog.Query(vector.Zero, -1, 0, -1, LF_OrgWarKills.KIND_WAR_KILL);
		ctx.Equal(before + 2, both.Count(), "the murder war kill is a second war_kill incident");
		if (both.Count() > before + 1)
			ctx.True(both.Get(both.Count() - 1).m_bMurder, "the tagged record keeps the murder verdict");

		// tagWarKills off: nothing filed.
		LF_OrgWarRules quiet = new LF_OrgWarRules();
		quiet.m_bTagWarKills = false;
		ctx.False(LF_OrgWarKills.Tag(warKill, quiet), "tagWarKills off files nothing");
		ctx.Equal(before + 2, LF_IncidentLog.Query(vector.Zero, -1, 0, -1, LF_OrgWarKills.KIND_WAR_KILL).Count(), "the count is unchanged with tagging off");

		// The war ends: the same pair is murder again.
		LF_OrgWars.End(war.m_sWarId, LF_OrgWars.REASON_FORFEIT, now + 1);
		ctx.EqualStr("", LF_OrgWarKills.SkipReasonFor(UID_A, UID_B, rules), "after the war ends a kill between the two is murder");

		// The bus wiring: LF_Murder's Watch() subscribes the tagger once.
		LF_OrgWarKills.ResetStatics();
		int subscribers = LF_EventBus.SubscriberCount(LF_OnPlayerKilled);
		LF_OrgWarKills.Watch();
		LF_OrgWarKills.Watch();
		ctx.Equal(subscribers + 1, LF_EventBus.SubscriberCount(LF_OnPlayerKilled), "Watch subscribes the tagger once; a second call is the same subscription");
	}
}
