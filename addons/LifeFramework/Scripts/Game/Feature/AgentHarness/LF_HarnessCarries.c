// Harness verb `carries <prefab text>` (#2935): does this client's character carry an item whose prefab
// path contains the text (case-insensitive)? `ok carried=<file>` or `error not_carried items=<n>`. The
// flow assert `expect item <prefab text>` (agent-box/harness.mjs) reads it after a purchase. Read-only.

//------------------------------------------------------------------------------------------------
class LF_HarnessCarries
{
	//! The file name of the first carried item whose prefab path contains the text, "" when none does.
	static string FindCarried(IEntity character, string text, out int itemCount)
	{
		itemCount = 0;
		if (!character || text.IsEmpty())
			return "";

		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(character.FindComponent(SCR_InventoryStorageManagerComponent));
		if (!inventory)
			return "";

		array<IEntity> items = {};
		inventory.GetItems(items);
		itemCount = items.Count();
		string wanted = text;
		wanted.ToLower();
		foreach (IEntity item : items)
		{
			if (!item || !item.GetPrefabData())
				continue;

			string prefab = item.GetPrefabData().GetPrefabName();
			string lower = prefab;
			lower.ToLower();
			if (lower.Contains(wanted))
				return FilePath.StripPath(prefab);
		}

		return "";
	}
}

//------------------------------------------------------------------------------------------------
modded class LF_AgentHarnessVerbs
{
	override bool Handle(LF_AgentCommand cmd)
	{
		if (cmd.Verb != "carries")
			return super.Handle(cmd);

		SCR_ChimeraCharacter character = LF_Utils.GetLocalCharacter();
		if (!character || cmd.Arg1.IsEmpty())
		{
			LF_AgentHarness.Answer(cmd.Id, "error", "usage: carries <prefab text> (needs a character)");
			return true;
		}

		int count;
		string found = LF_HarnessCarries.FindCarried(character, cmd.Arg1, count);
		if (found.IsEmpty())
		{
			LF_Debug.Info("AgentHarness", string.Format("carries %1: not carried (%2 items)", cmd.Arg1, count));
			LF_AgentHarness.Answer(cmd.Id, "error", "not_carried items=" + count.ToString());
			return true;
		}

		LF_AgentHarness.Answer(cmd.Id, "ok", "carried=" + found);
		return true;
	}
}
