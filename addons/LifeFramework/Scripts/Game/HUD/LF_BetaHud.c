//! The player's HUD (Beta.layout): a small brand mark top-left, cash and bank top-right with the
//! wanted badges under them, food and drink bottom-centre, health and stamina bottom-right.
//! Everything reads values the server already replicates to this player (cash from the inventory,
//! the bank mirror, the packed account state, the survival RplProps). Replicated values are read
//! every POLL_SECONDS, widgets are touched only when a value or a target changes, and the rules
//! (when to show, when to dim, what is low) live in LF_HudLogic. /hud toggles each part.
class LF_BetaHud : SCR_InfoDisplay
{
	static const float POLL_SECONDS = 0.25;	// lf-float: UI seconds
	//! How long the "+$100" takes to slide in, and how far it travels.
	static const float DELTA_SLIDE_SECONDS = 0.25;	// lf-float: UI seconds
	static const float DELTA_SLIDE_PIXELS = 14;
	static const float DELTA_GAP_PIXELS = 10;
	static const float PIP_FILLED_OPACITY = 0.95;
	static const float PIP_EMPTY_OPACITY = 0.2;

	//--------------------------- CONFIG VALUES ---------------------------
	protected bool m_bEnableHUD = true;
	protected bool m_bEnableStamina = true;
	protected bool m_bEnableHealth = true;
	//---------------------------------------------------------------------

	protected HorizontalLayoutWidget m_wPlayerStatsHUD;

	protected ProgressBarWidget m_wStaminaProgressBar;
	protected ImageWidget m_wStaminaProgress;
	protected ImageWidget m_wHealthProgress;
	protected OverlayWidget m_wHealthIndicator;
	protected OverlayWidget m_wStaminaIndicator;

	protected Widget m_wBrand;
	protected Widget m_wMoneyBlock;
	protected Widget m_wMoneyPlate;
	protected TextWidget m_wCash;
	protected TextWidget m_wCashDelta;
	protected TextWidget m_wBank;
	protected Widget m_wWanted;
	protected TextWidget m_wWantedCaption;
	protected ref array<Widget> m_aWantedPips = {};
	protected Widget m_wNeeds;
	protected ImageWidget m_wFoodIcon;
	protected ImageWidget m_wDrinkIcon;
	protected TextWidget m_wFoodValue;
	protected TextWidget m_wDrinkValue;

	protected SCR_ChimeraCharacter m_Character;
	protected SCR_CharacterControllerComponent m_PlayerController;
	protected DamageManagerComponent m_DMC;
	protected LF_CharacterSurvivalComponent m_Survival;
	//! The inventory manager OnMoneyChange is subscribed to, kept so the handler can be removed
	//! from it when the HUD stops drawing this body (a respawn draws a new one).
	protected ScriptedInventoryStorageManagerComponent m_MoneyInventory;

	protected bool m_bUsingProgressWidget;
	protected bool m_bStatChange;
	protected float m_fPreviousValues[4];
	protected float m_fTimeAccumulator;
	protected bool m_bGUIHidden;

	protected float m_fPollAccumulator = POLL_SECONDS;
	protected bool m_bHiddenByScreen;
	protected string m_sPrefsLine;

	protected int m_iBank = LF_HudLogic.BANK_UNKNOWN;
	protected bool m_bBankSeen;
	protected float m_fMoneyIdle;
	protected float m_fMoneyOpacity = -1;
	protected bool m_bMoneyForced;
	protected float m_fDeltaAge = -1;

	protected int m_iWanted = -1;
	protected float m_fWantedCaptionAge = -1;

	protected int m_iHunger = -1;
	protected int m_iThirst = -1;
	protected float m_fNeedRiseAge = -1;
	protected bool m_bNeedsShown;

