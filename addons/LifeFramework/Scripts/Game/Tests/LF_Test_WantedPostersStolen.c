// red-proof: let the people take every place (`int people = maxPosters;`) and ran
// `tools\cli test --no-build --only LF_Test_WantedPostersStolen` (2026-11-13). Observed:
//   [LFTEST]   - the face stays at its four places (expected 4, got 5)
//   [LFTEST]   - the newest report's plate (expected 'CD 56-78', got '')
// Restored the split and the test passed (17 assertions).

// tier: LOGIC
//! Stolen cars on the wanted board's face (docs/architecture/visual-boards-spike.md, issue 5):
//! reported cars take up to `stolenPosters` places after the wanted people, newest first, with the
//! plate, the name key and the prefab GUID (no path, no place); people take the places the cars
//! leave free; a report, a recovery and a chop rebuild the list; a closed report leaves the face.
class LF_Test_WantedPostersStolen : LF_Test
{
	protected static const string SUSPECT = "posters-stolen-suspect";

	override string GetName()
	{
		return "police/wanted-posters-stolen";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_WantedReports.ResetStatics();
		LF_StolenVehicles.ResetStatics();
		ctx.True(LF_WantedPosters.Install(), "the feed subscribes to the bus");
		ctx.True(LF_EventBus.SubscriberCount(LF_OnVehicleReportedStolen) > 0, "the feed listens for stolen reports");
		ctx.True(LF_EventBus.SubscriberCount(LF_OnVehicleRecovered) > 0, "the feed listens for recoveries");
		ctx.True(LF_EventBus.SubscriberCount(LF_OnVehicleChopped) > 0, "the feed listens for chopped cars");

		for (int i = 0; i < 4; i++)
			LF_WantedReports.Add(SUSPECT + i.ToString(), LF_WantedReports.OFFENCE_ROBBERY, "", "", "", false);

		LF_WantedPosters.Flush();
		LF_StolenReport older = LF_StolenVehicles.FileReport("posters-car-a", "AB 12-34", "#EL-Vehicle_S105", "owner-a", "Fuel station");
		LF_StolenReport newer = LF_StolenVehicles.FileReport("posters-car-b", "CD 56-78", "#EL-Vehicle_S105", "owner-b", "Square");
		older.m_sPrefabGuid = "54C3CC22DEBD57BE";
		newer.m_sPrefabGuid = "259EE7B78C51B624";
		ctx.True(LF_WantedPosters.IsQueued(), "a stolen report queues a rebuild");

		array<string> entries = {};
		ctx.Equal(4, LF_WantedPosters.BuildEntries(4, entries, 1), "the face stays at its four places");
		LF_WantedPosterEntry car = LF_WantedPosters.Decode(entries[3]);
		ctx.True(car && car.m_sKind == LF_WantedPosters.KIND_VEHICLE, "the last place is a stolen car");
		if (car)
		{
			ctx.EqualStr("CD 56-78", car.m_sName, "the newest report's plate");
			ctx.EqualStr("#EL-Vehicle_S105", car.m_sVehicle, "the car's name key");
			ctx.EqualStr("259EE7B78C51B624", car.m_sAppearance, "the prefab GUID for the picture");
		}

		ctx.False(entries[3].Contains("Square") || entries[3].Contains("owner-b"), "a stolen poster names no place and no owner");
		ctx.Equal(3, CountKind(entries, LF_WantedPosters.KIND_PERSON), "three people beside one car");

		LF_WantedPosters.BuildEntries(4, entries, 2);
		ctx.Equal(2, CountKind(entries, LF_WantedPosters.KIND_VEHICLE), "two car places give two cars");
		LF_WantedPosters.BuildEntries(4, entries, 0);
		ctx.Equal(0, CountKind(entries, LF_WantedPosters.KIND_VEHICLE), "no car places keep cars off the face");

		// Fewer wanted people than places: they keep the places, the car keeps its one.
		LF_WantedReports.ResetStatics();
		LF_WantedPosters.BuildEntries(4, entries, 1);
		ctx.Equal(1, entries.Count(), "with nobody wanted only the car is up");

		LF_StolenVehicles.Clear("posters-car-b", "test");
		LF_WantedPosters.BuildEntries(4, entries, 1);
		LF_WantedPosterEntry left = LF_WantedPosters.Decode(entries[0]);
		ctx.True(left && left.m_sName == "AB 12-34", "a closed report leaves the face; the older car takes the place");
		ctx.True(LF_WantedPosters.Decode("V;only") == null, "a short car entry is dropped");

		LF_StolenVehicles.ResetStatics();
		LF_WantedPosters.Flush();
	}

	protected static int CountKind(array<string> entries, string kind)
	{
		int count;
		foreach (string entry : entries)
		{
			LF_WantedPosterEntry poster = LF_WantedPosters.Decode(entry);
			if (poster && poster.m_sKind == kind)
				count++;
		}

		return count;
	}
}
