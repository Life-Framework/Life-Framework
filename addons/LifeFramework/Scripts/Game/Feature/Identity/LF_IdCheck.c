//------------------------------------------------------------------------------------------------
//! Showing the identity card to someone face to face (#902), and the police asking for it.
//!   Show ID    : LF_ShowIDAction on the other player's character; ShowTo below.
//!   Request ID : the `request_id` slot of the player interaction menu, offered to an on-duty
//!                officer. The server asks the citizen (a small sheet on their screen,
//!                LF_IdRequestSheet); Show holds the card up to the officer (ShowTo), Refuse tells
//!                the officer. An unanswered request lapses after REQUEST_SECONDS.
//! Server-authoritative: the client only names its answer; the server knows who asked whom.
//------------------------------------------------------------------------------------------------
class LF_IdCheck
{
	static const string SLOT = "request_id";
	//! An officer's request stays open this long: the officer is standing there waiting.
	static const float REQUEST_SECONDS = 30;	// lf-fixed: the client sheet lapses on the same clock

	//! Citizen player id -> the officer player id who asked, and when (world seconds).
	protected static ref map<int, int> s_mOfficer;
	protected static ref map<int, float> s_mAskedAt;

	//------------------------------------------------------------------------------------------------
	//! Server: shower holds the card they carry up to viewer, whose screen shows it for
	//! LF_PhotoIdCardConfig.ShowSeconds(). An on-duty officer who reads a wanted player's own card
	//! (valid or replaced: it is still their face) now knows who they are (the wanted posters name them).
	//! \return True when the card was shown.
	static bool ShowTo(IEntity shower, IEntity viewerEntity)
	{
		if (!Replication.IsServer())
			return false;

		int showerId = LF_PlayerInteraction.PlayerIdOf(shower);
		if (!shower || !viewerEntity || shower == viewerEntity)
		{
			LF_Debug.Warn(LF_PhotoIdCard.FEATURE, "show refused: no one to show the card to");
			return false;
		}

		LF_EInteractionRefusal refusal = LF_PlayerInteraction.CheckPair(shower, viewerEntity);
		if (refusal != LF_EInteractionRefusal.NONE)
		{
			LF_Debug.Info(LF_PhotoIdCard.FEATURE, "show refused: " + typename.EnumToString(LF_EInteractionRefusal, refusal));
			LF_NotificationManagerComponent.NotifyPlayer(showerId, "#EL-ID_Information", LF_PlayerInteraction.RefusalKey(refusal), 4.0, LF_ENotificationType.WARNING);
			return false;
		}

		// A card restored blank (a restart, a stash) is printed before it is chosen.
		LF_IdCard.RepairCarried(shower);
		string ownHolderId = OwnHolderId(showerId);
		LF_IdCardComponent card = LF_IdCard.FindShown(shower, ownHolderId);
		if (!card)
		{
			LF_Debug.Info(LF_PhotoIdCard.FEATURE, "show refused: player " + showerId + " carries no ID card");
			LF_NotificationManagerComponent.NotifyPlayer(showerId, "#EL-ID_Information", "#LF-ID_NoCard", 5.0, LF_ENotificationType.WARNING);
			return false;
		}

		int viewerId = LF_PlayerInteraction.PlayerIdOf(viewerEntity);
		if (viewerId <= 0 || !LF_IdCard.SendView(viewerId, card, shower, LF_PhotoIdCardConfig.ShowSeconds()))
		{
			LF_Debug.Warn(LF_PhotoIdCard.FEATURE, "show refused: the character facing player " + showerId + " is not a player");
			return false;
		}

		LF_PlayerInteraction.PlayGesture(shower, LF_PlayerInteraction.GESTURE_POINT);
		LF_PlayerInteraction.PlayHandoverSound(shower, LF_PlayerInteraction.SOUND_HANDOVER);

		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (accounts && !ownHolderId.IsEmpty() && card.GetHolderId() == ownHolderId && LF_PoliceUtils.IsOnDutyOfficer(accounts.GetFromCache(viewerId)))
			LF_WantedReports.Identify(LF_Utils.GetPlayerUID(showerId), "ID check");

		LF_NotificationManagerComponent.NotifyPlayer(showerId, "#EL-ID_Information", "#LF-ID_Shown", 4.0);
		LF_Debug.Info(LF_PhotoIdCard.FEATURE, string.Format("player %1 showed card %2 to player %3", showerId, LF_IdCard.NumberOf(card), viewerId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the persistence id of the player's active character, or empty.
	static string OwnHolderId(int playerId)
	{
		LF_PlayerAccountManager accounts = LF_PlayerAccountManager.GetInstance();
		if (!accounts)
			return string.Empty;

		LF_PlayerAccount account = accounts.GetFromCache(playerId);
		if (!account || !account.GetActiveCharacter())
			return string.Empty;

		return account.GetActiveCharacter().GetId();
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: whether a request asked at askedAt is still open at now (world seconds).
	static bool IsOpen(float askedAt, float now)
	{
		return askedAt >= 0 && now >= askedAt && now - askedAt <= REQUEST_SECONDS;
	}

	//------------------------------------------------------------------------------------------------
	protected static float Now()
	{
		if (!GetGame() || !GetGame().GetWorld())
			return 0;

		return GetGame().GetWorld().GetWorldTime() / 1000;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an on-duty officer asks the citizen in front of them for their card.
	//! \return True when the citizen was asked.
	static bool Request(IEntity officer, IEntity citizen)
	{
		if (!Replication.IsServer())
			return false;

		int officerId = LF_PlayerInteraction.PlayerIdOf(officer);
		if (!LF_PoliceUtils.IsOnDutyOfficer(LF_PoliceUtils.GetAccountForEntity(officer)))
		{
			LF_Debug.Warn(LF_PhotoIdCard.FEATURE, string.Format("ID request refused: player %1 is not an on-duty officer", officerId));
			return false;
		}

		SCR_PlayerController controller = LF_PlayerInteraction.ControllerOf(citizen);
		int citizenId = LF_PlayerInteraction.PlayerIdOf(citizen);
		if (!controller || citizenId <= 0)
		{
			LF_Debug.Warn(LF_PhotoIdCard.FEATURE, string.Format("ID request refused: player %1 asked someone who is not a player", officerId));
			return false;
		}

		if (!s_mOfficer)
		{
			s_mOfficer = new map<int, int>();
			s_mAskedAt = new map<int, float>();
		}

		// One open question at a time: the sheet is not reopened while the citizen is still asked.
		if (s_mAskedAt.Contains(citizenId) && IsOpen(s_mAskedAt.Get(citizenId), Now()))
		{
			LF_Debug.Info(LF_PhotoIdCard.FEATURE, string.Format("ID request refused: player %1 is already asked", citizenId));
			LF_NotificationManagerComponent.NotifyPlayer(officerId, "#EL-ID_Information", "#LF-IdCard_RequestWaiting", 4.0);
			return false;
		}

		s_mOfficer.Set(citizenId, officerId);
		s_mAskedAt.Set(citizenId, Now());
		controller.LF_AskForIdCard();
		LF_PlayerInteraction.PlayGesture(officer, LF_PlayerInteraction.GESTURE_HALT);
		LF_NotificationManagerComponent.NotifyPlayer(officerId, "#EL-ID_Information", "#LF-IdCard_RequestSent", 4.0);
		LF_Debug.Info(LF_PhotoIdCard.FEATURE, string.Format("officer %1 asked player %2 for their ID card", officerId, citizenId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the citizen answered the officer's request. Show holds the card up to the officer;
	//! refuse tells the officer. An answer with no open request is dropped (logged).
	static bool Answer(int citizenId, bool show)
	{
		if (!Replication.IsServer() || !s_mOfficer || !s_mOfficer.Contains(citizenId))
		{
			LF_Debug.Warn(LF_PhotoIdCard.FEATURE, string.Format("ID answer from player %1 dropped: nobody asked", citizenId));
			return false;
		}

		int officerId = s_mOfficer.Get(citizenId);
		float askedAt = s_mAskedAt.Get(citizenId);
		s_mOfficer.Remove(citizenId);
		s_mAskedAt.Remove(citizenId);
		if (!IsOpen(askedAt, Now()))
		{
			LF_Debug.Info(LF_PhotoIdCard.FEATURE, string.Format("ID answer from player %1 dropped: the request lapsed", citizenId));
			return false;
		}

		if (!show)
		{
			LF_NotificationManagerComponent.NotifyPlayer(officerId, "#EL-ID_Information", "#LF-IdCard_RequestRefused", 5.0, LF_ENotificationType.WARNING);
			LF_Debug.Info(LF_PhotoIdCard.FEATURE, string.Format("player %1 refused to show their ID card to officer %2", citizenId, officerId));
			return true;
		}

		IEntity citizen = GetGame().GetPlayerManager().GetPlayerControlledEntity(citizenId);
		IEntity officer = GetGame().GetPlayerManager().GetPlayerControlledEntity(officerId);
		if (ShowTo(citizen, officer))
			return true;

		// Willing, but no card on them or no longer in reach: the officer hears it.
		LF_NotificationManagerComponent.NotifyPlayer(officerId, "#EL-ID_Information", "#LF-IdCard_RequestNoCard", 5.0, LF_ENotificationType.WARNING);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Called from LF_StaticReset.ResetAll when a world ends.
	static void ResetStatics()
	{
		s_mOfficer = null;
		s_mAskedAt = null;
	}
}

//------------------------------------------------------------------------------------------------
//! "Request ID" in the player interaction menu, for an on-duty officer.
class LF_IdRequestRole : LF_PlayerRoleHandler
{
	//------------------------------------------------------------------------------------------------
	//! Every machine, once per world (LF_GameModeRoleplay.OnGameStart). Idempotent.
	static void Register()
	{
		if (LF_IdRequestRole.Cast(LF_PlayerInteraction.GetRole(LF_IdCheck.SLOT)))
			return;

		LF_PlayerInteraction.RegisterRole(LF_IdCheck.SLOT, new LF_IdRequestRole());
	}

	//------------------------------------------------------------------------------------------------
	override string GetActionName()
	{
		return "#LF-IdCard_Request";
	}

	//------------------------------------------------------------------------------------------------
	override bool CanOffer(IEntity user, IEntity target)
	{
		return LF_PlayerInteraction.IsOnDutyPolice(user);
	}

	//------------------------------------------------------------------------------------------------
	override void Perform(IEntity user, IEntity target)
	{
		LF_IdCheck.Request(user, target);
	}
}

//------------------------------------------------------------------------------------------------
//! Client: the officer's question on the citizen's screen. Show or Refuse goes back to the server;
//! closing the sheet without an answer refuses.
class LF_IdRequestSheet
{
	static const string SHOW = "IdRequestShowButton";
	static const string REFUSE = "IdRequestRefuseButton";

	protected static ref LF_UIScreen s_Screen;
	protected static bool s_bAnswered;

	//------------------------------------------------------------------------------------------------
	static array<ref LF_UIPart> BuildParts()
	{
		ref array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "IdRequestTitle", "#LF-IdCard_RequestTitle"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "IdRequestBody", "#LF-IdCard_RequestBody"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, SHOW, "#LF-IdCard_RequestShow"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, REFUSE, "#LF-IdCard_RequestRefuse"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Open()
	{
		if (s_Screen)
			s_Screen.Close();

		s_bAnswered = false;
		s_Screen = LF_UIScreen.Open(BuildParts(), LF_UISkin.IdCheck());
		if (!s_Screen)
		{
			LF_Debug.Error(LF_PhotoIdCard.FEATURE, "ID request sheet failed to open");
			return null;
		}

		s_Screen.CloseOnDeath();
		s_Screen.CloseWhenUncontrolled();
		s_Screen.GetOnButton().Insert(OnButton);
		s_Screen.GetOnClosed().Insert(OnClosed);
		if (GetGame().GetCallqueue())
			GetGame().GetCallqueue().CallLater(Lapse, Math.Round(LF_IdCheck.REQUEST_SECONDS * 1000), false, s_Screen);

		LF_Debug.Info(LF_PhotoIdCard.FEATURE, "ID request sheet open");
		return s_Screen;
	}

	//------------------------------------------------------------------------------------------------
	protected static void OnButton(string name)
	{
		if (name == SHOW)
			Send(true);
		else if (name == REFUSE)
			Send(false);

		if (s_Screen)
			s_Screen.Close();
	}

	//------------------------------------------------------------------------------------------------
	protected static void OnClosed()
	{
		if (!s_bAnswered)
			Send(false);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Lapse(LF_UIScreen screen)
	{
		if (screen && screen == s_Screen && !s_bAnswered)
		{
			s_bAnswered = true;
			screen.Close();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void Send(bool show)
	{
		if (s_bAnswered)
			return;

		s_bAnswered = true;
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller)
		{
			LF_Debug.Error(LF_PhotoIdCard.FEATURE, "ID answer not sent: no player controller");
			return;
		}

		controller.LF_AnswerIdRequest(show);
		LF_Debug.Info(LF_PhotoIdCard.FEATURE, string.Format("ID request answered: show %1", show));
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Current()
	{
		return s_Screen;
	}
}