	//---------------------------------------------------------------------
	void OnHealthChange(float value)
	{
		if (!m_wHealthProgress)
		{
			m_wHealthProgress = ImageWidget.Cast(m_wHealthIndicator.FindAnyWidget("m_healthProgress"));
			if (!m_wHealthProgress) return;
		}

		float currentHealth = m_DMC.GetHealth() / 100;

		if (!m_fPreviousValues[0])
		{
			m_fPreviousValues[0] = currentHealth;
		}
		else
		{
			if (float.AlmostEqual(m_fPreviousValues[0], currentHealth)) return;

			m_wHealthProgress.SetMaskProgress(value);
			m_bStatChange = true;
			SetProgressColor(m_wHealthProgress, value);
		}
	}

	//---------------------------------------------------------------------
	void OnStaminaChange(float value)
	{
		if (!m_wStaminaProgressBar)
		{
			if (!m_wStaminaProgress)
			{
				m_wStaminaProgress = ImageWidget.Cast(m_wStaminaIndicator.FindAnyWidget("m_staminaProgress"));
				if (!m_wStaminaProgress) return;
			}
		}

		float m_currentStam = m_PlayerController.GetStamina();

		if (!m_fPreviousValues[1])
		{
			m_fPreviousValues[1] = m_currentStam;
		}
		else
		{
			if (!float.AlmostEqual(m_fPreviousValues[1] , m_currentStam))
			{
				m_fPreviousValues[1] = m_currentStam;
				if (!m_wStaminaProgressBar)
				{
					m_wStaminaProgress.SetMaskProgress(value);
					SetProgressColor(m_wStaminaProgress, value);
				}
				else
				{
					m_wStaminaProgressBar.SetCurrent(value);
					SetProgressColor(m_wStaminaProgressBar, value);
				}

				m_bStatChange = true;
			}
		}
	}

	//---------------------------------------------------------------------
	//! Cash on the body changed (the inventory's money invoker): new figure, and the delta slides in.
	void OnMoneyChange(ResourceName prefab, int newAmount, int oldAmount)
	{
		if (!m_wCash)
			return;

		m_wCash.SetText(LF_MoneyFormat.FormatMoney(newAmount));
		string delta = LF_HudLogic.DeltaText(oldAmount, newAmount);
		if (delta.IsEmpty() || !m_wCashDelta)
			return;

		m_wCashDelta.SetText(delta);
		if (newAmount > oldAmount)
			m_wCashDelta.SetColor(Color.FromRGBA(140, 220, 140, 255));
		else
			m_wCashDelta.SetColor(Color.FromRGBA(235, 110, 100, 255));

		m_wCashDelta.SetOpacity(0);
		AnimateWidget.Opacity(m_wCashDelta, 1, UIConstants.FADE_RATE_FAST);
		AlignableSlot.SetPadding(m_wCashDelta, 0, 0, DELTA_GAP_PIXELS + DELTA_SLIDE_PIXELS, 0);
		m_fDeltaAge = 0;
		WakeMoney();
	}

	//---------------------------------------------------------------------
	override event void OnStartDraw(IEntity owner)
	{
		super.OnStartDraw(owner);

		if (!m_bEnableHUD) return;

		m_Character = SCR_ChimeraCharacter.Cast(owner);
		m_PlayerController = SCR_CharacterControllerComponent.Cast(owner.FindComponent(SCR_CharacterControllerComponent));
		m_DMC = DamageManagerComponent.Cast(owner.FindComponent(DamageManagerComponent));
		m_Survival = LF_CharacterSurvivalComponent.Cast(owner.FindComponent(LF_CharacterSurvivalComponent));
		FindSubtleWidgets();
		ResetSubtleState();

		// Cash display and its change subscription first: the stat row below may be missing.
		UnsubscribeMoney();
		int currentCash = LF_MoneyUtils.GetCash(owner);
		OnMoneyChange(LF_MoneyUtils.CashPrefab(), currentCash, currentCash);
		m_MoneyInventory = LF_Component<ScriptedInventoryStorageManagerComponent>.Find(owner);
		if (m_MoneyInventory) m_MoneyInventory.LF_GetOnAmountChanged(LF_MoneyUtils.CashPrefab()).Insert(OnMoneyChange);

		m_wPlayerStatsHUD = HorizontalLayoutWidget.Cast(m_wRoot.FindAnyWidget("m_playerStatsHUD"));
		if (!m_wPlayerStatsHUD) return;

		if (m_bEnableHealth)
			m_wHealthIndicator = OverlayWidget.Cast(m_wPlayerStatsHUD.FindAnyWidget("m_healthIndicator"));

		if (m_bEnableStamina)
		{
			m_wStaminaIndicator = OverlayWidget.Cast(m_wPlayerStatsHUD.FindAnyWidget("m_staminaIndicator"));
			m_bUsingProgressWidget = (!m_wStaminaIndicator || !m_wStaminaIndicator.IsEnabled());
			if (m_bUsingProgressWidget)
				m_wStaminaProgressBar = ProgressBarWidget.Cast(m_wRoot.FindAnyWidget("m_staminaProgressBar"));
		}
	}

