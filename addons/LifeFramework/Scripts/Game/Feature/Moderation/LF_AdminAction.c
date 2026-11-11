//------------------------------------------------------------------------------------------------
//! Admin actions are logged too (#246, epic #145 Moderation). Every Game Master editor action
//! (teleport, heal, spawn, delete, whitelist, kick), the vanilla #kick and #ban chat commands, and
//! the admin chat commands !wadd and !handled publish LF_OnAdminAction on the server; the incident
//! log records kind "admin" (subject the admin, other the target when there is one, how the
//! action). Only the Game Master incident view shows it, and that view answers administrators only.
//! Server only. A log failure never blocks the action.
//------------------------------------------------------------------------------------------------
class LF_AdminAction
{
	static const string FEATURE = "AdminAction";

	//------------------------------------------------------------------------------------------------
	//! Server: one admin action. Empty adminUid is refused (logged), never thrown.
	static void Report(string adminUid, string targetUid, string action, vector position)
	{
		if (!Replication.IsServer())
			return;

		if (adminUid.IsEmpty())
		{
			LF_Debug.Warn(FEATURE, string.Format("admin action '%1' has no admin account id; not logged", action));
			return;
		}

		LF_Debug.Info(FEATURE, string.Format("admin %1: %2 -> %3", adminUid, action, targetUid));
		LF_EventBus.Publish(LF_OnAdminAction.Create(adminUid, targetUid, action, position));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the admin chat command !wadd TYPE UID.
	static void ReportWhitelistAdd(string adminUid, LF_WhitelistType type, string targetUid)
	{
		Report(adminUid, targetUid, "!wadd " + SCR_Enum.GetEnumName(LF_WhitelistType, type), vector.Zero);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the admin chat command !handled NUMBER.
	static void ReportHandled(string adminUid, int number)
	{
		Report(adminUid, "", string.Format("!handled %1", number), vector.Zero);
	}

	//------------------------------------------------------------------------------------------------
	static string UidOfPlayer(int playerId)
	{
		if (playerId <= 0)
			return "";

		return LF_Utils.GetAccountUID(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: log a vanilla chat command. The target is the first argument when it is a connected
	//! player id; the whole argument text is kept as the detail.
	static void ReportCommand(string keyword, array<string> argv, int adminId)
	{
		string detail = keyword;
		string targetUid;
		if (argv)
		{
			for (int i = 1; i < argv.Count(); i++)
			{
				detail = detail + " " + argv[i];
				if (i == 1 && argv[i].ToInt() > 0)
					targetUid = UidOfPlayer(argv[i].ToInt());
			}
		}

		Report(UidOfPlayer(adminId), targetUid, detail, vector.Zero);
	}
}

//------------------------------------------------------------------------------------------------
//! Every server-side Game Master editor action (teleport, heal, spawn, delete, whitelist, kick...).
modded class SCR_BaseActionsEditorComponent
{
	//------------------------------------------------------------------------------------------------
	override protected void ActionPerform(SCR_BaseEditorAction action, SCR_EditableEntityComponent hoveredEntityComponent, set<SCR_EditableEntityComponent> selectedEntityComponents, vector cursorWorldPosition, int flags, int param)
	{
		super.ActionPerform(action, hoveredEntityComponent, selectedEntityComponents, cursorWorldPosition, flags, param);

		if (!Replication.IsServer() || !action)
			return;

		SCR_EditorManagerEntity editorManager = GetManager();
		if (!editorManager)
			return;

		string name = action.ToString();
		SCR_UIInfo info = action.GetInfo();
		if (info)
			name = info.GetName();

		IEntity target;
		if (hoveredEntityComponent)
			target = hoveredEntityComponent.GetOwner();
		else if (selectedEntityComponents && selectedEntityComponents.Count() > 0 && selectedEntityComponents[0])
			target = selectedEntityComponents[0].GetOwner();

		string targetUid;
		if (target)
			targetUid = LF_Utils.GetAccountUID(target);

		LF_AdminAction.Report(LF_AdminAction.UidOfPlayer(editorManager.GetPlayerID()), targetUid, name, cursorWorldPosition);
	}
}

//------------------------------------------------------------------------------------------------
//! The vanilla #kick chat command.
modded class KickCommand
{
	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		ref ScrServerCmdResult result = super.OnChatServerExecution(argv, playerId);
		LF_AdminAction.ReportCommand("#kick", argv, playerId);
		return result;
	}
}

//------------------------------------------------------------------------------------------------
//! The vanilla #ban chat command.
modded class BanCommand
{
	//------------------------------------------------------------------------------------------------
	override ref ScrServerCmdResult OnChatServerExecution(array<string> argv, int playerId)
	{
		ref ScrServerCmdResult result = super.OnChatServerExecution(argv, playerId);
		LF_AdminAction.ReportCommand("#ban", argv, playerId);
		return result;
	}
}
