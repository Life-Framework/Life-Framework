// red-proof: batched run `tools\cli test --no-build --red LF_Test_EveronRumorStanding` (RED-OK line in the PR): RedProof() skips the sales that earn standing and every ask, so the barkeeper stays neutral and no friendly player is told a crime rumor at the bar or by the informant.
//! #4595 (follows #4461 and #4476, W5.2/W5.4 of the world chapter): under the Everon 1989 header (the pack leg of
//! `tools\cli test`) crime rumors are reachable. The barkeeper prefab names a standing id (a placed NPC with no
//! resident row), so the bar counts a player's regard: two counter sales ten minutes apart make a seller friendly.
//! A friendly regular who asks "Heard anything?" at the bar, and of the informant (the night-shift resident), is
//! told a crime rumor from the pack's rows; a neutral player is told none by either. The informant's "Heard anything?"
//! reply sits on the greeting and on the first sub-node with every node at or under four replies, and the barkeeper's
//! sits on his greeting.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronRumorStanding : LF_Test
{
	static const ResourceName NPC_PREFAB = "{4439B0A5C0000003}Configs/Npcs/LF_Npc_Townsfolk_Barkeeper.et";
	static const string STANDING = "barkeeper";
	static const string INFORMANT = "night_shift";
	static const string TRADED = "traded";
	static const string REGULAR = "rs-regular";
	static const string STRANGER = "rs-stranger";
	static const int MAX_REPLIES = 4;
	static const int DAY = 12000;
	static const int DAYS = 10;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/rumor-standing";
	}

	//------------------------------------------------------------------------------------------------
	//! The node `id`'s reply that runs the hook `action` with `param`, or null.
	protected static LF_DialogueOption FindAction(LF_DialogueConfig config, string nodeId, string action, string param)
	{
		LF_DialogueNode node = LF_DialogueFlow.FindNode(config, nodeId);
		if (!node || !node.m_aOptions)
			return null;

		foreach (LF_DialogueOption option : node.m_aOptions)
		{
			if (option && option.m_sAction == action && option.m_sParam == param)
				return option;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The most replies any node of `config` offers.
	protected static int MostReplies(LF_DialogueConfig config)
	{
		int most;
		if (!config || !config.m_aNodes)
			return most;

		foreach (LF_DialogueNode node : config.m_aNodes)
		{
			if (node && node.m_aOptions && node.m_aOptions.Count() > most)
				most = node.m_aOptions.Count();
		}

		return most;
	}

	//------------------------------------------------------------------------------------------------
	//! Asks every day for DAYS days; true when any rumor told was a crime row.
	protected static bool CrimeToldWithin(string who, string giverKey, string giver, string tier)
	{
		for (int i = 0; i < DAYS; i++)
		{
			LF_ERumorTalk outcome;
			LF_RumorDef def = LF_ResidentTalkRumor.Ask(who, giverKey, giver, tier, DAY + i, new LF_RumorState(), outcome);
			if (def && def.m_bCrime)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void DialogueCheck(LF_TestContext ctx)
	{
		LF_DialogueConfig bar = LF_DialogueConfig.Load(LF_DialogueRegistry.Resolve("barkeeper"));
		ctx.NotNull(bar, "the barkeeper's dialogue loads");
		if (bar)
			ctx.NotNull(FindAction(bar, bar.m_sStart, "HeardAnything", "bar"), "the barkeeper's greeting offers 'Heard anything?' as a bar");

		LF_DialogueConfig informant = LF_DialogueConfig.Load(LF_DialogueRegistry.Resolve(INFORMANT));
		ctx.NotNull(informant, "the informant's dialogue loads");
		if (!informant)
			return;

		array<string> problems = {};
		LF_DialogueFlow.Problems(informant, problems);
		ctx.Equal(0, problems.Count(), "the informant's dialogue is playable");
		ctx.True(MostReplies(informant) <= MAX_REPLIES, "no informant node offers more than four replies");
		ctx.NotNull(FindAction(informant, informant.m_sStart, "HeardAnything", "informant"), "the informant's greeting offers 'Heard anything?'");
		ctx.NotNull(FindAction(informant, "zone", "HeardAnything", "informant"), "the informant's first sub-node offers 'Heard anything?' too");
		ctx.NotNull(LF_DialogueFlow.FindNode(informant, "greet_friendly"), "the informant still greets a friendly player on his own node");
		ctx.NotNull(FindAction(informant, "work", "Informant", ""), "the informant keeps his tip-off reply");
	}

	//------------------------------------------------------------------------------------------------
	protected void PrefabCheck(LF_TestContext ctx)
	{
		Resource npc = Resource.Load(NPC_PREFAB);
		ctx.True(npc && npc.IsValid(), "the barkeeper prefab loads");
		if (!npc || !npc.IsValid())
			return;

		string standing;
		IEntityComponentSource talk = SCR_BaseContainerTools.FindComponentSource(npc, LF_DialogueComponent);
		ctx.True(talk != null, "the barkeeper prefab carries a dialogue");
		if (talk)
			talk.Get("m_sStandingId", standing);

		ctx.EqualStr(STANDING, standing, "the barkeeper prefab names the standing id his regulars earn");
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MapKnown.ResetStatics();
		LF_ResidentTalkRumor.ResetStatics();
		LF_ResidentMemory.ResetStatics();

		PrefabCheck(ctx);
		DialogueCheck(ctx);

		LF_ResidentMemoryConfig memory = LF_ResidentMemoryConfig.Get();
		ctx.NotNull(memory, "the resident memory config loads");
		if (memory)
			ctx.NotNull(memory.FindReason(TRADED), "a counter sale is a memory reason");

		// A stranger is neutral at the bar; two sales ten minutes apart make the regular friendly.
		ctx.EqualStr(LF_ResidentMemory.TIER_NEUTRAL, LF_ResidentMemory.Tier(STANDING, REGULAR), "a new face is neutral to the barkeeper");
		if (!RedProof())
		{
			LF_ResidentMemory.Apply(STANDING, REGULAR, TRADED, 1000);
			LF_ResidentMemory.Apply(STANDING, REGULAR, TRADED, 1700);
		}

		string tier = LF_ResidentMemory.Tier(STANDING, REGULAR);
		ctx.EqualStr(LF_ResidentMemory.TIER_FRIENDLY, tier, "two sales ten minutes apart make a seller friendly to the barkeeper");

		// The pack's rows: a crime rumor is told at the bar to the friendly regular, never to a stranger.
		LF_RumorsConfig rumors = LF_RumorsConfig.Get();
		ctx.NotNull(rumors, "the pack's rumor rows load");
		// With --red nothing is asked, so every "told" check below fails.
		bool barStranger = true;
		bool barRegular;
		bool infStranger = true;
		bool infRegular;
		if (!RedProof())
		{
			barStranger = CrimeToldWithin(STRANGER, STANDING, "bar", LF_ResidentMemory.Tier(STANDING, STRANGER));
			barRegular = CrimeToldWithin(REGULAR, STANDING, "bar", tier);
			// The informant is a resident: a stranger is neutral to him; the regular is friendly once they have helped him.
			infStranger = CrimeToldWithin(STRANGER, INFORMANT, "informant", LF_ResidentMemory.Tier(INFORMANT, STRANGER));
			infRegular = CrimeToldWithin(REGULAR, INFORMANT, "informant", LF_ResidentMemory.TIER_FRIENDLY);
		}

		ctx.True(!barStranger, "the bar tells a stranger no crime rumor");
		ctx.True(barRegular, "the bar tells a friendly regular a crime rumor");
		ctx.True(!infStranger, "the informant tells a stranger no crime rumor");
		ctx.True(infRegular, "the informant tells a friendly regular a crime rumor");

		LF_ResidentTalkRumor.ResetStatics();
		LF_ResidentMemory.ResetStatics();
		LF_MapKnown.ResetStatics();
	}
}
