// red-proof: observed 2026-11-13 with `tools\cli test --no-build --only LF_Test_Handcuffs`: the owner hook without SetDisableWeaponControls failed `a cuffed character cannot use a weapon`; restored, green.
// tier: WORLD
//! police/handcuffs: the handcuffs the asset pipeline made (#664) load as a held gadget that the
//! restraint recognises, the worn model resolves, and the cuffed flag on a character disables its
//! weapon controls on the owner and gives them back when it clears. Who may cuff is
//! police/restraint-rules; the two-player cuff and release is the session proof `cuffs-proof`.
class LF_Test_Handcuffs : LF_Test
{
	static const ResourceName CUFFS_PREFAB = "{DE0B6FEBDCECC6FF}Prefabs/Items/Roleplay/Handcuffs.et";

	override string GetName()
	{
		return "police/handcuffs";
	}

	override void Run(LF_TestContext ctx)
	{
		RunPrefab(ctx);
		RunFlag(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity Spawn(ResourceName prefab)
	{
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
			return null;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		return GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunPrefab(LF_TestContext ctx)
	{
		IEntity cuffs = Spawn(CUFFS_PREFAB);
		if (!cuffs)
		{
			ctx.Fail("the handcuffs prefab does not spawn: " + CUFFS_PREFAB);
			return;
		}

		ctx.True(cuffs.FindComponent(LF_HandcuffsComponent) != null, "the handcuffs carry LF_HandcuffsComponent, the restraint's requirement");
		SCR_GadgetComponent gadget = SCR_GadgetComponent.Cast(cuffs.FindComponent(SCR_GadgetComponent));
		ctx.True(gadget != null, "the handcuffs are a gadget");
		if (gadget)
			ctx.True(gadget.CanBeHeld(), "the handcuffs can be held in the hand");

		InventoryItemComponent item = InventoryItemComponent.Cast(cuffs.FindComponent(InventoryItemComponent));
		ctx.True(item != null, "the handcuffs are an inventory item");
		if (item)
		{
			UIInfo info = item.GetAttributes().GetUIInfo();
			ctx.True(info && info.GetName() == "#LF-Item_Handcuffs_Name", "the handcuffs show their own name");
		}

		ctx.True(cuffs.GetVObject() != null, "the handcuffs have a mesh");
		ctx.True(LF_Utils.GetPrefabName(cuffs).Contains("Handcuffs.et"), "the consumed cuffs come back as the same prefab");
		SCR_EntityHelper.DeleteEntityAndChildren(cuffs);

		Resource worn = Resource.Load(LF_Restraint.WORN_PREFAB);
		ctx.True(worn && worn.IsValid(), "the worn handcuffs model resolves");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunFlag(LF_TestContext ctx)
	{
		IEntity entity = LF_TestCharacterFixture.Spawn(ctx);
		if (!entity)
			return;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		CharacterControllerComponent controller;
		if (character)
			controller = character.GetCharacterController();

		if (!controller)
		{
			ctx.Fail("the test character has no controller");
			LF_TestCharacterFixture.Delete(entity);
			return;
		}

		ctx.False(LF_Restraint.IsRestrained(entity), "a new character is not cuffed");
		ctx.True(LF_Restraint.HeldCuffs(entity) == null, "a character with empty hands holds no cuffs");
		character.LF_SetRestrained(true);
		ctx.True(LF_Restraint.IsRestrained(entity), "the cuffed flag is set");
		ctx.True(controller.GetDisableWeaponControls(), "a cuffed character cannot use a weapon");
		character.LF_SetRestrained(false);
		ctx.False(LF_Restraint.IsRestrained(entity), "the cuffed flag clears");
		ctx.False(controller.GetDisableWeaponControls(), "an uncuffed character has weapon controls again");
		ctx.Equal(0, LF_Restraint.ReleasePlayer("lf-test-nobody"), "a disconnect with no cuffs on drops nothing");

		LF_TestCharacterFixture.Delete(entity);
	}
}
