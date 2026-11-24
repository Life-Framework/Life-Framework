//------------------------------------------------------------------------------------------------
//! Session proof verb for the felling hold (#4833): `fellhold [aim]`. Round 5 could not reach the
//! "Fell tree" prompt from the harness: the prompt is a client-local helper (LF_FellPrompt) that the
//! engine's interaction ray never selects (`aim none`), and `use LF_FellAction` finds no host. This
//! verb takes the path the F key takes once the ray has selected the row: the same helper, the same
//! action object, its own CanBeShown / CanBePerformed checks, and PerformAction, which asks the server
//! (LF_AskFell -> TryFell) to re-check every rule. Nothing is forged: a missing axe, a tree out of
//! reach or a refusal says so in the reply.
//!
//!   fellhold aim   the prompt state only: the row's label and the tree it sits on
//!   fellhold       the hold's completion: PerformAction on the prompt (the server fells)
//!
//! Reply: `fellhold aim label=<row text> tree=<key> shown=<0|1> perform=<0|1>`; the hold adds
//! ` asked` and the server logs `[LFDebug:Lumber] felled <key> (N logs, <class>)`.
//! Errors: no_character, no_axe (not in hand), no_tree (nothing fellable within reach),
//! cannot_show, cannot_perform.
//------------------------------------------------------------------------------------------------
class LF_LumberHarness
{
	//------------------------------------------------------------------------------------------------
	//! ` right=<prefab|none> left=<prefab|none> weapon=<prefab|none>`: what the hands hold, the facts HoldsAxe reads.
	protected static string HandsNote(IEntity user)
	{
		string right = "none";
		string left = "none";
		string weapon = "none";
		CharacterControllerComponent controller = LF_Component<CharacterControllerComponent>.Find(user);
		if (controller)
		{
			right = NameOf(controller.GetRightHandItem());
			left = NameOf(controller.GetAttachedGadgetAtLeftHandSlot());
			BaseWeaponManagerComponent weapons = controller.GetWeaponManagerComponent();
			if (weapons && weapons.GetCurrentWeapon())
				weapon = NameOf(weapons.GetCurrentWeapon().GetOwner());
		}

		return string.Format("right=%1 left=%2 weapon=%3", right, left, weapon);
	}

	//------------------------------------------------------------------------------------------------
	protected static string NameOf(IEntity entity)
	{
		if (!entity)
			return "none";

		EntityPrefabData prefab = entity.GetPrefabData();
		if (!prefab)
			return "noprefab";

		return FilePath.StripPath(prefab.GetPrefabName());
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the answer to `fellhold`; \p performIt completes the hold, otherwise it only reads the prompt.
	static void Run(int commandId, bool performIt)
	{
		IEntity user = LF_Utils.GetLocalCharacter();
		if (!user)
		{
			LF_AgentHarness.Answer(commandId, "error", "no_character");
			return;
		}

		if (!LF_FellAction.HoldsAxe(user))
		{
			LF_AgentHarness.Answer(commandId, "error", "no_axe " + HandsNote(user));
			return;
		}

		// The prompt loop re-reads every 300 ms; read the surroundings now so a proof that just walked up does not race it.
		LF_FellPrompt.Refresh(user, true);
		IEntity tree = LF_FellPrompt.Target();
		LF_FellAction action = LF_FellPrompt.Action();
		if (!tree || !action)
		{
			LF_AgentHarness.Answer(commandId, "error", "no_tree");
			return;
		}

		string label;
		action.GetActionNameScript(label);
		string key = LF_FellAction.KeyOfTree(tree);
		bool shown = action.CanBeShownScript(user);
		bool canPerform = action.CanBePerformedScript(user);
		string state = string.Format("fellhold aim label=%1 tree=%2 shown=%3 perform=%4", label, key, shown, canPerform);
		if (!performIt)
		{
			LF_AgentHarness.Answer(commandId, "ok", state);
			return;
		}

		if (!shown)
		{
			LF_AgentHarness.Answer(commandId, "error", "cannot_show");
			return;
		}

		if (!canPerform)
		{
			LF_AgentHarness.Answer(commandId, "error", "cannot_perform");
			return;
		}

		LF_Debug.Info(LF_FellPrompt.FEATURE, string.Format("harness fellhold: hold completed on %1", key));
		action.PerformAction(tree, user);
		LF_AgentHarness.Answer(commandId, "ok", state + " asked");
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "fellhold")
			return super.Handle(cmd);

		LF_LumberHarness.Run(cmd.Id, cmd.Arg1 != "aim");
		return true;
	}
}
