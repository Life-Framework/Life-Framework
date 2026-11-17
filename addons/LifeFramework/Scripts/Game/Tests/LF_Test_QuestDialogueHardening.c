// red-proof: 2026-11-17, `tools\cli test --no-build --red LF_Test_QuestDialogueHardening` (RedProof() restores the old
// behaviour of each guard: the offer read as level 1 / unemployed / no faction, a late node taken by a closed scene,
// no Forget on disconnect, no QUEST bucket); every assertion that guards one fails, so the run is RED-OK.

// tier: LOGIC
//! Quests and dialogue hardening (#2403, life review 2026-11-17): the conversation offers a gated quest
//! to the players who meet its gates (one reader shared with accept), a late node does not reopen a
//! closed scene, a disconnect frees the dialogue session and the town-board gap, and the quest and
//! dialogue asks have their own rate bucket.
class LF_Test_QuestDialogueHardening : LF_Test
{
	protected static const int PLAYER = 990403;

	override string GetName()
	{
		return "quests/dialogue-hardening";
	}

	override void Run(LF_TestContext ctx)
	{
		RunOfferGates(ctx);
		RunLateNode(ctx);
		RunDisconnectLeaks(ctx);
		RunRateBucket(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_QuestDefinition Quest(string id, int minLevel, LF_EJobType job, string faction)
	{
		LF_QuestObjective objective = new LF_QuestObjective();
		objective.m_eType = LF_EQuestEventType.TALK;
		objective.m_sFilter = "giver";
		objective.m_iTarget = 1;

		LF_QuestDefinition def = new LF_QuestDefinition();
		def.m_sQuestId = id;
		def.m_iMinLevel = minLevel;
		def.m_eRequiredJob = job;
		def.m_sRequiredFaction = faction;
		def.m_aObjectives = {};
		def.m_aObjectives.Insert(objective);
		return def;
	}

	//------------------------------------------------------------------------------------------------
	//! A gated quest is offered to the player who meets the gates, and only to them.
	protected void RunOfferGates(LF_TestContext ctx)
	{
		array<ref LF_QuestDefinition> quests = {};
		quests.Insert(Quest("q_level", 3, LF_EJobType.UNEMPLOYED, ""));
		LF_QuestTracker tracker = new LF_QuestTracker();

		LF_TestQuestStanding veteran = new LF_TestQuestStanding(3, LF_EJobType.UNEMPLOYED, "");
		LF_TestQuestStanding novice = new LF_TestQuestStanding(1, LF_EJobType.UNEMPLOYED, "");
		if (RedProof())
			veteran = novice;

		LF_QuestDefinition offered = tracker.NextOffer(quests, veteran, 0);
		ctx.True(offered != null && offered.m_sQuestId == "q_level", "a level 3 player is offered the level 3 quest");
		ctx.True(tracker.NextOffer(quests, novice, 0) == null, "a level 1 player is not offered it");

		quests.Clear();
		quests.Insert(Quest("q_job", 0, LF_EJobType.POLICE, ""));
		LF_TestQuestStanding officer = new LF_TestQuestStanding(1, LF_EJobType.POLICE, "");
		if (RedProof())
			officer = novice;

		ctx.True(tracker.NextOffer(quests, officer, 0) != null, "a police officer is offered the police quest");
		ctx.True(tracker.NextOffer(quests, novice, 0) == null, "an unemployed player is not");

		quests.Clear();
		quests.Insert(Quest("q_faction", 0, LF_EJobType.UNEMPLOYED, "CIVILIAN"));
		LF_TestQuestStanding civilian = new LF_TestQuestStanding(1, LF_EJobType.UNEMPLOYED, "CIVILIAN");
		if (RedProof())
			civilian = novice;

		ctx.True(tracker.NextOffer(quests, civilian, 0) != null, "a player of the faction is offered the faction quest");
		ctx.True(tracker.NextOffer(quests, novice, 0) == null, "a player of no faction is not");

		// The offer and the accept read the same standing: whatever is offered can be taken.
		ctx.True(tracker.CanTake(quests[0], civilian, 0, tracker.GetCompletedQuestIds()), "an offered quest can be taken by the same standing");
	}

	//------------------------------------------------------------------------------------------------
	//! A node is taken only by an open scene that asked for it (waiting after a start or a reply).
	protected void RunLateNode(LF_TestContext ctx)
	{
		bool open = false;
		if (RedProof())
			open = true;

		ctx.False(LF_DialogueScene.AcceptsNode(open, true, true), "a late node does not reopen a closed scene");
		ctx.False(LF_DialogueScene.AcceptsNode(true, false, true), "a node nobody asked for is ignored by an open scene");
		ctx.False(LF_DialogueScene.AcceptsNode(true, true, false), "a node for another NPC is ignored");
		ctx.True(LF_DialogueScene.AcceptsNode(true, true, true), "the answer to a start or a reply is taken");
	}

	//------------------------------------------------------------------------------------------------
	//! A player who leaves holds no dialogue session and no town-board gap tick.
	protected void RunDisconnectLeaks(LF_TestContext ctx)
	{
		LF_Dialogue.ResetStatics();
		LF_TownBoardMapKnown.ResetStatics();

		LF_Dialogue.Track(PLAYER, EntityID.INVALID);
		ctx.Equal(1, LF_Dialogue.SessionCount(), "the dialogue holds the player's session");
		if (!RedProof())
			LF_Dialogue.Forget(PLAYER);

		ctx.Equal(0, LF_Dialogue.SessionCount(), "a disconnect frees the dialogue session");
		ctx.True(LF_Dialogue.SessionOf(PLAYER) == null, "and the player has no session");

		ctx.True(LF_TownBoardMapKnown.Admit(PLAYER, 1000), "the first town card ask is admitted");
		ctx.Equal(1, LF_TownBoardMapKnown.TrackedCount(), "the gap holds the player's tick");
		if (!RedProof())
			LF_TownBoardMapKnown.Forget(PLAYER);

		ctx.Equal(0, LF_TownBoardMapKnown.TrackedCount(), "a disconnect frees the gap tick");
		ctx.True(LF_TownBoardMapKnown.Admit(PLAYER, 1001), "the next player with this id is not held to the old gap");

		LF_Dialogue.ResetStatics();
		LF_TownBoardMapKnown.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	//! The quest and dialogue asks have a bucket of their own: eight in the window, then refused, and
	//! a full COMMS bucket does not touch it.
	protected void RunRateBucket(LF_TestContext ctx)
	{
		LF_RateLimiter limiter = new LF_RateLimiter();
		int refused;
		LF_ERequestKind kind = LF_ERequestKind.QUEST;
		if (RedProof())
			kind = LF_ERequestKind.COMMS;

		int window = LF_RateLimit.WindowMs(kind);
		int burst = LF_RateLimit.Burst(kind);
		ctx.True(burst >= 8, "a person clicking through a conversation is under the burst");
		for (int i = 0; i < burst; i++)
		{
			limiter.Take(PLAYER, kind, 100, window, burst, refused);
		}

		ctx.Equal(LF_ERateVerdict.REJECT, limiter.Take(PLAYER, kind, 101, window, burst, refused), "a script past the burst is refused");
		ctx.Equal(LF_ERateVerdict.ALLOW, limiter.Take(PLAYER, LF_ERequestKind.CHESS, 101, LF_RateLimit.WindowMs(LF_ERequestKind.CHESS), LF_RateLimit.Burst(LF_ERequestKind.CHESS), refused), "another kind is not touched");
		ctx.Equal(LF_ERateVerdict.ALLOW, limiter.Take(PLAYER, kind, 101 + window, window, burst, refused), "the next window admits again");
	}
}

//------------------------------------------------------------------------------------------------
//! A player's standing with fixed values, for LF_Test_QuestDialogueHardening.
class LF_TestQuestStanding : LF_QuestStanding
{
	protected int m_iLevel;
	protected LF_EJobType m_eJob;
	protected string m_sFaction;

	//------------------------------------------------------------------------------------------------
	void LF_TestQuestStanding(int level, LF_EJobType job, string faction)
	{
		m_iLevel = level;
		m_eJob = job;
		m_sFaction = faction;
	}

	override int LevelFor(LF_QuestDefinition def)
	{
		return m_iLevel;
	}

	override LF_EJobType JobType()
	{
		return m_eJob;
	}

	override string FactionKey()
	{
		return m_sFaction;
	}
}
