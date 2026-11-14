// red-proof: observed 2026-11-14 with `tools\cli test --no-build --only LF_Test_CuffingOpenState`: LF_Cuffing.End without Close failed `letting go shuts the cuffs`, `a refused end shuts the cuffs`, `a watch pass that finds the pair out of reach shuts the cuffs` and `letting go shuts the cuffs on the wrists`; restored, green.
// tier: WORLD
//! police/cuffing-open-state: the cuffs open while the hold runs and shut when it stops. A Cuff hold
//! swings the strands of the cuffs in the officer's hand open (LF_AssetAnimComponent, replicated, so a
//! late joiner sees them open); letting go, or a pair that moves out of reach, shuts them again and
//! drops the hold. An Uncuff hold opens the cuffs on the suspect's wrists (the replicated character
//! flag) and any end shuts them. The server accepts the end of a hold only if it saw the hold begin
//! and only after most of the action's duration. The worn model can open too.
class LF_Test_CuffingOpenState : LF_Test
{
	static const ResourceName CUFFS_PREFAB = "{DE0B6FEBDCECC6FF}Prefabs/Items/Roleplay/Handcuffs.et";

	override string GetName()
	{
		return "police/cuffing-open-state";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_Cuffing.ResetStatics();
		RunWorn(ctx);
		RunHold(ctx);
		LF_Cuffing.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnCuffs()
	{
		Resource res = Resource.Load(CUFFS_PREFAB);
		if (!res || !res.IsValid())
			return null;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		return GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
	}

	//------------------------------------------------------------------------------------------------
	//! The model that shows on the wrist carries the same two hinged strands as the item in hand.
	protected void RunWorn(LF_TestContext ctx)
	{
		Resource res = Resource.Load(LF_Restraint.WORN_PREFAB);
		if (!res || !res.IsValid())
		{
			ctx.Fail("the worn handcuffs prefab does not resolve");
			return;
		}

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		IEntity worn = GetGame().SpawnEntityPrefabLocal(res, GetGame().GetWorld(), params);
		if (!worn)
		{
			ctx.Fail("the worn handcuffs do not spawn");
			return;
		}

		LF_AssetAnimComponent anim = LF_AssetAnimComponent.Of(worn);
		ctx.True(anim != null, "the worn handcuffs carry an LF_AssetAnimComponent: the cuffs on a wrist can open");
		if (anim)
		{
			ctx.Equal(2, anim.GetBoneCount(), "the worn handcuffs swing both strands");
			ctx.False(anim.IsRunning(), "the worn handcuffs start shut");
			anim.ShowLocal(true, true);
			ctx.True(anim.IsRunning(), "a local show opens the worn handcuffs on this machine");
			anim.ShowLocal(false, false);
			ctx.False(anim.IsRunning(), "a local show shuts them again");
		}

		SCR_EntityHelper.DeleteEntityAndChildren(worn);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunHold(LF_TestContext ctx)
	{
		IEntity officer = LF_TestCharacterFixture.Spawn(ctx);
		IEntity suspect = LF_TestCharacterFixture.Spawn(ctx);
		IEntity cuffs = SpawnCuffs();
		if (!officer || !suspect || !cuffs)
		{
			ctx.Fail("the test needs two characters and the handcuffs prefab");
			LF_TestCharacterFixture.Delete(officer);
			LF_TestCharacterFixture.Delete(suspect);
			if (cuffs)
				SCR_EntityHelper.DeleteEntityAndChildren(cuffs);

			return;
		}

		LF_HandcuffsComponent handcuffs = LF_HandcuffsComponent.Cast(cuffs.FindComponent(LF_HandcuffsComponent));
		SCR_ChimeraCharacter prisoner = SCR_ChimeraCharacter.Cast(suspect);
		ctx.False(handcuffs.IsOpen(), "the cuffs start shut");

		// A hold nobody started cannot be finished: a modified client cannot skip the open phase.
		ctx.Equal(LF_ERestrain.NO_TARGET, LF_Cuffing.Complete(officer, suspect, LF_ECuffingKind.CUFF, 1.8), "the end of a hold that never began is refused");
		ctx.False(LF_Restraint.IsRestrained(suspect), "so nobody is cuffed");

		// Cuff: the hold opens the cuffs, letting go shuts them.
		ctx.True(LF_Cuffing.StartHold(officer, suspect, LF_ECuffingKind.CUFF, cuffs), "a cuff hold starts");
		ctx.True(handcuffs.IsOpen(), "the cuffs in the officer's hand are open during the hold");
		ctx.Equal(1, LF_Cuffing.GetActiveCount(), "the server tracks the hold");
		LF_Cuffing.Cancel(officer);
		ctx.False(handcuffs.IsOpen(), "letting go shuts the cuffs");
		ctx.Equal(0, LF_Cuffing.GetActiveCount(), "and drops the hold");

		// The end of a hold that was started but cut short is refused and shuts the cuffs.
		LF_Cuffing.StartHold(officer, suspect, LF_ECuffingKind.CUFF, cuffs);
		ctx.Equal(LF_ERestrain.NO_TARGET, LF_Cuffing.Complete(officer, suspect, LF_ECuffingKind.CUFF, 1.8), "the end of a hold that ran a moment is refused");
		ctx.False(handcuffs.IsOpen(), "a refused end shuts the cuffs");
		ctx.False(LF_Restraint.IsRestrained(suspect), "and nobody is cuffed");
		ctx.Equal(0, LF_Cuffing.GetActiveCount(), "and the hold is gone");

		// Moving out of reach (the test characters are no players, so the pair is out of reach at once) stops the hold.
		LF_Cuffing.StartHold(officer, suspect, LF_ECuffingKind.CUFF, cuffs);
		ctx.True(handcuffs.IsOpen(), "the cuffs open again for a second hold");
		LF_Cuffing.Watch();
		ctx.False(handcuffs.IsOpen(), "a watch pass that finds the pair out of reach shuts the cuffs");
		ctx.Equal(0, LF_Cuffing.GetActiveCount(), "and stops the hold");

		// Uncuff: the cuffs on the wrists open during the hold; every end shuts them.
		prisoner.LF_SetRestrained(true);
		ctx.True(LF_Cuffing.StartHold(officer, suspect, LF_ECuffingKind.RELEASE, null), "an uncuff hold starts");
		ctx.True(prisoner.LF_AreCuffsOpen(), "the cuffs on the wrists are open during the uncuff hold");
		LF_Cuffing.Cancel(officer);
		ctx.False(prisoner.LF_AreCuffsOpen(), "letting go shuts the cuffs on the wrists");
		ctx.True(LF_Restraint.IsRestrained(suspect), "and the suspect is still cuffed");

		LF_Cuffing.StartHold(officer, suspect, LF_ECuffingKind.RELEASE, null);
		ctx.True(prisoner.LF_AreCuffsOpen(), "the wrists are open again");
		ctx.True(LF_Cuffing.Complete(officer, suspect, LF_ECuffingKind.RELEASE, 0, true) != LF_ERestrain.OK, "a civilian's uncuff is refused by the rule");
		ctx.False(prisoner.LF_AreCuffsOpen(), "a refused uncuff shuts the cuffs on the wrists again");
		ctx.True(LF_Restraint.IsRestrained(suspect), "and the suspect stays cuffed");

		prisoner.LF_SetRestrained(false);
		ctx.False(prisoner.LF_AreCuffsOpen(), "a free character has shut cuffs");

		LF_Cuffing.ResetStatics();
		LF_TestCharacterFixture.Delete(officer);
		LF_TestCharacterFixture.Delete(suspect);
		SCR_EntityHelper.DeleteEntityAndChildren(cuffs);
	}
}
