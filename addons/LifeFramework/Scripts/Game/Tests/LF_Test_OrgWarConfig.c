// red-proof: the batched `--red` run (RedProof() drops the owner JSON) fails "the owner's fee wins (expected 1200, got 5000)"; see the PR for the RED-OK line.
// tier: LOGIC
//! The organization war rules (#2667, W2 of #1022): LF_OrgWarConfig loads Configs/Organizations/LF_OrgWars.conf
//! and layers the owner's `organizations.wars` keys over it. The core defaults are a 5000 fee from the
//! fund, a 600 s answer window, a 7200 s war, a 86400 s pair cooldown, 3 members a side, war kills not
//! murder and tagged. An owner override of the window and fee wins; a negative length keeps the default.
class LF_Test_OrgWarConfig : LF_Test
{
	override string GetName()
	{
		return "organizations/war-config";
	}

	protected LF_OrgWarRules RulesFor(string json, string source)
	{
		LF_ConfigService.Reset();
		LF_ConfigService.LoadOwnerString(json, source);
		LF_OrgWarConfig.Reload();
		return LF_OrgWarConfig.Rules();
	}

	override void Run(LF_TestContext ctx)
	{
		// No section, or an organizations section without wars: the core conf's values.
		LF_OrgWarRules shipped = RulesFor("{\"schemaVersion\":1}", "test:wars-none");
		ctx.Equal(5000, shipped.m_iFee, "the shipped war fee is 5000");
		ctx.Equal(600, shipped.m_iAnswerSeconds, "the shipped answer window is 600 s");
		ctx.Equal(7200, shipped.m_iLengthSeconds, "the shipped war length is 7200 s");
		ctx.Equal(86400, shipped.m_iCooldownSeconds, "the shipped pair cooldown is 86400 s");
		ctx.Equal(3, shipped.m_iMinMembers, "the shipped minimum is 3 members a side");
		ctx.False(shipped.m_bWarKillsAreMurder, "war kills are not murder by default");
		ctx.True(shipped.m_bTagWarKills, "war kills are tagged by default");

		LF_OrgWarRules other = RulesFor("{\"schemaVersion\":1,\"organizations\":{\"foundingFee\":300}}", "test:wars-other-keys");
		ctx.Equal(5000, other.m_iFee, "an organizations section without wars keeps the shipped fee");

		// The owner's window and fee win; the keys left out keep the shipped value.
		string json = "{\"schemaVersion\":1,\"organizations\":{\"wars\":{\"fee\":1200,\"answerSeconds\":90}}}";
		if (RedProof())
			json = "{\"schemaVersion\":1}";

		LF_OrgWarRules owner = RulesFor(json, "test:wars-owner");
		ctx.Equal(1200, owner.m_iFee, "the owner's fee wins");
		ctx.Equal(90, owner.m_iAnswerSeconds, "the owner's answer window wins");
		ctx.Equal(7200, owner.m_iLengthSeconds, "a key left out keeps the shipped war length");
		ctx.Equal(3, owner.m_iMinMembers, "a key left out keeps the shipped minimum");

		LF_OrgWarRules flags = RulesFor("{\"schemaVersion\":1,\"organizations\":{\"wars\":{\"warKillsAreMurder\":true,\"tagWarKills\":false,\"minMembers\":2,\"cooldownSeconds\":0}}}", "test:wars-flags");
		ctx.True(flags.m_bWarKillsAreMurder, "the owner can make war kills murder");
		ctx.False(flags.m_bTagWarKills, "the owner can switch the war tag off");
		ctx.Equal(2, flags.m_iMinMembers, "the owner's minimum wins");
		ctx.Equal(0, flags.m_iCooldownSeconds, "a zero cooldown is allowed");

		// A negative length is refused (the reader logs the error) and the shipped length stays.
		LF_OrgWarRules negative = RulesFor("{\"schemaVersion\":1,\"organizations\":{\"wars\":{\"lengthSeconds\":-5,\"fee\":700}}}", "test:wars-negative");
		ctx.Equal(7200, negative.m_iLengthSeconds, "a negative war length keeps the shipped 7200 s");
		ctx.Equal(700, negative.m_iFee, "a bad key does not stop the good ones");

		LF_OrgWarRules text = RulesFor("{\"schemaVersion\":1,\"organizations\":{\"wars\":{\"answerSeconds\":\"soon\",\"minMembers\":0}}}", "test:wars-text");
		ctx.Equal(600, text.m_iAnswerSeconds, "a non-numeric window keeps the shipped 600 s");
		ctx.Equal(3, text.m_iMinMembers, "a minimum of 0 is below the range and keeps 3");

		// The rules handed out are a copy: changing one does not change the config.
		owner.m_iFee = 1;
		RulesFor("{\"schemaVersion\":1,\"organizations\":{\"wars\":{\"fee\":1200}}}", "test:wars-copy");
		LF_OrgWarRules again = LF_OrgWarConfig.Rules();
		again.m_iFee = 2;
		ctx.Equal(1200, LF_OrgWarConfig.Rules().m_iFee, "Rules() hands out a copy");

		LF_ConfigService.Reset();
		LF_OrgWarConfig.Reload();
	}
}
