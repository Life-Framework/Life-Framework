// LF_ClinicShortage.c - the clinic is short after its drug cabinet is robbed (#4445, 0.3.0 Crime D1).
// Server side. A breach that opens a hospital cabinet (LF_OnBreachOpened whose target is an
// LF_Hospital cabinet) starts the shortage: for the configured time the pharmacy counter's buy prices
// are raised by a percent and a revive held near a clinic (an LF_Hospital place) takes longer. Then it
// ends on its own. It is the moral cost of the robbery (plan N18), never a block on revives.
//
// Fairness. A second robbery while the clinic is short restarts the clock at the full duration; it
// never stacks past one duration. The counter's sell price is untouched, so buying dear and selling
// back is never a profit, and nothing the medic is paid (duty pay, revive bills, bonuses) changes:
// only the hold is longer, and the percent is capped.
//
// Replication. One bool on the game mode (RplProp + BumpMe) so a client's revive hold and price list
// match the server. The server decides from its own clock. Not persisted: a restart ends the shortage.

//------------------------------------------------------------------------------------------------
//! The shortage tuning. The field defaults are the shipped core values; a pack lists a whole
//! LF_ClinicShortageConfig in its mission header overlays to change them (the last one wins).
[BaseContainerProps(configRoot: true)]
class LF_ClinicShortageConfig
{
	[Attribute("3600", UIWidgets.EditBox, "Seconds the clinic stays short after a cabinet robbery", params: "60 86400")]
	int m_iDurationSeconds;

	[Attribute("150", UIWidgets.EditBox, "Percent of the normal buy price the short counters charge (100 = no change)", params: "100 300")]
	int m_iPricePercent;

	[Attribute("150", UIWidgets.EditBox, "Percent of the normal hold a revive near the clinic takes while short (100 = no change)", params: "100 200")]
	int m_iReviveTimePercent;

	[Attribute("40", UIWidgets.EditBox, "Meters from a hospital place that count as at the clinic", params: "1 500")]
	float m_fClinicRadius;

	[Attribute("", UIWidgets.Auto, "Shop outlet ids (LF_ShopComponent.m_sOutletId) whose prices rise while short")]
	ref array<string> m_aOutlets;

	protected static ref LF_ClinicShortageConfig s_Config;

	//------------------------------------------------------------------------------------------------
	static LF_ClinicShortageConfig Defaults()
	{
		LF_ClinicShortageConfig config = new LF_ClinicShortageConfig();
		// The same values as the Attribute defaults above (a script-built instance does not read them).
		config.m_iDurationSeconds = 3600;
		config.m_iPricePercent = 150;
		config.m_iReviveTimePercent = 150;
		config.m_fClinicRadius = 40;
		config.m_aOutlets = {"pharmacy_counter"};
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! The live tuning: the core defaults, replaced by the last pack overlay of this class.
	static LF_ClinicShortageConfig Get()
	{
		if (s_Config)
			return s_Config;

		s_Config = Defaults();
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(ResourceName.Empty, LF_ClinicShortageConfig, "clinicShortage", resources, names);
		foreach (int i, Resource resource : resources)
		{
			LF_ClinicShortageConfig overlay = LF_ClinicShortageConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(resource.GetResource().ToBaseContainer()));
			if (!overlay)
			{
				LF_Debug.Error(LF_ClinicShortage.FEATURE, "clinic shortage overlay has an unexpected root class: " + names[i]);
				continue;
			}

			if (!overlay.m_aOutlets)
				overlay.m_aOutlets = {};
			s_Config = overlay;
			LF_Debug.Info(LF_ClinicShortage.FEATURE, "clinic shortage tuning from " + names[i]);
		}

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests swap the tuning; null drops back to the loaded one.
	static void UseForTest(LF_ClinicShortageConfig config)
	{
		s_Config = config;
	}
}

//------------------------------------------------------------------------------------------------
//! Listens on the server bus and ends the shortage when its clock runs out.
class LF_ClinicShortageListener : LF_EventSubscriber
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "LF_ClinicShortage";
	}

	//------------------------------------------------------------------------------------------------
	override bool Handle(notnull LF_Event busEvent)
	{
		LF_OnBreachOpened opened = LF_OnBreachOpened.Cast(busEvent);
		if (opened)
			LF_ClinicShortage.OnBreach(opened.m_sTargetId, opened.m_sPlayerUid);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	void Expire()
	{
		LF_ClinicShortage.Expire();
	}
}

//------------------------------------------------------------------------------------------------
class LF_ClinicShortage
{
	static const string FEATURE = "ClinicShortage";
	static const int MAX_PRICE_PERCENT = 300; // lf-fixed: safety cap on any config, so a typo never prices medicine out of reach
	static const int MAX_REVIVE_PERCENT = 200; // lf-fixed: safety cap on any config, the shortage never doubles a revive past twice

	//! Tests set the server clock in seconds; -1 reads the world clock.
	static int s_iNowOverride = -1;

	//! Server: world second the shortage ends; 0 when the clinic is stocked.
	protected static int s_iEndsAt;
	//! Client: the replicated flag (LF_GameModeRoleplay.m_bLF_ClinicShort).
	protected static bool s_bClientShort;
	protected static ref LF_ClinicShortageListener s_Listener;

