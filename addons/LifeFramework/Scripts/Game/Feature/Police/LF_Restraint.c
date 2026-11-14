//------------------------------------------------------------------------------------------------
//! Handcuffs (#664 asset, police V2). An on-duty officer holding handcuffs in the left hand cuffs a
//! downed, stunned, or held-up and unarmed player in front of them (the player interaction menu,
//! Character_Roleplay.et context LF_ID). The cuffs leave the officer's inventory and go onto the
//! suspect; release, an arrest at the cell, or the suspect leaving gives a pair back.
//!
//! A cuffed player keeps walking (escort and transport stay physical) but cannot use a weapon or a
//! held gadget: the owner's controller disables weapon controls and holsters what is in hand.
//! The server decides everything; the replicated flag on the character carries the state to every
//! client and to a late joiner. The base game has no surrender, hands-up or restrained animation
//! (docs/research/animation-pipeline.md); the cuffs show on the suspect's right wrist instead.
//------------------------------------------------------------------------------------------------
enum LF_ERestrain
{
	OK,
	NO_TARGET,
	NOT_POLICE,
	NO_CUFFS,
	NOT_COMPLIANT,
	ALREADY_RESTRAINED,
	NOT_RESTRAINED,
	RATE_LIMITED
}

//------------------------------------------------------------------------------------------------
//! Marks an item as handcuffs: holding one is the requirement to cuff, and the item consumed onto
//! the suspect is the prefab that comes back on release.
class LF_HandcuffsComponentClass : ScriptComponentClass
{
}