	//---------------------------------------------------------------------
	protected void FindSubtleWidgets()
	{
		if (!m_wRoot)
			return;

		m_wBrand = m_wRoot.FindAnyWidget("m_brand");
		m_wMoneyBlock = m_wRoot.FindAnyWidget("m_moneyBlock");
		m_wMoneyPlate = m_wRoot.FindAnyWidget("m_moneyPlate");
		m_wCash = TextWidget.Cast(m_wRoot.FindAnyWidget("m_cash"));
		m_wCashDelta = TextWidget.Cast(m_wRoot.FindAnyWidget("m_cashDelta"));
		m_wBank = TextWidget.Cast(m_wRoot.FindAnyWidget("m_bank"));
		m_wWanted = m_wRoot.FindAnyWidget("m_wanted");
		m_wWantedCaption = TextWidget.Cast(m_wRoot.FindAnyWidget("m_wantedCaption"));
		m_wNeeds = m_wRoot.FindAnyWidget("m_needs");
		m_wFoodIcon = ImageWidget.Cast(m_wRoot.FindAnyWidget("m_foodIcon"));
		m_wDrinkIcon = ImageWidget.Cast(m_wRoot.FindAnyWidget("m_drinkIcon"));
		m_wFoodValue = TextWidget.Cast(m_wRoot.FindAnyWidget("m_foodValue"));
		m_wDrinkValue = TextWidget.Cast(m_wRoot.FindAnyWidget("m_drinkValue"));

		m_aWantedPips.Clear();
		for (int i = 1; i <= LF_HudLogic.WANTED_PIPS; i++)
		{
			Widget pip = m_wRoot.FindAnyWidget("m_wantedPip" + i);
			if (pip)
				m_aWantedPips.Insert(pip);
		}

		if (!m_wCash || !m_wBank || !m_wWanted || !m_wNeeds || m_aWantedPips.Count() != LF_HudLogic.WANTED_PIPS)
			LF_Debug.Error(LF_HudLogic.FEATURE, "Beta.layout is missing a HUD widget, that part stays blank");
	}

	//---------------------------------------------------------------------
	//! A new body starts from unknown values so the first poll writes every widget.
	protected void ResetSubtleState()
	{
		m_fPollAccumulator = POLL_SECONDS;
		m_sPrefsLine = "";
		m_bHiddenByScreen = false;
		m_iBank = LF_HudLogic.BANK_UNKNOWN;
		m_bBankSeen = false;
		m_fMoneyIdle = 0;
		m_fMoneyOpacity = -1;
		m_fDeltaAge = -1;
		m_iWanted = -1;
		m_fWantedCaptionAge = -1;
		m_iHunger = -1;
		m_iThirst = -1;
		m_fNeedRiseAge = -1;
		m_bNeedsShown = false;
	}

	//---------------------------------------------------------------------
	//! The money handler must not outlive the body it listened to: the old inventory manager would
	//! keep calling it, and the next OnStartDraw would add a second copy (two updates per move).
	protected void UnsubscribeMoney()
	{
		if (m_MoneyInventory)
		{
			ScriptInvoker invoker = m_MoneyInventory.LF_GetOnAmountChanged(LF_MoneyUtils.CashPrefab());
			if (invoker) invoker.Remove(OnMoneyChange);
		}

		m_MoneyInventory = null;
	}

