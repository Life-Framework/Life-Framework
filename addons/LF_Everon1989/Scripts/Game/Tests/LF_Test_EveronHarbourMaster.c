// red-proof: observed 2026-11-17 (#2692) with `tools\cli test --no-build --red LF_Test_EveronHarbourMaster`: RedProof() skips the spawn, so `the harbour master spawns` fails (RED-OK everon/harbour-master, 3 assertions). The first run before the caption fix also failed on one assertion. Green without the hook (pack leg, worlds=LF_Everon1989_Test).
//! #2692 (M6 chapter 1, #2644; beat 3): under the Everon 1989 header (the pack leg of `tools\cli test`) the harbour
//! master Marcel Roux is a dressed dock worker with a talk action, his dialogue id resolves through the pack's index
//! overlay, and the dialogue has the `harbour.first_job` line (captions only, voice comes with #343) and a reply that
//! takes the crate run on. The sign-up itself needs a core dialogue hook (`LF_DialogueHook_<name>` that calls
//! `LF_PlayerJobComponent.SetJob`); none exists, so the take-the-job reply leads to the `harbour.go` line and the
//! hook is the follow-up card named in the PR. Found by prefab, never by a town coordinate.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronHarbourMaster : LF_Test
{
	static const ResourceName HARBOUR_MASTER = "{B45B7C6C168822D4}Configs/Npcs/LF_Npc_Townsfolk_HarbourMaster.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/harbour-master";
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
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");

		// The act: spawn the harbour master as the pack ships him. Red proof: no spawn.
		IEntity npc;
		if (!RedProof())
			npc = LF_Utils.SpawnEntityPrefab(HARBOUR_MASTER, "0 -50 0");

		ctx.True(npc != null, "the harbour master spawns");
		if (!npc)
			return;

		LF_DialogueComponent component = LF_DialogueComponent.Cast(npc.FindComponent(LF_DialogueComponent));
		ctx.NotNull(component, "the harbour master has a dialogue component");
		ctx.True(HasTalkAction(npc), "the harbour master has a talk action");

		// A dressed 1989 dock worker: a jacket, trousers and boots on him, not the default naked body.
		string wornNames;
		int worn;
		IEntity child = npc.GetChildren();
		while (child)
		{
			if (child.FindComponent(BaseLoadoutClothComponent))
			{
				worn++;
				wornNames += LF_Utils.GetPrefabName(child) + " ";
			}

			child = child.GetSibling();
		}

		ctx.True(worn >= 4, "the harbour master wears a jacket, trousers, boots and a hat (wears: " + wornNames + ")");

		// The id the component names resolves through the pack's index overlay.
		string path = LF_DialogueRegistry.Resolve("harbour_master");
		ctx.True(!path.IsEmpty(), "the harbour master's dialogue id harbour_master resolves through the pack index");

		LF_DialogueConfig config = LF_DialogueComponent.ConfigOf(npc);
		ctx.NotNull(config, "the harbour master has a playable dialogue");
		SCR_EntityHelper.DeleteEntityAndChildren(npc);
		if (!config)
			return;

		ctx.EqualStr("harbour_master", config.m_sId, "the dialogue is the harbour master's");
		ctx.EqualStr("Marcel Roux", WidgetManager.Translate(config.m_sSpeakerKey), "the speaker is Marcel Roux");

		array<string> problems = {};
		LF_DialogueFlow.Problems(config, problems);
		ctx.Equal(0, problems.Count(), "the harbour master's dialogue is playable");

		// Every line has its caption; captions only, so none is voiced yet.
		foreach (LF_DialogueLine each : config.m_aLines)
		{
			string caption = WidgetManager.Translate(each.m_sTextKey);
			ctx.True(caption != each.m_sTextKey && !caption.IsEmpty(), each.m_sId + " has its caption in the string table");
			ctx.True(each.IsTextOnly(), each.m_sId + " is captions only until the voice pipeline (#343)");
		}

		// The first job: the line, said by the node that follows the greeting.
		LF_DialogueLine firstJob = LF_DialogueFlow.FindLine(config, "harbour.first_job");
		ctx.NotNull(firstJob, "the harbour master has the harbour.first_job line");
		if (firstJob)
			ctx.True(WidgetManager.Translate(firstJob.m_sTextKey).IndexOf("Crates") != -1, "the first job is carrying crates");

		LF_DialogueNode start = LF_DialogueFlow.FindNode(config, config.m_sStart);
		ctx.NotNull(start, "the conversation has a start");
		LF_DialogueNode jobNode = LF_DialogueFlow.FindNode(config, "first_job");
		ctx.NotNull(jobNode, "the conversation has a first_job node");
		if (jobNode)
			ctx.EqualStr("harbour.first_job", jobNode.m_sLine, "the first_job node says harbour.first_job");

		// Asking about work leads to the job; taking it leads on to the crate-run line.
		bool asks;
		if (start && start.m_aOptions)
		{
			foreach (LF_DialogueOption ask : start.m_aOptions)
			{
				if (ask.m_sNext == "first_job")
					asks = true;
			}
		}

		ctx.True(asks, "the greeting offers a reply that asks for work");

		bool takes;
		if (jobNode && jobNode.m_aOptions)
		{
			foreach (LF_DialogueOption take : jobNode.m_aOptions)
			{
				if (take.m_sNext != "go")
					continue;

				takes = true;
				string reply = WidgetManager.Translate(take.m_sTextKey);
				ctx.True(reply != take.m_sTextKey && !reply.IsEmpty(), "the take-the-job reply has its text");
			}
		}

		ctx.True(takes, "the first job offers a reply that takes the crate run");
		LF_DialogueNode go = LF_DialogueFlow.FindNode(config, "go");
		ctx.NotNull(go, "taking the job leads to the go node");
		if (go)
			ctx.EqualStr("harbour.go", go.m_sLine, "the go node explains the crate run");

		// Every reply has its text, and the talk ends on the goodbye.
		foreach (LF_DialogueNode node : config.m_aNodes)
		{
			if (!node.m_aOptions)
				continue;

			foreach (LF_DialogueOption option : node.m_aOptions)
			{
				string text = WidgetManager.Translate(option.m_sTextKey);
				ctx.True(text != option.m_sTextKey && !text.IsEmpty(), "reply " + option.m_sTextKey + " has its text");
			}
		}

		LF_DialogueNode bye = LF_DialogueFlow.FindNode(config, "bye");
		ctx.NotNull(bye, "the conversation has a goodbye");
		if (bye)
			ctx.True(!bye.m_aOptions || bye.m_aOptions.IsEmpty(), "the goodbye ends the talk");
	}
}
