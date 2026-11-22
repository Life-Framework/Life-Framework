//------------------------------------------------------------------------------------------------
//! Felling a real map tree with an axe (#3260, design docs/roadmap/lumber.md "Felling a real map
//! tree"). `LF_FellAction` is the hold action: it targets one standing map tree (AssignTree), shows its
//! species class and the chop time, and on completion asks `TryFell` on the server. TryFell is the one
//! place that re-checks every LF_TreeFelling rule, so a forged perform cannot skip them. A fell
//! hides the tree (visibility and collision off, never killed, so it can regrow), records its key in the
//! LF_FelledTrees ledger, draws the fall on every screen and drops the logs along the fallen line (LF_TreeFall), takes the stamina, pays the XP and publishes
//! LF_OnTreeFelled and LF_OnGathered. A refusal toasts its Lumber string key and logs
//! `[LFDebug:Lumber] fell refused: <reason>`.
class LF_FellAction : ScriptedUserAction
{
	static const ResourceName AXE = "{9CAC68BA551417A4}Prefabs/Tools/Axe/Axe.et";
	static const ResourceName LOG_PREFAB = "{32F5A906D8A573BC}Prefabs/Resources/WoodCutting/WoodLog_01_Small.et";
	//! A hardwood tree drops the heavy log the sawmill's hardwood recipes take.
	static const ResourceName HARDWOOD_LOG_PREFAB = "{FC7D57CC9C0798D7}Prefabs/Resources/WoodCutting/WoodLog_Hardwood.et";
	//! The gather family a softwood fell pays through (LF_JobLevelSources pays gather_logging for it).
	static const string FAMILY_LOGGING = "LOGGING";
	//! Hardwood pays its own award (fell_hardwood) directly, so its event carries a family that pays nothing.
	static const string FAMILY_HARDWOOD = "LOGGING_HARDWOOD";
	static const string ACTION_FELL_HARDWOOD = "fell_hardwood";
	//! The prompt helper that carries this action (client side, never replicated; LF_FellPrompt spawns it).
	static const ResourceName HELPER = "{7A3C5E91D2B40F60}Prefabs/Lumber/LF_FellHelper.et";

	//! The Duration the helper prefab declares, in seconds; the longest hardwood row (60 s).
	static const float HELPER_DURATION_S = 60; // lf-fixed: mirrors the Duration in LF_FellHelper.et, not a tunable

	protected IEntity m_Tree;

