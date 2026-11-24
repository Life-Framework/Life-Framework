//------------------------------------------------------------------------------------------------
//! The rules of the "What to do next" list with no world: which activities a player may see, what
//! state each is in, whether "Start" may begin one, and whether a step's check holds. The facts come
//! through LF_ActivityFacts, so a LOGIC test fills them by hand and the server reads the live ones
//! (LF_ActivityLiveFacts).

//------------------------------------------------------------------------------------------------
enum LF_EActivityState
{
	LOCKED,		//!< The player may not do it yet (level, licence, faction): not listed
	AVAILABLE,	//!< Listed with "Start"
	ACTIVE,		//!< A step is running: listed with the step and "Guide me"
	DONE		//!< The last step is done: listed as done, nothing to press
}

//------------------------------------------------------------------------------------------------
//! What the rules need to know about one player. The defaults are a new civilian with nothing.
class LF_ActivityFacts
{
	int Level(string job)
	{
		return 1;
	}

	bool HasLicence(string licence)
	{
		return false;
	}

	//! "CIVILIAN", "POLICE" or "MEDIC".
	string Faction()
	{
		return "CIVILIAN";
	}

	bool OnDuty()
	{
		return false;
	}

	//! The job the player holds now (LF_EJobType name), "UNEMPLOYED" for none.
	string HeldJob()
	{
		return "UNEMPLOYED";
	}

	int Carried(string prefab)
	{
		return 0;
	}

	bool HasVehicleOut()
	{
		return false;
	}

	bool HasStoredVehicle()
	{
		return false;
	}

	int Xp(string job)
	{
		return 0;
	}

	//! Whether the player has their papers (the arrival clerk's ID card). True in a world whose quests
	//! hand out no card.
	bool Registered()
	{
		return true;
	}

	//! The quest's status for this player; AVAILABLE when they never took it.
	LF_EQuestStatus Status(string questId)
	{
		return LF_EQuestStatus.AVAILABLE;
	}

	//! Every trade (job id) the player can level, CIVILIAN left out: what a "*" trade gate looks at.
	void Trades(notnull array<string> into)
	{
		into.Clear();
	}

	//! Whether the player owns a vehicle of this capacity class (car, pickup, van, truck) or a bigger one.
	bool OwnsVehicleClass(string capacityClass)
	{
		return false;
	}

	//! Whether the player has completed a crime (the hidden crime record is not empty).
	bool HasCriminalRecord()
	{
		return false;
	}

	//! Whether the player belongs to a crew (an organization).
	bool InCrew()
	{
		return false;
	}

	//! Whether the player has picked a trade perk at the labour office.
	bool HasPickedPerk()
	{
		return false;
	}

	//! Whether the player's progression holds the flag (the truck licence, once the truck check is passed).
	bool HasFlag(string flag)
	{
		return false;
	}
}

