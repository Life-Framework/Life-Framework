//------------------------------------------------------------------------------------------------
//! Harness `aim` (client, read-only): what the interact key would act on right now.
//!
//! `look` lists every ScriptedUserAction within 4 m of the player whose CanBeShownScript is true. That is not what
//! the F key offers: the engine's interaction handler collects the actions of the one context the aim ray and its
//! sphere query end on, and a context whose entity the query does not find (a mesh-less entity with its context metres
//! away, a context behind a collider) is listed by `look` and never offered to a player. On 2026-11-19 the clothing
//! store's till was exactly that, and every harness check passed.
//!
//! Reply: `aim selected=<Class>@<entity> context=<name>@<entity> offered=<Class>@<entity>:<1|0>,...`, or `aim none`
//! when the handler has no context. A proof stands beside the thing (`stand`), reads `aim`, and fails when the action
//! it expects is not the selected or an offered one (the `:1` marks an action that can be performed now).
//! The reply ends with ` name=<label>` (the selected row's own label, so a proof can tell two rows of one class apart)
//! and ` menu=<top menu class|none>`: the engine offers no context at all while any menu or dialog is open (#4791).
//! `aim none` also ends with ` camera=<class> pitch=<deg> hit=<entity>@<m>`: where the view points and what it meets
//! first. On #4803 that showed the suspect staring at the floor (pitch -80) after a lost relative turn.
class LF_AimHarness
{
	//! Steepest pitch `PitchTo` sets, in degrees; the character's own view stops near here.
	static const float PITCH_LIMIT_DEG = 85; // lf-fixed: harness view limit, not a gameplay value