	//------------------------------------------------------------------------------------------------
	//! Server: subscribe once (the game mode calls it on start; tests call it too).
	static bool Install()
	{
		if (!s_Listener)
			s_Listener = new LF_ClinicShortageListener();

		if (LF_EventBus.IsSubscribed(LF_OnBreachOpened, s_Listener))
			return true;

		return LF_EventBus.Subscribe(LF_OnBreachOpened, s_Listener);
	}

	//------------------------------------------------------------------------------------------------
	static int Now()
	{
		if (s_iNowOverride >= 0)
			return s_iNowOverride;

		if (!GetGame() || !GetGame().GetWorld())
			return 0;

		int ms = GetGame().GetWorld().GetWorldTime();
		return ms / 1000;
	}

	//------------------------------------------------------------------------------------------------
	//! A breach opened: a hospital cabinet starts (or restarts) the shortage; anything else is ignored.
	static void OnBreach(string targetId, string playerUid)
	{
		if (!LF_Hospital.LF_IsCabinet(targetId))
			return;

		Start(targetId, playerUid);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the clinic is short from now for the configured time. A robbery while short restarts
	//! the clock at the full duration; it never adds a second duration.
	static void Start(string targetId, string playerUid)
	{
		LF_ClinicShortageConfig config = LF_ClinicShortageConfig.Get();
		int duration = LF_Utils.ClampInt(config.m_iDurationSeconds, 60, 86400);
		bool wasShort = IsActive();
		s_iEndsAt = Now() + duration;
		LF_Debug.Info(FEATURE, string.Format("clinic short for %1 s after cabinet %2 was robbed by %3 (restart %4)", duration, targetId, playerUid, wasShort));

		Publish(true);
		ScheduleExpire(duration);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the clock check. Ends the shortage when its time is up; early calls do nothing.
	static void Expire()
	{
		if (s_iEndsAt == 0)
			return;

		int left = SecondsLeft();
		if (left > 0)
		{
			ScheduleExpire(left);
			return;
		}

		s_iEndsAt = 0;
		LF_Debug.Info(FEATURE, "clinic restocked: the shortage is over");
		Publish(false);
	}

	//------------------------------------------------------------------------------------------------
	//! True while the clinic is short. The server reads its clock; a client reads the replicated flag.
	static bool IsActive()
	{
		if (!Replication.IsServer())
			return s_bClientShort;

		if (s_iEndsAt == 0)
			return false;

		return Now() < s_iEndsAt;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: seconds until the clinic is stocked again; 0 when it is not short (the door notice reads it).
	static int SecondsLeft()
	{
		if (s_iEndsAt == 0)
			return 0;

		return LF_Utils.MaxInt(0, s_iEndsAt - Now());
	}

	//------------------------------------------------------------------------------------------------
	//! The buy price a shop outlet charges now: raised by the percent at a short counter, else unchanged.
	//! An on-duty medic restocking is exempt: the shortage is the robbers' cost, not the medic's.
	static int UnitPrice(string outlet, int price, bool medicOnDuty)
	{
		if (medicOnDuty || price <= 0 || outlet.IsEmpty() || !IsActive())
			return price;

		LF_ClinicShortageConfig config = LF_ClinicShortageConfig.Get();
		if (!config.m_aOutlets || !config.m_aOutlets.Contains(outlet))
			return price;

		int percent = LF_Utils.ClampInt(config.m_iPricePercent, 100, MAX_PRICE_PERCENT);
		return (price * percent + 99) / 100;
	}

	//------------------------------------------------------------------------------------------------
	//! The revive hold's progress step: shorter (the hold longer) at the clinic while short.
	static float ReviveSlice(float timeSlice, bool atClinic)
	{
		if (!atClinic || !IsActive())
			return timeSlice;

		int percent = LF_Utils.ClampInt(LF_ClinicShortageConfig.Get().m_iReviveTimePercent, 100, MAX_REVIVE_PERCENT);
		return timeSlice * 100 / percent;
	}

	//------------------------------------------------------------------------------------------------
	//! Client or server: the character's replicated account says on duty as a medic.
	static bool IsMedicOnDuty(IEntity character)
	{
		if (!character)
			return false;

		return LF_MedicDuty.IsOnDutyState(LF_AccountReplication.GetState(character));
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a position is at the clinic: within the radius of a hospital place (till or cabinet).
	static bool IsAtClinic(vector position)
	{
		array<IEntity> places = {};
		LF_Places.FindAll(LF_Hospital.PLACE_KIND, places);
		float radius = LF_ClinicShortageConfig.Get().m_fClinicRadius;
		foreach (IEntity place : places)
		{
			if (place && vector.DistanceSq(place.GetOrigin(), position) <= radius * radius)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Client: the replicated flag arrived.
	static void OnReplicated(bool isShort)
	{
		s_bClientShort = isShort;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Publish(bool isShort)
	{
		LF_GameModeRoleplay gameMode = LF_GameModeRoleplay.Cast(GetGame().GetGameMode());
		if (gameMode)
			gameMode.LF_SetClinicShort(isShort);
	}

	//------------------------------------------------------------------------------------------------
	protected static void ScheduleExpire(int seconds)
	{
		if (!s_Listener || !GetGame() || !GetGame().GetCallqueue())
			return;

		GetGame().GetCallqueue().Remove(s_Listener.Expire);
		GetGame().GetCallqueue().CallLater(s_Listener.Expire, seconds * 1000 + 500, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the shortage and the listener (world end, tests).
	static void ResetStatics()
	{
		if (s_Listener)
		{
			LF_EventBus.Unsubscribe(LF_OnBreachOpened, s_Listener);
			if (GetGame() && GetGame().GetCallqueue())
				GetGame().GetCallqueue().Remove(s_Listener.Expire);
		}

		s_Listener = null;
		s_iEndsAt = 0;
		s_bClientShort = false;
		s_iNowOverride = -1;
		LF_ClinicShortageConfig.UseForTest(null);
	}
}

//------------------------------------------------------------------------------------------------
//! Which breach targets are hospital cabinets (the cabinet table is LF_Hospital's own).
modded class LF_Hospital
{
	static bool LF_IsCabinet(string targetId)
	{
		if (targetId.IsEmpty())
			return false;

		return Cabinets().Contains(targetId);
	}
}

//------------------------------------------------------------------------------------------------
//! The short counter charges more. The sell price is untouched (no buy-dear, sell-back loop). The
//! server knows the buyer inside Buy; a client's price list asks for its own character.
modded class LF_ShopComponent
{
	//! Buy running: 1 the buyer is an on-duty medic, 2 anyone else; 0 outside Buy.
	protected int m_iLF_ShortageBuyer;

	//------------------------------------------------------------------------------------------------
	override LF_EShopRefusal Buy(ResourceName prefab, int quantity, IEntity buyer, out int charged, out string missing)
	{
		m_iLF_ShortageBuyer = 2;
		if (LF_ClinicShortage.IsMedicOnDuty(buyer))
			m_iLF_ShortageBuyer = 1;

		LF_EShopRefusal result = super.Buy(prefab, quantity, buyer, charged, missing);
		m_iLF_ShortageBuyer = 0;
		return result;
	}

	//------------------------------------------------------------------------------------------------
	override int GetBuyPrice(ResourceName prefab)
	{
		bool medic = m_iLF_ShortageBuyer == 1;
		if (m_iLF_ShortageBuyer == 0 && !Replication.IsServer())
			medic = LF_ClinicShortage.IsMedicOnDuty(LF_Utils.GetLocalCharacter());

		return LF_ClinicShortage.UnitPrice(GetOutletId(), super.GetBuyPrice(prefab), medic);
	}

	//------------------------------------------------------------------------------------------------
	//! The menu rows and their Pay button read m_iBuyPrice, not GetBuyPrice: raise them the same way
	//! for the viewer, so the price shown is the price charged.
	override array<ref LF_ShopCatalogItem> GetShopItems()
	{
		array<ref LF_ShopCatalogItem> rows = super.GetShopItems();
		if (!rows || !LF_ClinicShortage.IsActive())
			return rows;

		bool medic = LF_ClinicShortage.IsMedicOnDuty(LF_Utils.GetLocalCharacter());
		string outlet = GetOutletId();
		foreach (LF_ShopCatalogItem row : rows)
		{
			if (row)
				row.m_iBuyPrice = LF_ClinicShortage.UnitPrice(outlet, row.m_iBuyPrice, medic);
		}

		return rows;
	}
}

//------------------------------------------------------------------------------------------------
//! A revive held at the clinic takes longer while it is short (the engine adds the slice to progress).
modded class LF_ReviveAction
{
	override float GetActionProgressScript(float fProgress, float timeSlice)
	{
		bool atClinic = false;
		if (LF_ClinicShortage.IsActive())
			atClinic = LF_ClinicShortage.IsAtClinic(GetOwner().GetOrigin());

		return super.GetActionProgressScript(fProgress, LF_ClinicShortage.ReviveSlice(timeSlice, atClinic));
	}
}

//------------------------------------------------------------------------------------------------
//! The one replicated flag, on the always-replicated game mode (JIP gets it in the snapshot).
modded class LF_GameModeRoleplay
{
	[RplProp(onRplName: "LF_OnClinicShortRpl")]
	protected bool m_bLF_ClinicShort;

	//------------------------------------------------------------------------------------------------
	protected override void OnGameStart()
	{
		super.OnGameStart();
		if (Replication.IsServer())
			LF_ClinicShortage.Install();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: set the flag; bumps only when it changes.
	void LF_SetClinicShort(bool isShort)
	{
		if (isShort == m_bLF_ClinicShort)
			return;

		m_bLF_ClinicShort = isShort;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	protected void LF_OnClinicShortRpl()
	{
		LF_ClinicShortage.OnReplicated(m_bLF_ClinicShort);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of the shortage statics (LF_StaticReset finds it by reflection).
class LF_ClinicShortage_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ClinicShortage";
	}

	override void Apply()
	{
		LF_ClinicShortage.ResetStatics();
	}
}
