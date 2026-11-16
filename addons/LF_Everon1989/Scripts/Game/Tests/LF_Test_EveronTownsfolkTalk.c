// red-proof: observed 2026-11-16 (#2164) with `tools\cli test --no-build --red LF_Test_EveronTownsfolkTalk`: RedProof() skips the spawn, so `- <who> spawns` fails, RED PROOF OK. Before the prefab change the first run failed too (no dialogue component, no talk action). Green without the hook (pack leg, worlds=LF_Everon1989_Test).
// tier: WORLD
//! #2164 (The relocated, #1015; beat 3 of #2142): under the Everon 1989 header (the pack leg of `tools\cli test`)
//! each of the three townsfolk (old man, cafe owner, night-shift worker) carries an `LF_DialogueComponent` and a
//! talk action, and the component's dialogue id resolves through the pack's index overlay to that person's voiced
//! conf. Found by prefab, never by a town coordinate; the pack owns the prefabs, the core names none of them.
class LF_Test_EveronTownsfolkTalk : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/townsfolk-talk";
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasTalkAction(IEntity npc)
	{
		BaseActionsManagerComponent manager = BaseActionsManagerComponent.Cast(npc.FindComponent(BaseActionsManagerComponent));
		if (!manager)
			return false;

		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			if (LF_DialogueTalkAction.Cast(action))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckPerson(LF_TestContext ctx, ResourceName prefab, string who, string dialogueId, string speakerKey)
	{
		// The act: load the townsperson's prefab.
		IEntity npc;
		if (!RedProof())
			npc = LF_Utils.SpawnEntityPrefab(prefab, "0 -50 0");

		ctx.True(npc != null, who + " spawns");
		if (!npc)
			return;

		LF_DialogueComponent component = LF_DialogueComponent.Cast(npc.FindComponent(LF_DialogueComponent));
		ctx.NotNull(component, who + " has a dialogue component");
		ctx.True(HasTalkAction(npc), who + " has a talk action");

		// The id the component names resolves through the pack's index overlay.
		string path = LF_DialogueRegistry.Resolve(dialogueId);
		ctx.True(!path.IsEmpty(), who + "'s dialogue id " + dialogueId + " resolves through the pack index");

		// And the playable config of the NPC is that dialogue, voiced.
		LF_DialogueConfig config = LF_DialogueComponent.ConfigOf(npc);
		ctx.NotNull(config, who + " has a playable dialogue");
		if (config)
		{
			ctx.EqualStr(dialogueId, config.m_sId, who + " talks with the " + dialogueId + " dialogue");
			ctx.EqualStr(speakerKey, config.m_sSpeakerKey, who + " is named over the dialogue");
			ctx.True(!config.m_sSoundProject.IsEmpty(), who + "'s dialogue is voiced (has an audio project)");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(npc);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");

		CheckPerson(ctx, "{ED4DD018891346D0}Configs/Npcs/LF_Npc_Townsfolk_OldMan.et", "the old man", "old_man", "#LF-Townsfolk_OldMan_Name");
		CheckPerson(ctx, "{83D04234954B8914}Configs/Npcs/LF_Npc_Townsfolk_CafeOwner.et", "the cafe owner", "cafe_owner", "#LF-Townsfolk_CafeOwner_Name");
		CheckPerson(ctx, "{FB61502DB411C69E}Configs/Npcs/LF_Npc_Townsfolk_NightShift.et", "the night-shift worker", "night_shift", "#LF-Townsfolk_NightShift_Name");
	}
}
