//------------------------------------------------------------------------------------------------
//! Harness stand-ins for the asset states proof (agent-box/proofs/asset-states-proof.mjs,
//! docs/asset-pipeline/states.md). The proof takes one picture per state an asset declares; these
//! two verbs put the item where a state needs it:
//!   itemgive <prefab>  the server puts one of the prefab in this character's inventory (the hand
//!                      state then holds it with the `hold` verb)
//!   itemdrop <prefab>  the server drops the carried item of that prefab, the way the inventory
//!                      screen's drop does, so it rests by the item's own RestingUP (the ground state)
//! Both run on the server through LF_AskHarnessScanner, behind the dev gate. A third, `heldxf`,
//! runs on the client: where the held gadget sits in the character's own frame, which is how the
//! hand state's HandAdditiveRotation/Translation are measured instead of guessed.
//------------------------------------------------------------------------------------------------
class LF_AssetStatesHarness
{
	static const string FEATURE = "AgentHarness";

	//------------------------------------------------------------------------------------------------
	//! \param tool "itemgive <prefab>" or "itemdrop <prefab>"
	//! \return "ok ..." or "error <reason>"
	static string Run(IEntity character, string tool)
	{
		array<string> parts = {};
		tool.Split(" ", parts, true);
		if (parts.Count() != 2)
			return "error usage_item_verb_prefab";

		if (!character)
			return "error no_character";

		ResourceName prefab = parts[1];
		if (parts[0] == "itemgive")
			return Give(character, prefab);

		if (parts[0] == "itemdrop")
			return Drop(character, prefab);

		if (parts[0] == "itemon" || parts[0] == "itemoff")
			return SetUseState(character, prefab, parts[0] == "itemon");

		return "error unknown_item_verb";
	}

	//------------------------------------------------------------------------------------------------
	static bool IsItemVerb(string tool)
	{
		return tool.StartsWith("itemgive ") || tool.StartsWith("itemdrop ") || tool.StartsWith("itemon ") || tool.StartsWith("itemoff ");
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the held gadget's origin and axes, and the left hand bones, in the local character's
	//! model frame (x right, y up, z forward), so a reading does not depend on where it stands.
	static string Held()
	{
		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(character);
		if (!character || !gadgets)
			return "error no_character";

		IEntity held = gadgets.GetHeldGadget();
		if (!held)
			return "error nothing_held";

		vector body[4];
		character.GetWorldTransform(body);
		vector item[4];
		held.GetWorldTransform(item);
		vector local[4];
		Math3D.MatrixInvMultiply4(body, item, local);
		string text = string.Format("item=%1 o=%2 x=%3 y=%4 z=%5", FilePath.StripPath(LF_Utils.GetPrefabName(held)), Vec(local[3]), Vec(local[0]), Vec(local[1]), Vec(local[2]));
		Animation animation = character.GetAnimation();
		if (!animation)
			return "ok " + text;

		array<string> bones = {"LeftHand", "LeftHandProp", "LeftHandMiddle1", "LeftHandIndex1", "LeftHandThumb2"};
		foreach (string bone : bones)
		{
			TNodeId id = animation.GetBoneIndex(bone);
			vector mat[4];
			if (id == -1 || !animation.GetBoneMatrix(id, mat))
				continue;

			text += string.Format(" %1=%2", bone, Vec(mat[3]));
			if (bone == "LeftHandProp")
				text += string.Format(" %1_x=%2 %1_y=%3 %1_z=%4", bone, Vec(mat[0]), Vec(mat[1]), Vec(mat[2]));
		}

		return "ok " + text;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Vec(vector v)
	{
		return string.Format("%1,%2,%3", v[0].ToString(-1, 4), v[1].ToString(-1, 4), v[2].ToString(-1, 4));
	}

	//------------------------------------------------------------------------------------------------
	protected static string Give(IEntity character, ResourceName prefab)
	{
		if (!Resource.Load(prefab).IsValid())
		{
			LF_Debug.Error(FEATURE, "itemgive: no prefab " + prefab);
			return "error no_prefab";
		}

		int added = LF_InventoryUtils.AddAmount(character, prefab, 1);
		LF_Debug.Info(FEATURE, string.Format("itemgive %1 added=%2", prefab, added));
		if (added < 1)
			return "error no_room";

		return "ok given " + prefab;
	}

	//------------------------------------------------------------------------------------------------
	//! The item's use state: SetActive on its LF_AssetAnimComponent (the cuffs' strands swing open while
	//! it is on). Looks in the inventory first, then for the nearest one of the prefab lying within 3 m.
	protected static string SetUseState(IEntity character, ResourceName prefab, bool active)
	{
		IEntity item;
		InventoryStorageManagerComponent manager = LF_InventoryUtils.GetResponsibleStorageManager(character);
		if (manager)
		{
			array<IEntity> items = LF_InventoryUtils.FindItemsByPrefab(manager, prefab);
			if (!items.IsEmpty())
				item = items[0];
		}

		if (!item)
		{
			s_SwitchTarget = null;
			s_SwitchPrefab = prefab;
			GetGame().GetWorld().QueryEntitiesBySphere(character.GetOrigin(), 3, FindLying, null, EQueryEntitiesFlags.ALL);
			item = s_SwitchTarget;
		}

		if (!item)
			return "error not_found";

		LF_AssetAnimComponent anim = LF_AssetAnimComponent.Cast(item.FindComponent(LF_AssetAnimComponent));
		if (!anim)
			return "error no_asset_anim";

		if (!anim.SetActive(active))
			return "error switch_refused";

		return string.Format("ok %1 active=%2", FilePath.StripPath(prefab), active);
	}

	protected static IEntity s_SwitchTarget;
	protected static ResourceName s_SwitchPrefab;

	//------------------------------------------------------------------------------------------------
	protected static bool FindLying(IEntity entity)
	{
		if (!entity || LF_Utils.GetPrefabName(entity) != s_SwitchPrefab)
			return true;

		s_SwitchTarget = entity;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Drop(IEntity character, ResourceName prefab)
	{
		InventoryStorageManagerComponent manager = LF_InventoryUtils.GetResponsibleStorageManager(character);
		if (!manager)
			return "error no_inventory";

		array<IEntity> items = LF_InventoryUtils.FindItemsByPrefab(manager, prefab);
		if (items.IsEmpty())
			return "error not_carried";

		// A held gadget is still in its storage slot; taking it out of that slot drops it from the hand too.
		InventoryItemComponent item = InventoryItemComponent.Cast(items[0].FindComponent(InventoryItemComponent));
		if (!item || !item.GetParentSlot() || !manager.TryRemoveItemFromStorage(items[0], item.GetParentSlot().GetStorage()))
		{
			LF_Debug.Warn(FEATURE, "itemdrop refused for " + prefab);
			return "error drop_refused";
		}

		vector angles = items[0].GetAngles();
		LF_Debug.Info(FEATURE, string.Format("itemdrop %1 at %2 angles %3", prefab, items[0].GetOrigin(), angles));
		return string.Format("ok dropped at %1 angles %2", items[0].GetOrigin(), angles);
	}
}