	//---------------------------------------------------------------------
	override event void OnStopDraw(IEntity owner)
	{
		UnsubscribeMoney();
		super.OnStopDraw(owner);
	}

	//---------------------------------------------------------------------
	override event void UpdateValues(IEntity owner, float timeSlice)
	{
		UpdateSubtle(timeSlice);

		if (!m_PlayerController) return;

		m_bStatChange = false;
		if (m_bEnableHealth && m_wHealthIndicator) OnHealthChange(m_DMC.GetHealth());
		if (m_bEnableStamina && (m_wStaminaIndicator || m_wStaminaProgressBar)) OnStaminaChange(m_PlayerController.GetStamina());

		if (m_bStatChange)
		{
			m_fTimeAccumulator = 0;
			if (m_bGUIHidden)
			{
				m_bGUIHidden = false;
				ShowStatsHUD(true);
			}
		}
		else
		{
			m_fTimeAccumulator += timeSlice;
			if (m_fTimeAccumulator > 4.0 && !m_bGUIHidden)
			{
				m_bGUIHidden = true;
				ShowStatsHUD(false);
			}
		}
	}

	//---------------------------------------------------------------------
	//! Timers every frame (a few float adds), replicated reads every POLL_SECONDS.
	protected void UpdateSubtle(float timeSlice)
	{
		if (!m_wRoot || !m_Character)
			return;

		m_fMoneyIdle += timeSlice;
		if (m_fDeltaAge >= 0)
			TickDelta(timeSlice);

		if (m_fWantedCaptionAge >= 0)
		{
			m_fWantedCaptionAge += timeSlice;
			if (m_fWantedCaptionAge >= LF_HudLogic.WantedCaptionSeconds())
			{
				m_fWantedCaptionAge = -1;
				if (m_wWantedCaption)
					AnimateWidget.Opacity(m_wWantedCaption, 0, UIConstants.FADE_RATE_SLOW);
				TintPips(Color.FromRGBA(245, 242, 235, 255));
			}
		}

		if (m_fNeedRiseAge >= 0)
			m_fNeedRiseAge += timeSlice;

		m_fPollAccumulator += timeSlice;
		if (m_fPollAccumulator < POLL_SECONDS)
			return;

		m_fPollAccumulator = 0;
		Poll();
	}

	//---------------------------------------------------------------------
	protected void Poll()
	{
		// Hidden behind a full-screen menu (pause, a kit sheet) and during a dialogue; the inventory
		// and the map keep it and bring the money to full brightness.
		bool inventoryOpen = SCR_InventoryMenuUI.LF_GetCurrentInstance() != null;
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		bool mapOpen = mapEntity && mapEntity.IsOpen();
		bool menuOpen = false;
		MenuManager menus = GetGame().GetMenuManager();
		if (menus)
			menuOpen = menus.IsAnyMenuOpen() || menus.IsAnyDialogOpen();

		bool hide = (menuOpen && !inventoryOpen && !mapOpen) || LF_DialogueScene.IsOpen() || LF_DialogueCamera.IsActive();
		if (hide != m_bHiddenByScreen)
		{
			m_bHiddenByScreen = hide;
			m_wRoot.SetVisible(!hide);
		}

		ApplyPrefs();
		// The brand mark sat on the map's grid numbers (first-2-hours review C6): off while the map is open.
		if (m_wBrand)
		{
			bool brandOn = LF_HudLogic.BrandVisible(LF_HudPrefs.Get().IsOn(LF_HudPrefs.PART_BRAND), mapOpen);
			if (brandOn != m_wBrand.IsVisible())
				m_wBrand.SetVisible(brandOn);
		}

		m_bMoneyForced = inventoryOpen || mapOpen;
		PollBank();
		PollWanted();
		PollNeeds();
		UpdateMoneyOpacity();
	}

