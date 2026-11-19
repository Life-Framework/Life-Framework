//------------------------------------------------------------------------------------------------
//! Throw the food or drink in the hands (the throw key, "LF_ThrowHeld", G on a keyboard). It leaves
//! the hands as a plain world item on an arc and lands where it lands: no damage, no effect on
//! anyone, and it can be picked up again. The client only sends its aim; the server decides
//! (LF_ConsumeRules: not down, cuffed, seated or swimming), takes the item out of the inventory and
//! launches it. A stack throws one unit. Vanilla has no throw for a non-weapon item, so this is the
//! item's own rigid body given a velocity, the way a dropped item falls.
//------------------------------------------------------------------------------------------------
class LF_FoodThrow
{
	static const string ACTION = "LF_ThrowHeld";
	static const float SPEED = 7.5;			// lf-fixed: m/s along the aim
	static const float LIFT = 1.2;			// lf-fixed: extra m/s upward, a lob rather than a flat line
	static const float START_HEIGHT = 1.45;	// lf-fixed: metres above the feet, about the hand when raised
	static const float START_AHEAD = 0.5;	// lf-fixed: metres along the aim, clear of the thrower's own body
	static const int COOLDOWN_MS = 600;		// lf-fixed: least gap between two throws from one player
	static const int LAUNCH_DELAY_MS = 250;	// lf-fixed: the engine re-enables the item's physics after the drop, and every client must have seen the drop first

	protected static EntityID s_LastThrown;
	protected static vector s_vLaunch;
	protected static vector s_vFrom;

	//------------------------------------------------------------------------------------------------
	//! The food item in the hands of `character` (left-hand gadget or right-hand item), or null.
	static IEntity HeldFood(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera)
			return null;

		CharacterControllerComponent controller = chimera.GetCharacterController();
		if (!controller)
			return null;

		IEntity held = controller.GetAttachedGadgetAtLeftHandSlot();
		if (IsFood(held))
			return held;

		held = controller.GetRightHandItem();
		if (IsFood(held))
			return held;

		return null;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsFood(IEntity item)
	{
		if (!item)
			return false;

		SCR_ConsumableItemComponent consumable = SCR_ConsumableItemComponent.Cast(item.FindComponent(SCR_ConsumableItemComponent));
		return consumable && LF_ConsumableEffectEat.Cast(consumable.GetConsumableEffect());
	}

	//------------------------------------------------------------------------------------------------
	//! The last item thrown on this machine (the harness and the tests read it).
	static IEntity LastThrown()
	{
		return GetGame().GetWorld().FindEntityByID(s_LastThrown);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the throw key (or the harness). Asks the server to throw what is in the hands.
	static void Ask()
	{
		IEntity character = SCR_PlayerController.GetLocalControlledEntity();
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!character || !controller || !HeldFood(character))
			return;

		string why = LF_ConsumeRules.RefusalFor(character);
		if (!why.IsEmpty())
		{
			LF_ConsumeRules.Tell(why);
			return;
		}

		controller.LF_AskThrowFood(AimDirection(character));
	}

