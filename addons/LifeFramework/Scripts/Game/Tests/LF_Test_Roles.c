// red-proof: Observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_Roles`: changed the expected seed hash to 410749; got
// `[LFTEST] FAIL npc/roles`, `- the seed hash matches the generator's (expected 410749, got 410748)`. Restored; green.
// tier: LOGIC
//! NPC roles, with no world: every role has outfits and a playable conversation, the outfit an NPC
//! wears is stable for its seed and differs between seeds, and a bare caption key is a valid line.
class LF_Test_Roles : LF_Test
{
	override string GetName()
	{
		return "npc/roles";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_Roles.ResetStatics();
		ctx.True(LF_Roles.All().Count() >= 12, "the table has the town's roles");

		foreach (LF_RoleDef role : LF_Roles.All())
		{
			ctx.True(!role.m_aOutfits.IsEmpty() && !role.m_aPrimary.IsEmpty(), role.m_sId + " has an outfit");
			string outfit = LF_Roles.PickOutfit(role.m_sId, "SomeNpc");
			ctx.True(role.m_aPrimary.Find(outfit) != -1, role.m_sId + " picks one of its own outfits");

			LF_DialogueConfig config = LF_Roles.BuildDialogue(role.m_sId);
			if (role.m_bOwnDialogue)
			{
				ctx.True(config == null, role.m_sId + " keeps its authored dialogue");
				continue;
			}

			ctx.True(config != null, role.m_sId + " has a conversation");
			if (!config)
				continue;

			array<string> problems = {};
			LF_DialogueFlow.Problems(config, problems);
			ctx.Equal(0, problems.Count(), role.m_sId + " conversation has no problems" + problems.Count());
			LF_DialogueNode greet = LF_DialogueFlow.FindNode(config, config.m_sStart);
			ctx.True(greet != null && !greet.m_sLine.IsEmpty(), role.m_sId + " greets");
			ctx.True(greet != null && greet.m_aOptions.Count() >= 2 && greet.m_aOptions.Count() <= LF_DialogueFlow.MAX_OPTIONS, role.m_sId + " offers 2 to 4 replies");
			ctx.True(LF_DialogueFlow.IsRaw(greet.m_sLine), role.m_sId + " greeting is a bare caption key");
		}

		ctx.True(LF_Roles.BuildDialogue("no_such_role") == null, "an unknown role has no conversation");
		ctx.EqualStr("", LF_Roles.PickOutfit("no_such_role", "x"), "an unknown role has no outfit");

		// Same seed, same outfit; the hash is the one tools/outfits/generate.mjs gives each placed NPC.
		ctx.Equal(410748, LF_Roles.SeedHash("npc_a"), "the seed hash matches the generator's");
		ctx.EqualStr(LF_Roles.PickOutfit("townsfolk", "npc_a"), LF_Roles.PickOutfit("townsfolk", "npc_a"), "an NPC always gets the same outfit");
		ctx.True(LF_Roles.PickOutfit("townsfolk", "npc_a") != LF_Roles.PickOutfit("townsfolk", "npc_b"), "two townsfolk do not get the same outfit");

		LF_Roles.ResetStatics();
	}
}
