//------------------------------------------------------------------------------------------------
//! One row of the "What to do next" tab as the server sends it. Keys, not text: the client prints them.
class LF_ActivityRow
{
	string m_sId;
	string m_sNameKey;
	string m_sPitchKey;
	int m_iPayPerHour;
	//! Metres from the player to where it happens; -1 when the world does not place it.
	int m_iDistance = -1;
	LF_EActivityState m_eState;
	//! ACTIVE: the running step (0-based) and its title; DONE: the step count.
	int m_iStep;
	int m_iSteps;
	string m_sStepTitleKey;
}

//------------------------------------------------------------------------------------------------
//! The wire of the tab: rows joined by ';', fields by '|'. Every field is an id, a key or a number,
//! none of which holds either character; a row that would is cut out on the server (logged).
class LF_ActivityData
{
	static const int VERSION = 2;
	static const string ROW_SEP = ";";
	static const string FIELD_SEP = "|";
	static const int FIELDS = 9;

	ref array<ref LF_ActivityRow> m_aRows = {};
	//! The player has no papers yet: the tab sends them to the clerk instead of listing runs.
	bool m_bNeedsPapers;

	//------------------------------------------------------------------------------------------------
	static bool Clean(string value)
	{
		return value.IndexOf(ROW_SEP) == -1 && value.IndexOf(FIELD_SEP) == -1;
	}

	//------------------------------------------------------------------------------------------------
	string Encode()
	{
		string papers = "0";
		if (m_bNeedsPapers)
			papers = "1";

		string wire = VERSION.ToString() + FIELD_SEP + papers;
		foreach (LF_ActivityRow row : m_aRows)
		{
			if (!Clean(row.m_sId) || !Clean(row.m_sNameKey) || !Clean(row.m_sPitchKey) || !Clean(row.m_sStepTitleKey))
			{
				LF_Debug.Error(LF_ActivityTable.FEATURE, "activity row " + row.m_sId + " holds a separator and is left out");
				continue;
			}

			string fields = row.m_sId + FIELD_SEP + row.m_sNameKey + FIELD_SEP + row.m_sPitchKey + FIELD_SEP + row.m_iPayPerHour.ToString();
			fields += FIELD_SEP + row.m_iDistance.ToString() + FIELD_SEP + row.m_eState.ToString() + FIELD_SEP + row.m_iStep.ToString();
			fields += FIELD_SEP + row.m_iSteps.ToString() + FIELD_SEP + row.m_sStepTitleKey;
			wire += ROW_SEP + fields;
		}

		return wire;
	}

	//------------------------------------------------------------------------------------------------
	//! \return null on a wire of another version or a broken row.
	static LF_ActivityData Decode(string wire)
	{
		array<string> rows = {};
		wire.Split(ROW_SEP, rows, false);
		if (rows.IsEmpty())
			return null;

		array<string> head = {};
		rows[0].Split(FIELD_SEP, head, false);
		if (head.Count() != 2 || head[0].ToInt() != VERSION)
			return null;

		LF_ActivityData data = new LF_ActivityData();
		data.m_bNeedsPapers = head[1] == "1";
		for (int i = 1; i < rows.Count(); i++)
		{
			array<string> fields = {};
			rows[i].Split(FIELD_SEP, fields, false);
			if (fields.Count() != FIELDS)
				return null;

			LF_ActivityRow row = new LF_ActivityRow();
			row.m_sId = fields[0];
			row.m_sNameKey = fields[1];
			row.m_sPitchKey = fields[2];
			row.m_iPayPerHour = fields[3].ToInt();
			row.m_iDistance = fields[4].ToInt();
			row.m_eState = fields[5].ToInt();
			row.m_iStep = fields[6].ToInt();
			row.m_iSteps = fields[7].ToInt();
			row.m_sStepTitleKey = fields[8];
			data.m_aRows.Insert(row);
		}

		return data;
	}

	//------------------------------------------------------------------------------------------------
	//! The run in progress first, then what can start, then what is done; table order inside each
	//! (review B1: the step the player is on was hidden below the fold).
	static void SortRows(notnull array<ref LF_ActivityRow> rows)
	{
		array<ref LF_ActivityRow> sorted = {};
		array<LF_EActivityState> order = {LF_EActivityState.ACTIVE, LF_EActivityState.AVAILABLE, LF_EActivityState.DONE, LF_EActivityState.LOCKED};
		foreach (LF_EActivityState state : order)
		{
			foreach (LF_ActivityRow row : rows)
			{
				if (row.m_eState == state)
					sorted.Insert(row);
			}
		}

		rows.Clear();
		foreach (LF_ActivityRow row : sorted)
		{
			rows.Insert(row);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the rows one body may see, in table order, with its distances and states.
	static LF_ActivityData Build(notnull array<ref LF_ActivityDef> all, notnull LF_ActivityFacts facts, vector from, bool hasPosition)
	{
		LF_ActivityData data = new LF_ActivityData();
		data.m_bNeedsPapers = !facts.Registered();
		array<LF_ActivityDef> listed = {};
		LF_ActivityRules.Listed(all, facts, listed);
		foreach (LF_ActivityDef def : listed)
		{
			LF_ActivityRow row = new LF_ActivityRow();
			row.m_sId = def.m_sId;
			row.m_sNameKey = def.m_sNameKey;
			row.m_sPitchKey = def.m_sPitchKey;
			row.m_iPayPerHour = def.m_iPayPerHour;
			row.m_eState = LF_ActivityRules.StateOf(def, facts);
			row.m_iSteps = def.StepCount();
			if (hasPosition && !def.m_sPlaceKind.IsEmpty())
			{
				float distance = LF_ActivityTargets.DistanceTo(def.m_sPlaceKind, def.m_sPlaceId, from);
				if (distance >= 0)
					row.m_iDistance = Math.Round(distance);
			}

			if (row.m_eState == LF_EActivityState.ACTIVE)
			{
				int running = LF_ActivityRules.RunningStep(def, facts);
				if (running < 0)
					running = LF_Utils.MinInt(LF_ActivityRules.DoneSteps(def, facts), def.StepCount() - 1);

				row.m_iStep = running;
				row.m_sStepTitleKey = def.m_aSteps[running].m_sTitleKey;
			}
			else if (row.m_eState == LF_EActivityState.DONE)
			{
				row.m_iStep = def.StepCount();
			}

			data.m_aRows.Insert(row);
		}

		SortRows(data.m_aRows);
		return data;
	}
}
