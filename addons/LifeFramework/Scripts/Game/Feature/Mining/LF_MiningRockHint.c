//------------------------------------------------------------------------------------------------
//! A pickaxe swung at plain rock (first-2h review B8). Vanilla boulders take a melee hit with no
//! feedback, so a new miner swings at the big grey rock beside the ore and thinks mining is broken.
//! The server tells the miner once in a while that it is plain rock and what ore looks like.
class LF_MiningRockHint
{
	static const string FEATURE = "Mining";
	static const string HINT_KEY = "#LF-Mining_NotOre";
	//! Seconds between two hints to one player: a few swings in a row read one hint.
	static const float HINT_GAP_SECONDS = 20; // lf-fixed: nag guard, not a gameplay tunable

	//! uid -> world seconds of the last hint.
	protected static ref map<string, float> s_mLastHint;

	//------------------------------------------------------------------------------------------------
	//! Pure: a pickaxe hit on a rock that is not ore. \p targetPrefab is the hit entity's prefab path,
	//! \p targetIsOre whether the hit entity (or its parent) is an ore rock.
	static bool IsPlainRockHit(string weaponPrefab, string targetPrefab, bool targetIsOre)
	{
		if (targetIsOre || weaponPrefab.IsEmpty() || targetPrefab.IsEmpty())
			return false;

		if (!weaponPrefab.Contains("Pickaxe"))
			return false;

		return targetPrefab.Contains("/Rocks/");
	}

	//------------------------------------------------------------------------------------------------
	//! Pure: whether a hint is due at \p now when the last one was at \p last (negative: never).
	static bool HintDue(float last, float now)
	{
		return last < 0 || now - last >= HINT_GAP_SECONDS;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a melee hit by \p attacker with \p weapon on \p target.
	static void OnMeleeHit(IEntity attacker, IEntity weapon, IEntity target)
	{
		if (!Replication.IsServer() || !attacker || !weapon || !target)
			return;

		bool isOre = LF_Component<LF_OreNodeComponent>.Find(target) != null;
		if (!isOre && target.GetParent())
			isOre = LF_Component<LF_OreNodeComponent>.Find(target.GetParent()) != null;

		if (!IsPlainRockHit(LF_Utils.GetPrefabName(weapon), LF_Utils.GetPrefabName(target), isOre))
			return;

		string uid = LF_Utils.GetPlayerUID(attacker);
		if (uid.IsEmpty())
			return;

		if (!s_mLastHint)
			s_mLastHint = new map<string, float>();

		float now = GetGame().GetWorld().GetWorldTime() / 1000; // lf-float: world seconds for a nag gap
		float last = -1;
		s_mLastHint.Find(uid, last);
		if (!HintDue(last, now))
			return;

		s_mLastHint.Set(uid, now);
		LF_Debug.Info(FEATURE, string.Format("%1 swung a pickaxe at plain rock %2: hinted", uid, LF_Utils.GetPrefabName(target)));
		LF_Utils.NotifyEntity(attacker, HINT_KEY, "", 5.0);
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mLastHint = null;
	}
}

//------------------------------------------------------------------------------------------------
modded class SCR_MeleeComponent
{
	//------------------------------------------------------------------------------------------------
	//! Runs on the authority only (ProcessMeleeAttack returns on proxies) before the vanilla damage.
	override protected void HandleDamageDelayed()
	{
		if (m_MeleeHitData)
			LF_MiningRockHint.OnMeleeHit(GetOwner(), m_MeleeHitData.m_Weapon, m_MeleeHitData.m_Entity);

		super.HandleDamageDelayed();
	}
}

//------------------------------------------------------------------------------------------------
class LF_MiningRockHint_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_MiningRockHint";
	}

	override void Apply()
	{
		LF_MiningRockHint.ResetStatics();
	}
}
