// red-proof: observed 2026-11-22 with `tools\cli test --no-build --red LF_Test_RoleDialogues` (batched with the pack's dialogue-voices test): RedProof() skips LF_Roles.DialogueFor, `RED-OK npc/role-dialogues (fails with the hook on, 15 assertions)`.
//! 2026-11-22: a role whose conversation is a registered dialogue (roles.json `dialogue`, role_<id>) plays that file
//! through LF_Roles.DialogueFor: in the core alone (no pack patch) it loads, is playable, greets with the four
//! questions and offers the shop after "what do you do", like the conversation it replaced. The pack's voiced
//! patches are checked by everon/dialogue-voices.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_RoleDialogues : LF_Test
{
	override string GetName()
	{
		return "npc/role-dialogues";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_Roles.ResetStatics();
		int registered;
		foreach (LF_RoleDef role : LF_Roles.All())
		{
			if (role.m_sDialogueId.IsEmpty())
				continue;

			registered++;
			LF_DialogueConfig config;
			if (!RedProof())
				config = LF_Roles.DialogueFor(role.m_sId);

			ctx.NotNull(config, role.m_sId + " loads its dialogue " + role.m_sDialogueId);
			if (!config)
				continue;

			ctx.EqualStr(role.m_sDialogueId, config.m_sId, role.m_sId + " plays the dialogue it names");
			array<string> problems = {};
			LF_DialogueFlow.Problems(config, problems);
			ctx.Equal(0, problems.Count(), role.m_sId + " dialogue has no problems");

			LF_DialogueNode greet = LF_DialogueFlow.FindNode(config, config.m_sStart);
			ctx.True(greet != null && greet.m_aOptions && greet.m_aOptions.Count() == 4, role.m_sId + " greets with four questions");

			LF_DialogueNode what = LF_DialogueFlow.FindNode(config, "what");
			bool shop;
			if (what && what.m_aOptions)
			{
				foreach (LF_DialogueOption option : what.m_aOptions)
				{
					if (option && option.m_sAction == LF_ShopkeeperTalk.HOOK && option.m_sCondition == LF_ShopkeeperTalk.HOOK)
						shop = true;
				}
			}

			ctx.True(shop, role.m_sId + " offers its shop after saying what it does");
		}

		ctx.True(registered >= 7, "the shop and service roles play registered dialogues");
		LF_Roles.ResetStatics();
	}
}
