// red-proof: 2026-11-18, cli test --no-build --red (batched): RED-OK with no roles added, "an on-duty officer sees the police armoury" fails. Green with AddRolesFromState.
// tier: LOGIC
//------------------------------------------------------------------------------------------------
//! map/poi-role-audience (review 2026-11-17, docs/reviews/2026-11-17-map.md): the client's map audience
//! takes its roles from the replicated account state. Neither audience builder (LF_MapPlacesUI,
//! LF_MapPlacesKnown) added a role, so a ROLE kind such as the police armoury never showed, even to an
//! officer on duty. Off duty, or in another service, it stays hidden.
class LF_Test_MapPoiRoleAudience : LF_Test
{
	override string GetName()
	{
		return "map/poi-role-audience";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_EMapPoiState ArmouryFor(int state)
	{
		LF_MapPoiAudience audience = new LF_MapPoiAudience();
		if (!RedProof())
			audience.AddRolesFromState(state);

		return LF_MapPoiRules.Resolve(LF_EMapPoiVisibility.ROLE, LF_MapShares.ROLE_POLICE, true, LF_EMapPoiKnown.NONE, audience.GetRoles());
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		int onDutyPolice = LF_AccountStateBits.Encode(true, LF_Faction.POLICE, true, 0, 1);
		int offDutyPolice = LF_AccountStateBits.Encode(true, LF_Faction.POLICE, false, 0, 1);
		int onDutyMedic = LF_AccountStateBits.Encode(true, LF_Faction.MEDIC, true, 0, 0);
		LF_EMapPoiState hidden = LF_MapPoiRules.Resolve(LF_EMapPoiVisibility.ROLE, LF_MapShares.ROLE_POLICE, true, LF_EMapPoiKnown.NONE, new array<string>());

		ctx.True(ArmouryFor(onDutyPolice) != hidden, "an on-duty officer sees the police armoury");
		ctx.True(ArmouryFor(offDutyPolice) == hidden, "off duty the armoury stays hidden");
		ctx.True(ArmouryFor(onDutyMedic) == hidden, "a medic on duty does not see the police armoury");
		ctx.True(ArmouryFor(0) == hidden, "no account sees no role place");

		LF_MapPoiAudience medic = new LF_MapPoiAudience();
		medic.AddRolesFromState(onDutyMedic);
		ctx.True(medic.GetRoles().Find(LF_MapShares.ROLE_MEDIC) != -1, "an on-duty medic holds the medic role");
	}
}
