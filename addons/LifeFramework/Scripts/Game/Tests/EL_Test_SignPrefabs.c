// red-proof: expected 2 Read actions; `tools\cli test --no-build --tier all`
// (2026-11-08) failed signs/prefabs with "sign carries one Read action:
// ...LF_Sign_Board.et (expected 2, got 1)" for every sign prefab.

// tier: WORLD
class EL_Test_SignPrefabs : EL_Test
{
	override string GetName()
	{
		return "signs/prefabs";
	}

	override void Run(EL_TestContext ctx)
	{
		array<ResourceName> prefabs = {
			"{220A96D43DC3C3AE}Prefabs/Props/Signs/LF_Sign_Board.et",
			"{7B452ADB636331A9}Prefabs/Props/Signs/LF_Sign_Pub.et",
			"{70924D289627911B}Prefabs/Props/Signs/LF_Sign_Notice.et",
			"{F82F6440A45EE705}Prefabs/Props/Signs/LF_Sign_Chalkboard.et",
			"{D8D55097BA6DB899}Prefabs/Props/Signs/EL_MiningSign.et",
			"{C6940227F886B8EF}Prefabs/Props/Signs/EL_WhitelistSign.et",
			"{884DF4029C8307DB}Prefabs/Props/Signs/LF_Sign_Plate.et",
			"{9182BD264965878C}Prefabs/Props/Signs/LF_Sign_PlateSmall.et",
			"{4697D9464B596C24}Prefabs/Props/Signs/LF_Sign_Fascia.et",
			"{7863BDC6872929FF}Prefabs/Props/Signs/LF_Sign_Posts.et",
			"{B7B489CA36A0E9F8}Prefabs/Props/Signs/LF_Sign_PolePlate.et"
		};

		Resource paintLayout = Resource.Load("{5783648244305C88}UI/Layouts/Signs/LF_SignPaintFace.layout");
		ctx.True(paintLayout && paintLayout.IsValid(), "the painted face layout loads");
		Resource boardLayout = Resource.Load("{FE97102988E1EC48}UI/Layouts/Signs/LF_SignBoardFace.layout");
		ctx.True(boardLayout && boardLayout.IsValid(), "the cork board face layout loads");

		foreach (ResourceName path : prefabs)
		{
			ExpectOneRead(ctx, path);
		}

		Resource face = Resource.Load(LF_SignComponent.FACE_PREFAB);
		ctx.True(face && face.IsValid(), "the board face sheet prefab loads");
		Resource faceLayout = Resource.Load(LF_SignComponent.FACE_LAYOUT);
		ctx.True(faceLayout && faceLayout.IsValid(), "the board face layout loads");

		LF_SignDef mining = LF_SignCatalog.Find("mining");
		ctx.True(LF_SignCatalog.IsUsable(mining), "LF_Signs.conf has the mining sign");
		ctx.True(LF_SignCatalog.IsUsable(LF_SignCatalog.Find("police_whitelist")), "LF_Signs.conf has the police whitelist sign");
		ctx.True(LF_SignCatalog.Find("no_such_sign") == null, "LF_Signs.conf has no unknown id");
		if (mining)
			ctx.EqualStr("Read: Mining area", LF_SignCatalog.ReadName(mining), "the mining sign reads Read: Mining area");
	}

	//------------------------------------------------------------------------------------------------
	protected void ExpectOneRead(EL_TestContext ctx, ResourceName path)
	{
		Resource resource = Resource.Load(path);
		ctx.True(resource && resource.IsValid(), "sign prefab loads: " + path);
		if (!resource || !resource.IsValid())
			return;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.LOCAL;
		IEntity sign = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		ctx.NotNull(sign, "sign spawns: " + path);
		if (!sign)
			return;

		ctx.NotNull(EL_Component<LF_SignComponent>.Find(sign), "sign carries LF_SignComponent: " + path);

		int reads = 0;
		ActionsManagerComponent actions = EL_Component<ActionsManagerComponent>.Find(sign);
		if (actions)
		{
			array<BaseUserAction> actionList = {};
			actions.GetActionsList(actionList);
			foreach (BaseUserAction action : actionList)
			{
				if (LF_SignReadAction.Cast(action))
					reads++;
			}
		}

		ctx.Equal(1, reads, "sign carries one Read action: " + path);
		SCR_EntityHelper.DeleteEntityAndChildren(sign);
	}
}
