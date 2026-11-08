// red-proof: observed 2026-11-08 with `tools\cli test --no-build --only EL_Test_PlaytestOffers` and
// LF_ArmoryDrawAction.ShownTo returning true (the old "shown to everyone" offer):
// [ELTEST] FAIL playtest/offers
// [ELTEST]   - a civilian is not shown the armory actions
// [ELTEST]   - a player with no account is not shown the armory actions
// [ELTEST]   - an off-duty officer is not shown the armory actions
// Restored; green.

// tier: LOGIC
//! Playtest QA #157: police-only offers are hidden from civilians, the convoy refusal has a
//! reason the player can read, and a crop never indexes a stage it does not have.
class EL_Test_PlaytestOffers : EL_Test
{
	override string GetName()
	{
		return "playtest/offers";
	}

	override void Run(EL_TestContext ctx)
	{
		// 1. Armory draw and promote: on-duty police only.
		int civilian = EL_AccountStateBits.Encode(true, EL_Faction.CIVILIAN, false, 0, 0);
		int offDuty = EL_AccountStateBits.Encode(true, EL_Faction.POLICE, false, 0, 0);
		int onDuty = EL_AccountStateBits.Encode(true, EL_Faction.POLICE, true, 0, 0);
		ctx.False(LF_ArmoryDrawAction.ShownTo(civilian), "a civilian is not shown the armory actions");
		ctx.False(LF_ArmoryDrawAction.ShownTo(0), "a player with no account is not shown the armory actions");
		ctx.False(LF_ArmoryDrawAction.ShownTo(offDuty), "an off-duty officer is not shown the armory actions");
		ctx.True(LF_ArmoryDrawAction.ShownTo(onDuty), "an on-duty officer is shown the armory actions");

		// 2. Convoy refusal reasons.
		EL_OrganizationManager.Reset();
		LF_Hideout.Reset();
		LF_Contraband.Reset();
		string loner = "playtest-offers-loner";
		string holderFounder = "playtest-offers-holder";
		string otherFounder = "playtest-offers-other";
		ctx.EqualStr("#LF-Contraband_JoinOrg", LF_Contraband.StartRefusalKey(loner), "no organization: the player is told to join one");
		ctx.False(LF_Contraband.Start(loner, null), "no organization: the convoy does not start");

		EL_Organization holder = EL_OrganizationManager.Create(holderFounder, "Hideout Holders");
		EL_Organization other = EL_OrganizationManager.Create(otherFounder, "Street Rivals");
		ctx.NotNull(holder, "the holding organization exists");
		ctx.NotNull(other, "the other organization exists");
		if (holder && other)
		{
			ctx.True(LF_Hideout.Assign(LF_Hideout.HIDEOUT_ID, holder.GetId()), "the holder takes the hideout");
			ctx.EqualStr("#LF-Contraband_NotHideoutOwner", LF_Contraband.StartRefusalKey(otherFounder), "another organization is told it needs the hideout");
			ctx.EqualStr("", LF_Contraband.StartRefusalKey(holderFounder), "the holder may start");
			ctx.True(LF_Contraband.Start(holderFounder, null), "the holder starts the convoy");
		}

		EL_OrganizationManager.Reset();
		LF_Hideout.Reset();
		LF_Contraband.Reset();

		// 3. Crop stages.
		array<ref EL_CropStage> stages = {};
		stages.Insert(new EL_CropStage());
		stages.Insert(new EL_CropStage());
		ctx.True(EL_BaseCrop.StageAt(null, 0) == null, "no stage list has no stage");
		ctx.True(EL_BaseCrop.StageAt(stages, -1) == null, "the unset stage -1 is no stage");
		ctx.True(EL_BaseCrop.StageAt(stages, 2) == null, "a stage past the list is no stage");
		ctx.True(EL_BaseCrop.StageAt(stages, 1) != null, "an authored stage is read");
		ctx.False(EL_BaseCrop.RunsGrowth(false, true), "a client never runs crop growth");
		ctx.True(EL_BaseCrop.RunsGrowth(true, true), "the server runs crop growth");
		ctx.False(EL_BaseCrop.RunsGrowth(true, false), "a crop without replication does not grow");

		// 4. A toast is visible from its first frame and gone after its time plus the fade.
		ctx.True(EL_NotificationToastUI.OpacityAt(0, 5) > 0, "a fresh toast is not transparent");
		ctx.EqualFloat(1, EL_NotificationToastUI.OpacityAt(1, 5), 0.001, "a toast is fully shown while its time runs");
		ctx.EqualFloat(0, EL_NotificationToastUI.OpacityAt(6, 5), 0.001, "a toast is gone after its time and the fade");
	}
}
