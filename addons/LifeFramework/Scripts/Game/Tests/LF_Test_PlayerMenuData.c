// red-proof: observed 2026-11-22 `tools/cli test --no-build --red LF_Test_PlayerMenuXp,LF_Test_PlayerMenuData`: RED PROOF OK (both fail with the hook on). Green with `--only`.
//! The personal file's data, no world: the wire round-trips every field, a name carrying the wire's
//! separators cannot break it, a wire of another version is refused; the errand list puts the guided
//! chain first in chain order (not map order) with exactly one CURRENT step, lists other errands only
//! once taken, and the group counts read done over total.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_PlayerMenuData : LF_Test
{
	override string GetName()
	{
		return "playermenu/data";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_PlayerMenuData data = new LF_PlayerMenuData();
		data.m_sName = "Ann|a; Novak";
		data.m_eFaction = LF_Faction.MEDIC;
		data.m_bOnDuty = true;
		data.m_iCash = 1250;
		data.m_iBank = 2000000000;
		data.m_iHouses = 1;
		data.m_iGarageUsed = 1;
		data.m_iGarageCap = 2;
		data.m_iPlaytimeSeconds = 7260;
		data.m_iWanted = 2;
		data.m_aLicenceKeys.Insert("#LF-License_Driving_Name");
		LF_JobTrackDef track = LF_JobTrackDef.Make("FISHER", {100, 300});
		data.m_aSkills.Insert(LF_PlayerMenuServer.SkillOf(track, 150));

		array<ref LF_PlayerMenuQuestInput> inputs = {};
		// Map order is not chain order: the third step comes first here.
		inputs.Insert(LF_PlayerMenuQuestInput.Make("arrive_c", "arrive_b", true, -1, false));
		inputs.Insert(LF_PlayerMenuQuestInput.Make("side_taken", "", false, LF_EQuestStatus.ACTIVE, false));
		inputs.Insert(LF_PlayerMenuQuestInput.Make("arrive_a", "", true, LF_EQuestStatus.TURNED_IN, true));
		inputs.Insert(LF_PlayerMenuQuestInput.Make("side_never", "", false, -1, false));
		inputs.Insert(LF_PlayerMenuQuestInput.Make("arrive_b", "arrive_a", true, -1, false));
		inputs[0].m_iTarget = 3;
		data.m_aQuests = LF_PlayerMenuQuests.Order(inputs);

		ctx.Equal(4, data.m_aQuests.Count(), "three chain steps and the one errand taken; the untaken one is left out");
		ctx.EqualStr("arrive_a", data.m_aQuests[0].m_sId, "the chain starts at the step with no previous step");
		ctx.EqualStr("arrive_b", data.m_aQuests[1].m_sId, "then the step after it");
		ctx.EqualStr("arrive_c", data.m_aQuests[2].m_sId, "then the last step");
		ctx.Equal(LF_EPlayerMenuQuestState.DONE, data.m_aQuests[0].m_eState, "a turned-in step is DONE");
		ctx.Equal(LF_EPlayerMenuQuestState.CURRENT, data.m_aQuests[1].m_eState, "the first step not done, after a done one, is CURRENT");
		ctx.Equal(LF_EPlayerMenuQuestState.LATER, data.m_aQuests[2].m_eState, "a step whose previous step is open is LATER");
		ctx.Equal(LF_EPlayerMenuQuestGroup.ERRANDS, data.m_aQuests[3].m_eGroup, "an unguided errand goes in the other group");
		ctx.Equal(LF_EPlayerMenuQuestState.CURRENT, data.m_aQuests[3].m_eState, "an accepted errand is CURRENT");

		int done;
		ctx.Equal(3, data.CountGroup(LF_EPlayerMenuQuestGroup.ARRIVAL, done), "the first errands count three");
		ctx.Equal(1, done, "one of them done");

		// RedProof(): the wire is cut after its version, so the person record never arrives (`--red`).
		string wire = data.Encode();
		if (RedProof())
			wire = "V|1;";

		LF_PlayerMenuData back = LF_PlayerMenuData.Decode(wire);
		ctx.NotNull(back, "the wire decodes");
		if (!back)
			return;

		ctx.EqualStr("Ann a  Novak", back.m_sName, "the separators in a name become spaces, the rest survives");
		ctx.Equal(LF_Faction.MEDIC, back.m_eFaction, "role");
		ctx.True(back.m_bOnDuty, "duty");
		ctx.Equal(1250, back.m_iCash, "cash");
		ctx.Equal(2000000000, back.m_iBank, "a large bank balance stays exact");
		ctx.Equal(1, back.m_iHouses, "houses");
		ctx.Equal(1, back.m_iGarageUsed, "garage used");
		ctx.Equal(2, back.m_iGarageCap, "garage slots");
		ctx.Equal(7260, back.m_iPlaytimeSeconds, "playtime");
		ctx.Equal(2, back.m_iWanted, "wanted level");
		ctx.Equal(1, back.m_aLicenceKeys.Count(), "one licence");
		ctx.EqualStr("#LF-License_Driving_Name", back.m_aLicenceKeys[0], "the licence key");
		ctx.Equal(1, back.m_aSkills.Count(), "one skill");
		ctx.Equal(2, back.m_aSkills[0].m_iLevel, "the skill's level");
		ctx.Equal(100, back.m_aSkills[0].m_iFloor, "the skill's floor");
		ctx.Equal(300, back.m_aSkills[0].m_iNext, "the skill's next level");
		ctx.Equal(4, back.m_aQuests.Count(), "four errands");
		ctx.EqualStr("arrive_c", back.m_aQuests[2].m_sId, "errand order survives");
		ctx.Equal(3, back.m_aQuests[2].m_iTarget, "an errand's target survives");
		ctx.Equal(LF_EPlayerMenuQuestState.CURRENT, back.m_aQuests[1].m_eState, "an errand's badge survives");

		ctx.True(LF_PlayerMenuData.Decode("V|0;" + wire) == null, "a wire of another version is refused");
		ctx.True(LF_PlayerMenuData.Decode("") == null, "an empty wire is refused");
	}
}
