// LF_QuestEnums.c - shared enums for the quest system.

//------------------------------------------------------------------------------------------------
//! Lifecycle of a single quest for a single player.
enum LF_EQuestStatus
{
	AVAILABLE,   // Not accepted yet; prerequisites gate visibility
	ACTIVE,      // Accepted, objectives in progress
	COMPLETED,   // All objectives done, reward not yet claimed
	TURNED_IN,   // Reward claimed; re-acceptable after cooldown when repeatable
	FAILED       // Timed out (or abandoned in a future version)
}

//------------------------------------------------------------------------------------------------
//! World event types the quest manager routes to trackers. Each maps to an existing
//! gameplay signal: a kill, a gather/process completion, a turn-in inventory report,
//! a talk interaction, or an area entry.
enum LF_EQuestEventType
{
	KILL,
	GATHER,
	PROCESS,
	COLLECT_REPORT,
	TALK,
	AREA_ENTER
}

//------------------------------------------------------------------------------------------------
//! How an event advances an objective's progress.
enum LF_EQuestProgressMode
{
	ADD,   // Event adds to progress (kill, gather, process, talk, area)
	SET    // Event replaces progress with the reported value (collect)
}