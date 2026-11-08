// red-proof: observed 2026-11-08 (tools/cli test --no-build --tier all): with
// EL_CharacterATMComponent.IsAtATM returning true, the run printed
// "[ELTEST] FAIL security/service-range" / "- an ATM 200 m away does not serve the player"
// and "- the organization desk is not an ATM". Restored, it passes.

// tier: WORLD
//! The ATM and organization desk RPCs only move cash at the placed site. Reads the DebugWorld
//! placements ATM_Debug and Roadmap_Org_Desk.
class EL_Test_ServiceRange : EL_Test
{
	override string GetName()
	{
		return "security/service-range";
	}

	override void Run(EL_TestContext ctx)
	{
		if (!GetGame() || !GetGame().GetWorld())
		{
			ctx.Fail("no world");
			return;
		}

		IEntity atm = GetGame().GetWorld().FindEntityByName("ATM_Debug");
		ctx.True(atm != null, "DebugWorld places ATM_Debug");
		if (atm)
		{
			vector atmOrigin = atm.GetOrigin();
			vector atmNear = "2 0 0";
			vector atmFar = "200 0 0";
			ctx.True(EL_CharacterATMComponent.IsAtATM(atmOrigin + atmNear), "a player 2 m from the ATM is served");
			ctx.False(EL_CharacterATMComponent.IsAtATM(atmOrigin + atmFar), "an ATM 200 m away does not serve the player");
		}

		IEntity desk = GetGame().GetWorld().FindEntityByName("Roadmap_Org_Desk");
		ctx.True(desk != null, "DebugWorld places Roadmap_Org_Desk");
		if (desk)
		{
			vector deskOrigin = desk.GetOrigin();
			vector deskNear = "2 0 0";
			vector deskFar = "200 0 0";
			ctx.True(EL_OrganizationPlayer.IsAtDesk(deskOrigin + deskNear), "a player 2 m from the desk is served");
			ctx.False(EL_OrganizationPlayer.IsAtDesk(deskOrigin + deskFar), "a desk 200 m away does not serve the player");
			ctx.False(EL_CharacterATMComponent.IsAtATM(deskOrigin), "the organization desk is not an ATM");
		}
	}
}
