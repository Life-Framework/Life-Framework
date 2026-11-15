// red-proof: observed 2026-11-15 with `tools\cli test --no-build --no-cache --only LF_Test_AmmoList`: with the taser line removed from AmmoList.conf the test boot threw the issue's VME (`unable to find {504A05CCC2926A5C}Prefabs/Weapons/Handguns/Taser/Ammo_Taser.et in the SCR_MagazineRepackingSystem ammunition list`) and failed; restored, green.
// tier: WORLD
//! The ammunition list the magazine repacking system reads (#1643). Every magazine looks its rounds
//! up in `AmmoList.conf` when it is created; a round that is missing is a Virtual Machine Exception
//! the first time a saved taser magazine loads. The list is read here the way the system reads it,
//! and the live system is asked for the taser round.
class LF_Test_AmmoList : LF_Test
{
	static const ResourceName AMMO_LIST = "{DAA31F8A78F30455}Configs/Weapons/Ammo/AmmoList.conf";
	static const ResourceName TASER_AMMO = "{504A05CCC2926A5C}Prefabs/Weapons/Handguns/Taser/Ammo_Taser.et";
	static const ResourceName TASER_MAGAZINE = "{22DC576156907CBF}Prefabs/Weapons/Handguns/Taser/Magazine_Taser.et";
	// A vanilla round: the mod's list adds to the game's, it must not replace it.
	static const ResourceName VANILLA_AMMO = "{9CE4CC890A2A574F}Prefabs/Weapons/Ammo/Ammo_556x45_Ball_M193.et";

	override string GetName()
	{
		return "resources/ammo_list";
	}

	override void Run(LF_TestContext ctx)
	{
		Resource res = BaseContainerTools.LoadContainer(AMMO_LIST);
		if (!res || !res.IsValid())
		{
			ctx.Fail("the ammunition list does not load: " + AMMO_LIST);
			return;
		}

		SCR_AmmoList list = SCR_AmmoList.Cast(BaseContainerTools.CreateInstanceFromContainer(res.GetResource().ToBaseContainer()));
		if (!list || !list.m_aAmmoList)
		{
			ctx.Fail("the ammunition list has no m_aAmmoList");
			return;
		}

		ctx.True(list.m_aAmmoList.Contains(TASER_AMMO), "the taser round is in AmmoList.conf");
		ctx.True(list.m_aAmmoList.Contains(VANILLA_AMMO), "the game's own rounds are still in AmmoList.conf");

		SCR_MagazineRepackingSystem system = SCR_MagazineRepackingSystem.GetInstance();
		if (!system || !system.IsReady())
		{
			ctx.Fail("the magazine repacking system is not running or has not read the ammunition list");
			return;
		}

		ctx.True(system.GetAmmoTypeId(TASER_AMMO) >= 0, "the repacking system knows the taser round");
		ctx.True(system.GetAmmoTypeId(VANILLA_AMMO) >= 0, "the repacking system knows a vanilla round");

		// What a save load does: create the taser magazine. Its ammunition composition is set up from the list.
		Resource mag = Resource.Load(TASER_MAGAZINE);
		if (!mag || !mag.IsValid())
		{
			ctx.Fail("the taser magazine prefab does not load: " + TASER_MAGAZINE);
			return;
		}

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		IEntity entity = GetGame().SpawnEntityPrefab(mag, GetGame().GetWorld(), params);
		ctx.True(entity != null, "the taser magazine spawns");
		if (entity)
		{
			SCR_MagazineComponent component = SCR_MagazineComponent.Cast(entity.FindComponent(SCR_MagazineComponent));
			ctx.True(component != null, "the taser magazine has a magazine component");
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
		}
	}
}
