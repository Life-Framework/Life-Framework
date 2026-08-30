// LF_QuestLog.c - client/server quest-log wire format.
//
// The tracker state (map<string, LF_QuestState>) is not RplProp-able, so the
// server encodes it into a compact string and pushes it to the owning client by
// RPC; the client decodes it back for the quest menu. Format:
//
//   questId|statusInt|index:progress,index:progress;questId|...
//
// Quest ids are simple tokens, so '|' ';' ',' ':' are safe separators.

class LF_QuestLogEntry
{
	string m_sQuestId;
	int m_iStatus;                 // LF_EQuestStatus numeric value
	ref array<int> m_aProgress;    // sparse: index -> progress
};

class LF_QuestLogFormat
{
//------------------------------------------------------------------------------------------------
	static string Encode(map<string, ref LF_QuestState> states)
	{
		string result = "";
		if (!states)
			return result;

		foreach (string questId, LF_QuestState state : states)
		{
			if (!result.IsEmpty())
				result += ";";
			result += questId;
			result += "|";
			result += state.GetStatusValue().ToString();
			result += "|";

			map<int, int> progress = state.GetProgressMap();
			if (progress)
			{
				bool first = true;
				foreach (int index, int value : progress)
				{
					if (!first)
						result += ",";
					first = false;
					result += index.ToString();
					result += ":";
					result += value.ToString();
				}
			}
		}
return result;
	}

	//------------------------------------------------------------------------------------------------
	static bool Decode(string serialized, out array<ref LF_QuestLogEntry> entries)
	{
		entries = new array<ref LF_QuestLogEntry>();
		if (serialized.IsEmpty())
			return true;

		TStringArray questParts = {};
		serialized.Split(";", questParts, true);
		foreach (string part : questParts)
		{
			TStringArray fields = {};
			part.Split("|", fields, true);
			if (fields.Count() < 2)
				continue;

			LF_QuestLogEntry entry = new LF_QuestLogEntry();
			entry.m_sQuestId = fields[0];
			entry.m_iStatus = fields[1].ToInt();
			entry.m_aProgress = new array<int>();

			if (fields.Count() >= 3 && !fields[2].IsEmpty())
			{
				TStringArray pairs = {};
				fields[2].Split(",", pairs, true);
				foreach (string pair : pairs)
				{
					TStringArray kv = {};
					pair.Split(":", kv, true);
					if (kv.Count() != 2)
						continue;
					int index = kv[0].ToInt();
					int value = kv[1].ToInt();
					while (entry.m_aProgress.Count() <= index)
						entry.m_aProgress.Insert(0);
					entry.m_aProgress[index] = value;
				}
			}

			entries.Insert(entry);
		}
		return true;
	}
};
