//------------------------------------------------------------------------------------------------
//! What a player reads when a job-level gate refuses them (#2700). The job families (farmer, miner,
//! lumberjack, medic, police, delivery) say "do more runs first" in their own words and show no number
//! (outline decision 7). Every other trade (Smelting, Salvage, Civilian) names itself and the level in
//! words, "Needs Smelting 8" (#4856): a gate with no row of its own never leaves a player guessing.
//! Pure, so a LOGIC test can drive it. Core text stays neutral; the pack's 1989 phrasing overrides the
//! family rows by Id.
class LF_JobLevelWords
{
	//------------------------------------------------------------------------------------------------
	//! \param job The job the gate reads.
	//! \param have The player's level in that job.
	//! \param need The level the gate wants.
	//! \return Why the player is refused. A job family gets a `#` localization key (the Near row for one
	//! level short, the Far row for further, in the job's own words, no number); any other job gets the
	//! finished line "Needs <Trade> <level>" (`Needs`). Empty when the player meets the gate. Show it
	//! with `LF_Terms.Shown`, which takes a key or finished text.
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

		return Needs(job, need);
	}

	//------------------------------------------------------------------------------------------------
	//! \return "Needs Smelting 8": the trade's printed name and the level the gate wants, or
	//! "Needs level 8" for a gate that names no trade.
	static string Needs(string job, int need)
	{
		string trade = LF_PlayerMenuSheet.SkillName(job);
		if (trade.IsEmpty())
			return LF_Terms.Text("#LF-Level_NeedsAny", need.ToString());

		return LF_Terms.Text("#LF-Level_Needs", trade, need.ToString());
	}
}
