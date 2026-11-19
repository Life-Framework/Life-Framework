// red-proof: 2026-11-19, `tools\cli test --no-build --red LF_Test_ArrivalGuide` (RedProof() skips every Refresh, so no waypoint or map mark is sent): RED-OK, fails with the hook on; green without it (22+ assertions).
//! Records the waypoints LF_ArrivalGuide sends: "mark:<uid>:<id>:<label>@<x>,<z>" and "clear:<uid>:<id>".
class LF_ArrivalGuideTestWaypoints : LF_JobWaypointSink
{
	ref array<string> m_aSent = {};

	override void Mark(string uid, string id, string label, int x, int z)
	{
		m_aSent.Insert(string.Format("mark:%1:%2:%3@%4,%5", uid, id, label, x, z));
	}

	override void Clear(string uid, string id)
	{
		m_aSent.Insert(string.Format("clear:%1:%2", uid, id));
	}

	string Last()
	{
		if (m_aSent.IsEmpty())
			return string.Empty;

		return m_aSent[m_aSent.Count() - 1];
	}
}

//------------------------------------------------------------------------------------------------
//! The arrival chain guides a new player (first-hour fix): every step of the clerk's chain names a target
//! the world places, so the waypoint and the map mark have somewhere to go; the guide moves to the next
//! step's target as each is turned in and clears at the end; and the mark is the one character's own
//! (the PLAYER share keyed by their uid, nobody else's). The clerk is a spawned fixture; the signs and
//! the ATM come from the test world.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ArrivalGuide : LF_Test
{
	static const ResourceName CLERK_PREFAB = "{DE27E685D6FE93D7}Prefabs/Characters/Quests/QuestGiver_Clerk.et";
	static const string UID = "arrival-guide-test-uid";
	static const string OTHER_UID = "arrival-guide-other-uid";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "quests/arrival-guide";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity clerk = LF_Utils.SpawnEntityPrefab(CLERK_PREFAB, "60 0 60");
		ctx.True(clerk != null, "the clerk fixture spawns");
		if (!clerk)
			return;

		LF_MapShares.ResetStatics();
		LF_ArrivalGuide.ResetStatics();
		RunGuide(ctx, clerk);
		LF_ArrivalGuide.ResetStatics();
		LF_MapShares.ResetStatics();
		LF_MapShares.EnsureStarted();
		SCR_EntityHelper.DeleteEntityAndChildren(clerk);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunGuide(LF_TestContext ctx, IEntity clerk)
	{
		LF_QuestManager manager = LF_QuestManager.GetInstance();
		array<string> steps = {"arrival_register", "arrival_board", "arrival_work", "arrival_bank", "arrival_done"};
		vector from = clerk.GetOrigin();

		// Every step of the chain names a target the world places.
		foreach (string stepId : steps)
		{
			LF_QuestDefinition def = manager.GetDefinition(stepId);
			ctx.NotNull(def, "the chain has the step " + stepId);
			if (!def)
				continue;

			ctx.False(def.m_sTargetKind.IsEmpty(), "the step names a target kind: " + stepId);
			vector target;
			ctx.True(LF_ArrivalGuide.ResolveTarget(def, from, target), "the step's target resolves in the world: " + stepId + " (" + def.m_sTargetKind + " " + def.m_sTargetId + ")");
			ctx.False(LF_ArrivalGuide.LabelOf(def).IsEmpty(), "the step has a mark label: " + stepId);
		}

		// The clerk is the first and the last target, found as the placed giver.
		vector clerkTarget;
		LF_QuestDefinition first = manager.GetDefinition("arrival_register");
		if (first && LF_ArrivalGuide.ResolveTarget(first, from, clerkTarget))
			ctx.True(vector.DistanceXZ(clerkTarget, clerk.GetOrigin()) < 0.5, "the first target is the clerk's own position");

		LF_ArrivalGuideTestWaypoints waypoints = new LF_ArrivalGuideTestWaypoints();
		LF_ArrivalGuide.SetSink(waypoints);
		array<string> turnedIn = {};
		string key = LF_ArrivalGuide.KeyOf(UID);

		foreach (int i, string stepId : steps)
		{
			string guided = string.Empty;
			if (!RedProof())
				guided = LF_ArrivalGuide.Refresh(UID, from, turnedIn);

			ctx.EqualStr(stepId, guided, "the guide is on step " + (i + 1).ToString());
			ctx.True(waypoints.Last().StartsWith("mark:" + UID + ":" + LF_ArrivalGuide.WAYPOINT_ID + ":"), "a waypoint is sent for " + stepId + ": " + waypoints.Last());
			ctx.True(LF_MapShares.Has(key), "the character's own map mark stands for " + stepId);
			ctx.False(LF_MapShares.Has(LF_ArrivalGuide.KeyOf(OTHER_UID)), "nobody else has a mark");
			turnedIn.Insert(stepId);
		}

		// The last step turned in: no step is left, both marks go.
		string afterChain = "skipped";
		if (!RedProof())
			afterChain = LF_ArrivalGuide.Refresh(UID, from, turnedIn);

		ctx.EqualStr(string.Empty, afterChain, "no step is left after the chain");
		ctx.EqualStr("clear:" + UID + ":" + LF_ArrivalGuide.WAYPOINT_ID, waypoints.Last(), "the waypoint is cleared at the end");
		ctx.False(LF_MapShares.Has(key), "the map mark is removed at the end");

		// A rejoin mid-quest (steps 1 and 2 saved as turned in) is guided to step 3 again.
		turnedIn.Clear();
		turnedIn.Insert("arrival_register");
		turnedIn.Insert("arrival_board");
		ctx.EqualStr("arrival_work", LF_ArrivalGuide.Refresh(UID, from, turnedIn), "a rejoin mid-quest is guided to the step it is on");
		ctx.True(LF_MapShares.Has(key), "the rejoined character's mark stands");
		LF_ArrivalGuide.Done(UID);
	}
}
