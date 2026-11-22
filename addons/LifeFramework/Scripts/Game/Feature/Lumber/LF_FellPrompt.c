//------------------------------------------------------------------------------------------------
//! The "Fell tree" prompt (#3554, design docs/roadmap/lumber.md "Felling a real map tree"). Map trees
//! carry no actions, so a small helper entity (Prefabs/Lumber/LF_FellHelper.et, the vanilla foliage
//! helper's pattern) carries LF_FellAction and moves to the nearest standing fellable tree while the
//! local player holds an axe. The helper is local to that player's machine and has no RplComponent: it
//! is only a place for the hold prompt. When the hold completes the machine asks the server
//! (SCR_PlayerController.LF_AskFell), which finds the tree itself and re-checks every rule in TryFell.
//! Nothing here runs on a dedicated server (it has no local player).
//------------------------------------------------------------------------------------------------
class LF_FellPrompt
{
	static const string FEATURE = "Lumber";
	//! How often the local player's surroundings are re-read, in ms.
	static const int TICK_MS = 300; // lf-fixed: client poll period of the prompt, not a design number
	//! Height above the tree's foot the helper sits (the action context's centre), in metres.
	static const float HELPER_HEIGHT_M = 1.3;
	//! Metres the server looks round a position a client names, to find that exact map tree.
	static const float RESOLVE_M = 1.0;

	protected static bool s_bRunning;
	protected static IEntity s_Helper;
	protected static IEntity s_Target;

	//------------------------------------------------------------------------------------------------
	//! Starts the local player's prompt loop once. Called when the local player gets a character.
	static void Start()
	{
		if (s_bRunning)
			return;

		s_bRunning = true;
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
		LF_Debug.Info(FEATURE, "fell prompt started");
	}

	//------------------------------------------------------------------------------------------------
	//! True when `user` is the character this machine's player controls.
	static bool IsLocalUser(IEntity user)
	{
		if (!user)
			return false;

		PlayerController controller = GetGame().GetPlayerController();
		return controller && controller.GetControlledEntity() == user;
	}

	//------------------------------------------------------------------------------------------------
	//! True when a tree of this prefab can be felled at all (a species with a yield row and a size of 1
	//! or more). The server decides for real; this only keeps the prompt off shrubs and saplings.
	static bool IsFellable(IEntity tree, LF_TreeFellingConfig config)
	{
		if (!tree || !config || !Tree.Cast(tree))
			return false;

		ResourceName prefab = LF_FellAction.TreePrefabOf(tree);
		if (prefab.IsEmpty())
			return false;

		LF_TreeKind kind = LF_TreeFelling.KindOf(prefab, config);
		if (kind.m_eClass == LF_TreeWoodClass.NONE || kind.m_bExcluded || kind.m_iSize < 1)
			return false;

		return LF_TreeFelling.YieldFor(prefab, config) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! The nearest standing fellable map tree within `radius` of `from`, or null. A felled (hidden)
	//! tree and a shrub do not count, so a bush beside a pine never takes the prompt.
	static IEntity NearestFellable(vector from, float radius, LF_TreeFellingConfig config)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world || !config)
			return null;

