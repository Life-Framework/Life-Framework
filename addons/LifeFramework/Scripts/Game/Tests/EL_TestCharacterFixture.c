//------------------------------------------------------------------------------------------------
//! Shared WORLD-tier fixture: a character spawned the way EL_SpawnLogic does (WORLD transform at
//! a spawn point) wearing a jacket, so MoneyStack has a deposit storage whatever the prefab's
//! default loadout is. Same shape as the setup EL_Test_E2E_Narrative proved.
//! Every step that does not resolve is a test FAIL with its reason, never a silent skip.
class EL_TestCharacterFixture
{
	static const ResourceName CHARACTER_PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";
	static const ResourceName JACKET_PREFAB = "{9F546CCA2582D16F}Prefabs/Characters/Uniforms/Jacket_M88.et";

	//------------------------------------------------------------------------------------------------
	//! \return The character, or null after recording the FAIL reason on ctx.
	static IEntity Spawn(EL_TestContext ctx)
	{
		Resource res = Resource.Load(CHARACTER_PREFAB);
		if (!res.IsValid())
		{
			ctx.Fail("fixture: character prefab does not load: " + CHARACTER_PREFAB);
			return null;
		}

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;

		SCR_SpawnPoint spawnPoint = SCR_SpawnPoint.GetRandomSpawnPointForFaction("CIVILIAN");
		if (!spawnPoint)
			spawnPoint = SCR_SpawnPoint.GetRandomSpawnPointDeathmatch();
		if (!spawnPoint)
		{
			ctx.Fail("fixture: no spawn point resolves for the character");
			return null;
		}

		vector position, yawPitchRoll;
		spawnPoint.GetPositionAndRotation(position, yawPitchRoll);
		Math3D.AnglesToMatrix(yawPitchRoll, params.Transform);
		params.Transform[3] = position + "0 0.1 0";

		IEntity character = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!character)
		{
			ctx.Fail("fixture: character prefab did not spawn an entity");
			return null;
		}

		InventoryStorageManagerComponent storage = EL_InventoryUtils.GetResponsibleStorageManager(character);
		if (!storage)
		{
			ctx.Fail("fixture: character inventory storage manager did not resolve");
			Delete(character);
			return null;
		}

		IEntity jacket = EL_Utils.SpawnEntityPrefab(JACKET_PREFAB, character.GetOrigin());
		if (!jacket)
		{
			ctx.Fail("fixture: jacket did not spawn: " + JACKET_PREFAB);
			Delete(character);
			return null;
		}

		if (!storage.TryInsertItem(jacket, EStoragePurpose.PURPOSE_LOADOUT_PROXY))
		{
			ctx.Fail("fixture: jacket did not insert into the character loadout");
			Delete(jacket);
			Delete(character);
			return null;
		}

		return character;
	}

	//------------------------------------------------------------------------------------------------
	static void Delete(IEntity entity)
	{
		if (entity)
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! Whether the character holds the prefab anywhere. A weapon lands in the equipped-weapon
	//! storage, which the deposit-count helpers skip, so that storage is checked too.
	static bool HasItem(IEntity character, ResourceName prefab)
	{
		SCR_InventoryStorageManagerComponent manager = EL_Component<SCR_InventoryStorageManagerComponent>.Find(character);
		if (manager && EL_InventoryUtils.FindItemsByPrefab(manager, prefab).Count() > 0)
			return true;

		EquipedWeaponStorageComponent weaponStorage = EL_Component<EquipedWeaponStorageComponent>.Find(character);
		if (!weaponStorage)
			return false;

		array<IEntity> weapons = {};
		weaponStorage.GetAll(weapons);
		foreach (IEntity weapon : weapons)
		{
			if (EL_Utils.GetPrefabName(weapon) == prefab)
				return true;
		}

		return false;
	}
}
