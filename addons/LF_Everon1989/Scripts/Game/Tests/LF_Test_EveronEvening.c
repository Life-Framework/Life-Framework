// red-proof: observed 2026-11-19 (#3180) with `tools\cli test --no-build --only LF_Test_EveronEvening` on main: cafe_owner went home at 19:00 (`the cafe owner is at work serving at 21:30` failed), no neighbour sat at the cafe or square after 19:00 and the cafe radio stopped at 19:00 (`some radio plays at 19:00` failed). `RedProof()` skips the timetable build, so nobody is anywhere.
//! #3180: under the Everon 1989 header (the pack leg of `tools\cli test`) the evening is not empty. At 21:30
//! the cafe owner is at work serving, at least two other residents are at the cafe or the square (a work
//! line of a CafeTable or ChessTable place, not home), and for every hour from 07 to 23 at least one of the
//! cafe and square radios plays. Everyone but the night-shift worker is home by 23:30. The test reads the built timetable
//! (LF_ResidentTimetable.Build drops a line whose kind is unknown), not the conf text.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronEvening : LF_Test
{
	static const string CAFE_SLOT = "atmo_cafe_radio";
	static const string SQUARE_SLOT = "atmo_square_radio";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/evening";
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsSocialKind(string kind)
	{
		return kind == "CafeTable" || kind == "ChessTable";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ResidentsConfig config = LF_ResidentsConfig.Get();
		ctx.NotNull(config, "the residents config loads under the pack");
		if (!config || !config.m_aResidents)
			return;

		int evening = 21 * 60 + 30;
		int lateNight = 23 * 60 + 30;
		int atSocial;
		bool ownerServing;
		foreach (LF_ResidentDef def : config.m_aResidents)
		{
			if (!def || RedProof())
				continue;

			// Every kind the pack names counts as placed: the check is about the day, not the map.
			LF_ResidentTimetable day = LF_ResidentTimetable.Build(def, new LF_Test_AnyKinds());
			if (!day)
				continue;

			LF_ResidentSlot now = day.At(evening);
			if (def.m_sId == "cafe_owner")
			{
				ownerServing = now.m_sRole == LF_ResidentTimetable.ROLE_WORK && now.m_sActivity == "serve" && now.m_sKind == "CafeTable";
			}
			else if (now.m_sRole == LF_ResidentTimetable.ROLE_WORK && IsSocialKind(now.m_sKind))
			{
				atSocial++;
			}

			// The night-shift worker keeps the fuel till all night by design; everyone else is home.
			if (def.m_sId != "night_shift")
				ctx.EqualStr(LF_ResidentTimetable.ROLE_HOME, day.Where(lateNight), def.m_sId + " is home at 23:30");
		}

		ctx.True(ownerServing, "the cafe owner is at work serving at 21:30");
		ctx.True(atSocial >= 2, string.Format("at least two neighbours are at the cafe or square at 21:30 (%1)", atSocial));

		LF_RadioPlaylist cafe = LF_RadioPlaylists.Find(CAFE_SLOT);
		LF_RadioPlaylist square = LF_RadioPlaylists.Find(SQUARE_SLOT);
		ctx.NotNull(cafe, "the pack has a cafe radio playlist");
		ctx.NotNull(square, "the pack has a square radio playlist");
		for (int hour = 7; hour <= 23; hour++)
		{
			bool plays;
			if (!RedProof())
			{
				// Both ends of the hour: the half hour is where a window edge would leave a gap.
				bool full = (cafe && cafe.InHours(hour)) || (square && square.InHours(hour));
				bool half = (cafe && cafe.InHours(hour + 0.5)) || (square && square.InHours(hour + 0.5));
				plays = full && half;
			}

			ctx.True(plays, string.Format("some radio plays at %1:00 and %1:30", hour));
		}
	}
}
