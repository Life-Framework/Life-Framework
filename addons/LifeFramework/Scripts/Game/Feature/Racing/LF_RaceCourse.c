// LF_RaceCourse.c - the rules of a race course (#1922). Pure and static: no world, no replication.
// The race feature calls these on the server for every checkpoint trigger; a licence course can
// reuse them. A refusal logs through LF_Debug with its reason.

class LF_RaceCourse
{
	//------------------------------------------------------------------------------------------------
	//! Whether a course can be raced. Logs every defect, so a bad .conf row names itself.
	static bool IsValid(LF_RaceCourseRecord course)
	{
		if (!course)
		{
			LF_Debug.Error("Racing", "race-course refused: no course");
			return false;
		}

		bool ok = true;
		if (course.m_sId == "")
		{
			LF_Debug.Error("Racing", "race-course refused: a course has no id");
			ok = false;
		}

		if (!course.m_aCheckpoints || course.m_aCheckpoints.Count() < 2)
		{
			LF_Debug.Error("Racing", string.Format("race-course '%1' refused: needs at least two checkpoints", course.m_sId));
			ok = false;
		}
		else
		{
			for (int i = 0; i < course.m_aCheckpoints.Count(); i++)
			{
				string id = course.m_aCheckpoints[i];
				if (id == "")
				{
					LF_Debug.Error("Racing", string.Format("race-course '%1' refused: checkpoint %2 has no id", course.m_sId, i));
					ok = false;
				}
				else if (course.m_aCheckpoints.Find(id) != i)
				{
					LF_Debug.Error("Racing", string.Format("race-course '%1' refused: checkpoint '%2' is listed twice", course.m_sId, id));
					ok = false;
				}
			}
		}

		if (course.m_iLaps < 1)
		{
			LF_Debug.Error("Racing", string.Format("race-course '%1' refused: laps %2 is below 1", course.m_sId, course.m_iLaps));
			ok = false;
		}

		if (course.m_iEntryFee < 0)
		{
			LF_Debug.Error("Racing", string.Format("race-course '%1' refused: entry fee %2 is negative", course.m_sId, course.m_iEntryFee));
			ok = false;
		}

		if (course.m_iMinEntrants < 1)
		{
			LF_Debug.Error("Racing", string.Format("race-course '%1' refused: minimum entrants %2 is below 1", course.m_sId, course.m_iMinEntrants));
			ok = false;
		}

		if (course.m_iMinEntrants > LF_RaceRunner.MAX_ENTRANTS)
		{
			LF_Debug.Error("Racing", string.Format("race-course '%1' refused: minimum entrants %2 is above the lobby limit %3, so every race would be cancelled", course.m_sId, course.m_iMinEntrants, LF_RaceRunner.MAX_ENTRANTS));
			ok = false;
		}

		if (course.m_iAfkGraceSeconds < 0 || course.m_iSettleSeconds < 0)
		{
			LF_Debug.Error("Racing", string.Format("race-course '%1' refused: afk grace %2 or settle seconds %3 is negative", course.m_sId, course.m_iAfkGraceSeconds, course.m_iSettleSeconds));
			ok = false;
		}

		if (course.m_iLobbySeconds < 0)
		{
			LF_Debug.Error("Racing", string.Format("race-course '%1' refused: lobby seconds %2 is negative", course.m_sId, course.m_iLobbySeconds));
			ok = false;
		}

		if (!course.m_aPayoutPercent || course.m_aPayoutPercent.Count() < 1)
		{
			LF_Debug.Error("Racing", string.Format("race-course '%1' refused: no payout split", course.m_sId));
			ok = false;
		}
		else
		{
			int sum = 0;
			for (int p = 0; p < course.m_aPayoutPercent.Count(); p++)
			{
				if (course.m_aPayoutPercent[p] < 0)
				{
					LF_Debug.Error("Racing", string.Format("race-course '%1' refused: payout place %2 is negative", course.m_sId, p + 1));
					ok = false;
				}
				sum += course.m_aPayoutPercent[p];
			}

			if (sum != 100)
			{
				LF_Debug.Error("Racing", string.Format("race-course '%1' refused: payout split adds up to %2, not 100", course.m_sId, sum));
				ok = false;
			}
		}

		return ok;
	}