		LF_FellQuery query = new LF_FellQuery();
		world.QueryEntitiesBySphere(from, radius, query.Collect, null, EQueryEntitiesFlags.ALL);
		IEntity best;
		float bestGap = float.MAX;
		foreach (IEntity entity : query.m_aHits)
		{
			if (LF_FelledTrees.IsHidden(entity) || !IsFellable(entity, config))
				continue;

			float gap = vector.Distance(entity.GetOrigin(), from);
			if (gap < bestGap)
			{
				bestGap = gap;
				best = entity;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! The server's half of an ask: the standing fellable map tree at `at` (within RESOLVE_M), or null.
	//! The client names a position, never an entity, so a forged ask can only name a real tree.
	static IEntity ResolveTree(vector at)
	{
		return NearestFellable(at, RESOLVE_M, LF_TreeFellingConfig.Get());
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tick()
	{
		IEntity user = SCR_PlayerController.GetLocalControlledEntity();
		bool usable = user && !CompartmentAccessComponent.GetVehicleIn(user);
		Refresh(user, usable && LF_FellAction.HoldsAxe(user));
	}

	//------------------------------------------------------------------------------------------------
	//! One look round the player: the prompt goes to the nearest standing fellable tree within reach
	//! while the axe is held, and off the tree otherwise. Public so the test drives it without an axe.
	static void Refresh(IEntity user, bool holdsAxe)
	{
		LF_TreeFellingConfig config = LF_TreeFellingConfig.Get();
		if (!user || !config || !holdsAxe)
		{
			Park();
			return;
		}

		IEntity tree = NearestFellable(user.GetOrigin(), config.m_fReachM, config);
		if (!tree)
		{
			Park();
			return;
		}

		if (tree == s_Target && s_Helper)
			return;

		Aim(tree);
	}

	//------------------------------------------------------------------------------------------------
	//! Moves the helper onto `tree` and points its action at it.
	protected static void Aim(IEntity tree)
	{
		if (!s_Helper)
			s_Helper = LF_Utils.SpawnEntityPrefab(LF_FellAction.HELPER, tree.GetOrigin(), "0 0 0", false);

		if (!s_Helper)
		{
			LF_Debug.Error(FEATURE, "fell prompt helper did not spawn: " + LF_FellAction.HELPER);
			return;
		}

		vector at = tree.GetOrigin();
		at[1] = at[1] + HELPER_HEIGHT_M;
		s_Helper.SetOrigin(at);
		s_Helper.Update();
		LF_FellAction action = Action();
		if (!action)
		{
			LF_Debug.Error(FEATURE, "fell prompt helper has no LF_FellAction");
			return;
		}

		action.AssignTree(tree);
		s_Target = tree;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes the prompt off the tree it was on (an axe put away, a tree out of reach).
	protected static void Park()
	{
		if (!s_Target)
			return;

		LF_FellAction action = Action();
		if (action)
			action.AssignTree(null);

		s_Target = null;
	}

	//------------------------------------------------------------------------------------------------
	static LF_FellAction Action()
	{
		if (!s_Helper)
			return null;

		ActionsManagerComponent manager = ActionsManagerComponent.Cast(s_Helper.FindComponent(ActionsManagerComponent));
		if (!manager)
			return null;

		return LF_FellAction.Cast(manager.GetFirstAction());
	}

	//------------------------------------------------------------------------------------------------
	//! The tree the prompt is on now (null when none), for the proof.
	static IEntity Target()
	{
		return s_Target;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		if (s_bRunning)
			GetGame().GetCallqueue().Remove(Tick);

		if (s_Helper)
			SCR_EntityHelper.DeleteEntityAndChildren(s_Helper);

		s_bRunning = false;
		s_Helper = null;
		s_Target = null;
	}
}

//------------------------------------------------------------------------------------------------
//! The fell request and the stamina cost, on the player's own controller.
modded class SCR_PlayerController
{
	//------------------------------------------------------------------------------------------------
	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);
		if (to && GetGame().GetPlayerController() == this)
			LF_FellPrompt.Start();
	}

	//------------------------------------------------------------------------------------------------
	//! Client: ask the server to fell the map tree standing at `at`.
	void LF_AskFell(vector at)
	{
		Rpc(RpcAsk_LF_Fell, at);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: find the tree at the named spot, then run the whole fell; a refusal toasts its reason.
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_LF_Fell(vector at)
	{
		IEntity user = GetControlledEntity();
		if (!user)
			return;

		IEntity tree = LF_FellPrompt.ResolveTree(at);
		array<IEntity> logs = {};
		LF_FellRefusal refusal = LF_FellRefusal.NOT_A_TREE;
		if (tree)
			refusal = LF_FellAction.TryFell(user, tree, LF_FellAction.HoldsAxe(user), logs);
		else
			LF_Debug.Warn(LF_FellPrompt.FEATURE, string.Format("fell ask from player %1 names no standing tree", GetPlayerId()));

		if (refusal != LF_FellRefusal.OK)
			LF_Utils.NotifyEntity(user, LF_TreeFelling.RefusalKey(refusal), "", 4.0);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: take `fraction` of the stamina bar from this player's character, on the owner's machine.
	void LF_SpendFellStamina(float fraction)
	{
		Rpc(RpcDo_LF_FellStamina, fraction);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_LF_FellStamina(float fraction)
	{
		LF_FellAction.ApplyStamina(GetControlledEntity(), fraction);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_FellPrompt's statics (LF_StaticReset finds it by reflection).
class LF_FellPrompt_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_FellPrompt";
	}

	override void Apply()
	{
		LF_FellPrompt.ResetStatics();
	}
}