	//---------------------------------------------------------------------
	protected void ApplyPrefs()
	{
		LF_HudPrefs prefs = LF_HudPrefs.Get();
		string line = prefs.ToLine();
		if (line == m_sPrefsLine)
			return;

		m_sPrefsLine = line;
		if (m_wBrand) m_wBrand.SetVisible(prefs.IsOn(LF_HudPrefs.PART_BRAND));
		if (m_wMoneyBlock) m_wMoneyBlock.SetVisible(prefs.IsOn(LF_HudPrefs.PART_MONEY));
		if (m_wMoneyPlate) m_wMoneyPlate.SetVisible(prefs.IsOn(LF_HudPrefs.PART_MONEY));
		if (m_wWanted) m_wWanted.SetVisible(prefs.IsOn(LF_HudPrefs.PART_WANTED));
		if (m_wNeeds) m_wNeeds.SetVisible(prefs.IsOn(LF_HudPrefs.PART_NEEDS));
	}

	//---------------------------------------------------------------------
	protected void PollBank()
	{
		int bank = m_Character.LF_GetHudBank();
		if ((m_bBankSeen && bank == m_iBank) || !m_wBank)
			return;

		bool first = !m_bBankSeen;
		m_bBankSeen = true;
		m_iBank = bank;
		string amount = LF_HudLogic.BankAmountText(bank);
		if (amount.IsEmpty())
			m_wBank.SetText("");
		else
			m_wBank.SetText(WidgetManager.Translate("#LF-Hud_Bank", amount));

		if (!first)
			WakeMoney();
	}

	//---------------------------------------------------------------------
	protected void PollWanted()
	{
		if (!m_wWanted)
			return;

		int level = LF_AccountStateBits.GetWantedLevel(m_Character.LF_GetAccountState());
		if (level == m_iWanted)
			return;

		bool rose = m_iWanted >= 0 && LF_HudLogic.WantedRose(m_iWanted, level);
		m_iWanted = level;
		int filled = LF_HudLogic.WantedPips(level);
		foreach (int i, Widget pip : m_aWantedPips)
		{
			if (i < filled)
				pip.SetOpacity(PIP_FILLED_OPACITY);
			else
				pip.SetOpacity(PIP_EMPTY_OPACITY);
		}

		if (filled > 0)
			AnimateWidget.Opacity(m_wWanted, 1, UIConstants.FADE_RATE_FAST);
		else
			AnimateWidget.Opacity(m_wWanted, 0, UIConstants.FADE_RATE_SLOW);

		if (rose)
		{
			// The pulse: the badges flash red with the caption, then settle back to white.
			TintPips(Color.FromRGBA(235, 95, 85, 255));
			if (m_wWantedCaption)
				AnimateWidget.Opacity(m_wWantedCaption, 1, UIConstants.FADE_RATE_FAST);
			m_fWantedCaptionAge = 0;
		}
	}

	//---------------------------------------------------------------------
	protected void TintPips(Color color)
	{
		foreach (Widget pip : m_aWantedPips)
			AnimateWidget.Color(pip, color, UIConstants.FADE_RATE_DEFAULT);
	}

	//---------------------------------------------------------------------
	protected void PollNeeds()
	{
		if (!m_Survival || !m_wNeeds)
			return;

		int hunger = m_Survival.GetReplicatedHunger();
		int thirst = m_Survival.GetReplicatedThirst();
		if (hunger != m_iHunger || thirst != m_iThirst)
		{
			if (m_iHunger >= 0 && (hunger > m_iHunger || thirst > m_iThirst))
				m_fNeedRiseAge = 0;

			m_iHunger = hunger;
			m_iThirst = thirst;
			SetNeed(m_wFoodValue, m_wFoodIcon, hunger);
			SetNeed(m_wDrinkValue, m_wDrinkIcon, thirst);
		}

		bool show = LF_HudLogic.NeedsVisible(m_iHunger, m_iThirst, m_fNeedRiseAge);
		if (show == m_bNeedsShown)
			return;

		m_bNeedsShown = show;
		if (show)
			AnimateWidget.Opacity(m_wNeeds, 1, UIConstants.FADE_RATE_DEFAULT);
		else
			AnimateWidget.Opacity(m_wNeeds, 0, UIConstants.FADE_RATE_SLOW);
	}