	//------------------------------------------------------------------------------------------------
	//! The checkpoint id the racer must pass next, or empty when the racer is finished or the course is bad.
	static string NextCheckpoint(LF_RaceCourseRecord course, LF_RaceProgress progress)
	{
		if (!course || !progress || !course.m_aCheckpoints || progress.m_bFinished)
			return "";

		if (progress.m_iNextIndex < 0 || progress.m_iNextIndex >= course.m_aCheckpoints.Count())
			return "";

		return course.m_aCheckpoints[progress.m_iNextIndex];
	}

	//------------------------------------------------------------------------------------------------
	//! The racer reached a checkpoint, timeMs after the start. Server only. Counts it when it is the
	//! next one in order; every other case leaves the progress alone, logs why, and returns the reason.
	static LF_ERaceCheck Pass(LF_RaceCourseRecord course, LF_RaceProgress progress, string checkpointId, int timeMs)
	{
		if (!course || !progress || !course.m_aCheckpoints || course.m_aCheckpoints.Count() < 1)
		{
			LF_Debug.Error("Racing", "race-course pass refused: no usable course or progress");
			return LF_ERaceCheck.CLOSED;
		}

		if (progress.m_bFinished)
		{
			LF_Debug.Warn("Racing", string.Format("race-course '%1' pass of '%2' refused: the racer already finished", course.m_sId, checkpointId));
			return LF_ERaceCheck.CLOSED;
		}

		int index = course.m_aCheckpoints.Find(checkpointId);
		if (index < 0)
		{
			LF_Debug.Warn("Racing", string.Format("race-course '%1' pass of '%2' refused: not a checkpoint of this course", course.m_sId, checkpointId));
			return LF_ERaceCheck.UNKNOWN;
		}

		if (index > progress.m_iNextIndex)
		{
			LF_Debug.Warn("Racing", string.Format("race-course '%1' pass of '%2' refused: skipped '%3' (shortcut)", course.m_sId, checkpointId, NextCheckpoint(course, progress)));
			return LF_ERaceCheck.SKIPPED;
		}

		if (index < progress.m_iNextIndex)
		{
			LF_Debug.Warn("Racing", string.Format("race-course '%1' pass of '%2' refused: already passed this lap", course.m_sId, checkpointId));
			return LF_ERaceCheck.REPEATED;
		}

		progress.m_iNextIndex = progress.m_iNextIndex + 1;
		if (progress.m_iNextIndex < course.m_aCheckpoints.Count())
			return LF_ERaceCheck.ACCEPTED;

		progress.m_iNextIndex = 0;
		progress.m_iLapsDone = progress.m_iLapsDone + 1;
		if (progress.m_iLapsDone < course.m_iLaps)
		{
			LF_Debug.Info("Racing", string.Format("race-course '%1' lap %2 of %3 done", course.m_sId, progress.m_iLapsDone, course.m_iLaps));
			return LF_ERaceCheck.LAP_DONE;
		}

		progress.m_bFinished = true;
		progress.m_iFinishMs = timeMs;
		LF_Debug.Info("Racing", string.Format("race-course '%1' finished at %2 ms", course.m_sId, timeMs));
		return LF_ERaceCheck.FINISHED;
	}

	//------------------------------------------------------------------------------------------------
	//! Orders racers by finish time, fastest first. finishMs[i] is racer i's time, or below 0 when the
	//! racer did not finish; those are left out. A tie keeps the lower racer index first, so the order
	//! is the same on every run. order receives the racer indices, best place first.
	static void FinishOrder(notnull array<int> finishMs, notnull array<int> order)
	{
		order.Clear();
		for (int i = 0; i < finishMs.Count(); i++)
		{
			if (finishMs[i] < 0)
				continue;

			// Insert after every racer that is not slower: a stable insertion sort.
			int at = order.Count();
			while (at > 0 && finishMs[order[at - 1]] > finishMs[i])
				at--;

			order.InsertAt(i, at);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The share of a pot for one finishing place (1 is first), in whole dollars, rounded down.
	//! A place past the split, or a bad course, gets 0.
	static int PayoutFor(LF_RaceCourseRecord course, int place, int pot)
	{
		if (!course || !course.m_aPayoutPercent || place < 1 || place > course.m_aPayoutPercent.Count() || pot < 0)
			return 0;

		return (pot * course.m_aPayoutPercent[place - 1]) / 100;
	}
}
