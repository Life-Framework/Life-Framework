// LF_QuestEvent.c - pure record of a world event routed to quest trackers.
// The manager translates actual engine events (kill, gather, area entry, ...)
// into this struct; objectives match on type + filter, and add or set progress.

class LF_QuestEvent
{
	LF_EQuestEventType m_eType;

	//! Filter key depending on type: faction key (KILL), resource name (GATHER/PROCESS),
	//! giver id (TALK), area id (AREA_ENTER). Empty string matches any.
	string m_sFilter;

	//! Value depending on type: count reported for COLLECT_REPORT, 1 for the rest.
	int m_iValue;
};