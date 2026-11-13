// red-proof: made LF_WantedPosters.BuildEntries skip its one-poster-per-suspect check
// (`if (false && seen.Contains(...))`) and ran `tools\cli test --no-build --only LF_Test_WantedPostersFeed`
// (2026-11-12). Observed:
//   [LFTEST]   - one poster per suspect, not per report (expected 2, got 3)
//   [LFTEST]   - the boards hold both posters (expected 2, got 3)
// Restored the check and the test passed (26 assertions).

// tier: LOGIC
//! The wanted board's face feed (docs/architecture/visual-boards-spike.md, issue 1): one poster per
//! wanted suspect, newest first, capped; the name only once identified; the level from the
//! character's standing; a wanted level of 0 takes the poster down. A change is an event that
//! queues one rebuild, and an unchanged list sends nothing. The entry carries ids and short
//! strings, survives a separator in a typed name, and a malformed one is dropped, not drawn.
class LF_Test_WantedPostersFeed : LF_Test
{
	protected static const string ROBBER = "posters-feed-robber";
	protected static const string THIEF = "posters-feed-thief";

	override string GetName()
	{
		return "police/wanted-posters-feed";
	}

	protected static LF_PlayerAccount Suspect(string uid, string first, string last)
	{
		LF_PlayerAccount account = LF_PlayerAccountManager.GetOrCreate(uid);
		if (!account.GetActiveCharacter())
			account.AddCharacter(LF_PlayerCharacter.Create(LF_TestCharacterFixture.CHARACTER_PREFAB, first, last, 25), true);

		return account;
	}

	override void Run(LF_TestContext ctx)
	{
		LF_WantedReports.ResetStatics();
		LF_PlayerAccount robber = Suspect(ROBBER, "Rob", "Tilly");
		LF_PlayerAccount thief = Suspect(THIEF, "Tess", "Kade");
		robber.SetWantedLevel(0);
		thief.SetWantedLevel(0);
		LF_WantedPosters.Flush();

		// Events, not a poll: the feed subscribes (the game mode does it at start; an earlier test in
		// the suite may have reset the bus), and a wanted change queues a rebuild.
		ctx.True(LF_WantedPosters.Install(), "the feed subscribes to the bus");
		ctx.True(LF_EventBus.SubscriberCount(LF_OnWantedChanged) > 0, "the feed listens for wanted level changes");
		ctx.True(LF_EventBus.SubscriberCount(LF_OnSuspectIdentified) > 0, "the feed listens for identifications");
		LF_WantedPosters.Flush();
		robber.SetWantedLevel(2);
		ctx.True(LF_WantedPosters.IsQueued(), "a wanted level change queues one rebuild");

		LF_WantedReports.Add(ROBBER, LF_WantedReports.OFFENCE_ROBBERY, "#AR-Jacket_M88|#AR-Pants_M88", "", "Fuel station", false);
		thief.SetWantedLevel(1);
		LF_WantedReports.Add(THIEF, LF_WantedReports.OFFENCE_VEHICLE_THEFT, "", "#AR-Vehicle_S105", "", false);
		LF_WantedReports.Add(ROBBER, LF_WantedReports.OFFENCE_BREAK_IN, "", "", "", false);

		array<string> entries = {};
		ctx.Equal(2, LF_WantedPosters.BuildEntries(4, entries), "one poster per suspect, not per report");
		LF_WantedPosterEntry first = LF_WantedPosters.Decode(entries[0]);
		LF_WantedPosterEntry second = LF_WantedPosters.Decode(entries[1]);
		ctx.True(first && second, "both entries decode");
		if (!first || !second)
			return;

		ctx.EqualStr(LF_WantedReports.OFFENCE_BREAK_IN, first.m_sOffence, "the newest report leads: the robber's break-in");
		ctx.EqualStr("", first.m_sName, "no name before the suspect is identified");
		ctx.Equal(2, first.m_iLevel, "the poster shows the character's wanted level");
		ctx.EqualStr("#AR-Vehicle_S105", second.m_sVehicle, "the thief's poster carries the vehicle key");
		ctx.False(entries[0].Contains("Fuel station"), "the public poster does not say where");
		ctx.False(entries[0].Contains("{"), "an entry carries no resource path");

		LF_WantedReports.Identify(ROBBER, "test ID check");
		LF_WantedPosters.BuildEntries(4, entries);
		ctx.EqualStr("Rob Tilly", LF_WantedPosters.Decode(entries[0]).m_sName, "the identified robber is named");
		ctx.Equal(1, LF_WantedPosters.BuildEntries(1, entries), "the face holds at most the configured posters");

		// One broadcast per change; an unchanged list sends nothing.
		int sent = LF_WantedPosters.BroadcastCount();
		ctx.True(LF_WantedPosters.Flush(), "a changed list is sent");
		ctx.False(LF_WantedPosters.Flush(), "the same list again is not sent");
		ctx.Equal(sent + 1, LF_WantedPosters.BroadcastCount(), "one broadcast for the change");
		ctx.Equal(2, LF_WantedPosters.Current().Count(), "the boards hold both posters");

		// Wanted back to 0: the robber's poster comes down.
		robber.SetWantedLevel(0);
		LF_WantedPosters.BuildEntries(4, entries);
		ctx.Equal(1, entries.Count(), "a suspect no longer wanted leaves the face");
		ctx.EqualStr("", LF_WantedPosters.Decode(entries[0]).m_sName, "the thief's poster stays, unnamed");

		// A typed separator cannot shift the fields; a malformed entry is dropped.
		LF_WantedPosterEntry odd = LF_WantedPosters.Decode(LF_WantedPosters.EncodePerson("Ann;Lee", LF_WantedReports.OFFENCE_MURDER, 9, "", ""));
		ctx.True(odd != null, "a name with a separator still decodes");
		if (odd)
		{
			ctx.EqualStr("Ann,Lee", odd.m_sName, "the separator in the name is replaced");
			ctx.Equal(5, odd.m_iLevel, "the level is clamped to 5");
		}

		ctx.True(LF_WantedPosters.Decode("P;only;three") == null, "a malformed entry is dropped");

		// The description line: what was seen, and nothing printed when nothing was seen.
		ctx.EqualStr("", LF_WantedPosters.Description("", ""), "nothing seen: no description line");
		ctx.False(LF_WantedPosters.Description("#AR-Jacket_M88", "").Contains(";"), "clothing alone is one clause");
		ctx.True(LF_WantedPosters.Description("", "#AR-Vehicle_S105").Length() > 0, "a vehicle alone still describes");

		thief.SetWantedLevel(0);
		LF_WantedReports.ResetStatics();
		LF_WantedPosters.Flush();
	}
}
