//------------------------------------------------------------------------------------------------
//! Where a step's target is. Kinds: `garage` (the nearest vehicle garage), `job` (the nearest
//! workplace of the job the id names: the miner desk, the woodlot), `giver` (a placed quest giver),
//! anything else an LF_Places kind (the id picks one place, empty means the nearest). The core holds no
//! coordinate: every answer is a placed entity's origin.
class LF_ActivityTargets
{
	static const string KIND_GARAGE = "garage";
	static const string KIND_JOB = "job";
	static const string KIND_GIVER = "giver";

	//------------------------------------------------------------------------------------------------
	//! \return false when the world does not place the target.
	static bool Resolve(string kind, string id, vector from, out vector position)
	{
		if (kind.IsEmpty())
			return false;

		array<IEntity> candidates = {};
		if (kind == KIND_GARAGE)
		{
			foreach (LF_VehicleGarageComponent garage : LF_VehicleGarageComponent.GetAll())
			{
				if (garage && garage.GetOwner())
					candidates.Insert(garage.GetOwner());
			}
		}
		else if (kind == KIND_JOB)
		{
			LF_Workplace.FindAll(id, candidates);
		}
		else if (kind == KIND_GIVER)
		{
			LF_QuestManager manager = LF_QuestManager.GetInstance();
			LF_QuestGiverComponent giver;
			if (manager)
				giver = manager.GetGiver(id);

			if (giver && giver.GetOwner())
				candidates.Insert(giver.GetOwner());
		}
		else if (!id.IsEmpty())
		{
			return LF_Places.FindOrigin(kind, id, position);
		}
		else
		{
			LF_Places.FindAll(kind, candidates);
		}

		return Nearest(candidates, from, position);
	}

	//------------------------------------------------------------------------------------------------
	static bool Nearest(notnull array<IEntity> candidates, vector from, out vector position)
	{
		float best = -1;
		foreach (IEntity candidate : candidates)
		{
			if (!candidate)
				continue;

			float distance = vector.DistanceSqXZ(candidate.GetOrigin(), from);
			if (best < 0 || distance < best)
			{
				best = distance;
				position = candidate.GetOrigin();
			}
		}

		return best >= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Metres on the ground from `from` to the target; -1 when the world does not place it.
	static float DistanceTo(string kind, string id, vector from)
	{
		vector position;
		if (!Resolve(kind, id, from, position))
			return -1;

		return vector.DistanceXZ(position, from);
	}
}

//------------------------------------------------------------------------------------------------
//! The live facts of one character on the server: their body, their saved progress, their account.
class LF_ActivityLiveFacts : LF_ActivityFacts
{
	protected IEntity m_Body;
	protected string m_sUid;
	protected LF_PlayerAccount m_Account;
	protected LF_QuestTracker m_Tracker;

	//------------------------------------------------------------------------------------------------
	void LF_ActivityLiveFacts(IEntity body)
	{
		m_Body = body;
		if (!body)
			return;

		m_sUid = LF_Utils.GetPlayerUID(body);
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (accounts)
			m_Account = accounts.GetAccount(body);

		LF_QuestTrackerComponent tracker = LF_QuestTrackerComponent.Cast(body.FindComponent(LF_QuestTrackerComponent));
		if (tracker)
			m_Tracker = tracker.GetTracker();
	}

	//------------------------------------------------------------------------------------------------
	string Uid()
	{
		return m_sUid;
	}

