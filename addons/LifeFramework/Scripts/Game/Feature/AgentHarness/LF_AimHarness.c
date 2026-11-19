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
class LF_AimHarness
{
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
			LF_AgentHarness.Answer(id, "ok", "aim none");
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

		LF_AgentHarness.Answer(id, "ok", "aim selected=" + DescribeAction(handler.GetSelectedAction()) + " context=" + context.GetContextName() + " (" + contextOwner + ") offered=" + offered);
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
