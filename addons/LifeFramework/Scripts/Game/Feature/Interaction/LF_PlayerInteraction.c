//------------------------------------------------------------------------------------------------
//! One player interaction menu (decision 2026-11-07, M1-12). Facing another player within REACH
//! offers a short list on their character's action context: Introduce, Show ID, Give cash, Give
//! item, then role actions when the role allows (police on duty: search, arrest; the hidden
//! `fine` and `org_invite` slots wait for LF_PlayerInteraction.RegisterRole).
//!
//! The server decides everything: range, both alive, the cash and item really carried. A client
//! only opens the amount sheet it is sent and names the target and amount back.
//------------------------------------------------------------------------------------------------
enum LF_EInteractionRefusal
{
	NONE,
	NO_TARGET,
	SELF,
	NOT_A_PLAYER,
	DEAD,
	TOO_FAR
}

//------------------------------------------------------------------------------------------------
enum LF_EPlayerAction
{
	INTRODUCE,
	SHOW_ID,
	GIVE_CASH,
	GIVE_ITEM,
	SEARCH,
	ARREST
}

//------------------------------------------------------------------------------------------------
//! A role action a later scope adds without editing the character prefab. Register it on every
//! machine (it is asked for the offer on the client and the server) with
//! LF_PlayerInteraction.RegisterRole("fine", handler).
class LF_PlayerRoleHandler : Managed
{
	//! Localization key shown in the action list.
	string GetActionName()
	{
		return string.Empty;
	}

	//! The entry name for this user facing this target. Defaults to GetActionName(); a handler
	//! whose entry reads differently per target (Invite / Remove / Join) overrides this.
	string GetActionNameFor(IEntity user, IEntity target)
	{
		return GetActionName();
	}

	//! Client and server: is the action offered to user on target?
	bool CanOffer(IEntity user, IEntity target)
	{
		return false;
	}

	//! Server only, after range and life checks passed.
	void Perform(IEntity user, IEntity target)
	{
	}
}

//------------------------------------------------------------------------------------------------
class LF_PlayerInteraction
{
	//! Metres between the two characters' origins. The menu is for the person in front of you.
	static const float REACH = 2.0;
	//! Gesture ids from the base-game commanding gestures (Configs/Commanding/Gestures).
	static const int GESTURE_HALT = 2;
	static const int GESTURE_POINT = 1;
	static const int GESTURE_MS = 2000;
	static const string SOUND_HANDOVER = "SOUND_PICK_UP";
	protected static const string DIGITS = "0123456789";

	protected static ref map<string, ref LF_PlayerRoleHandler> s_mRoles;