	//---------------------------------------------------------------------
	//! One need: its figure and icon. Under NEED_SHOW_BELOW the pair reads full strength (red once low); a need
	//! still above it fades back, so the icon that matters is the one the eye finds (first-2-hours review C15).
	protected void SetNeed(TextWidget value, ImageWidget icon, int points)
	{
		if (!value)
			return;

		value.SetText(LF_HudLogic.NeedText(points));
		Color tint = Color.FromRGBA(245, 242, 235, 255);
		if (LF_HudLogic.NeedLow(points))
			tint = Color.FromRGBA(235, 95, 85, 255);

		value.SetColor(tint);
		float opacity = LF_HudLogic.NeedOpacity(points);
		value.SetOpacity(opacity);
		if (icon)
		{
			icon.SetColor(tint);
			icon.SetOpacity(opacity);
		}
	}

	//---------------------------------------------------------------------
	protected void WakeMoney()
	{
		m_fMoneyIdle = 0;
		UpdateMoneyOpacity();
	}

	//---------------------------------------------------------------------
	protected void UpdateMoneyOpacity()
	{
		if (!m_wMoneyBlock)
			return;

		float target = LF_HudLogic.MoneyOpacity(m_fMoneyIdle, m_bMoneyForced);
		if (target == m_fMoneyOpacity)
			return;

		m_fMoneyOpacity = target;
		AnimateWidget.Opacity(m_wMoneyBlock, target, UIConstants.FADE_RATE_SLOW);
		if (m_wMoneyPlate)
			AnimateWidget.Opacity(m_wMoneyPlate, target, UIConstants.FADE_RATE_SLOW);

		if (m_wWanted && m_iWanted > 0)
			AnimateWidget.Opacity(m_wWanted, Math.Max(target, 0.7), UIConstants.FADE_RATE_SLOW);
	}

	//---------------------------------------------------------------------
	//! The delta glides toward the cash figure, holds, then fades.
	protected void TickDelta(float timeSlice)
	{
		m_fDeltaAge += timeSlice;
		if (!m_wCashDelta)
		{
			m_fDeltaAge = -1;
			return;
		}

		if (m_fDeltaAge <= DELTA_SLIDE_SECONDS)
		{
			float left = 1 - m_fDeltaAge / DELTA_SLIDE_SECONDS;	// lf-float: UI seconds
			AlignableSlot.SetPadding(m_wCashDelta, 0, 0, DELTA_GAP_PIXELS + DELTA_SLIDE_PIXELS * left, 0);
			return;
		}

		if (m_fDeltaAge >= LF_HudLogic.DeltaSeconds())
		{
			m_fDeltaAge = -1;
			AlignableSlot.SetPadding(m_wCashDelta, 0, 0, DELTA_GAP_PIXELS, 0);
			AnimateWidget.Opacity(m_wCashDelta, 0, UIConstants.FADE_RATE_SLOW);
		}
	}

	//---------------------------------------------------------------------
	void ShowStatsHUD(bool show)
	{
		AnimateWidget.Opacity(m_wPlayerStatsHUD, show, UIConstants.FADE_RATE_SLOW);

		if (m_bUsingProgressWidget)
		{
			AnimateWidget.Opacity(m_wStaminaProgress, show, UIConstants.FADE_RATE_SLOW);
		}
	}

	//---------------------------------------------------------------------
	void SetProgressColor(Widget bar, float value)
	{
		if (value >= 0.65)
		{
			bar.SetColor(Color.White);
		}
		else if (value > 0.35 && value < 0.65)
		{
			bar.SetColor(Color.Yellow);
		}
		else
		{
			bar.SetColor(Color.Red);
		}
	}
}
