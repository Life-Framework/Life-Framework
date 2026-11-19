//------------------------------------------------------------------------------------------------
//! Felling a real map tree with an axe (#3260, design docs/roadmap/lumber.md "Felling a real map
//! tree"). `LF_FellAction` is the hold action: it targets one standing map tree (AssignTree), shows its
//! species class and the chop time, and on completion asks `TryFell` on the server. TryFell is the one
//! place that re-checks every LF_TreeFelling rule, so a forged perform cannot skip them. A fell
//! hides the tree (visibility and collision off, never killed, so it can regrow), records its key in the
//! LF_FelledTrees ledger, drops the logs beside the trunk, takes the stamina, pays the XP and publishes
//! LF_OnTreeFelled and LF_OnGathered. A refusal toasts its Lumber string key and logs
//! `[LFDebug:Lumber] fell refused: <reason>`.
class LF_FellAction : ScriptedUserAction
{
	static const ResourceName AXE = "{9CAC68BA551417A4}Prefabs/Tools/Axe/Axe.et";
	static const ResourceName LOG_PREFAB = "{32F5A906D8A573BC}Prefabs/Resources/WoodCutting/WoodLog_01_Small.et";
	//! The gather family a softwood fell pays through (LF_JobLevelSources pays gather_logging for it).
	static const string FAMILY_LOGGING = "LOGGING";
	//! Hardwood pays its own award (fell_hardwood) directly, so its event carries a family that pays nothing.
	static const string FAMILY_HARDWOOD = "LOGGING_HARDWOOD";
	static const string ACTION_FELL_HARDWOOD = "fell_hardwood";
	//! Metres from the trunk the logs land, and the angle between two logs.
	static const float LOG_RING_M = 1.2;
	static const float LOG_STEP_DEG = 60;

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
		if (!pOwnerEntity || !pUserEntity || !LF_NetworkUtils.ShouldApplyAction(pOwnerEntity))
			return;

		array<IEntity> logs = {};
		LF_FellRefusal refusal = TryFell(pUserEntity, m_Tree, HoldsAxe(pUserEntity), logs);
		if (refusal != LF_FellRefusal.OK)
			LF_Utils.NotifyEntity(pUserEntity, LF_TreeFelling.RefusalKey(refusal), "", 4.0);

		m_Tree = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the whole fell. Re-checks every rule from facts it measures itself (the axe is the
	//! caller's, read from the user's hands), then hides the tree, records the key, drops the logs,
	//! takes the stamina, pays the XP and publishes the events.
	//! \param axeInHands what HoldsAxe(user) said; a test passes it directly
	//! \param[out] logs the log entities dropped, for the caller to track
	//! \return OK on a fell, else the first refusal (logged, nothing changed)
	static LF_FellRefusal TryFell(IEntity user, IEntity tree, bool axeInHands, out array<IEntity> logs)
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

		LF_FelledTrees.Hide(tree, key);
		DropLogs(tree, yield.m_iLogs, logs);
		SpendStamina(user, yield.m_iStaminaPct);
		LF_Debug.Info(LF_FelledTrees.FEATURE, string.Format("felled %1 (%2 logs, %3)", key, logs.Count(), typename.EnumToString(LF_TreeWoodClass, kind.m_eClass)));

		string uid = LF_Utils.GetPlayerUID(user);
		PayXp(uid, kind.m_eClass, yield, logs.Count());
		LF_EventBus.Publish(LF_OnTreeFelled.Create(uid, key, kind.m_eClass, kind.m_iSize, logs.Count()));
		LF_Utils.NotifyEntity(user, "#LF-Lumber_Felled", "", 4.0, logs.Count().ToString());
		return LF_FellRefusal.OK;
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
	//! One log per count on a ring round the trunk, each on the ground. Fills `logs` with what spawned.
	protected static void DropLogs(IEntity tree, int count, out array<IEntity> logs)
	{
		BaseWorld world = GetGame().GetWorld();
		vector foot = tree.GetOrigin();
		for (int i = 0; i < count; i++)
		{
			float angle = i * LOG_STEP_DEG * Math.DEG2RAD;
			vector at = foot + Vector(Math.Sin(angle) * LOG_RING_M, 0, Math.Cos(angle) * LOG_RING_M);
			at[1] = world.GetSurfaceY(at[0], at[2]) + 0.15;
			IEntity log = LF_Utils.SpawnEntityPrefab(LOG_PREFAB, at, Vector(angle * Math.RAD2DEG, 0, 0));
			if (!log)
			{
				LF_Debug.Error(LF_FelledTrees.FEATURE, "log prefab did not spawn: " + LOG_PREFAB);
				continue;
			}

			logs.Insert(log);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Best effort: AddStamina has to run on the owner, so a remote player's cost may not land.
	protected static void SpendStamina(IEntity user, int percent)
	{
		CharacterStaminaComponent stamina = LF_Component<CharacterStaminaComponent>.Find(user);
		if (stamina)
			stamina.AddStamina(-percent / 100.0);
	}

	//------------------------------------------------------------------------------------------------
	//! Softwood pays gather_logging through LF_OnGathered (family LOGGING); hardwood pays fell_hardwood
	//! directly and its event carries a family that pays nothing, so a fell never pays twice.
	protected static void PayXp(string uid, LF_TreeWoodClass woodClass, LF_TreeYieldRow yield, int logs)
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

		LF_EventBus.Publish(LF_OnGathered.Create(uid, "fell_tree", family, LOG_PREFAB, logs, 1.0));
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
