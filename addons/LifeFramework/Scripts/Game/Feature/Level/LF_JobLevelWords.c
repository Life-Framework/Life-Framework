//------------------------------------------------------------------------------------------------
//! What a player reads when a job-level gate refuses them (#2700). Levels are never shown
//! (outline decision 7): the gate says "do more runs first", not "level 3 needed". Pure, so a LOGIC
//! test can drive it. Core text stays neutral; the pack's 1989 phrasing overrides the rows by Id.
class LF_JobLevelWords
{
	//------------------------------------------------------------------------------------------------
	//! \param job The job the gate reads.
	//! \param have The player's level in that job.
	//! \param need The level the gate wants.
	//! \return A `#` localization key saying why the player is refused: the Near row for one level
	//! short, the Far row for further, in the job's own words (a generic row for any other job).
	//! Empty when the player meets the gate. The key never carries a number.
	static string Refusal(string job, int have, int need)
	{
		if (have >= need)
			return "";

		bool near = need - have <= 1;

		if (job == LF_JobLevels.JOB_FARMER)
		{
			if (near)
				return "#LF-Level_Near_Farmer";
			return "#LF-Level_Far_Farmer";
		}

		if (job == "MINER")
		{
			if (near)
				return "#LF-Level_Near_Miner";
			return "#LF-Level_Far_Miner";
		}

		if (job == "LUMBERJACK")
		{
			if (near)
				return "#LF-Level_Near_Lumberjack";
			return "#LF-Level_Far_Lumberjack";
		}

		if (job == "MEDIC")
		{
			if (near)
				return "#LF-Level_Near_Medic";
			return "#LF-Level_Far_Medic";
		}

		if (job == "POLICE")
		{
			if (near)
				return "#LF-Level_Near_Police";
			return "#LF-Level_Far_Police";
		}

		if (job == LF_JobLevels.JOB_DELIVERY)
		{
			if (near)
				return "#LF-Level_Near_Delivery";
			return "#LF-Level_Far_Delivery";
		}

		if (near)
			return "#LF-Level_Near_Any";
		return "#LF-Level_Far_Any";
	}
}