class LF_HandcuffsComponent : ScriptComponent
{
	//------------------------------------------------------------------------------------------------
	//! Server: the cuffs' use state (docs/asset-pipeline/states.md). Open swings both strands out on their
	//! hinges (LF_AssetAnimComponent, replicated, late joiners included); closed puts them back. The
	//! cuffing hold (LF_Cuffing) opens them in the officer's hand and closes them before they go onto
	//! the suspect. Every change plays its click on every machine in earshot.
	bool SetOpen(bool open)
	{
		LF_AssetAnimComponent anim = LF_Component<LF_AssetAnimComponent>.Find(GetOwner());
		if (!anim)
		{
			LF_Debug.Error(LF_Restraint.FEATURE, "handcuffs have no LF_AssetAnimComponent: cannot open them");
			return false;
		}

		bool changed = anim.IsRunning() != open;
		if (!anim.SetActive(open))
			return false;

		if (changed)
			Click(open);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool IsOpen()
	{
		LF_AssetAnimComponent anim = LF_Component<LF_AssetAnimComponent>.Find(GetOwner());
		return anim && anim.IsRunning();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: tell every machine to play the click of the strands opening or shutting.
	protected void Click(bool open)
	{
		RplComponent rpl = RplComponent.Cast(GetOwner().FindComponent(RplComponent));
		if (rpl)
			Rpc(RpcDo_Click, open);

		RpcDo_Click(open);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_Click(bool open)
	{
		LF_Restraint.PlayClick(GetOwner(), open);
	}
}

//------------------------------------------------------------------------------------------------
class LF_RestraintHold : Managed
{
	string m_sOfficerUid;
	ResourceName m_sCuffsPrefab;
}

//------------------------------------------------------------------------------------------------
class LF_Restraint
{
	static const string FEATURE = "Police";
	//! Shown on the suspect's wrist while cuffed: the handcuffs model with no item, physics or actions.
	static const ResourceName WORN_PREFAB = "{7BD876D8BA3FF91E}Prefabs/Items/Roleplay/Handcuffs_Worn.et";
	//! The bone weapons attach to (vanilla scripts look it up by this name); a "RightHand" bone does not exist.
	static const string WORN_BONE = "righthandprop";
	//! The prop bone's +Z is where the hand points (a held pistol's barrel). The model lies flat with
	//! its rings in the XZ plane, so a quarter turn about X puts a ring's axis along the forearm, and
	//! that ring (WORN_RING_Z along the model's length) is moved to WORN_WRIST_Z, behind the palm.
	static const float WORN_PITCH = 90; // lf-fixed: geometry of the handcuffs model, not a gameplay knob
	static const float WORN_RING_Z = 0.0525; // lf-fixed: ring centre in Handcuffs.xob
	static const float WORN_WRIST_Z = -0.07; // lf-fixed: wrist behind righthandprop on the vanilla skeleton

	//! The click of the cuffs shutting (SOUND_LF_CLOSE) and opening (SOUND_LF_OPEN): tools/assets/handcuffs/sounds.json.
	static const ResourceName CLICK_PROJECT = "{E8150CA363574149}Sounds/Items/Handcuffs/Items_Handcuffs_Use.acp";
	static const string CLICK_CLOSE = "SOUND_LF_CLOSE";
	static const string CLICK_OPEN = "SOUND_LF_OPEN";

	//! Prisoner uid -> who cuffed them and which cuffs. Server only.
	protected static ref map<string, ref LF_RestraintHold> s_mHolds;

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mHolds = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: may this officer cuff this target? The first failing reason wins.
	static LF_ERestrain DecideRestrain(bool onDutyPolice, bool holdsCuffs, bool targetDowned, bool targetCompliant, bool targetRestrained)
	{
		if (!onDutyPolice)
			return LF_ERestrain.NOT_POLICE;

		if (targetRestrained)
			return LF_ERestrain.ALREADY_RESTRAINED;

		if (!holdsCuffs)
			return LF_ERestrain.NO_CUFFS;

		if (!targetDowned && !targetCompliant)
			return LF_ERestrain.NOT_COMPLIANT;

		return LF_ERestrain.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: may this officer take the cuffs off?
	static LF_ERestrain DecideRelease(bool onDutyPolice, bool targetRestrained)
	{
		if (!onDutyPolice)
			return LF_ERestrain.NOT_POLICE;

		if (!targetRestrained)
			return LF_ERestrain.NOT_RESTRAINED;

		return LF_ERestrain.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: a held-up player complies while the hold-up is recent and their hands hold no weapon.
	//! \param heldUpAt Unix seconds of the last hold-up aimed at them, or -1 for none.
	static bool IsCompliant(int heldUpAt, int now, int windowSeconds, bool armed)
	{
		if (armed || heldUpAt < 0 || windowSeconds <= 0)
			return false;

		return now - heldUpAt <= windowSeconds;
	}

	//------------------------------------------------------------------------------------------------
	static string RefusalKey(LF_ERestrain result)
	{
		switch (result)
		{
			case LF_ERestrain.NOT_POLICE: return "#LF-Restrain_NotOfficer";
			case LF_ERestrain.NO_CUFFS: return "#LF-Restrain_NoCuffs";
			case LF_ERestrain.NOT_COMPLIANT: return "#LF-Restrain_NotCompliant";
			case LF_ERestrain.ALREADY_RESTRAINED: return "#LF-Restrain_Already";
			case LF_ERestrain.NOT_RESTRAINED: return "#LF-Restrain_NotCuffed";
		}

		return "#LF-Interact_TooFar";
	}

	//------------------------------------------------------------------------------------------------
	//! The handcuffs in this character's left hand, or null. Works on the owner and on the server: the
	//! held gadget follows the controller's left-hand attachment, which the base game replicates.
	static IEntity HeldCuffs(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera)
			return null;

		IEntity held;
		SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(character);
		if (gadgets)
			held = gadgets.GetHeldGadget();

		if (!held && chimera.GetCharacterController())
			held = chimera.GetCharacterController().GetAttachedGadgetAtLeftHandSlot();

		if (held && held.FindComponent(LF_HandcuffsComponent))
			return held;

		return null;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsRestrained(IEntity character)
	{
		SCR_ChimeraCharacter chimera = SCR_ChimeraCharacter.Cast(character);
		if (!chimera)
			return false;

		return chimera.LF_IsRestrained();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the target was held up recently and holds no weapon (LF_HoldUp keeps the time).
	static bool IsCompliantNow(IEntity target)
	{
		return IsCompliant(LF_HoldUp.HeldUpAt(LF_Utils.GetPlayerUID(target)), System.GetUnixTime(), LF_PoliceConfig.CompliantSeconds(), LF_HoldUp.IsArmed(target));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: who cuffed this prisoner, or "" when nobody holds them.
	static string HolderOf(string prisonerUid)
	{
		if (!s_mHolds || !s_mHolds.Contains(prisonerUid))
			return string.Empty;

		return s_mHolds.Get(prisonerUid).m_sOfficerUid;
	}

	//------------------------------------------------------------------------------------------------
	static int HoldCount()
	{
		if (!s_mHolds)
			return 0;

		return s_mHolds.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: may this officer cuff this target right now, with what they hold? No side effects: the
	//! cuffing hold asks it when it starts and on every watch pass.
	static LF_ERestrain CheckRestrain(IEntity officer, IEntity target)
	{
		bool downed = LF_Downed.IsDowned(target);
		bool compliant = !downed && IsCompliantNow(target);
		return DecideRestrain(LF_PoliceUtils.IsOnDutyOfficer(LF_PoliceUtils.GetAccountForEntity(officer)), HeldCuffs(officer) != null, downed, compliant, IsRestrained(target));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: may this officer take the cuffs off this target right now? No side effects.
	static LF_ERestrain CheckRelease(IEntity officer, IEntity target)
	{
		return DecideRelease(LF_PoliceUtils.IsOnDutyOfficer(LF_PoliceUtils.GetAccountForEntity(officer)), IsRestrained(target));
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine with a speaker: the click of cuffs shutting or opening, from `at`. Out of earshot
	//! (the project's range) the sound manager returns nothing and the click is skipped.
	static void PlayClick(IEntity at, bool open)
	{
		if (!at || System.IsConsoleApp() || RplSession.Mode() == RplMode.Dedicated)
			return;

		string eventName = CLICK_CLOSE;
		if (open)
			eventName = CLICK_OPEN;

		SCR_SoundManagerModule sound = SCR_SoundManagerModule.GetInstance(at.GetWorld());
		if (!sound)
		{
			LF_Debug.Info(FEATURE, string.Format("%1 not played: this machine has no sound manager", eventName));
			return;
		}

		SCR_AudioSourceConfiguration config = new SCR_AudioSourceConfiguration();
		config.m_sSoundProject = CLICK_PROJECT;
		config.m_sSoundEventName = eventName;
		// Not static: the cuffs are in a hand, or on a wrist, and move with it.
		config.m_eFlags = 0;
		SCR_AudioSource audio = sound.CreateAudioSource(at, config);
		if (!audio)
		{
			LF_Debug.Info(FEATURE, string.Format("%1 not played: out of earshot here (or %2 failed to load)", eventName, CLICK_PROJECT));
			return;
		}

		sound.PlayAudioSource(audio);
		LF_Debug.Info(FEATURE, string.Format("%1 played at %2", eventName, LF_Utils.GetPrefabName(at)));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: cuff target with the handcuffs officer holds. Logs and toasts every refusal.
	static LF_ERestrain Restrain(IEntity officer, IEntity target)
	{
		if (!Replication.IsServer())
			return LF_ERestrain.NO_TARGET;

		if (!officer || !target)
		{
			LF_Debug.Error(FEATURE, "restrain refused: officer or target is null");
			return LF_ERestrain.NO_TARGET;
		}

		int officerId = LF_PlayerInteraction.PlayerIdOf(officer);
		if (!LF_RateLimit.AllowCharacter(officer, LF_ERequestKind.SPAWN, "restrain"))
			return LF_ERestrain.RATE_LIMITED;

		string officerUid = LF_Utils.GetPlayerUID(officer);
		string prisonerUid = LF_Utils.GetPlayerUID(target);
		if (officerUid.IsEmpty() || prisonerUid.IsEmpty())
		{
			LF_Debug.Error(FEATURE, "restrain refused: officer or target has no account id");
			return LF_ERestrain.NO_TARGET;
		}

		IEntity cuffs = HeldCuffs(officer);
		bool downed = LF_Downed.IsDowned(target);
		LF_ERestrain result = CheckRestrain(officer, target);
		if (result != LF_ERestrain.OK)
		{
			LF_Debug.Info(FEATURE, string.Format("restrain refused: %1 -> %2 (%3)", officerUid, prisonerUid, typename.EnumToString(LF_ERestrain, result)));
			if (officerId > 0)
				LF_NotificationManagerComponent.NotifyPlayer(officerId, "#LF-Restrain_Title", RefusalKey(result), 3.0, LF_ENotificationType.WARNING);
			return result;
		}

		ResourceName prefab = LF_Utils.GetPrefabName(cuffs);
		InventoryStorageManagerComponent inventory = LF_Component<InventoryStorageManagerComponent>.Find(officer);
		if (!inventory || !inventory.TryDeleteItem(cuffs))
		{
			LF_Debug.Error(FEATURE, string.Format("restrain failed: the handcuffs of %1 could not leave the inventory", officerUid));
			return LF_ERestrain.NO_CUFFS;
		}

		LF_RestraintHold hold = new LF_RestraintHold();
		hold.m_sOfficerUid = officerUid;
		hold.m_sCuffsPrefab = prefab;
		if (!s_mHolds)
			s_mHolds = new map<string, ref LF_RestraintHold>();

		s_mHolds.Set(prisonerUid, hold);
		SCR_ChimeraCharacter.Cast(target).LF_SetRestrained(true);

		string how = "held up";
		if (downed)
			how = "downed";

		LF_Debug.Info(FEATURE, string.Format("restrained %1 by %2 (%3, cuffs %4)", prisonerUid, officerUid, how, prefab));
		LF_PlayerInteraction.PlayHandoverSound(target, LF_PlayerInteraction.SOUND_HANDOVER);
		LF_NotificationManagerComponent.NotifyPlayer(officerId, "#LF-Restrain_Title", "#LF-Restrain_Cuffed", 3.0, LF_ENotificationType.POLICE_ALERT);
		LF_NotificationManagerComponent.NotifyPlayer(LF_PlayerInteraction.PlayerIdOf(target), "#LF-Restrain_Title", "#LF-Restrain_YouAreCuffed", 4.0, LF_ENotificationType.WARNING);
		return LF_ERestrain.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an on-duty officer takes the cuffs off target and gets a pair back.
	static LF_ERestrain Release(IEntity officer, IEntity target)
	{
		if (!Replication.IsServer())
			return LF_ERestrain.NO_TARGET;

		if (!officer || !target)
		{
			LF_Debug.Error(FEATURE, "release refused: officer or target is null");
			return LF_ERestrain.NO_TARGET;
		}

		int officerId = LF_PlayerInteraction.PlayerIdOf(officer);
		if (!LF_RateLimit.AllowCharacter(officer, LF_ERequestKind.SPAWN, "release restraint"))
			return LF_ERestrain.RATE_LIMITED;

		LF_ERestrain result = CheckRelease(officer, target);
		string prisonerUid = LF_Utils.GetPlayerUID(target);
		if (result != LF_ERestrain.OK)
		{
			LF_Debug.Info(FEATURE, string.Format("release refused: %1 -> %2 (%3)", LF_Utils.GetPlayerUID(officer), prisonerUid, typename.EnumToString(LF_ERestrain, result)));
			if (officerId > 0)
				LF_NotificationManagerComponent.NotifyPlayer(officerId, "#LF-Restrain_Title", RefusalKey(result), 3.0, LF_ENotificationType.WARNING);
			return result;
		}

		Unrestrain(target, prisonerUid, officer, "released");
		LF_NotificationManagerComponent.NotifyPlayer(officerId, "#LF-Restrain_Title", "#LF-Restrain_Released", 3.0, LF_ENotificationType.SUCCESS);
		LF_NotificationManagerComponent.NotifyPlayer(LF_PlayerInteraction.PlayerIdOf(target), "#LF-Restrain_Title", "#LF-Restrain_Released", 3.0, LF_ENotificationType.SUCCESS);
		return LF_ERestrain.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an arrest at the cell takes the cuffs off; they go back to the arresting officer.
	static void OnJailed(string prisonerUid, string officerUid, IEntity prisoner)
	{
		if (!s_mHolds || !s_mHolds.Contains(prisonerUid))
			return;

		Unrestrain(prisoner, prisonerUid, FindCharacter(officerUid), "jailed");
	}

	//------------------------------------------------------------------------------------------------
	//! Server, on every character death: a cuffed body that dies (bleed-out, give-up) gives the cuffs
	//! back to the officer who cuffed it; the respawned body is free. Without this the hold outlived
	//! the body and the cuffs were lost (two-client session, 2026-11-13: "bled out" with no
	//! "restraint off" line).
	static void OnDied(IEntity victim)
	{
		if (!IsRestrained(victim))
			return;

		string uid = LF_Utils.GetPlayerUID(victim);
		IEntity officer;
		if (s_mHolds && s_mHolds.Contains(uid))
			officer = FindCharacter(s_mHolds.Get(uid).m_sOfficerUid);

		Unrestrain(victim, uid, officer, "died");
	}

	//------------------------------------------------------------------------------------------------
	//! Server, on disconnect: a prisoner who leaves frees the cuffs back to the officer who cuffed
	//! them (or loses them, logged, when that officer is gone too). An officer who leaves keeps no
	//! hold: any on-duty officer may release the prisoner. Rejoin rules are #691's.
	//! \return 1 when a hold on playerUid was dropped.
	static int ReleasePlayer(string playerUid)
	{
		if (playerUid.IsEmpty() || !s_mHolds || !s_mHolds.Contains(playerUid))
			return 0;

		Unrestrain(FindCharacter(playerUid), playerUid, FindCharacter(s_mHolds.Get(playerUid).m_sOfficerUid), "disconnected");
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: clear the flag, drop the hold, and put the cuffs in returnTo's inventory (at their
	//! feet when it is full, lost when nobody is there).
	protected static void Unrestrain(IEntity prisoner, string prisonerUid, IEntity returnTo, string why)
	{
		ResourceName prefab;
		if (s_mHolds && s_mHolds.Contains(prisonerUid))
		{
			prefab = s_mHolds.Get(prisonerUid).m_sCuffsPrefab;
			s_mHolds.Remove(prisonerUid);
		}

		SCR_ChimeraCharacter chimera = SCR_ChimeraCharacter.Cast(prisoner);
		if (chimera)
		{
			chimera.LF_SetCuffsOpen(false);
			chimera.LF_SetRestrained(false);
		}

		string returned = "lost: nobody to take them";
		if (!prefab.IsEmpty() && returnTo)
		{
			InventoryStorageManagerComponent inventory = LF_Component<InventoryStorageManagerComponent>.Find(returnTo);
			if (inventory && LF_InventoryUtils.AddItem(inventory, prefab))
				returned = "back in the inventory of " + LF_Utils.GetPlayerUID(returnTo);
			else if (LF_Utils.SpawnEntityPrefab(prefab, returnTo.GetOrigin()))
				returned = "dropped at the feet of " + LF_Utils.GetPlayerUID(returnTo);
		}
		else if (prefab.IsEmpty())
		{
			returned = "none recorded";
		}

		LF_Debug.Info(FEATURE, string.Format("restraint off %1 (%2); cuffs %3", prisonerUid, why, returned));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the connected character of a player uid, or null.
	protected static IEntity FindCharacter(string uid)
	{
		PlayerManager players = GetGame().GetPlayerManager();
		if (uid.IsEmpty() || !players)
			return null;

		array<int> ids = {};
		players.GetPlayers(ids);
		foreach (int id : ids)
		{
			if (LF_Utils.GetPlayerUID(id) == uid)
				return players.GetPlayerControlledEntity(id);
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! Interaction menu entry on another player: Cuff. Offered to an on-duty officer holding handcuffs.
//! A short hold (`Duration` on the action, the vanilla progress bar): the cuffs open in the officer's
//! hand, then close and go onto the suspect (LF_Cuffing). Whether the target is downed or held up is
//! decided on the server; a refusal says why.
class LF_RestrainAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Police"))
			return false;

		if (!LF_PlayerInteraction.CanOffer(user, GetOwner()))
			return false;

		if (!LF_PlayerInteraction.IsOnDutyPolice(user) || LF_Restraint.IsRestrained(GetOwner()))
			return false;

		return LF_Restraint.HeldCuffs(user) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! The hold began on the acting client: ask the server to open the cuffs.
	override void OnActionStart(IEntity pUserEntity)
	{
		LF_Cuffing.AskFromAction(pUserEntity, GetOwner(), LF_ECuffingKind.CUFF, LF_ECuffingPhase.START);
	}

	//------------------------------------------------------------------------------------------------
	override void OnActionCanceled(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		LF_Cuffing.AskFromAction(pUserEntity, pOwnerEntity, LF_ECuffingKind.CUFF, LF_ECuffingPhase.CANCEL);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Police", "LF_RestrainAction"))
			return;

		if (!Replication.IsServer())
			return;

		LF_EInteractionRefusal refusal = LF_PlayerInteraction.CheckPair(pUserEntity, pOwnerEntity);
		if (refusal != LF_EInteractionRefusal.NONE)
		{
			LF_Debug.Warn(LF_Restraint.FEATURE, "restrain action refused: " + typename.EnumToString(LF_EInteractionRefusal, refusal));
			LF_Cuffing.End(pUserEntity, "refused at the end of the hold");
			return;
		}

		LF_Cuffing.Complete(pUserEntity, pOwnerEntity, LF_ECuffingKind.CUFF, GetActionDuration());
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Restrain_Action";
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Interaction menu entry on a cuffed player: Uncuff. Any on-duty officer may take the cuffs off.
//! The same kind of hold: the cuffs on the wrist open, then come off.
class LF_ReleaseRestraintAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		if (!LF_Systems.IsEnabled("Police"))
			return false;

		if (!LF_PlayerInteraction.CanOffer(user, GetOwner()))
			return false;

		return LF_PlayerInteraction.IsOnDutyPolice(user) && LF_Restraint.IsRestrained(GetOwner());
	}

	//------------------------------------------------------------------------------------------------
	override void OnActionStart(IEntity pUserEntity)
	{
		LF_Cuffing.AskFromAction(pUserEntity, GetOwner(), LF_ECuffingKind.RELEASE, LF_ECuffingPhase.START);
	}

	//------------------------------------------------------------------------------------------------
	override void OnActionCanceled(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		LF_Cuffing.AskFromAction(pUserEntity, pOwnerEntity, LF_ECuffingKind.RELEASE, LF_ECuffingPhase.CANCEL);
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (LF_Systems.Refuses("Police", "LF_ReleaseRestraintAction"))
			return;

		if (!Replication.IsServer())
			return;

		LF_EInteractionRefusal refusal = LF_PlayerInteraction.CheckPair(pUserEntity, pOwnerEntity);
		if (refusal != LF_EInteractionRefusal.NONE)
		{
			LF_Debug.Warn(LF_Restraint.FEATURE, "release action refused: " + typename.EnumToString(LF_EInteractionRefusal, refusal));
			LF_Cuffing.End(pUserEntity, "refused at the end of the hold");
			return;
		}

		LF_Cuffing.Complete(pUserEntity, pOwnerEntity, LF_ECuffingKind.RELEASE, GetActionDuration());
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-Restrain_Release";
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The cuffed flag on the character, replicated so every client (and a late joiner) reads it.
modded class SCR_ChimeraCharacter
{
	[RplProp(onRplName: "LF_OnRestrainedReplicated")]
	protected bool m_bLF_Restrained;

	//! The cuffs on the wrist are unlocked and open: the officer is taking them off. Replicated, so a
	//! player who joins in the middle of it sees them open.
	[RplProp(onRplName: "LF_OnCuffsOpenReplicated")]
	protected bool m_bLF_CuffsOpen;

	//! Client-local model on the wrist; never replicated.
	protected IEntity m_LF_WornCuffs;
	//! What the worn model shows here, so a click plays only when it changes.
	protected bool m_bLF_WornShownOpen;

	//------------------------------------------------------------------------------------------------
	//! Server only.
	void LF_SetRestrained(bool restrained)
	{
		if (!Replication.IsServer() || restrained == m_bLF_Restrained)
			return;

		m_bLF_Restrained = restrained;
		Replication.BumpMe();
		// The server is the owner of a hosted player's body and of test characters.
		LF_OnRestrainedReplicated();
	}

	//------------------------------------------------------------------------------------------------
	bool LF_IsRestrained()
	{
		return m_bLF_Restrained;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only: the cuffs on the wrist open (the release hold) or shut again.
	void LF_SetCuffsOpen(bool open)
	{
		if (!Replication.IsServer() || open == m_bLF_CuffsOpen)
			return;

		m_bLF_CuffsOpen = open;
		Replication.BumpMe();
		LF_OnCuffsOpenReplicated();
	}

	//------------------------------------------------------------------------------------------------
	bool LF_AreCuffsOpen()
	{
		return m_bLF_CuffsOpen;
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_OnCuffsOpenReplicated()
	{
		LF_ShowWornOpen(false);
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_OnRestrainedReplicated()
	{
		LF_ShowWornCuffs(m_bLF_Restrained);

		RplComponent rpl = RplComponent.Cast(FindComponent(RplComponent));
		if (!rpl || !rpl.IsOwner())
			return;

		CharacterControllerComponent controller = GetCharacterController();
		if (!controller)
			return;

		// Weapon input is read on the owner. Cuffed hands hold nothing.
		controller.SetDisableWeaponControls(m_bLF_Restrained);
		if (m_bLF_Restrained)
		{
			controller.SelectWeapon(null);
			SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(this);
			if (gadgets && gadgets.GetHeldGadget())
				gadgets.RemoveHeldGadget();
		}

		LF_Debug.Info(LF_Restraint.FEATURE, string.Format("restrained flag %1 reached the owner: weapon controls disabled %2", m_bLF_Restrained, controller.GetDisableWeaponControls()));
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine with a renderer: the handcuffs model on the right wrist while cuffed.
	protected void LF_ShowWornCuffs(bool show)
	{
		if (!show)
		{
			if (m_LF_WornCuffs)
				SCR_EntityHelper.DeleteEntityAndChildren(m_LF_WornCuffs);

			m_LF_WornCuffs = null;
			m_bLF_WornShownOpen = false;
			return;
		}

		if (m_LF_WornCuffs || System.IsConsoleApp())
			return;

		Animation anim = GetAnimation();
		if (!anim)
			return;

		TNodeId bone = anim.GetBoneIndex(LF_Restraint.WORN_BONE);
		if (bone < 0)
		{
			LF_Debug.Warn(LF_Restraint.FEATURE, "worn handcuffs not shown: no bone " + LF_Restraint.WORN_BONE);
			return;
		}

		Resource resource = Resource.Load(LF_Restraint.WORN_PREFAB);
		if (!resource || !resource.IsValid())
		{
			LF_Debug.Error(LF_Restraint.FEATURE, "worn handcuffs prefab does not resolve: " + LF_Restraint.WORN_PREFAB);
			return;
		}

		// The vanilla loiter props' way (SCR_LoiterItemData.SpawnPrefabs): spawn at the character, attach
		// with the default AUTO_TRANSFORM, then set the transform in the bone's space. Spawned at the world
		// origin and attached with NONE, the model stayed at the world origin (session log 2026-11-13).
		EntitySpawnParams params();
		GetTransform(params.Transform);
		m_LF_WornCuffs = GetGame().SpawnEntityPrefabLocal(resource, GetWorld(), params);
		if (!m_LF_WornCuffs)
			return;

		AddChild(m_LF_WornCuffs, bone);
		vector local[4];
		Math3D.AnglesToMatrix(Vector(0, LF_Restraint.WORN_PITCH, 0), local);
		vector ring = Vector(0, 0, LF_Restraint.WORN_RING_Z);
		local[3] = Vector(0, 0, LF_Restraint.WORN_WRIST_Z) - ring.Multiply3(local);
		m_LF_WornCuffs.SetLocalTransform(local);
		vector hand[4];
		anim.GetBoneMatrix(bone, hand);
		vector worn[4];
		m_LF_WornCuffs.GetWorldTransform(worn);
		LF_Debug.Info(LF_Restraint.FEATURE, string.Format("worn handcuffs at %1, the hand at %2", worn[3], CoordToParent(hand[3])));
		// Joined, or woke up, while the release hold is running: show them open at once.
		m_bLF_WornShownOpen = false;
		LF_ShowWornOpen(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Every machine with a renderer: swing the worn cuffs' strands open or shut to match the replicated
	//! flag, with the click. `snap` skips the swing (a model that has only just appeared).
	protected void LF_ShowWornOpen(bool snap)
	{
		if (!m_LF_WornCuffs || m_bLF_CuffsOpen == m_bLF_WornShownOpen)
			return;

		LF_AssetAnimComponent anim = LF_AssetAnimComponent.Of(m_LF_WornCuffs);
		if (!anim)
		{
			LF_Debug.Error(LF_Restraint.FEATURE, "the worn handcuffs have no LF_AssetAnimComponent: they cannot open");
			return;
		}

		m_bLF_WornShownOpen = m_bLF_CuffsOpen;
		anim.ShowLocal(m_bLF_CuffsOpen, snap);
		if (!snap)
			LF_Restraint.PlayClick(m_LF_WornCuffs, m_bLF_CuffsOpen);

		LF_Debug.Info(LF_Restraint.FEATURE, string.Format("worn handcuffs shown open=%1 on %2", m_bLF_CuffsOpen, LF_Utils.GetPlayerUID(this)));
	}
}
