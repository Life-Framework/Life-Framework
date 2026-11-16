// tier: LOGIC
// red-proof: 2026-11-16, `--red LF_Test_RadioPlaylist` (RedProof skips marking the tracks playable): RED-OK, failed "the missing track 1 is skipped (expected 2, got -1)", "the list still wraps past the gap", "two tracks are playable". Then `--only` green.
//! Radio playlists (LF_RadioPlaylist, Feature/Atmosphere): a list rotates in order and wraps, an
//! hour-limited list plays only in its hours, a missing track is skipped with one error and a list
//! with nothing playable answers -1. Pure data in, indexes out; no world, no sound project.
class LF_Test_RadioPlaylist : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "atmosphere/radio-playlist";
	}

	//------------------------------------------------------------------------------------------------
	//! A list of `count` tracks; `playable` marks them playable without loading a project.
	protected LF_RadioPlaylist Make(string slot, int count, bool playable)
	{
		LF_RadioPlaylist list = new LF_RadioPlaylist();
		list.m_sSlot = slot;
		list.m_aTracks = {};
		for (int i = 0; i < count; i++)
		{
			LF_RadioTrack track = new LF_RadioTrack();
			track.m_fSeconds = 60;
			if (!RedProof())
				track.SetPlayable(playable);

			list.m_aTracks.Insert(track);
		}

		return list;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// Rotates in order and wraps.
		LF_RadioPlaylist three = Make("test_rotate", 3, true);
		ctx.Equal(0, three.Next(-1), "the first start plays track 0");
		ctx.Equal(1, three.Next(0), "track 0 is followed by track 1");
		ctx.Equal(2, three.Next(1), "track 1 is followed by track 2");
		ctx.Equal(0, three.Next(2), "the last track wraps to track 0");
		ctx.Equal(3, three.PlayableCount(), "three playable tracks");

		// One playable track follows itself (it keeps looping).
		LF_RadioPlaylist one = Make("test_single", 1, true);
		ctx.Equal(0, one.Next(0), "a one-track list plays that track again");

		// Hours: a list limited to 20:00-02:00 plays only there; no hours is always.
		LF_RadioPlaylist night = Make("test_night", 2, true);
		night.m_fHourFrom = 20;
		night.m_fHourTo = 2;
		ctx.True(night.InHours(21), "21:00 is inside 20:00-02:00");
		ctx.True(night.InHours(1), "01:00 is inside 20:00-02:00, past midnight");
		ctx.False(night.InHours(12), "noon is outside 20:00-02:00");
		ctx.False(night.InHours(3), "03:00 is outside 20:00-02:00");
		ctx.True(three.InHours(12), "a list without hours plays at noon");
		ctx.True(three.InHours(0), "a list without hours plays at midnight");

		// A missing track is skipped, with an error, once.
		LF_RadioPlaylist gap = Make("test_gap", 3, true);
		gap.m_aTracks[1] = new LF_RadioTrack(); // no project: the real probe calls it missing, with no engine load
		ctx.Equal(2, gap.Next(0), "the missing track 1 is skipped");
		ctx.Equal(0, gap.Next(2), "the list still wraps past the gap");
		ctx.Equal(2, gap.PlayableCount(),"two tracks are playable");
		ctx.False(gap.m_aTracks[1].TakeMissingNote(), "the skip said so already, once");

		// Nothing playable: the radio has no track.
		LF_RadioPlaylist none = Make("test_none", 2, false);
		ctx.Equal(-1, none.Next(-1), "a list with no playable track answers -1");
		LF_RadioPlaylist empty = new LF_RadioPlaylist();
		ctx.Equal(-1, empty.Next(-1), "a list with no tracks answers -1");

		// The registry finds a list by slot; Reset clears only this class's statics.
		LF_RadioPlaylists.Add(three);
		ctx.True(LF_RadioPlaylists.Find("test_rotate") == three, "Find returns the list of that slot");
		ctx.True(LF_RadioPlaylists.Find("test_other") == null, "a slot without a list is null");
		LF_RadioPlaylists.ResetStatics();
	}
}