	//------------------------------------------------------------------------------------------------
	//! Where the player looks: the camera's forward, else the body's.
	protected static vector AimDirection(IEntity character)
	{
		vector ahead = character.GetTransformAxis(2);
		CameraManager cameras = GetGame().GetCameraManager();
		if (cameras)
		{
			CameraBase camera = cameras.CurrentCamera();
			if (camera)
			{
				vector view[4];
				camera.GetWorldTransform(view);
				ahead = view[2];
			}
		}

		return ahead;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: throw what `character` holds along `aim`. "" on success, else why not.
	static string Throw(IEntity character, vector aim)
	{
		if (!Replication.IsServer() || !character)
			return "error not_server";

		string uid = LF_Utils.GetPlayerUid(character);
		string why = LF_ConsumeRules.RefusalFor(character);
		if (!why.IsEmpty())
		{
			LF_Debug.Info(LF_ConsumeRules.FEATURE, string.Format("throw by %1 refused: %2", uid, why));
			Notify(character, why);
			return "error " + why;
		}

		IEntity held = HeldFood(character);
		if (!held)
		{
			LF_Debug.Info(LF_ConsumeRules.FEATURE, string.Format("throw by %1 ignored: no food in the hands", uid));
			return "error no_food_held";
		}

		// An empty, NaN or absurd aim from a client falls back to the way the body faces.
		if (!(aim.LengthSq() > 0.0001) || aim.LengthSq() > 1000000)
			aim = character.GetTransformAxis(2);
		aim.Normalize();

		IEntity thrown = TakeOne(character, held);
		if (!thrown)
			return "error drop_refused";

		vector from = character.GetOrigin() + Vector(0, START_HEIGHT, 0) + aim * START_AHEAD;
		s_LastThrown = thrown.GetID();
		s_vFrom = from;
		s_vLaunch = aim * SPEED + Vector(0, LIFT, 0);
		thrown.SetOrigin(from);
		thrown.Update();
		// The one pending launch is the last throw: the delay is far shorter than the time to pick up
		// and throw another.
		GetGame().GetCallqueue().CallLater(Launch, LAUNCH_DELAY_MS, false);
		LF_Debug.Info(LF_ConsumeRules.FEATURE, string.Format("%1 threw %2 from %3 at %4", uid, LF_Utils.GetPrefabName(thrown), from, s_vLaunch));
		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! The item that leaves the hands: the held item itself, or a fresh one when it is a stack (the
	//! stack gives up one unit).
	protected static IEntity TakeOne(IEntity character, IEntity held)
	{
		LF_QuantityComponent quantity = LF_QuantityComponent.Cast(held.FindComponent(LF_QuantityComponent));
		if (quantity && quantity.GetQuantity() > 1)
		{
			IEntity single = LF_Utils.SpawnEntityPrefab(LF_Utils.GetPrefabName(held), character.GetOrigin());
			if (!single)
			{
				LF_Debug.Error(LF_ConsumeRules.FEATURE, "throw: the stack's prefab did not spawn");
				return null;
			}

			LF_QuantityComponent singleQuantity = LF_QuantityComponent.Cast(single.FindComponent(LF_QuantityComponent));
			if (singleQuantity)
				singleQuantity.SetQuantity(1);
			quantity.RemoveQuantity(1);
			return single;
		}

		InventoryStorageManagerComponent manager = LF_InventoryUtils.GetResponsibleStorageManager(character);
		InventoryItemComponent itemComponent = InventoryItemComponent.Cast(held.FindComponent(InventoryItemComponent));
		if (!manager || !itemComponent || !itemComponent.GetParentSlot() || !manager.TryRemoveItemFromStorage(held, itemComponent.GetParentSlot().GetStorage()))
		{
			LF_Debug.Error(LF_ConsumeRules.FEATURE, "throw: the inventory would not release " + LF_Utils.GetPrefabName(held));
			return null;
		}

		return held;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Launch()
	{
		IEntity item = LastThrown();
		if (!item)
			return;

		// A rigid body is simulated on every machine that holds a copy; a velocity set on the server
		// alone leaves each client's copy lying at the hands (the tin showed beside the thrower while
		// the server had it 7 m away). The item's own component sends the launch to every machine.
		SCR_ConsumableItemComponent consumable = SCR_ConsumableItemComponent.Cast(item.FindComponent(SCR_ConsumableItemComponent));
		if (consumable)
			consumable.LF_LaunchEverywhere(s_vFrom, s_vLaunch);
		else
			WakeAndLaunch(item, s_vFrom, s_vLaunch);

		LF_Debug.Info(LF_ConsumeRules.FEATURE, "throw launched");
	}

	//------------------------------------------------------------------------------------------------
	//! On every machine: put the item at the launch point and give it the velocity. A drop leaves it
	//! without live physics and may leave it hidden, so it is woken explicitly.
	static void WakeAndLaunch(IEntity item, vector from, vector velocity)
	{
		if (!item)
			return;

		item.SetOrigin(from);
		item.Update();
		Physics physics = item.GetPhysics();
		if (!physics)
		{
			LF_Debug.Error(LF_ConsumeRules.FEATURE, "throw: the item has no physics, it drops where it is");
			return;
		}

		InventoryItemComponent itemComponent = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
		if (itemComponent)
		{
			itemComponent.ActivateOwner(true);
			itemComponent.ShowOwner();
			itemComponent.EnablePhysics();
		}

		physics.SetActive(ActiveState.ACTIVE);
		physics.SetVelocity(velocity);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Notify(IEntity character, string key)
	{
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(character);
		if (playerId > 0)
			LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Consume_Title", key, 3, LF_ENotificationType.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_LastThrown = EntityID.INVALID;
	}
}

//------------------------------------------------------------------------------------------------
class LF_FoodThrow_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_FoodThrow";
	}

	override void Apply()
	{
		LF_FoodThrow.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! The client-to-server bridge for the throw (the owner's controller is the one entity a client may RPC on).
modded class SCR_PlayerController
{
	protected int m_iLF_ThrowCooldownAt;

	//------------------------------------------------------------------------------------------------
	//! Client entry (also the harness): ask the server to throw the food in this player's hands.
	void LF_AskThrowFood(vector aim)
	{
		if (Replication.IsServer())
		{
			LF_FoodThrow.Throw(GetControlledEntity(), aim);
			return;
		}

		Rpc(RpcAsk_LF_ThrowFood, aim);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_ThrowFood(vector aim)
	{
		// One throw per cooldown: a flood of requests takes one item and the rest find empty hands.
		int now = System.GetTickCount();
		if (m_iLF_ThrowCooldownAt != 0 && now - m_iLF_ThrowCooldownAt < LF_FoodThrow.COOLDOWN_MS)
			return;

		m_iLF_ThrowCooldownAt = now;
		LF_FoodThrow.Throw(GetControlledEntity(), aim);
	}
}