//------------------------------------------------------------------------------------------------
class LF_ActivityRules
{
	//! A trade gate of "*" reads the best level over every trade.
	static const string ANY_TRADE = "*";

	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	//! The player's level in the gate's trade: the trade itself, or for "*" the best of any trade.
	static int TradeLevelOf(string trade, notnull LF_ActivityFacts facts)
	{
		if (trade != ANY_TRADE)
			return facts.Level(trade);

		int best = 1;
		array<string> trades = {};
		facts.Trades(trades);
		foreach (string each : trades)
			best = LF_Utils.MaxInt(best, facts.Level(each));

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the trade level or the owned vehicle of the activity's work gate is met. An activity with
	//! both is opened by either one ("a van or Hauling 5").
	static bool WorkGateMet(notnull LF_ActivityDef def, notnull LF_ActivityFacts facts)
	{
		if (!def.m_sTrade.IsEmpty() && def.m_iMinTradeLevel > 0 && TradeLevelOf(def.m_sTrade, facts) >= def.m_iMinTradeLevel)
			return true;

		return !def.m_sOwnsClass.IsEmpty() && facts.OwnsVehicleClass(def.m_sOwnsClass);
	}

	//------------------------------------------------------------------------------------------------
	//! Why the player may not see the activity yet ("papers", "level", "licence", "faction", "trade",
	//! "record"), or empty.
	static string Gate(notnull LF_ActivityDef def, notnull LF_ActivityFacts facts)
	{
		// The clerk comes first: no run starts before the newcomer has their papers (review B13).
		if (!facts.Registered())
			return "papers";

		if (def.m_iMinLevel > 0 && facts.Level(def.JobOrCivilian()) < def.m_iMinLevel)
			return "level";

		if (!def.m_sLicence.IsEmpty() && !facts.HasLicence(def.m_sLicence))
			return "licence";

		if (!def.m_sFaction.IsEmpty() && facts.Faction() != def.m_sFaction)
			return "faction";

		if (def.HasWorkGate() && !WorkGateMet(def, facts))
			return "trade";

		if (def.m_bNeedsRecord && !facts.HasCriminalRecord())
			return "record";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! The index of the step that is running (ACTIVE or COMPLETED), or -1.
	static int RunningStep(notnull LF_ActivityDef def, notnull LF_ActivityFacts facts)
	{
		for (int i = 0; i < def.StepCount(); i++)
		{
			LF_EQuestStatus status = facts.Status(LF_ActivityTable.QuestIdOf(def.m_sId, def.m_aSteps[i].m_sStepId));
			if (status == LF_EQuestStatus.ACTIVE || status == LF_EQuestStatus.COMPLETED)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! How many steps are turned in.
	static int DoneSteps(notnull LF_ActivityDef def, notnull LF_ActivityFacts facts)
	{
		int done = 0;
		foreach (LF_ActivityStep step : def.m_aSteps)
		{
			if (facts.Status(LF_ActivityTable.QuestIdOf(def.m_sId, step.m_sStepId)) == LF_EQuestStatus.TURNED_IN)
				done++;
		}

		return done;
	}

	//------------------------------------------------------------------------------------------------
	//! DONE once the last step is turned in; ACTIVE while a step runs or part of the chain is done (a
	//! restart settles the next step back); LOCKED when the gate refuses; else AVAILABLE. A started
	//! activity stays listed even when its gate would now refuse it.
	static LF_EActivityState StateOf(notnull LF_ActivityDef def, notnull LF_ActivityFacts facts)
	{
		int count = def.StepCount();
		if (count == 0)
			return LF_EActivityState.LOCKED;

		string lastId = LF_ActivityTable.QuestIdOf(def.m_sId, def.m_aSteps[count - 1].m_sStepId);
		if (facts.Status(lastId) == LF_EQuestStatus.TURNED_IN)
			return LF_EActivityState.DONE;

		if (RunningStep(def, facts) >= 0 || DoneSteps(def, facts) > 0)
			return LF_EActivityState.ACTIVE;

		if (!Gate(def, facts).IsEmpty())
			return LF_EActivityState.LOCKED;

		return LF_EActivityState.AVAILABLE;
	}

	//------------------------------------------------------------------------------------------------
	//! The activities the tab lists, in table order: everything not LOCKED.
	static void Listed(notnull array<ref LF_ActivityDef> all, notnull LF_ActivityFacts facts, notnull array<LF_ActivityDef> into)
	{
		into.Clear();
		foreach (LF_ActivityDef def : all)
		{
			if (def && StateOf(def, facts) != LF_EActivityState.LOCKED)
				into.Insert(def);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Why "Start" may not begin the activity now ("unknown", "locked", "running", "done"), or empty.
	static string StartRefusal(LF_ActivityDef def, notnull LF_ActivityFacts facts)
	{
		if (!def)
			return "unknown";

		LF_EActivityState state = StateOf(def, facts);
		if (state == LF_EActivityState.LOCKED)
			return "locked";

		if (state == LF_EActivityState.ACTIVE)
			return "running";

		if (state == LF_EActivityState.DONE)
			return "done";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a checked step is done. ARRIVE takes the distance to the target (negative when the world
	//! does not place it: never there); XP takes the XP the job had when the step started.
	static bool CheckHolds(notnull LF_ActivityStep step, notnull LF_ActivityFacts facts, float distance, int xpAtStart)
	{
		switch (step.m_eCheck)
		{
			case LF_EActivityCheck.JOB:
				return facts.HeldJob() == step.m_sCheckParam;

			case LF_EActivityCheck.ITEM:
				return facts.Carried(step.m_sCheckParam) >= LF_Utils.MaxInt(step.m_iCheckCount, 1);

			case LF_EActivityCheck.VEHICLE:
				// A car out is what the step asks; a player with none in the garage has nothing to take out.
				return facts.HasVehicleOut() || !facts.HasStoredVehicle();

			case LF_EActivityCheck.ARRIVE:
				return distance >= 0 && distance <= step.m_fArriveRadius;

			case LF_EActivityCheck.DUTY:
				return facts.OnDuty() && facts.Faction() == step.m_sCheckParam;

			case LF_EActivityCheck.XP:
				return facts.Xp(step.m_sCheckParam) - xpAtStart >= LF_Utils.MaxInt(step.m_iCheckCount, 1);

			case LF_EActivityCheck.LICENCE:
				return facts.HasLicence(step.m_sCheckParam);

			case LF_EActivityCheck.CREW:
				return facts.InCrew();

			case LF_EActivityCheck.PERK:
				return facts.HasPickedPerk();

			case LF_EActivityCheck.FLAG:
				return facts.HasFlag(step.m_sCheckParam);
		}

		return false;
	}
}