	//------------------------------------------------------------------------------------------------
	//! Aiming angles (radians: yaw, pitch, roll) with the pitch set to `degrees`, clamped to the limit.
	//! Absolute, so sending it twice, late, or after a lost relative turn lands on the same view (#4803).
	static vector PitchTo(vector aim, float degrees)
	{
		vector result = aim;
		result[1] = Math.Clamp(degrees, -PITCH_LIMIT_DEG, PITCH_LIMIT_DEG) * Math.DEG2RAD; // lf-float: a view angle
		return result;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Describe(string kind, IEntity entity)
	{
		string name;
		if (entity)
			name = entity.GetName();

		if (name.IsEmpty() && entity)
		{
			EntityPrefabData prefab = entity.GetPrefabData();
			if (prefab)
				name = FilePath.StripPath(prefab.GetPrefabName());
		}

		if (name.IsEmpty())
			return kind;

		return kind + "@" + name;
	}

	//------------------------------------------------------------------------------------------------
	protected static string DescribeAction(BaseUserAction action)
	{
		if (!action)
			return "none";

		IEntity owner;
		ScriptedUserAction scripted = ScriptedUserAction.Cast(action);
		if (scripted)
			owner = scripted.GetOwner();

		return Describe(action.Type().ToString(), owner);
	}

	//------------------------------------------------------------------------------------------------
	static void Report(int id)
	{
		PlayerController controller = GetGame().GetPlayerController();
		if (!controller)
		{
			LF_AgentHarness.Answer(id, "error", "no_controller");
			return;
		}

		SCR_InteractionHandlerComponent handler = SCR_InteractionHandlerComponent.Cast(controller.FindComponent(SCR_InteractionHandlerComponent));
		if (!handler)
		{
			IEntity controlled = controller.GetControlledEntity();
			if (controlled)
				handler = SCR_InteractionHandlerComponent.Cast(controlled.FindComponent(SCR_InteractionHandlerComponent));
		}

		if (!handler)
		{
			LF_AgentHarness.Answer(id, "error", "no_interaction");
			return;
		}

		UserActionContext context = handler.GetCurrentContext();
		if (!context)
		{
			LF_AgentHarness.Answer(id, "ok", "aim none" + MenuNote() + CharacterNote() + TraceNote());
			return;
		}

		array<BaseUserAction> actions = {};
		array<bool> canPerform = {};
		handler.GetFilteredActions(actions, canPerform);

		string offered;
		foreach (int i, BaseUserAction action : actions)
		{
			if (!offered.IsEmpty())
				offered = offered + ",";

			string mark = "0";
			if (i < canPerform.Count() && canPerform[i])
				mark = "1";

			offered = offered + DescribeAction(action) + ":" + mark;
		}

		if (offered.IsEmpty())
			offered = "none";

		string contextOwner = "";
		if (actions.Count() > 0)
			contextOwner = DescribeAction(actions[0]);

		string label = "none";
		BaseUserAction selected = handler.GetSelectedAction();
		if (selected)
			label = selected.GetActionName();

		LF_AgentHarness.Answer(id, "ok", "aim selected=" + DescribeAction(selected) + " context=" + context.GetContextName() + " (" + contextOwner + ") offered=" + offered + " name=" + label + MenuNote() + CharacterNote());
	}

	//------------------------------------------------------------------------------------------------
	//! ` camera=<class> pitch=<deg> hit=<entity>@<m> hitparent=<entity>`: what the view ray from the current
	//! camera meets first within 3 m, the character excluded. The handler's sphere cast starts the same way,
	//! so a blank handler with every gate open names its blocker here.
	protected static string TraceNote()
	{
		CameraManager cameras = GetGame().GetCameraManager();
		CameraBase camera;
		if (cameras)
			camera = cameras.CurrentCamera();

		if (!camera)
			return " camera=none";

		vector mat[4];
		camera.GetWorldTransform(mat);
		vector dir = mat[2];
		float pitch = Math.Asin(dir[1]) * Math.RAD2DEG; // lf-float: a view angle in degrees
		string note = string.Format(" camera=%1 pitch=%2", camera.Type().ToString(), Math.Round(pitch));

		TraceParam trace = new TraceParam();
		trace.Start = mat[3];
		trace.End = mat[3] + dir * 3;
		trace.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
		trace.Exclude = LF_Utils.GetLocalCharacter();
		trace.LayerMask = -1;
		float fraction = camera.GetWorld().TraceMove(trace, null);
		if (fraction >= 1 || !trace.TraceEnt)
			return note + " hit=none";

		note = note + string.Format(" hit=%1@%2", Describe("e", trace.TraceEnt), (fraction * 3).ToString(4, 2));
		IEntity parent = trace.TraceEnt.GetParent();
		if (parent)
			note = note + " hitparent=" + Describe("e", parent);

		return note;
	}

	//------------------------------------------------------------------------------------------------
	//! ` menu=<class>` for the top open menu or dialog, else ` menu=none`.
	protected static string MenuNote()
	{
		MenuManager menus = GetGame().GetMenuManager();
		if (!menus || (!menus.IsAnyMenuOpen() && !menus.IsAnyDialogOpen()))
			return " menu=none";

		MenuBase top = menus.GetTopMenu();
		if (!top)
			return " menu=dialog";

		return " menu=" + top.Type().ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! ` caninteract=<0|1> using=<0|1> weapon=<prefab|none>`: the character's own interaction gate
	//! (SCR_CharacterControllerComponent.CanInteract), which also empties the handler.
	protected static string CharacterNote()
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(LF_Utils.GetLocalCharacter());
		if (!character)
			return " caninteract=nocharacter";

		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(character.GetCharacterController());
		if (!controller)
			return " caninteract=nocontroller";

		string weapon = "none";
		BaseWeaponManagerComponent weapons = controller.GetWeaponManagerComponent();
		if (weapons && weapons.GetCurrentWeapon() && weapons.GetCurrentWeapon().GetOwner())
		{
			EntityPrefabData prefab = weapons.GetCurrentWeapon().GetOwner().GetPrefabData();
			if (prefab)
				weapon = FilePath.StripPath(prefab.GetPrefabName());
		}

		return string.Format(" caninteract=%1 using=%2 weapon=%3", controller.CanInteract(), controller.IsUsingItem(), weapon);
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "aim")
			return super.Handle(cmd);

		LF_AimHarness.Report(cmd.Id);
		return true;
	}
}
