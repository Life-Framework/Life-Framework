//------------------------------------------------------------------------------------------------
//! Client <-> server bridge for the first-join menus. Hosted on the PlayerController: it exists
//! (and is owned by the joining client) before any character is spawned.
//!   server -> owner : LF_ShowRegistrationForm, LF_ShowIdCardView (RpcDo_*, RplRcver.Owner)
//!   owner -> server : EL_AskCreateCharacter (RpcAsk_*)
//! There is no faction pick: everyone registers as a civilian and police clock in at the duty desk.
//! The server identifies the player by this controller's id, never by a client-sent value, and
//! EL_CharacterCreationManager re-validates every choice. A listen host's own controller is
//! local on both ends, so it calls the handlers directly instead of RPCing to itself.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	protected bool EL_IsLocalController()
	{
		return GetGame().GetPlayerController() == this;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: open the registration form (R-89) on this player's machine.
	//! \param refusal Localization key written on the form, empty on the first open.
	void LF_ShowRegistrationForm(string refusal)
	{
		if (EL_IsLocalController())
			RpcDo_LF_ShowRegistrationForm(refusal);
		else
			Rpc(RpcDo_LF_ShowRegistrationForm, refusal);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: show this player the ID card another player holds up. The card's printed fields
	//! travel as values; the shower travels as an RplId so the view closes when they walk off.
	void LF_ShowIdCardView(string firstName, string lastName, int age, RplId shower)
	{
		if (EL_IsLocalController())
			RpcDo_LF_ShowIdCardView(firstName, lastName, age, shower);
		else
			Rpc(RpcDo_LF_ShowIdCardView, firstName, lastName, age, shower);
	}

	//------------------------------------------------------------------------------------------------
	//! Client: send the name/age form.
	void EL_AskCreateCharacter(string firstName, string lastName, int age)
	{
		if (Replication.IsServer())
			RpcAsk_EL_CreateCharacter(firstName, lastName, age);
		else
			Rpc(RpcAsk_EL_CreateCharacter, firstName, lastName, age);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_ShowRegistrationForm(string refusal)
	{
		if (!GetGame().GetWorkspace())
		{
			EL_Debug.Error("CharacterCreation", "registration form not opened: no workspace on this machine");
			return;
		}

		EL_DestroySpawnSplash();
		if (!LF_RegistrationForm.Open(this, refusal))
			EL_Debug.Error("CharacterCreation", "failed to open the registration form");
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_ShowIdCardView(string firstName, string lastName, int age, RplId shower)
	{
		if (!GetGame().GetWorkspace())
		{
			EL_Debug.Error("IdCard", "card view not opened: no workspace on this machine");
			return;
		}

		LF_IdCardView.Open(firstName, lastName, age, EL_NetworkUtils.FindEntityByRplId(shower));
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_EL_CreateCharacter(string firstName, string lastName, int age)
	{
		EL_CharacterCreationManager manager = EL_CharacterCreationManager.GetInstance();
		if (!manager)
		{
			EL_Debug.Error("CharacterCreation", "character request dropped: no creation manager on the server");
			return;
		}

		manager.ServerCreateCharacter(GetPlayerId(), firstName, lastName, age);
	}

	//------------------------------------------------------------------------------------------------
	//! The respawn system's splash covers every screen until told otherwise, and the audit chain
	//! that normally clears it never runs for the local player of an offline session.
	protected void EL_DestroySpawnSplash()
	{
		SCR_RespawnSystemComponent respawnSystem = SCR_RespawnSystemComponent.GetInstance();
		if (respawnSystem)
			respawnSystem.DestroyLoadingPlaceholder();
	}

	//------------------------------------------------------------------------------------------------
	//! A returning player whose character the save restored never sees the form and is never
	//! spawned (EL_SpawnLogic: "already controls a character"), so nothing else clears the splash.
	//! It stayed on screen with game sound paused after every restart. Taking control of a living
	//! character is the moment the splash has to go, whichever path handed it over.
	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);

		if (!to || !EL_IsLocalController() || !GetGame().GetWorkspace())
			return;

		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(to);
		if (!character)
			return;

		CharacterControllerComponent characterController = CharacterControllerComponent.Cast(character.FindComponent(CharacterControllerComponent));
		if (characterController && characterController.GetLifeState() == ECharacterLifeState.DEAD)
			return;

		EL_DestroySpawnSplash();
		EL_Debug.Info("CharacterCreation", "spawn splash cleared: local player controls a character");
	}
}
