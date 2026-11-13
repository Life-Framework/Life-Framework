// LF_PoliceApplicationForm.c - the police application form at the station desk.
// Client-only, on LF_UIScreen: three blanks (name, why, availability), Submit and Close. The checks
// here only write the error line; the server re-checks (LF_PoliceApplications.Submit).

class LF_PoliceApplicationForm
{
	static const string PART_NAME = "PolAppName";
	static const string PART_WHY = "PolAppWhy";
	static const string PART_AVAILABILITY = "PolAppAvailability";
	static const string PART_ERROR = "PolAppError";
	static const string PART_SUBMIT = "PolAppSubmit";

	protected static ref LF_PoliceApplicationForm s_Form;

	protected ref LF_UIScreen m_Screen;
	protected SCR_PlayerController m_Controller;

	//------------------------------------------------------------------------------------------------
	static array<ref LF_UIPart> BuildParts()
	{
		ref array<ref LF_UIPart> parts = {};
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.TITLE, "PolAppTitle", "#LF-PoliceApp_Title"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, "PolAppIntro", "#LF-PoliceApp_Intro"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.FIELD, PART_NAME, "#LF-PoliceApp_Name"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.FIELD, PART_WHY, "#LF-PoliceApp_Why"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.FIELD, PART_AVAILABILITY, "#LF-PoliceApp_Availability"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.LINE, PART_ERROR, ""));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.BUTTON, PART_SUBMIT, "#LF-PoliceApp_Submit"));
		parts.Insert(LF_UIPart.Create(LF_EUIPartKind.CLOSE, "PolAppClose", "#LF-PoliceApp_Close"));
		return parts;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Open(SCR_PlayerController controller)
	{
		Hide();
		if (!controller)
		{
			LF_Debug.Error("Police", "application form refused: no player controller");
			return null;
		}

		LF_PoliceApplicationForm form = new LF_PoliceApplicationForm();
		form.m_Controller = controller;
		form.m_Screen = LF_UIScreen.Open(BuildParts());
		if (!form.m_Screen)
		{
			LF_Debug.Error("Police", "application form failed to open");
			return null;
		}

		form.m_Screen.CloseOnDeath();
		form.m_Screen.GetOnButton().Insert(form.OnButton);
		s_Form = form;
		LF_Debug.Info("Police", "application form open");
		return form.m_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static LF_UIScreen Current()
	{
		if (!s_Form)
			return null;

		return s_Form.m_Screen;
	}

	//------------------------------------------------------------------------------------------------
	static void Hide()
	{
		if (!s_Form)
			return;

		LF_PoliceApplicationForm form = s_Form;
		s_Form = null;
		if (form.m_Screen)
			form.m_Screen.Close();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnButton(string name)
	{
		if (name != PART_SUBMIT || !m_Screen)
			return;

		string applicant = m_Screen.GetFieldText(PART_NAME).Trim();
		string why = m_Screen.GetFieldText(PART_WHY).Trim();
		string availability = m_Screen.GetFieldText(PART_AVAILABILITY).Trim();

		string refusal = LF_PoliceApplicationRules.RefuseForm(applicant, why, availability);
		if (!refusal.IsEmpty())
		{
			m_Screen.SetLine(PART_ERROR, refusal);
			return;
		}

		if (!m_Controller)
		{
			LF_Debug.Error("Police", "application not sent: no player controller");
			return;
		}

		SCR_PlayerController controller = m_Controller;
		Hide();
		controller.LF_AskPoliceApplication(applicant, why, availability);
	}
}

//------------------------------------------------------------------------------------------------
//! Client <-> server bridge for the application, on the PlayerController (owned by the client).
//!   owner -> server : LF_AskPoliceApplication, LF_AskPoliceReview (RpcAsk_*)
//!   server -> owner : LF_ShowPoliceApplicationForm (RpcDo_*)
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	//! Server: open the form on this player's machine.
	void LF_ShowPoliceApplicationForm()
	{
		if (GetGame().GetPlayerController() == this)
			RpcDo_LF_ShowPoliceApplicationForm();
		else
			Rpc(RpcDo_LF_ShowPoliceApplicationForm);
	}

	//------------------------------------------------------------------------------------------------
	void LF_AskPoliceApplication(string applicant, string why, string availability)
	{
		if (Replication.IsServer())
			RpcAsk_LF_PoliceApplication(applicant, why, availability);
		else
			Rpc(RpcAsk_LF_PoliceApplication, applicant, why, availability);
	}

	//------------------------------------------------------------------------------------------------
	void LF_AskPoliceReview()
	{
		if (Replication.IsServer())
			RpcAsk_LF_PoliceReview();
		else
			Rpc(RpcAsk_LF_PoliceReview);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_ShowPoliceApplicationForm()
	{
		if (!GetGame().GetWorkspace())
		{
			LF_Debug.Error("Police", "application form not opened: no workspace on this machine");
			return;
		}

		LF_PoliceApplicationForm.Open(this);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the player must stand at a desk that carries the apply action.
	protected bool LF_AtApplicationDesk(IEntity user)
	{
		if (!user)
			return false;

		return LF_ActionSite.FindNear(user.GetOrigin(), LF_PoliceConfig.ApplyDeskRange(), "LF_PoliceApplyAction") != null;
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_PoliceApplication(string applicant, string why, string availability)
	{
		IEntity user = GetControlledEntity();
		if (!LF_AtApplicationDesk(user))
		{
			LF_Debug.Warn("Police", string.Format("application from player %1 dropped: not at the duty desk", GetPlayerId()));
			return;
		}

		string uid = LF_PoliceApplications.ApplicantUid(GetPlayerId());
		bool onRoster = LF_WhitelistManager.IsWhitelistedForFaction(uid, LF_EFactionType.POLICE);
		string refusal = LF_PoliceApplications.Submit(uid, applicant, why, availability, onRoster);
		if (!refusal.IsEmpty())
		{
			LF_Utils.NotifyEntity(user, refusal, "#LF-PoliceApp_Status", 4.0);
			return;
		}

		LF_Utils.NotifyEntity(user, "#LF-PoliceApp_Receipt", "#LF-PoliceApp_Status", 5.0);

		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		if (header && header.m_bLF_InstantPoliceApproval && LF_PoliceApplications.ApproveInstant(uid))
			LF_Utils.NotifyEntity(user, "#LF-PoliceApp_Approved", "#LF-PoliceApp_Status", 5.0);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: an administrator at the desk approves the application waiting longest.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_PoliceReview()
	{
		IEntity user = GetControlledEntity();
		if (!LF_AtApplicationDesk(user))
		{
			LF_Debug.Warn("Police", string.Format("application review from player %1 dropped: not at the duty desk", GetPlayerId()));
			return;
		}

		bool isAdmin = AdminChatChannel.IsAdminPlayer(GetPlayerId());
		LF_PoliceApplication app = LF_PoliceApplications.FindOldestOpen();
		if (!app)
		{
			LF_Utils.NotifyEntity(user, "#LF-PoliceApp_NoneOpen", "#LF-PoliceApp_Status", 4.0);
			return;
		}

		if (!LF_PoliceApplications.Approve(app.m_sUid, isAdmin))
		{
			LF_Utils.NotifyEntity(user, "#LF-PoliceApp_NotAdmin", "#LF-PoliceApp_Status", 4.0);
			return;
		}

		LF_Utils.NotifyEntity(user, "#LF-PoliceApp_ApprovedOther", "#LF-PoliceApp_Status", 5.0, app.m_sName);
	}
}
