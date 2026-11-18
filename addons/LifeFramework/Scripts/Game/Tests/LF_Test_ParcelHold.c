// red-proof: observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_ParcelHold` (the RedProof() hook reads the additive of an empty
// carry set instead of the parcel's): FAIL - the parcel lifts off the hand bone. Real prefab: green; the old wrench-pose prefab (offset 0 0 0)
// is the same failing state.

//! How the courier parcel sits in the hand. The hold borrows the vanilla supply-box carry set
//! (MedicalKit_supplies, what a mortar ammo box uses), whose hand bone sits at the top of the box. With the
//! wrench set and no offset the box origin was the fist, so the hand went through the middle of the box.
//! The parcel's own hand offset lifts the box until the hand is on its top edge; a zero offset is that bug.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ParcelHold : LF_Test
{
	//! The box is 0.14 m thick: the hand must grip at least the top edge, so the offset lifts it by more than this.
	static const float MIN_LIFT = 0.1; // lf-fixed: test bound for the grip, not a gameplay knob

	override string GetName()
	{
		return "jobs/parcel-hold";
	}

	override void Run(LF_TestContext ctx)
	{
		Resource resource = Resource.Load(LF_CivilianJob.ParcelPrefab());
		ctx.True(resource.IsValid(), "the parcel prefab loads");
		if (!resource.IsValid())
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		IEntity parcel = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		ctx.True(parcel != null, "the parcel spawns");
		if (!parcel)
			return;

		InventoryItemComponent item = InventoryItemComponent.Cast(parcel.FindComponent(InventoryItemComponent));
		ctx.NotNull(item, "the parcel is an inventory item");
		if (item)
		{
			ItemAnimationAttributes hold = ItemAnimationAttributes.Cast(item.FindAttribute(ItemAnimationAttributes));
			ctx.NotNull(hold, "the parcel has a hold (ItemAnimationAttributes)");
			if (hold)
			{
				vector additive[4];
				Math3D.MatrixIdentity4(additive);
				if (!RedProof())
					hold.GetAdditiveTransformLS(additive);

				ctx.True(additive[3][1] > MIN_LIFT, string.Format("the parcel lifts off the hand bone, so the hand grips its top edge (offset y %1)", additive[3][1]));
			}
		}

		SCR_EntityHelper.DeleteEntityAndChildren(parcel);
	}
}
