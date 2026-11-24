// red-proof: under `--red` each tier is probed with the other tool's prefab (a tier that lost its own mesh), so the "keeps the base's hand mesh" lines fail. See the RED-OK line in the PR.
//! #4180 (round 5, A8): a tier tool is its base tool in another finish, so it keeps the base's hand mesh and weapon
//! components: the same .xob in the MeshObject, a WeaponComponent, the weapon attachments storage and the melee properties. A tier that lost one of these draws as nothing in the hand.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronToolHandMesh : LF_Test
{
	static const ResourceName BASE_PICKAXE = "{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et";
	static const ResourceName BASE_AXE = "{9CAC68BA551417A4}Prefabs/Tools/Axe/Axe.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/tool-hand-mesh";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		array<ResourceName> axes = {
			"{473A848D505EC104}Prefabs/Tools/Tiers/LF_Tool_Axe_Worn.et",
			"{C17E26FDABD6E423}Prefabs/Tools/Tiers/LF_Tool_Axe_Standard.et",
			"{B97D14FD730FB045}Prefabs/Tools/Tiers/LF_Tool_Axe_Fine.et"};
		array<ResourceName> pickaxes = {
			"{D9D9C619E82ECEA7}Prefabs/Tools/Tiers/LF_Tool_Pickaxe_Worn.et",
			"{A1E3A11E03AEFC5C}Prefabs/Tools/Tiers/LF_Tool_Pickaxe_Standard.et",
			"{67822853D125045C}Prefabs/Tools/Tiers/LF_Tool_Pickaxe_Fine.et"};
		foreach (ResourceName tier : axes)
			CheckTier(ctx, tier, BASE_AXE);

		foreach (ResourceName tier : pickaxes)
			CheckTier(ctx, tier, BASE_PICKAXE);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckTier(LF_TestContext ctx, ResourceName tier, ResourceName base)
	{
		ResourceName probe = tier;
		if (RedProof())
		{
			// The other tool's prefab stands in for a tier that lost its own mesh: the mesh names differ.
			probe = BASE_PICKAXE;
			if (base == BASE_PICKAXE)
				probe = BASE_AXE;
		}

		IEntity baseTool = LF_Utils.SpawnEntityPrefab(base, "0 0 0");
		IEntity tierTool = LF_Utils.SpawnEntityPrefab(probe, "0 0 0");
		ctx.NotNull(baseTool, string.Format("the base %1 spawns", base));
		ctx.NotNull(tierTool, string.Format("%1 spawns", tier));
		if (baseTool && tierTool)
		{
			ResourceName baseMesh = MeshOf(baseTool);
			ctx.False(baseMesh.IsEmpty(), string.Format("the base %1 has a hand mesh", base));
			ctx.EqualStr(baseMesh, MeshOf(tierTool), string.Format("%1 keeps the base's hand mesh", tier));
			ctx.NotNull(tierTool.FindComponent(WeaponComponent), string.Format("%1 keeps the WeaponComponent", tier));
			ctx.NotNull(tierTool.FindComponent(SCR_WeaponAttachmentsStorageComponent), string.Format("%1 keeps the attachments storage", tier));
			ctx.NotNull(tierTool.FindComponent(SCR_MeleeWeaponProperties), string.Format("%1 keeps the melee weapon properties", tier));
		}

		if (baseTool)
			SCR_EntityHelper.DeleteEntityAndChildren(baseTool);

		if (tierTool)
			SCR_EntityHelper.DeleteEntityAndChildren(tierTool);
	}

	//------------------------------------------------------------------------------------------------
	protected ResourceName MeshOf(IEntity tool)
	{
		VObject mesh = tool.GetVObject();
		if (!mesh)
			return "";

		return mesh.GetResourceName();
	}
}