	//------------------------------------------------------------------------------------------------
	override int Level(string job)
	{
		return LF_JobLevels.LevelOf(m_sUid, job);
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLicence(string licence)
	{
		LF_ELicenseType type = typename.StringToEnum(LF_ELicenseType, licence);
		if (typename.EnumToString(LF_ELicenseType, type) != licence)
		{
			LF_Debug.Error(LF_ActivityTable.FEATURE, "activity gates on '" + licence + "', which is not a licence");
			return false;
		}

		LF_LicenseManagerComponent licences;
		if (m_Body)
			licences = LF_LicenseManagerComponent.Cast(m_Body.FindComponent(LF_LicenseManagerComponent));

		if (licences)
			return licences.HasLicense(type);

		array<LF_ELicenseType> held = {};
		if (!m_sUid.IsEmpty())
			LF_Progression.GetLicenses(m_sUid, held);

		return held.Find(type) != -1;
	}

	//------------------------------------------------------------------------------------------------
	override string Faction()
	{
		if (!m_Account)
			return "CIVILIAN";

		return typename.EnumToString(LF_Faction, m_Account.GetFaction());
	}

	//------------------------------------------------------------------------------------------------
	override bool OnDuty()
	{
		return m_Account && m_Account.IsOnDuty();
	}

	//------------------------------------------------------------------------------------------------
	override string HeldJob()
	{
		LF_PlayerJobComponent jobs = LF_Component<LF_PlayerJobComponent>.Find(m_Body);
		if (!jobs)
			return "UNEMPLOYED";

		return typename.EnumToString(LF_EJobType, jobs.GetJob());
	}

	//------------------------------------------------------------------------------------------------
	override int Carried(string prefab)
	{
		if (!m_Body || prefab.IsEmpty())
			return 0;

		return LF_Utils.MaxInt(LF_InventoryUtils.GetAmount(m_Body, prefab), 0);
	}

	//------------------------------------------------------------------------------------------------
	override bool HasVehicleOut()
	{
		return !m_sUid.IsEmpty() && !LF_VehicleOwnership.LatestInWorld(m_sUid).IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	override bool HasStoredVehicle()
	{
		return !m_sUid.IsEmpty() && !LF_VehicleOwnership.LatestStored(m_sUid).IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Papers in hand: a quest that hands out the ID card is turned in, or no quest hands one out.
	override bool Registered()
	{
		LF_QuestManager manager = LF_QuestManager.GetInstance();
		if (!manager)
			return true;

		bool cardQuest = false;
		foreach (string questId, LF_QuestDefinition def : manager.GetDefinitions())
		{
			if (def && def.m_bRewardIdCard)
			{
				cardQuest = true;
				break;
			}
		}

		if (!cardQuest)
			return true;

		return manager.HasEarnedIdCard(m_sUid);
	}

	//------------------------------------------------------------------------------------------------
	override int Xp(string job)
	{
		return LF_JobLevels.XpOf(m_sUid, job);
	}

	//------------------------------------------------------------------------------------------------
	override LF_EQuestStatus Status(string questId)
	{
		if (!m_Tracker)
			return LF_EQuestStatus.AVAILABLE;

		return m_Tracker.GetStatus(questId);
	}

	//------------------------------------------------------------------------------------------------
	override void Trades(notnull array<string> into)
	{
		into.Clear();
		LF_JobsConfig config = LF_JobsConfig.Get();
		if (!config || !config.m_aTracks)
			return;

		foreach (LF_JobTrackDef track : config.m_aTracks)
		{
			if (track && track.m_sJob != LF_JobLevels.JOB_CIVILIAN)
				into.Insert(track.m_sJob);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A vehicle the player owns (in the world, stored or impounded) whose price row names this capacity
	//! class or a bigger one on the ladder.
	override bool OwnsVehicleClass(string capacityClass)
	{
		LF_CapacityConfig capacity = LF_CapacityConfig.Get();
		LF_VehiclePricesConfig prices = LF_VehiclePricesConfig.Get();
		if (m_sUid.IsEmpty() || !capacity || !prices)
			return false;

		int needed = LF_CapacityRules.RungOf(capacity.m_aRows, capacityClass);
		if (needed < 0)
		{
			LF_Debug.Error(LF_ActivityTable.FEATURE, "activity gates on vehicle class '" + capacityClass + "', which the capacity ladder does not list");
			return false;
		}

		foreach (LF_VehicleRecord record : LF_VehicleOwnership.ExportAll())
		{
			if (!record || record.m_sOwnerUid != m_sUid)
				continue;

			string ownedClass = prices.CapacityClassOf(record.m_sPrefab);
			if (ownedClass.IsEmpty())
				continue;

			if (LF_CapacityRules.RungOf(capacity.m_aRows, ownedClass) >= needed)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasCriminalRecord()
	{
		return !m_sUid.IsEmpty() && LF_CrimeRecord.Total(m_sUid) > 0;
	}

	//------------------------------------------------------------------------------------------------
	override bool InCrew()
	{
		return LF_OrganizationManager.GetByCharacter(LF_Hideout.CharacterIdOf(m_Body)) != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasPickedPerk()
	{
		if (m_sUid.IsEmpty())
			return false;

		array<ref LF_TradePerkRow> rows = LF_TradePerks.RowsOf(m_sUid);
		return rows && !rows.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	override bool HasFlag(string flag)
	{
		return !m_sUid.IsEmpty() && LF_Progression.HasFlag(m_sUid, flag);
	}
}

//------------------------------------------------------------------------------------------------
//! Server: runs a player's guided starters. "Start" accepts an activity's first step; the quest system
//! chains the rest (LF_ActivityTable.BuildQuest). While a step runs, its target is the player's one
//! waypoint (with the distance) and a mark on their own map, and the step's note shows as it starts.
//! A checked step is read every CHECK_MS and passed (its SIGNAL reported) once the check holds, at
//! once when it already holds (the job held, the pickaxe carried). With two activities running, the
//! waypoint follows the one started or advanced last ("Guide me" puts it back on another).
//!
//! Nothing is saved here: the steps are quests (their turn-ins are saved), the focus and the XP
//! baselines are rebuilt from the running steps on spawn.
class LF_ActivityGuide
{
	static const string WAYPOINT_ID = "activity";
	static const string KEY_PREFIX = "activity:";
	static const int ICON = 0; // lf-fixed: vanilla marker config index, as the arrival guide
	static const int COLOUR = 3; // lf-fixed: vanilla marker colour index, apart from the arrival guide's
	static const float EXPIRY_SECONDS = 7200; // lf-fixed: safety net only; the next step moves the mark
	static const int CHECK_MS = 2000; // lf-fixed: how often a checked step is read, not a server tunable
	static const float NOTE_SECONDS = 10.0; // lf-fixed: long enough to read one sentence

	//! uid -> quest id of the step the waypoint follows.
	protected static ref map<string, string> s_mFocus;
	protected static map<string, string> LzFocus()
	{
		if (!s_mFocus)
			s_mFocus = new map<string, string>();
		return s_mFocus;
	}
	//! "uid|questId" -> the job XP when the step started (XP checks).
	protected static ref map<string, int> s_mXpAtStart = new map<string, int>();
	protected static ref LF_JobWaypointSink s_Sink;
	protected static bool s_bTicking;

	//------------------------------------------------------------------------------------------------
	static void SetSink(LF_JobWaypointSink sink)
	{
		s_Sink = sink;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_JobWaypointSink Sink()
	{
		if (!s_Sink)
			s_Sink = new LF_JobWaypointRplSink();

		return s_Sink;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: "Start" for one player's body. \return empty when the first step began, else why not
	//! ("unknown", "locked", "running", "done", "refused" when the quest system said no).
	static string Start(IEntity body, string activityId)
	{
		if (!Replication.IsServer() || !body)
			return "refused";

		LF_ActivityTable.EnsureRegistered();
		LF_ActivityDef def = LF_ActivityTable.Find(activityId);
		LF_ActivityLiveFacts facts = new LF_ActivityLiveFacts(body);
		string refusal = LF_ActivityRules.StartRefusal(def, facts);
		if (!refusal.IsEmpty())
		{
			LF_Debug.Info(LF_ActivityTable.FEATURE, string.Format("start of %1 refused for %2: %3", activityId, facts.Uid(), refusal));
			return refusal;
		}

		LF_QuestTrackerComponent tracker = LF_QuestTrackerComponent.Cast(body.FindComponent(LF_QuestTrackerComponent));
		if (!tracker)
		{
			LF_Debug.Error(LF_ActivityTable.FEATURE, "start of " + activityId + " refused: the body has no quest tracker");
			return "refused";
		}

		string firstQuest = LF_ActivityTable.QuestIdOf(def.m_sId, def.m_aSteps[0].m_sStepId);
		// The step's own note is the news (OnStepStarted), so the accept stays quiet.
		if (!tracker.AcceptQuest(firstQuest, false, true))
		{
			LF_Debug.Warn(LF_ActivityTable.FEATURE, string.Format("start of %1 refused for %2: the quest system did not take %3", activityId, facts.Uid(), firstQuest));
			return "refused";
		}

		LF_Debug.Info(LF_ActivityTable.FEATURE, string.Format("%1 started %2 (%3 steps)", facts.Uid(), activityId, def.StepCount()));
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: "Guide me" on a running activity: the waypoint goes back to its step. \return false when
	//! nothing of it runs.
	static bool Focus(IEntity body, string activityId)
	{
		LF_ActivityDef def = LF_ActivityTable.Find(activityId);
		if (!def || !body)
			return false;

		LF_ActivityLiveFacts facts = new LF_ActivityLiveFacts(body);
		int running = LF_ActivityRules.RunningStep(def, facts);
		if (running < 0)
		{
			LF_Debug.Info(LF_ActivityTable.FEATURE, string.Format("guide for %1 refused for %2: no step runs", activityId, facts.Uid()));
			return false;
		}

		string questId = LF_ActivityTable.QuestIdOf(def.m_sId, def.m_aSteps[running].m_sStepId);
		LzFocus();
		s_mFocus.Set(facts.Uid(), questId);
		Mark(body, facts.Uid(), def, running);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a step's quest was accepted (Start, or the chain). The waypoint follows it and a check
	//! that already holds passes it at once. The note shows here for a step the player started; a chain
	//! step's note is already the quest system's errand toast (its description).
	static void OnStepStarted(IEntity body, string questId, bool startedByPlayer)
	{
		LF_ActivityDef def;
		int index;
		if (!body || !LF_ActivityTable.StepOf(questId, def, index))
			return;

		LF_ActivityLiveFacts facts = new LF_ActivityLiveFacts(body);
		string uid = facts.Uid();
		if (uid.IsEmpty())
			return;

		LF_ActivityStep step = def.m_aSteps[index];
		LzFocus();
		s_mFocus.Set(uid, questId);
		if (step.m_eCheck == LF_EActivityCheck.XP)
			s_mXpAtStart.Set(uid + "|" + questId, facts.Xp(step.m_sCheckParam));

		LF_Debug.Info(LF_ActivityTable.FEATURE, string.Format("%1 is on %2 step %3/%4 (%5)", uid, def.m_sId, index + 1, def.StepCount(), step.m_sStepId));
		if (startedByPlayer && !step.m_sNoteKey.IsEmpty())
			LF_Utils.NotifyEntity(body, step.m_sNoteKey, step.m_sTitleKey, NOTE_SECONDS, "", LF_NotificationManagerComponent.SLOT_STEP);

		Mark(body, uid, def, index);
		EnsureTicking();
		// Already done (the job held, the tool carried): pass it now rather than at the next read.
		CheckBody(body);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: after a turn-in or a spawn, puts the mark on the focused step, or on another running
	//! step, or clears it. \return The quest id guided to, or empty.
	static string Refresh(IEntity body)
	{
		if (!body)
			return string.Empty;

		LF_ActivityLiveFacts facts = new LF_ActivityLiveFacts(body);
		string uid = facts.Uid();
		if (uid.IsEmpty())
			return string.Empty;

		LF_ActivityDef def;
		int index;
		LzFocus();
		string focus = s_mFocus.Get(uid);
		LF_EQuestStatus status = facts.Status(focus);
		bool focusRuns = status == LF_EQuestStatus.ACTIVE || status == LF_EQuestStatus.COMPLETED;
		// Between two steps (the focused step was just turned in, the chain starts the next one right
		// after): the marks stay where they are. Clearing here let the arrival guide's mark flash up
		// before the next step's mark replaced it (review B12).
		if (!focusRuns && BetweenSteps(focus, facts))
			return focus;

		if (!focusRuns || !LF_ActivityTable.StepOf(focus, def, index))
		{
			focus = AnyRunning(facts, def, index);
			if (focus.IsEmpty())
			{
				Clear(uid);
				return string.Empty;
			}

			LzFocus();
			s_mFocus.Set(uid, focus);
		}

		Mark(body, uid, def, index);
		EnsureTicking();
		return focus;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether `questId` is a turned-in activity step whose next step has not started yet: the moment
	//! between a turn-in and the chain accepting the next step.
	static bool BetweenSteps(string questId, notnull LF_ActivityFacts facts)
	{
		LF_ActivityDef def;
		int index;
		if (questId.IsEmpty() || !LF_ActivityTable.StepOf(questId, def, index))
			return false;

		if (facts.Status(questId) != LF_EQuestStatus.TURNED_IN || index + 1 >= def.StepCount())
			return false;

		string next = LF_ActivityTable.QuestIdOf(def.m_sId, def.m_aSteps[index + 1].m_sStepId);
		return facts.Status(next) == LF_EQuestStatus.AVAILABLE;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the note of the step the waypoint follows, again (a rejoin or a respawn mid-run: the
	//! player reads where they were). \return false when no activity step runs or it has no note.
	static bool Remind(IEntity body)
	{
		if (!body || !Replication.IsServer())
			return false;

		LF_ActivityLiveFacts facts = new LF_ActivityLiveFacts(body);
		string uid = facts.Uid();
		if (uid.IsEmpty())
			return false;

		LF_ActivityDef def;
		int index;
		LzFocus();
		string focus = s_mFocus.Get(uid);
		LF_EQuestStatus status = facts.Status(focus);
		if (status != LF_EQuestStatus.ACTIVE && status != LF_EQuestStatus.COMPLETED)
			focus = AnyRunning(facts, def, index);

		if (focus.IsEmpty() || !LF_ActivityTable.StepOf(focus, def, index))
			return false;

		LF_ActivityStep step = def.m_aSteps[index];
		if (step.m_sNoteKey.IsEmpty())
			return false;

		LF_Debug.Info(LF_ActivityTable.FEATURE, string.Format("%1 is reminded of %2 step %3", uid, def.m_sId, step.m_sStepId));
		LF_Utils.NotifyEntity(body, step.m_sNoteKey, step.m_sTitleKey, NOTE_SECONDS, "", LF_NotificationManagerComponent.SLOT_STEP);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The first running step of any activity, or empty.
	protected static string AnyRunning(notnull LF_ActivityFacts facts, out LF_ActivityDef def, out int index)
	{
		LF_ActivityConfig config = LF_ActivityTable.Get();
		if (!config)
			return string.Empty;

		foreach (LF_ActivityDef activity : config.m_aActivities)
		{
			int running = LF_ActivityRules.RunningStep(activity, facts);
			if (running < 0)
				continue;

			def = activity;
			index = running;
			return LF_ActivityTable.QuestIdOf(activity.m_sId, activity.m_aSteps[running].m_sStepId);
		}

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts the step's target on the player's screen and map, or clears both when the step has none or
	//! the world does not place it.
	protected static void Mark(IEntity body, string uid, notnull LF_ActivityDef def, int index)
	{
		LF_ActivityStep step = def.m_aSteps[index];
		vector target;
		if (step.m_sTargetKind.IsEmpty() || !LF_ActivityTargets.Resolve(step.m_sTargetKind, step.m_sTargetId, body.GetOrigin(), target))
		{
			if (!step.m_sTargetKind.IsEmpty())
				LF_Debug.Warn(LF_ActivityTable.FEATURE, string.Format("no waypoint for %1 step %2: no %3 '%4' is placed", def.m_sId, step.m_sStepId, step.m_sTargetKind, step.m_sTargetId));

			Clear(uid);
			return;
		}

		int x = Math.Round(target[0]);
		int z = Math.Round(target[2]);
		string label = step.m_sTargetLabelKey;
		if (label.IsEmpty())
			label = step.m_sTitleKey;

		LF_MapShareRow row = LF_MapShareRow.Make(KEY_PREFIX + uid, LF_EMapShareAudience.PLAYER, uid, x, z, ICON, COLOUR, label, LF_MapShares.Now() + EXPIRY_SECONDS);
		if (!LF_MapShares.Put(row))
			LF_Debug.Warn(LF_ActivityTable.FEATURE, "the map mark for " + uid + " was refused; the waypoint still goes");

		Sink().Mark(uid, WAYPOINT_ID, label, x, z);
		Sink().Objective(uid, WAYPOINT_ID, step.m_sTitleKey);
		LF_Debug.Info(LF_ActivityTable.FEATURE, string.Format("%1 is guided to %2 step %3: waypoint at %4 %5", uid, def.m_sId, step.m_sStepId, x, z));
	}

	//------------------------------------------------------------------------------------------------
	static void Clear(string uid)
	{
		if (uid.IsEmpty())
			return;

		LzFocus();
		s_mFocus.Remove(uid);
		bool removed = LF_MapShares.Remove(KEY_PREFIX + uid);
		Sink().Clear(uid, WAYPOINT_ID);
		Sink().Objective(uid, WAYPOINT_ID, string.Empty);
		if (removed)
			LF_Debug.Info(LF_ActivityTable.FEATURE, uid + " has no activity step left: the waypoint and the map mark are cleared");
	}

	//------------------------------------------------------------------------------------------------
	//! Server: reads every running checked step of one body and passes those whose check holds.
	//! \return How many steps were passed.
	static int CheckBody(IEntity body)
	{
		if (!body || !Replication.IsServer())
			return 0;

		LF_ActivityConfig config = LF_ActivityTable.Get();
		LF_QuestManager manager = LF_QuestManager.GetInstance();
		if (!config || !manager)
			return 0;

		LF_ActivityLiveFacts facts = new LF_ActivityLiveFacts(body);
		string uid = facts.Uid();
		int passed = 0;
		foreach (LF_ActivityDef def : config.m_aActivities)
		{
			int running = LF_ActivityRules.RunningStep(def, facts);
			if (running < 0)
				continue;

			LF_ActivityStep step = def.m_aSteps[running];
			string questId = LF_ActivityTable.QuestIdOf(def.m_sId, step.m_sStepId);
			if (step.m_eCheck == LF_EActivityCheck.NONE || facts.Status(questId) != LF_EQuestStatus.ACTIVE)
				continue;

			float distance = -1;
			if (step.m_eCheck == LF_EActivityCheck.ARRIVE)
				distance = LF_ActivityTargets.DistanceTo(step.m_sTargetKind, step.m_sTargetId, body.GetOrigin());

			int xpAtStart;
			if (step.m_eCheck == LF_EActivityCheck.XP && !s_mXpAtStart.Find(uid + "|" + questId, xpAtStart))
			{
				// A spawn mid-step (rejoin, restart) counts from now.
				xpAtStart = facts.Xp(step.m_sCheckParam);
				s_mXpAtStart.Set(uid + "|" + questId, xpAtStart);
			}

			if (!LF_ActivityRules.CheckHolds(step, facts, distance, xpAtStart))
				continue;

			LF_Debug.Info(LF_ActivityTable.FEATURE, string.Format("%1 passed %2 step %3 (%4)", uid, def.m_sId, step.m_sStepId, typename.EnumToString(LF_EActivityCheck, step.m_eCheck)));
			s_mXpAtStart.Remove(uid + "|" + questId);
			// The signal completes the step; the quest system turns it in and starts the next one.
			if (manager.ReportSignal(body, LF_ActivityTable.SignalOf(questId)))
				passed++;
		}

		return passed;
	}

	//------------------------------------------------------------------------------------------------
	protected static void EnsureTicking()
	{
		if (s_bTicking || !GetGame() || !GetGame().GetCallqueue())
			return;

		s_bTicking = true;
		GetGame().GetCallqueue().CallLater(Tick, CHECK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Every CHECK_MS: the players the guide follows. Stops when it follows nobody.
	protected static void Tick()
	{
		LzFocus();
		if (s_mFocus.IsEmpty())
		{
			StopTicking();
			return;
		}

		PlayerManager players = GetGame().GetPlayerManager();
		if (!players)
			return;

		array<int> ids = {};
		players.GetPlayers(ids);
		foreach (int playerId : ids)
		{
			IEntity body = players.GetPlayerControlledEntity(playerId);
			LzFocus();
			if (body && s_mFocus.Contains(LF_Utils.GetPlayerUID(body)))
				CheckBody(body);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void StopTicking()
	{
		if (!s_bTicking)
			return;

		s_bTicking = false;
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(Tick);
	}

	//------------------------------------------------------------------------------------------------
	//! A player left: their focus and baselines go (the marks went with their client).
	static void Forget(string uid)
	{
		if (uid.IsEmpty())
			return;

		LzFocus();
		s_mFocus.Remove(uid);
		array<string> keys = {};
		foreach (string key, int xp : s_mXpAtStart)
		{
			if (key.StartsWith(uid + "|"))
				keys.Insert(key);
		}

		foreach (string key : keys)
		{
			s_mXpAtStart.Remove(key);
		}
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		StopTicking();
		s_mFocus = new map<string, string>();
		s_mXpAtStart = new map<string, int>();
		s_Sink = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ActivityGuide's statics (LF_StaticReset finds it by reflection).
class LF_ActivityGuide_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ActivityGuide";
	}

	override void Apply()
	{
		LF_ActivityGuide.ResetStatics();
	}
}