	//------------------------------------------------------------------------------------------------
	//! Pure reach rule, shared by the offer and the server.
	static LF_EInteractionRefusal CheckReach(bool hasTarget, bool isSelf, bool targetIsPlayer, bool userAlive, bool targetAlive, float distance)
	{
		if (!hasTarget)
			return LF_EInteractionRefusal.NO_TARGET;

		if (isSelf)
			return LF_EInteractionRefusal.SELF;

		if (!targetIsPlayer)
			return LF_EInteractionRefusal.NOT_A_PLAYER;

		if (!userAlive || !targetAlive)
			return LF_EInteractionRefusal.DEAD;

		if (distance > REACH)
			return LF_EInteractionRefusal.TOO_FAR;

		return LF_EInteractionRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Which fixed actions the list offers. Role slots are asked of their handler instead.
	static bool Offers(LF_EPlayerAction action, bool onDutyPolice)
	{
		if (action == LF_EPlayerAction.SEARCH || action == LF_EPlayerAction.ARREST)
			return onDutyPolice;

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Typed cash amount. \return The amount, or -1 for anything but a positive whole number.
	static int ParseAmount(string text)
	{
		string trimmed = text.Trim();
		if (trimmed.IsEmpty() || trimmed.Length() > 7)
			return -1;

		for (int i = 0; i < trimmed.Length(); i++)
		{
			if (!DIGITS.Contains(trimmed.Get(i)))
				return -1;
		}

		int amount = trimmed.ToInt();
		if (amount <= 0)
			return -1;

		return amount;
	}

	//------------------------------------------------------------------------------------------------
	static string RefusalKey(LF_EInteractionRefusal refusal)
	{
		if (refusal == LF_EInteractionRefusal.TOO_FAR)
			return "#LF-Interact_TooFar";

		if (refusal == LF_EInteractionRefusal.DEAD)
			return "#LF-Interact_NotAlive";

		return "#LF-Interact_NoOne";
	}

	//------------------------------------------------------------------------------------------------
	static bool IsAlive(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera)
			return false;

		CharacterControllerComponent controller = chimera.GetCharacterController();
		if (!controller)
			return false;

		return controller.GetLifeState() != ECharacterLifeState.DEAD;
	}

	//------------------------------------------------------------------------------------------------
	static int PlayerIdOf(IEntity character)
	{
		if (!character || !GetGame() || !GetGame().GetPlayerManager())
			return 0;

		return GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(character);
	}

	//------------------------------------------------------------------------------------------------
	//! Reach rule on live entities. Works on a client (offer) and on the server (decision).
	static LF_EInteractionRefusal CheckPair(IEntity user, IEntity target)
	{
		if (!user || !target)
			return CheckReach(false, false, false, false, false, 0);

		float distance = vector.Distance(user.GetOrigin(), target.GetOrigin());
		return CheckReach(true, user == target, PlayerIdOf(target) > 0, IsAlive(user), IsAlive(target), distance);
	}

	//------------------------------------------------------------------------------------------------
	static bool CanOffer(IEntity user, IEntity target)
	{
		return CheckPair(user, target) == LF_EInteractionRefusal.NONE;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsOnDutyPolice(IEntity user)
	{
		return LF_AccountStateBits.IsOnDutyPolice(LF_AccountReplication.GetState(user));
	}

	//------------------------------------------------------------------------------------------------
	//! Extension seam for wave-3 scopes. Call on every machine (client and server).
	static void RegisterRole(string slot, LF_PlayerRoleHandler handler)
	{
		if (slot.IsEmpty() || !handler)
		{
			LF_Debug.Error("Interaction", "role registration refused: empty slot or null handler");
			return;
		}

		if (!s_mRoles)
			s_mRoles = new map<string, ref LF_PlayerRoleHandler>();

		s_mRoles.Set(slot, handler);
		LF_Debug.Info("Interaction", "role slot registered: " + slot);
	}

	//------------------------------------------------------------------------------------------------
	static LF_PlayerRoleHandler GetRole(string slot)
	{
		if (!s_mRoles)
			return null;

		return s_mRoles.Get(slot);
	}

	//------------------------------------------------------------------------------------------------
	static void UnregisterRole(string slot)
	{
		if (s_mRoles)
			s_mRoles.Remove(slot);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the decision every action re-runs. Tells the user why on a refusal.
	protected static bool ServerAllows(IEntity user, IEntity target, string what)
	{
		if (!Replication.IsServer())
			return false;

		LF_EInteractionRefusal refusal = CheckPair(user, target);
		if (refusal == LF_EInteractionRefusal.NONE)
			return true;

		LF_Debug.Info("Interaction", string.Format("%1 refused: %2", what, typename.EnumToString(LF_EInteractionRefusal, refusal)));
		LF_NotificationManagerComponent.NotifyPlayer(PlayerIdOf(user), "#LF-Interact_Title", RefusalKey(refusal), 4.0, LF_ENotificationType.WARNING);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	static SCR_PlayerController ControllerOf(IEntity character)
	{
		int playerId = PlayerIdOf(character);
		if (playerId <= 0)
			return null;

		return SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the name printed on the player's active character, or empty.
	static string DisplayName(string playerUid)
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!accounts || playerUid.IsEmpty())
			return string.Empty;

		LF_PlayerAccount account = accounts.GetFromCache(playerUid);
		if (!account)
			return string.Empty;

		LF_PlayerCharacter character = account.GetActiveCharacter();
		if (!character)
			return string.Empty;

		return (character.GetFirstName() + " " + character.GetLastName()).Trim();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: plays a base-game gesture on the user's own machine (the owner animates its body).
	static void PlayGesture(IEntity user, int gestureId)
	{
		if (gestureId <= 0)
			return;

		SCR_PlayerController controller = ControllerOf(user);
		if (!controller)
			return;

		controller.LF_PlayInteractionGesture(gestureId, GESTURE_MS);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the hand-over sound at the receiver.
	static void PlayHandoverSound(IEntity receiver, string soundEvent)
	{
		if (soundEvent.IsEmpty() || !receiver)
			return;

		SCR_InventoryStorageManagerComponent inventory = LF_Component<SCR_InventoryStorageManagerComponent>.Find(receiver);
		if (inventory)
			inventory.RpcAsk_PlaySound(LF_NetworkUtils.GetRplId(receiver), soundEvent);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: target learns user's name. The target's machine shows it when they look at user.
	static bool Introduce(IEntity user, IEntity target, int gestureId)
	{
		if (!ServerAllows(user, target, "introduce"))
			return false;

		string userUid = LF_Utils.GetPlayerUID(user);
		string targetUid = LF_Utils.GetPlayerUID(target);
		if (userUid.IsEmpty() || targetUid.IsEmpty())
		{
			LF_Debug.Error("Interaction", "introduce refused: a player has no account id");
			return false;
		}

		string name = DisplayName(userUid);
		if (name.IsEmpty())
		{
			LF_Debug.Warn("Interaction", "introduce refused: player " + userUid + " has no character name");
			return false;
		}

		PlayGesture(user, gestureId);

		int userId = PlayerIdOf(user);
		int targetId = PlayerIdOf(target);
		if (!LF_Introductions.Introduce(targetUid, userUid))
		{
			LF_Debug.Info("Interaction", string.Format("player %1 already knows player %2", targetId, userId));
			LF_NotificationManagerComponent.NotifyPlayer(userId, "#LF-Interact_Title", "#LF-Interact_AlreadyKnown", 3.0);
			SendName(target, userId, name);
			return true;
		}

		SendName(target, userId, name);
		LF_NotificationManagerComponent.NotifyPlayer(userId, "#LF-Interact_Title", "#LF-Interact_Introduced", 3.0, LF_ENotificationType.SUCCESS);
		LF_NotificationManagerComponent.NotifyPlayer(targetId, "#LF-Interact_Title", "#LF-Interact_MetSomeone", 4.0, LF_ENotificationType.INFO, name);
		LF_Debug.Info("Interaction", string.Format("player %1 introduced themselves to player %2", userId, targetId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void SendName(IEntity knower, int knownPlayerId, string name)
	{
		SCR_PlayerController controller = ControllerOf(knower);
		if (controller)
			controller.LF_LearnName(knownPlayerId, name);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: after a spawn, the player's machine learns the names it knows of everyone online,
	//! and everyone online who knows this player learns their name again.
	static void SyncNames(int playerId)
	{
		if (!Replication.IsServer() || playerId <= 0)
			return;

		PlayerManager players = GetGame().GetPlayerManager();
		string uid = LF_Utils.GetPlayerUID(playerId);
		if (uid.IsEmpty())
			return;

		SCR_PlayerController self = SCR_PlayerController.Cast(players.GetPlayerController(playerId));
		string ownName = DisplayName(uid);
		array<int> online = {};
		players.GetPlayers(online);
		int sent = 0;
		foreach (int otherId : online)
		{
			if (otherId == playerId)
				continue;

			string otherUid = LF_Utils.GetPlayerUID(otherId);
			if (otherUid.IsEmpty())
				continue;

			if (self && LF_Introductions.Knows(uid, otherUid))
			{
				string otherName = DisplayName(otherUid);
				if (!otherName.IsEmpty())
				{
					self.LF_LearnName(otherId, otherName);
					sent++;
				}
			}

			if (!ownName.IsEmpty() && LF_Introductions.Knows(otherUid, uid))
			{
				SCR_PlayerController other = SCR_PlayerController.Cast(players.GetPlayerController(otherId));
				if (other)
				{
					other.LF_LearnName(playerId, ownName);
					sent++;
				}
			}
		}

		LF_Debug.Info("Interaction", string.Format("names synced for player %1: %2 sent", playerId, sent));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: opens the amount sheet on the giver's machine. The sheet answers with AskGiveCash.
	static bool OpenGiveCash(IEntity user, IEntity target)
	{
		if (!ServerAllows(user, target, "give cash"))
			return false;

		LF_EntityCashWallet wallet = new LF_EntityCashWallet(user);
		int carried = wallet.Carried();
		if (carried <= 0)
		{
			LF_NotificationManagerComponent.NotifyPlayer(PlayerIdOf(user), "#LF-Interact_Title", "#LF-Interact_NoCash", 3.0, LF_ENotificationType.WARNING);
			return false;
		}

		SCR_PlayerController controller = ControllerOf(user);
		if (!controller)
		{
			LF_Debug.Error("Interaction", "give cash refused: the giver has no player controller");
			return false;
		}

		controller.LF_OpenGiveCash(LF_NetworkUtils.GetRplId(target), carried);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: hands amount of the giver's carried cash to target, after every check again.
	static LF_EGiveCash GiveCash(IEntity user, IEntity target, int amount, int gestureId)
	{
		if (!ServerAllows(user, target, "give cash"))
			return LF_EGiveCash.REFUSED_REACH;

		int moved;
		LF_EntityCashWallet from = new LF_EntityCashWallet(user, "give-cash", LF_Utils.GetPlayerUID(target));
		LF_EntityCashWallet to = new LF_EntityCashWallet(target, "give-cash", LF_Utils.GetPlayerUID(user));
		LF_EGiveCash result = LF_CashHandover.Move(from, to, amount, moved);
		int userId = PlayerIdOf(user);
		int targetId = PlayerIdOf(target);
		if (result != LF_EGiveCash.GIVEN)
		{
			LF_Debug.Info("Interaction", string.Format("give cash refused: player %1 -> %2 amount %3: %4", userId, targetId, amount, typename.EnumToString(LF_EGiveCash, result)));
			string reason = "#LF-Interact_CashFailed";
			if (result == LF_EGiveCash.REFUSED_NOT_CARRIED)
				reason = "#LF-Interact_NotEnoughCash";
			else if (result == LF_EGiveCash.REFUSED_AMOUNT)
				reason = "#LF-Interact_BadAmount";

			LF_NotificationManagerComponent.NotifyPlayer(userId, "#LF-Interact_Title", reason, 4.0, LF_ENotificationType.WARNING);
			return result;
		}

		PlayGesture(user, gestureId);
		PlayHandoverSound(target, SOUND_HANDOVER);
		LF_NotificationManagerComponent.NotifyPlayer(userId, "#LF-Interact_Title", "#LF-Interact_CashGiven", 3.0, LF_ENotificationType.SUCCESS, moved.ToString());
		LF_NotificationManagerComponent.NotifyPlayer(targetId, "#LF-Interact_Title", "#LF-Interact_CashReceived", 4.0, LF_ENotificationType.SUCCESS, moved.ToString());
		LF_Debug.Info("Interaction", string.Format("player %1 gave %2 cash to player %3", userId, moved, targetId));
		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: asks the giver's machine which item is in hand (the owner knows its hands).
	static bool OfferItemInHand(IEntity user, IEntity target)
	{
		if (!ServerAllows(user, target, "give item"))
			return false;

		SCR_PlayerController controller = ControllerOf(user);
		if (!controller)
		{
			LF_Debug.Error("Interaction", "give item refused: the giver has no player controller");
			return false;
		}

		controller.LF_OfferItemInHand(LF_NetworkUtils.GetRplId(target));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Owner: the item the character holds (weapon, gadget, or hand-carried item), or null.
	static IEntity ItemInHand(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera)
			return null;

		CharacterControllerComponent controller = chimera.GetCharacterController();
		if (!controller)
			return null;

		IEntity gadget = controller.GetAttachedGadgetAtLeftHandSlot();
		if (gadget)
			return gadget;

		IEntity right = controller.GetRightHandItem();
		if (right)
			return right;

		BaseWeaponManagerComponent weapons = controller.GetWeaponManagerComponent();
		if (weapons)
		{
			BaseWeaponComponent weapon = weapons.GetCurrentWeapon();
			if (weapon && weapon.GetOwner())
				return weapon.GetOwner();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: moves item from user to target. The item must sit in user's own inventory.
	static bool GiveItem(IEntity user, IEntity target, IEntity item, int gestureId)
	{
		if (!ServerAllows(user, target, "give item"))
			return false;

		int userId = PlayerIdOf(user);
		int targetId = PlayerIdOf(target);
		if (!item || item == user || LF_InventoryUtils.GetStorageHierachyRoot(item) != user)
		{
			LF_Debug.Warn("Interaction", string.Format("give item refused: player %1 does not carry that item", userId));
			LF_NotificationManagerComponent.NotifyPlayer(userId, "#LF-Interact_Title", "#LF-Interact_NothingInHand", 3.0, LF_ENotificationType.WARNING);
			return false;
		}

		InventoryStorageManagerComponent giverInventory = LF_Component<InventoryStorageManagerComponent>.Find(user);
		InventoryStorageManagerComponent receiverInventory = LF_Component<InventoryStorageManagerComponent>.Find(target);
		if (!giverInventory || !receiverInventory)
		{
			LF_Debug.Error("Interaction", "give item refused: a character has no inventory manager");
			return false;
		}

		BaseInventoryStorageComponent storage = receiverInventory.FindStorageForItem(item, EStoragePurpose.PURPOSE_DEPOSIT);
		if (!storage)
			storage = receiverInventory.FindStorageForItem(item, EStoragePurpose.PURPOSE_ANY);

		if (!storage)
		{
			LF_Debug.Info("Interaction", string.Format("give item refused: player %1 has no room", targetId));
			LF_NotificationManagerComponent.NotifyPlayer(userId, "#LF-Interact_Title", "#LF-Interact_NoRoom", 3.0, LF_ENotificationType.WARNING);
			return false;
		}

		if (!giverInventory.TryMoveItemToStorage(item, storage))
		{
			LF_Debug.Warn("Interaction", string.Format("give item failed: move from player %1 to player %2 refused by the inventory", userId, targetId));
			LF_NotificationManagerComponent.NotifyPlayer(userId, "#LF-Interact_Title", "#LF-Interact_NoRoom", 3.0, LF_ENotificationType.WARNING);
			return false;
		}

		PlayGesture(user, gestureId);
		PlayHandoverSound(target, SOUND_HANDOVER);
		LF_NotificationManagerComponent.NotifyPlayer(userId, "#LF-Interact_Title", "#LF-Interact_ItemGiven", 3.0, LF_ENotificationType.SUCCESS);
		LF_NotificationManagerComponent.NotifyPlayer(targetId, "#LF-Interact_Title", "#LF-Interact_ItemReceived", 4.0, LF_ENotificationType.SUCCESS);
		LF_Debug.Info("Interaction", string.Format("player %1 gave %2 to player %3", userId, LF_Utils.GetPrefabName(item), targetId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a role slot's perform, after range and life checks.
	static bool PerformRole(string slot, IEntity user, IEntity target)
	{
		LF_PlayerRoleHandler handler = GetRole(slot);
		if (!handler)
		{
			LF_Debug.Warn("Interaction", "role action refused: no handler for slot " + slot);
			return false;
		}

		if (!ServerAllows(user, target, "role " + slot))
			return false;

		if (!handler.CanOffer(user, target))
		{
			LF_Debug.Info("Interaction", string.Format("role %1 refused by its handler for player %2", slot, PlayerIdOf(user)));
			return false;
		}

		handler.Perform(user, target);
		return true;
	}
}
