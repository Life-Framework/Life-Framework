//------------------------------------------------------------------------------------------------
//! The race courses file (#1922): Configs/Racing/. A course is data: the ordered ids of its
//! checkpoints, the laps, the entry fee, the minimum entrants, the lobby wait and the payout split.
//! A checkpoint is named by id only. The pack places a checkpoint component in the world and it
//! registers under that id in LF_Places; no coordinate or entity name is written here.
[BaseContainerProps(configRoot: true)]
class LF_RaceCoursesConfig
{
	[Attribute("", UIWidgets.Object, "The courses. A pack adds its own courses by id")]
	ref array<ref LF_RaceCourseRecord> m_aCourses;
}

//------------------------------------------------------------------------------------------------
//! One race course. The id is what a race saves and a sign names, so never rename one.
[BaseContainerProps()]
class LF_RaceCourseRecord : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Stable course id, lower case (harbour_sprint). Races and results refer to a course by it")]
	string m_sId;

	[Attribute("", UIWidgets.Auto, "Checkpoint ids in the order a racer must pass them. The last one closes a lap. Each id is a checkpoint placed in the world; ids are unique inside the course")]
	ref array<string> m_aCheckpoints;

	[Attribute("1", UIWidgets.EditBox, "Laps to run. The finish is the last checkpoint of the last lap")]
	int m_iLaps;

	[Attribute("0", UIWidgets.EditBox, "Entry fee in whole dollars, paid into the pot when the racer joins")]
	int m_iEntryFee;

	[Attribute("2", UIWidgets.EditBox, "Entrants needed before the lobby may start the race")]
	int m_iMinEntrants;

	[Attribute("60", UIWidgets.EditBox, "Seconds the lobby waits for entrants before it starts or cancels")]
	int m_iLobbySeconds;

	[Attribute("", UIWidgets.Auto, "Percent of the pot paid to first, second, third place and so on. Adds up to 100")]
	ref array<int> m_aPayoutPercent;
}

//------------------------------------------------------------------------------------------------
//! What a racer has done so far in one race. Server only, never persisted: a race that is cut by
//! a restart is abandoned and the entry fees come back (the race slice owns that).
class LF_RaceProgress
{
	//! Checkpoints passed inside the current lap; also the index of the next one.
	int m_iNextIndex;
	//! Laps completed.
	int m_iLapsDone;
	bool m_bFinished;
	//! Milliseconds from the race start to the finish, -1 until the racer finishes.
	int m_iFinishMs = -1;
}

//------------------------------------------------------------------------------------------------
//! The verdict on one checkpoint pass.
enum LF_ERaceCheck
{
	//! Counted, the race goes on in the same lap.
	ACCEPTED,
	//! Counted, and the lap closed; more laps remain.
	LAP_DONE,
	//! Counted, and it was the last checkpoint of the last lap.
	FINISHED,
	//! Refused: the racer is ahead of the next checkpoint (a shortcut).
	SKIPPED,
	//! Refused: a checkpoint the racer already passed this lap.
	REPEATED,
	//! Refused: the id is not in this course.
	UNKNOWN,
	//! Refused: the racer already finished, or the course is unusable.
	CLOSED
}