	//------------------------------------------------------------------------------------------------
	//! Points the action at one standing map tree (the helper that carries this action calls it).
	void AssignTree(IEntity tree)
	{
		m_Tree = tree;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the axe is in the user's hands (right hand or the left gadget slot).
	static bool HoldsAxe(IEntity user)
	{
		if (!user)
			return false;

		return LF_GatherAction.HasToolIn(user, AXE, false);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!m_Tree || LF_FelledTrees.IsHidden(m_Tree))
			return false;

		return HoldsAxe(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!m_Tree || !HoldsAxe(user))
			return false;

		string key = KeyOfTree(m_Tree);
		if (key.IsEmpty() || LF_FelledTrees.IsFelled(key))
		{
			SetCannotPerformReason(LF_TreeFelling.RefusalKey(LF_FellRefusal.ALREADY_FELLED));
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! "Fell tree (softwood, 20 s)": the species class and how long the hold takes.
	override bool GetActionNameScript(out string outName)
	{
		LF_TreeFellingConfig config = LF_TreeFellingConfig.Get();
		if (!m_Tree || !config)
		{
			outName = WidgetManager.Translate("#LF-Lumber_Fell", WidgetManager.Translate("#LF-Lumber_ClassTree"), "");
			return true;
		}

		LF_TreeYieldRow yield = LF_TreeFelling.YieldFor(PrefabOf(m_Tree), config);
		LF_TreeKind kind = LF_TreeFelling.KindOf(PrefabOf(m_Tree), config);
		string className = ClassName(kind.m_eClass);
		string seconds = "";
		if (yield)
			seconds = yield.m_iChopSeconds.ToString();

		outName = WidgetManager.Translate("#LF-Lumber_Fell", WidgetManager.Translate(className), seconds);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		// The prompt helper lives on the machine of the player who holds the axe (LF_FellPrompt), so this
		// runs there: a listen host fells at once, a client asks the server, which re-checks everything.
		if (!pUserEntity || !m_Tree || !LF_FellPrompt.IsLocalUser(pUserEntity))
			return;

		vector at = m_Tree.GetOrigin();
		m_Tree = null;
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
			return;

		controller.LF_AskFell(at);
	}

	//------------------------------------------------------------------------------------------------
	//! The hold lasts the chop seconds of the tree's yield row: the prefab's Duration is the longest
	//! row (HELPER_DURATION_S), and a quicker tree speeds the progress up by the ratio.
	override float GetActionProgressScript(float fProgress, float timeSlice)
	{
		LF_TreeFellingConfig config = LF_TreeFellingConfig.Get();
		if (m_Tree && config)
		{
			LF_TreeYieldRow yield = LF_TreeFelling.YieldFor(PrefabOf(m_Tree), config);
			if (yield && yield.m_iChopSeconds > 0)
				timeSlice = timeSlice * HELPER_DURATION_S / yield.m_iChopSeconds;
		}

		return super.GetActionProgressScript(fProgress, timeSlice);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the whole fell. Re-checks every rule from facts it measures itself (the axe is the
	//! caller's, read from the user's hands), then hides the tree, records the key, drops the logs,
	//! takes the stamina, pays the XP and publishes the events.
	//! \param axeInHands what HoldsAxe(user) said; a test passes it directly
	//! \param[out] logs the log entities dropped, for the caller to track (empty while they wait for the landing)
	//! \param dropDelayMs when the logs drop: -1 when the trunk lands (LF_TreeFallPlan.FALL_MS), 0 at once (a test)
	//! \return OK on a fell, else the first refusal (logged, nothing changed)
	static LF_FellRefusal TryFell(IEntity user, IEntity tree, bool axeInHands, out array<IEntity> logs, int dropDelayMs = -1)
	{
		logs = {};
		if (!user || !tree)
			return Refuse(LF_FellRefusal.NOT_A_TREE, "no user or no tree");

		LF_TreeFellingConfig config = LF_TreeFellingConfig.Get();
		ResourceName prefab = PrefabOf(tree);
		string key = KeyOfTree(tree);
		if (!Tree.Cast(tree) || prefab.IsEmpty())
			return Refuse(LF_FellRefusal.NOT_A_TREE, "not a map tree");

		LF_FellFacts facts = MeasureFacts(user, tree, prefab, key, axeInHands, config);
		LF_FellRefusal refusal = LF_TreeFelling.Check(facts, config);
		if (refusal != LF_FellRefusal.OK)
			return Refuse(refusal, key);

		LF_TreeKind kind = LF_TreeFelling.KindOf(prefab, config);
		LF_TreeYieldRow yield = LF_TreeFelling.YieldFor(prefab, config);
		if (!LF_FelledTrees.Ledger().Fell(key, LF_FelledTrees.NowS(), config.RegrowSecondsOf(kind.m_eClass)))
			return Refuse(LF_FellRefusal.ALREADY_FELLED, key);

		// Read the tree before it is hidden: where it stands, how tall, and which way it falls (away from the player).
		vector foot = tree.GetOrigin();
		float height = LF_TreeFall.HeightOf(tree);
		float yaw = LF_TreeFallPlan.FallYaw(user.GetOrigin(), foot);
		LF_TreeFall.Broadcast(key, yaw);
		LF_FelledTrees.Hide(tree, key);
		LF_FelledTreesReplication.Publish(key);
		LF_TreeRegrow.EnsureTicking();
		if (dropDelayMs < 0)
			dropDelayMs = LF_TreeFallPlan.FALL_MS;

		ResourceName logPrefab = LogPrefabOf(kind.m_eClass);
		LF_TreeFall.ScheduleDrop(logPrefab, foot, yaw, height, yield.m_iLogs, dropDelayMs, logs);
		SpendStamina(user, yield.m_iStaminaPct);
		LF_Debug.Info(LF_FelledTrees.FEATURE, string.Format("felled %1 (%2 logs, %3)", key, yield.m_iLogs, typename.EnumToString(LF_TreeWoodClass, kind.m_eClass)));

		string uid = LF_Utils.GetPlayerUID(user);
		PayXp(uid, kind.m_eClass, yield, yield.m_iLogs, logPrefab);
		LF_EventBus.Publish(LF_OnTreeFelled.Create(uid, key, kind.m_eClass, kind.m_iSize, yield.m_iLogs));
		LF_Utils.NotifyEntity(user, "#LF-Lumber_Felled", "", 4.0, yield.m_iLogs.ToString());
		return LF_FellRefusal.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! The log item a tree of this wood drops: hardwood gives the heavy hardwood log, anything else the softwood log.
	static ResourceName LogPrefabOf(LF_TreeWoodClass woodClass)
	{
		if (woodClass == LF_TreeWoodClass.HARDWOOD)
			return HARDWOOD_LOG_PREFAB;

		return LOG_PREFAB;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_FellRefusal Refuse(LF_FellRefusal reason, string detail)
	{
		LF_Debug.Warn(LF_FelledTrees.FEATURE, string.Format("fell refused: %1 (%2)", typename.EnumToString(LF_FellRefusal, reason), detail));
		return reason;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_FellFacts MeasureFacts(IEntity user, IEntity tree, ResourceName prefab, string key, bool axeInHands, LF_TreeFellingConfig config)
	{
		LF_FellFacts facts = new LF_FellFacts();
		facts.m_bHasAxe = axeInHands;
		facts.m_sPrefab = prefab;
		facts.m_bFelled = LF_FelledTrees.IsFelled(key);
		facts.m_fDistanceM = vector.Distance(user.GetOrigin(), tree.GetOrigin());
		facts.m_bInSafeZone = LF_Zones.IsSafe(tree.GetOrigin());
		if (config)
		{
			facts.m_fRoadDistanceM = LF_FelledTrees.RoadDistance(tree.GetOrigin(), config.m_fRoadClearM);
			facts.m_fBuildingDistanceM = LF_FelledTrees.BuildingDistance(tree.GetOrigin());
		}

		CharacterStaminaComponent stamina = LF_Component<CharacterStaminaComponent>.Find(user);
		if (stamina)
			facts.m_fStamina = stamina.GetStamina();

		return facts;
	}

	//------------------------------------------------------------------------------------------------
	//! AddStamina has to run on the machine that owns the character, so the cost goes to the owner's
	//! controller (an Owner RPC; a host's own player runs it at once). With no controller (a test body)
	//! it is applied here.
	protected static void SpendStamina(IEntity user, int percent)
	{
		SCR_PlayerController controller;
		PlayerManager players = GetGame().GetPlayerManager();
		if (players)
			controller = SCR_PlayerController.Cast(players.GetPlayerController(players.GetPlayerIdFromControlledEntity(user)));

		if (controller)
		{
			controller.LF_SpendFellStamina(percent / 100.0);
			return;
		}

		ApplyStamina(user, percent / 100.0);
	}

	//------------------------------------------------------------------------------------------------
	//! Takes `fraction` of the stamina bar from the character; runs on the machine that owns it.
	static void ApplyStamina(IEntity user, float fraction)
	{
		CharacterStaminaComponent stamina = LF_Component<CharacterStaminaComponent>.Find(user);
		if (stamina)
			stamina.AddStamina(-fraction);
	}

	//------------------------------------------------------------------------------------------------
	//! Softwood pays gather_logging through LF_OnGathered (family LOGGING); hardwood pays fell_hardwood
	//! directly and its event carries a family that pays nothing, so a fell never pays twice.
	protected static void PayXp(string uid, LF_TreeWoodClass woodClass, LF_TreeYieldRow yield, int logs, ResourceName logPrefab)
	{
		string family = FAMILY_LOGGING;
		if (woodClass == LF_TreeWoodClass.HARDWOOD)
		{
			family = FAMILY_HARDWOOD;
			string action = yield.m_sXpAction;
			if (action.IsEmpty())
				action = ACTION_FELL_HARDWOOD;

			LF_JobLevels.Award(uid, action);
		}

		LF_EventBus.Publish(LF_OnGathered.Create(uid, "fell_tree", family, logPrefab, logs, 1.0));
	}

	//------------------------------------------------------------------------------------------------
	static ResourceName PrefabOf(IEntity tree)
	{
		EntityPrefabData data = tree.GetPrefabData();
		if (!data)
			return ResourceName.Empty;

		return data.GetPrefabName();
	}

	//------------------------------------------------------------------------------------------------
	static string KeyOfTree(IEntity tree)
	{
		ResourceName prefab = PrefabOf(tree);
		if (prefab.IsEmpty())
			return string.Empty;

		return LF_TreeFelling.KeyOf(tree.GetOrigin(), prefab);
	}

	//------------------------------------------------------------------------------------------------
	//! The string key of a wood class, for the prompt.
	static string ClassName(LF_TreeWoodClass woodClass)
	{
		if (woodClass == LF_TreeWoodClass.HARDWOOD)
			return "#LF-Lumber_ClassHardwood";

		if (woodClass == LF_TreeWoodClass.SOFTWOOD)
			return "#LF-Lumber_ClassSoftwood";

		return "#LF-Lumber_ClassTree";
	}
}
