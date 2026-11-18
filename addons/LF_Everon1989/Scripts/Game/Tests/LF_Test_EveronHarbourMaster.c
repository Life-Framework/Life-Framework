// red-proof: (#2740 sign-up check: without `m_sAction "JobSignUp"` on the reply the action assertion and `the player's job to PARCEL` fail; the hook's own red proof is LF_Test_DialogueJobSignUp.) Observed 2026-11-17 (#2692) with `tools\cli test --no-build --red LF_Test_EveronHarbourMaster`: RedProof() skips the spawn, so `the harbour master spawns` fails (RED-OK everon/harbour-master, 3 assertions). The first run before the caption fix also failed on one assertion. Green without the hook (pack leg, worlds=LF_Everon1989_Test).
//! #2692 (M6 chapter 1, #2644; beat 3): under the Everon 1989 header (the pack leg of `tools\cli test`) the harbour
//! master Marcel Roux is a dressed dock worker with a talk action, his dialogue id resolves through the pack's index
//! overlay, and the dialogue has the `first_job` line (voiced since #3050) and a reply that
//! takes the crate run on. The take-the-job reply runs the core `JobSignUp` dialogue hook with PARCEL (#2740) and
//! leads to the `go` line; choosing it sets the player's job to PARCEL. Found by prefab, never by a town
//! coordinate. #3050: the greeting offers the harbour crates contract at Known and a come-back answer below it; see ContractCheck.
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
	//! #2740: the take-the-job reply runs the JobSignUp hook with PARCEL, and choosing it (the
	//! server running the reply's action) sets the player's job to PARCEL. The hook's own red proof is
	//! `dialogue/job-sign-up` (LF_Test_DialogueJobSignUp); this one fails without the reply's action or param.
	protected void SignUpCheck(LF_TestContext ctx, LF_DialogueOption take)
	{
		ctx.EqualStr("JobSignUp", take.m_sAction, "the take-the-job reply signs the player up through the JobSignUp hook");
		ctx.EqualStr("PARCEL", take.m_sParam, "the take-the-job reply names the parcel job");

		LF_DialogueHook hook = LF_DialogueHook.Find(take.m_sAction);
		ctx.NotNull(hook, "the reply's hook has a class");
		IEntity player = LF_ExploitGuardsWorldFixture.SpawnCharacter(ctx, "100 0 -60", false);
		if (!hook || !player)
		{
			LF_ExploitGuardsWorldFixture.Despawn(player);
			return;
		}

		string uid = "harbour-master-signup-uid";
		LF_Utils.SetUidForTests(player, uid);
		LF_PlayerJobComponent jobs = LF_Component<LF_PlayerJobComponent>.Find(player);
		ctx.NotNull(jobs, "the player has a job component");
		if (jobs)
		{
			ctx.Equal(LF_EJobType.UNEMPLOYED, jobs.GetJob(), "the player is unemployed before taking the job");
			array<string> spoken = {};
			hook.Run(player, null, take.m_sParam, spoken);

			ctx.Equal(LF_EJobType.PARCEL, jobs.GetJob(), "choosing the take-the-job reply sets the player's job to PARCEL");
		}

		LF_Utils.SetUidForTests(null, "");
		LF_CivilianJob.ForgetCareer(uid);
		LF_ExploitGuardsWorldFixture.Despawn(player);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_DialogueOption FindReply(LF_DialogueNode node, string next, string condition)
	{
		if (!node || !node.m_aOptions)
			return null;

		foreach (LF_DialogueOption option : node.m_aOptions)
		{
			if (option.m_sNext == next && option.m_sCondition == condition)
				return option;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! #3050: the harbour crates contract. The greeting offers the bigger load only to a character the town
	//! knows (condition Contract) and a come-back answer to one it does not (ContractLocked); the offer's
	//! accept reply names the Contract action with the pack's contract id, and running it as the server does
	//! starts the contract at Known and starts nothing below it. The NPC answers the start in a recorded line.
	protected void ContractCheck(LF_TestContext ctx, LF_DialogueConfig config)
	{
		string contractId = "harbour_crates";
		LF_DialogueNode start = LF_DialogueFlow.FindNode(config, config.m_sStart);
		LF_DialogueOption offerReply = FindReply(start, "crates_offer", "Contract");
		LF_DialogueOption earlyReply = FindReply(start, "crates_early", "ContractLocked");
		ctx.NotNull(offerReply, "the greeting offers the bigger load behind the Contract condition");
		ctx.NotNull(earlyReply, "the greeting has the come-back reply behind the ContractLocked condition");
		if (offerReply)
			ctx.EqualStr(contractId, offerReply.m_sParam, "the offer reply names the harbour_crates contract");
		if (earlyReply)
			ctx.EqualStr(contractId, earlyReply.m_sParam, "the come-back reply names the harbour_crates contract");

		LF_DialogueNode early = LF_DialogueFlow.FindNode(config, "crates_early");
		ctx.NotNull(early, "the come-back reply leads to a node");
		if (early)
		{
			LF_DialogueLine earlyLine = LF_DialogueFlow.FindLine(config, early.m_sLine);
			ctx.True(earlyLine && WidgetManager.Translate(earlyLine.m_sTextKey).IndexOf("Come back") != -1, "the come-back line asks the newcomer to come back when the town knows them");
		}

		LF_DialogueNode offer = LF_DialogueFlow.FindNode(config, "crates_offer");
		ctx.NotNull(offer, "the offer reply leads to the crates_offer node");
		LF_DialogueOption accept = FindReply(offer, "crates_done", string.Empty);
		ctx.NotNull(accept, "the offer has an accept reply");
		ctx.NotNull(FindReply(offer, "bye", string.Empty), "the offer has a refuse reply that ends the talk");
		if (!accept || !offerReply || !earlyReply)
			return;

		ctx.EqualStr("Contract", accept.m_sAction, "the accept reply runs the Contract action");
		ctx.EqualStr(contractId, accept.m_sParam, "the accept reply names the harbour_crates contract");
		ctx.True(LF_DialogueFlow.SpokenFor(config, LF_DialogueHook_Contract.KEY_STARTED) != LF_DialogueHook_Contract.KEY_STARTED, "the NPC answers a started contract in a recorded line");
		ctx.True(LF_DialogueFlow.SpokenFor(config, LF_ReputationContracts.KEY_BUSY) != LF_ReputationContracts.KEY_BUSY, "the NPC answers a busy refusal in a recorded line");

		LF_DialogueHook offers = LF_DialogueHook.Find(offerReply.m_sCondition);
		LF_DialogueHook locked = LF_DialogueHook.Find(earlyReply.m_sCondition);
		LF_DialogueHook action = LF_DialogueHook.Find(accept.m_sAction);
		ctx.True(offers && locked && action, "the contract hooks have their classes");
		IEntity player = LF_TestCharacterFixture.Spawn(ctx);
		if (!offers || !locked || !action || !player)
		{
			LF_TestCharacterFixture.Delete(player);
			return;
		}

		string uid = "harbour-master-crates-uid";
		LF_Utils.SetUidForTests(player, uid);
		LF_ReputationConfig.Reload();
		LF_ReputationContractsConfig.Reload();
		LF_JobsConfig.Reload();
		LF_ReputationConfig standing = LF_ReputationConfig.Get();
		LF_ReputationFactionDef town;
		if (standing)
			town = standing.FindFaction("TOWN");

		// The town's daily cap would hold the test points back; lift it on this loaded copy (reloaded below).
		if (town)
			town.m_iDailyCap = 0;

		LF_Reputation.Forget(uid);
		LF_ReputationContracts.Forget(uid);
		LF_CivilianJob.ForgetCareer(uid);
		int deposit = LF_CivilianJob.DepositFor(LF_EJobType.PARCEL);
		LF_MoneyUtils.GiveCash(player, deposit * 2);

		// A newcomer: only the come-back reply shows, and a forced pick starts nothing.
		ctx.False(offers.Allows(player, null, offerReply.m_sParam), "a newcomer is not offered the crates contract");
		ctx.True(locked.Allows(player, null, earlyReply.m_sParam), "a newcomer is given the come-back reply");
		array<string> spoken = {};
		action.Run(player, null, accept.m_sParam, spoken);
		ctx.EqualStr("", LF_ReputationContracts.ActiveId(uid), "a forced pick below Known starts no contract");

		// At Known: the offer shows, the come-back reply hides, and the accept reply starts the contract. Red proof: skip the pick.
		LF_Reputation.Add(uid, "TOWN", 100, "test");
		ctx.True(offers.Allows(player, null, offerReply.m_sParam), "a Known character is offered the crates contract");
		ctx.False(locked.Allows(player, null, earlyReply.m_sParam), "a Known character is not given the come-back reply");
		spoken.Clear();
		if (!RedProof())
			action.Run(player, null, accept.m_sParam, spoken);

		ctx.EqualStr(contractId, LF_ReputationContracts.ActiveId(uid), "the accept reply starts the harbour crates contract");
		ctx.True(spoken.Contains(LF_DialogueHook_Contract.KEY_STARTED), "the harbour master answers that the contract is on");

		LF_CivilianJob.AbandonRun(uid);
		LF_ReputationContracts.Forget(uid);
		LF_Reputation.Forget(uid);
		LF_CivilianJob.ForgetCareer(uid);
		LF_ReputationConfig.Reload();
		LF_ReputationContractsConfig.Reload();
		LF_Utils.SetUidForTests(null, "");
		LF_TestCharacterFixture.Delete(player);
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

		// Every line has its caption and a recording (#3050).
		foreach (LF_DialogueLine each : config.m_aLines)
		{
			string caption = WidgetManager.Translate(each.m_sTextKey);
			ctx.True(caption != each.m_sTextKey && !caption.IsEmpty(), each.m_sId + " has its caption in the string table");
			ctx.False(each.IsTextOnly(), each.m_sId + " is voiced (ElevenLabs pipeline, #3050)");
		}

		// The first job: the line, said by the node that follows the greeting.
		LF_DialogueLine firstJob = LF_DialogueFlow.FindLine(config, "first_job");
		ctx.NotNull(firstJob, "the harbour master has the first_job line");
		if (firstJob)
			ctx.True(WidgetManager.Translate(firstJob.m_sTextKey).IndexOf("Crates") != -1, "the first job is carrying crates");

		LF_DialogueNode start = LF_DialogueFlow.FindNode(config, config.m_sStart);
		ctx.NotNull(start, "the conversation has a start");
		LF_DialogueNode jobNode = LF_DialogueFlow.FindNode(config, "first_job");
		ctx.NotNull(jobNode, "the conversation has a first_job node");
		if (jobNode)
			ctx.EqualStr("first_job", jobNode.m_sLine, "the first_job node says first_job");

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
				SignUpCheck(ctx, take);
			}
		}

		ctx.True(takes, "the first job offers a reply that takes the crate run");
		LF_DialogueNode go = LF_DialogueFlow.FindNode(config, "go");
		ctx.NotNull(go, "taking the job leads to the go node");
		if (go)
			ctx.EqualStr("go", go.m_sLine, "the go node explains the crate run");

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

		ContractCheck(ctx, config);
	}
}
