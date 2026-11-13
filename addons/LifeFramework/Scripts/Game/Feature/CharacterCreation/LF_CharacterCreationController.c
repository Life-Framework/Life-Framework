//------------------------------------------------------------------------------------------------
//! Client <-> server bridge for the first-join menus. Hosted on the PlayerController: it exists
//! (and is owned by the joining client) before any character is spawned.
//!   server -> owner : LF_ShowRegistrationForm (RpcDo_*, RplRcver.Owner); the ID card view is in
//!                     Feature/Identity/LF_IdentityController.c
//!   owner -> server : LF_AskCreateCharacter (RpcAsk_*)
//! There is no faction pick: everyone registers as a civilian and police clock in at the duty desk.
//! The server identifies the player by this controller's id, never by a client-sent value, and
//! LF_CharacterCreationManager re-validates every choice. A listen host's own controller is
//! local on both ends, so it calls the handlers directly instead of RPCing to itself.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	protected bool LF_IsLocalController()
	{
		return GetGame().GetPlayerController() == this;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: open the registration form (R-89) on this player's machine.
	//! \param refusal Localization key written on the form, empty on the first open.
	void LF_ShowRegistrationForm(string refusal)
	{
		if (LF_IsLocalController())
			RpcDo_LF_ShowRegistrationForm(refusal);
		else
			Rpc(RpcDo_LF_ShowRegistrationForm, refusal);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: open the character pick (#331) on this player's machine.
	//! \param names The account's character names in slot order, one per line.
	//! \param canRegister True while the account has a free slot (offers "register a new resident").
	void LF_ShowCharacterSelect(string names, bool canRegister)
	{
		if (LF_IsLocalController())
			RpcDo_LF_ShowCharacterSelect(names, canRegister);
		else
			Rpc(RpcDo_LF_ShowCharacterSelect, names, canRegister);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: play the character in this slot (0-based), or LF_CharacterSelectSheet.SLOT_NEW to
	//! register a new one.
	void LF_AskSelectCharacter(int slot)
	{
		if (Replication.IsServer())
			RpcAsk_LF_SelectCharacter(slot);
		else
			Rpc(RpcAsk_LF_SelectCharacter, slot);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_ShowCharacterSelect(string names, bool canRegister)
	{
		if (!GetGame().GetWorkspace())
		{
			LF_Debug.Error("CharacterCreation", "character pick not opened: no workspace on this machine");
			return;
		}

		LF_DestroySpawnSplash();
		if (!LF_CharacterSelectSheet.Open(this, names, canRegister))
			LF_Debug.Error("CharacterCreation", "failed to open the character pick");
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_SelectCharacter(int slot)
	{
		LF_CharacterCreationManager manager = LF_CharacterCreationManager.GetInstance();
		if (!manager)
		{
			LF_Debug.Error("CharacterCreation", "character pick dropped: no creation manager on the server");
			return;
		}

		manager.ServerSelectCharacter(GetPlayerId(), slot);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: send the name/age form.
	void LF_AskCreateCharacter(string firstName, string lastName, int age)
	{
		if (Replication.IsServer())
			RpcAsk_LF_CreateCharacter(firstName, lastName, age);
		else
			Rpc(RpcAsk_LF_CreateCharacter, firstName, lastName, age);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_ShowRegistrationForm(string refusal)
	{
		if (!GetGame().GetWorkspace())
		{
			LF_Debug.Error("CharacterCreation", "registration form not opened: no workspace on this machine");
			return;
		}

		LF_DestroySpawnSplash();
		if (!LF_RegistrationForm.Open(this, refusal))
			LF_Debug.Error("CharacterCreation", "failed to open the registration form");
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_CreateCharacter(string firstName, string lastName, int age)
	{
		LF_CharacterCreationManager manager = LF_CharacterCreationManager.GetInstance();
		if (!manager)
		{
			LF_Debug.Error("CharacterCreation", "character request dropped: no creation manager on the server");
			return;
		}

		manager.ServerCreateCharacter(GetPlayerId(), firstName, lastName, age);
	}

	//------------------------------------------------------------------------------------------------
	//! The respawn system's splash covers every screen until told otherwise, and the audit chain
	//! that normally clears it never runs for the local player of an offline session.
	protected void LF_DestroySpawnSplash()
	{
		SCR_RespawnSystemComponent respawnSystem = SCR_RespawnSystemComponent.GetInstance();
		if (respawnSystem)
			respawnSystem.DestroyLoadingPlaceholder();
	}

	//------------------------------------------------------------------------------------------------
	//! A returning player whose character the save restored never sees the form and is never
	//! spawned (LF_SpawnLogic: "already controls a character"), so nothing else clears the splash.
	//! It stayed on screen with game sound paused after every restart. Taking control of a living
	//! character is the moment the splash has to go, whichever path handed it over.
	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);

		if (!to || !LF_IsLocalController() || !GetGame().GetWorkspace())
			return;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(to);
		if (!character)
			return;

		CharacterControllerComponent characterController = CharacterControllerComponent.Cast(character.FindComponent(CharacterControllerComponent));
		if (characterController && characterController.GetLifeState() == ECharacterLifeState.DEAD)
			return;

		LF_DestroySpawnSplash();
		LF_Debug.Info("CharacterCreation", "spawn splash cleared: local player controls a character");
	}
}
