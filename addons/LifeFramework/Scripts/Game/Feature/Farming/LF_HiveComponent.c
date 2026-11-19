//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "LifeFramework/Feature/Farming", description: "A beehive that fills with combs over fair game hours; a player takes them. Action: LF_HiveTakeAction.")]
class LF_HiveComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! A beehive (#3447): the server works it on the game clock by LF_HiveRules (no work at night or in
//! rain, a cap of LF_HiveConfig.m_iMaxCombs), and "Take honeycomb (n)" hands every comb out as
//! honeycomb items. The replicated comb count is all a client needs for the prompt.
//!
//! PERSISTENCE: the "hives" save section (LF_HiveSavePayload) keeps combs and the work toward the
//! next one by the hive key. An authored m_sHiveKey must be unique in the world; empty, the key is
//! the hive's position (a placed hive), so it must not move.
class LF_HiveComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, "Stable key, unique in the world. Empty: the hive's position is its key")]
	protected string m_sHiveKey;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Item one comb gives", "et")]
	protected ResourceName m_HoneycombPrefab;

	[Attribute("3", UIWidgets.Auto, "Metres from the hive a player must stand")]
	protected float m_fReach;

	//! Combs inside. Server writes, every machine words the prompt from it.
	[RplProp()]
	protected int m_iCombs;

	protected ref LF_HiveState m_State = new LF_HiveState();

	//! Live hives by key. Lives here so the component's own OnDelete removes it.
	protected static ref map<string, LF_HiveComponent> s_mLive;

	//------------------------------------------------------------------------------------------------
	static map<string, LF_HiveComponent> Live()
	{
		if (!s_mLive)
			s_mLive = new map<string, LF_HiveComponent>();

		return s_mLive;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetLive()
	{
		s_mLive = null;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		// The key may be the position, which a spawned entity has only once it is fully created.
		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		if (!Replication.IsServer())
			return;

		if (m_sHiveKey.IsEmpty())
			m_sHiveKey = KeyAt(owner.GetOrigin());

		LF_Hives.Register(this);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (s_mLive && s_mLive.Get(m_sHiveKey) == this)
		{
			s_mLive.Remove(m_sHiveKey);
			LF_Hives.Unregister(this);
		}

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! The key of a placed hive with no authored key: its position to the decimetre.
	static string KeyAt(vector origin)
	{
		int x = Math.Round(origin[0] * 10);
		int z = Math.Round(origin[2] * 10);
		return string.Format("hive:%1:%2", x, z);
	}

	//------------------------------------------------------------------------------------------------
	string GetHiveKey()
	{
		return m_sHiveKey;
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetHoneycombPrefab()
	{
		return m_HoneycombPrefab;
	}

	//------------------------------------------------------------------------------------------------
	//! Combs inside; the same answer on the server and on a client.
	int GetCombs()
	{
		return m_iCombs;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: game hours of work toward the next comb.
	float GetProgressHours()
	{
		return m_State.m_fProgressHours;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the hive works for `hours` game hours (the LF_Hives tick, or a test). \return Combs gained.
	int ServerAdvance(float hours, bool raining, bool night)
	{
		LF_HiveConfig config = LF_HiveConfig.Get();
		if (!config)
			return 0;

		int gained = LF_HiveRules.Advance(m_State, config.m_fHoursPerComb, config.m_iMaxCombs, hours, raining, night);
		if (gained > 0)
		{
			Sync();
			LF_Debug.Info(LF_Hives.FEATURE, string.Format("hive %1 filled %2 comb(s), now %3", m_sHiveKey, gained, m_iCombs));
		}

		return gained;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a player takes every comb as honeycomb items. A full inventory drops the rest at the
	//! player's feet; a grant that gives nothing leaves the combs in the hive.
	//! \param[out] given Items handed out.
	LF_EHiveTake ServerTake(IEntity user, out int given)
	{
		given = 0;
		if (!Replication.IsServer())
			return LF_EHiveTake.EMPTY;

		if (LF_Systems.Refuses(LF_FarmPlots.SYSTEM, "taking honeycomb at " + m_sHiveKey))
			return LF_EHiveTake.EMPTY;

		// Out of reach counts as not allowed: the hive is not this player's to work from there.
		if (!user || vector.Distance(user.GetOrigin(), GetOwner().GetOrigin()) > m_fReach)
			return LF_EHiveTake.NOT_OWNER;

		if (m_HoneycombPrefab.IsEmpty())
		{
			LF_Debug.Error(LF_Hives.FEATURE, string.Format("hive %1 has no honeycomb prefab; nothing can be taken", m_sHiveKey));
			return LF_EHiveTake.EMPTY;
		}

		int combs = m_State.m_iCombs;
		if (combs <= 0)
			return LF_EHiveTake.EMPTY;

		given = LF_InventoryUtils.AddAmount(user, m_HoneycombPrefab, combs, true);
		if (given <= 0)
		{
			LF_Debug.Error(LF_Hives.FEATURE, string.Format("hive %1 honeycomb grant failed: %2 x%3, the combs stay", m_sHiveKey, m_HoneycombPrefab, combs));
			return LF_EHiveTake.EMPTY;
		}

		int taken;
		LF_HiveRules.Take(m_State, false, string.Empty, string.Empty, taken);
		Sync();
		LF_Debug.Info(LF_Hives.FEATURE, string.Format("hive %1: %2 took %3 honeycomb", m_sHiveKey, LF_Utils.GetPlayerUID(user), given));
		return LF_EHiveTake.OK;
	}

	//------------------------------------------------------------------------------------------------
	protected void Sync()
	{
		if (m_iCombs == m_State.m_iCombs)
			return;

		m_iCombs = m_State.m_iCombs;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: the hive's combs and work toward the next.
	LF_HiveRow Export()
	{
		if (m_sHiveKey.IsEmpty())
			return null;

		int minutes = Math.Round(m_State.m_fProgressHours * 60);
		return LF_HiveRow.Create(m_sHiveKey, m_State.m_iCombs, minutes);
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence seam: puts a saved hive back, or empties it for a null row. The comb count is held to
	//! the config's cap. Idempotent.
	void Import(LF_HiveRow row)
	{
		m_State.m_iCombs = 0;
		m_State.m_fProgressHours = 0;
		if (row)
		{
			int cap = 100;
			LF_HiveConfig config = LF_HiveConfig.Get();
			if (config)
				cap = config.m_iMaxCombs;

			m_State.m_iCombs = LF_Utils.ClampInt(row.m_iCombs, 0, cap);
			m_State.m_fProgressHours = LF_Utils.MaxInt(0, row.m_iProgressMinutes) / 60.0;
			LF_Debug.Info(LF_Hives.FEATURE, string.Format("hive %1 restored: %2 comb(s), %3 min toward the next", m_sHiveKey, m_State.m_iCombs, row.m_iProgressMinutes));
		}

		Sync();
	}
}

//------------------------------------------------------------------------------------------------
//! "Take honeycomb (n)" at a hive with combs (#3447). The server checks everything again.
class LF_HiveTakeAction : ScriptedUserAction
{
	protected LF_HiveComponent m_Hive;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Hive = LF_Component<LF_HiveComponent>.Find(pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return m_Hive && m_Hive.GetCombs() > 0 && LF_Systems.IsEnabled(LF_FarmPlots.SYSTEM);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (!m_Hive)
			return false;

		outName = WidgetManager.Translate("#LF-Hive_Take", m_Hive.GetCombs());
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!m_Hive || !pUserEntity)
		{
			LF_Debug.Error(LF_Hives.FEATURE, "hive action performed without a hive or a user");
			return;
		}

		if (!Replication.IsServer())
			return;

		int given;
		LF_EHiveTake result = m_Hive.ServerTake(pUserEntity, given);
		if (result != LF_EHiveTake.OK)
		{
			LF_Debug.Info(LF_Hives.FEATURE, string.Format("hive %1 refused a take (%2)", m_Hive.GetHiveKey(), typename.EnumToString(LF_EHiveTake, result)));
			if (result == LF_EHiveTake.EMPTY)
				LF_Utils.NotifyEntity(pUserEntity, "#LF-Hive_Empty", "#LF-Farm_Title", 4.0);
			return;
		}

		SCR_InventoryStorageManagerComponent inventory = LF_Component<SCR_InventoryStorageManagerComponent>.Find(pUserEntity);
		if (inventory)
			inventory.RpcAsk_PlaySound(LF_NetworkUtils.GetRplId(pUserEntity), "SOUND_PICK_UP");
	}
}
