class LF_GatherAction : ScriptedUserAction
{
	//! The system a server switches this action off with (#1075).
	static const string SYSTEM = "Gathering";

	[Attribute("", UIWidgets.EditBox, "Chain node id. When set, tool, yield, and cooldown come from ResourceChains.conf", category: "General")]
	protected string m_sNodeId;

	[Attribute(desc: "Prefab what item is gathered", category: "General")]
	protected ResourceName m_GatherItemPrefab;

	[Attribute(defvalue:"1", desc: "Amount of items to receive", category: "General")]
	protected int m_GatherAmount;

	[Attribute(desc: "Item required for gathering", category: "Requirements")]
	protected ResourceName m_GatherToolRequirement;

	[Attribute(desc: "Check entire inventory for required item too", category: "Requirements")]
	protected bool m_CheckInventoryForToolRequirement;

	[Attribute(desc: "Maximum amount of times to gather before resource has timeout. 0 = unlimited", category: "Limits")]
	protected int m_GatherAmountMax;

	[Attribute(desc: "Cooldown until gathering attempts are restocked, in milliseconds", category: "Limits")]
	protected float m_GatherTimeout;

	[Attribute("#LF-Gather_Action", UIWidgets.EditBox, "Prompt, %1 is the item name. Gather %1 by default; a seed crate says Take %1", category: "General")]
	protected string m_sActionFormat;

	[Attribute(desc: "Stock model: milliseconds for one gather to grow back. The spot starts full at the gather maximum and refills one at a time. 0 = the cooldown model above", category: "Limits")]
	protected float m_GatherRegrowMs;

	protected int m_iRemainingGathers;
	protected WorldTimestamp m_NextRestock;

	//! Stock model state (#2409). Server only: only the peer that applies the action touches it.
	protected ref LF_GatherStock m_Stock;
	protected WorldTimestamp m_StockEpoch;

	protected string m_sDisplayName;

	//! Node row of the last catalog apply, and the speed factor of the tool the user held at the last check.
	protected ref LF_ResourceNodeDef m_Node;
	protected float m_fToolSpeed = 1.0;

	//------------------------------------------------------------------------------------------------
	// User has performed the action
	// play a pickup sound and then add the correct amount to the users inventory
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!pOwnerEntity || !LF_NetworkUtils.ShouldApplyAction(pOwnerEntity)) return;
		if (LF_Systems.Refuses(SYSTEM, "gather " + m_GatherItemPrefab))
			return;
		if (!LF_NetworkUtils.IsOwner(pOwnerEntity))
			LF_Debug.Warn("Gathering", string.Format("gather applied without replication owner on %1", pOwnerEntity.GetName()));
		if (!ApplyNodeFromCatalog())
			return;
		if (!IsGatherRequestConfigured(m_GatherItemPrefab, m_GatherAmount))
		{
			LF_Debug.Error("Gathering", "rejected gather: item prefab or amount is not configured");
			return;
		}

		// Server-side re-validation: the client's CanBePerformedScript is not a gate.
		if (!LF_CanGatherServer(pUserEntity))
		{
			LF_Debug.Warn("Gathering", string.Format("rejected gather (tool/cooldown) prefab=%1", m_GatherItemPrefab));
			return;
		}

		// Stock model: take the gather before the grant, so a rejected one costs nothing.
		if (!TakeStock(pOwnerEntity))
		{
			RefuseEmpty(pOwnerEntity, pUserEntity);
			return;
		}

		SCR_InventoryStorageManagerComponent inventoryManager = LF_Component<SCR_InventoryStorageManagerComponent>.Find(pUserEntity);
		int added = LF_InventoryUtils.AddAmount(inventoryManager, m_GatherItemPrefab, m_GatherAmount);
		if (added > 0)
		{
			inventoryManager.RpcAsk_PlaySound(LF_NetworkUtils.GetRplId(pUserEntity), "SOUND_PICK_UP");
			LF_Debug.Log("Gathering", string.Format("gathered %1 x%2", m_GatherItemPrefab, added));
			// The level source awards XP from this event; the toast reports what it actually paid.
			string uid = LF_Utils.GetPlayerUID(pUserEntity);
			string family = LF_GatherTools.JobFamily(m_Node);
			string xpJob = FamilyJob(family);
			int xpBefore = LF_JobLevels.XpOf(uid, xpJob);
			LF_EventBus.Publish(LF_OnGathered.Create(uid, m_sNodeId, family, m_GatherItemPrefab, added, ToolSpeedFor(pUserEntity)));
			int xpPaid = 0;
			if (!xpJob.IsEmpty())
				xpPaid = LF_JobLevels.XpOf(uid, xpJob) - xpBefore;
			NotifyGathered(pUserEntity, m_GatherItemPrefab, added, xpPaid);

			// Notify job manager for reward
			LF_JobManager jobManager = LF_JobManager.GetInstance();
			if (jobManager)
			{
				jobManager.OnGatherCompleted(pUserEntity, m_GatherItemPrefab);
			}
		}
		else
		{
			LF_Debug.Error("Gathering", string.Format("gather grant failed prefab=%1 requested=%2", m_GatherItemPrefab, m_GatherAmount));
			GiveBackStock(pOwnerEntity);
		}

		// No limit configured, or the stock model took its gather above: nothing to consume
		if (m_GatherAmountMax <= 0 || m_GatherRegrowMs > 0) return;

		//Replenish gathering count
		if(m_iRemainingGathers <= 0) m_iRemainingGathers = m_GatherAmountMax;

		//Consume one gathering
		m_iRemainingGathers--;

		//Initalize timeout if resource depleted
		if(m_iRemainingGathers <= 0)
		{
			m_NextRestock = pOwnerEntity.GetWorld().GetTimestamp().PlusMilliseconds(m_GatherTimeout);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A server with Gathering off offers nothing on the resource (#1075).
	override bool CanBeShownScript(IEntity user)
	{
		return LF_Systems.IsEnabled(SYSTEM);
	}

	//------------------------------------------------------------------------------------------------
	// Formats name for action when hovering
	override bool GetActionNameScript(out string outName)
	{
		if (!m_sDisplayName)
		{
			// UIInfo names are usually #keys; translate before formatting or the raw key shows.
			string itemName = "#LF-Gather_Unknown";
			UIInfo uiInfo = LF_UIInfoUtils.GetInfo(m_GatherItemPrefab);
			if (uiInfo && !uiInfo.GetName().IsEmpty())
				itemName = uiInfo.GetName();
			string format = m_sActionFormat;
			if (format.IsEmpty())
				format = "#LF-Gather_Action";
			m_sDisplayName = WidgetManager.Translate(format, WidgetManager.Translate(itemName));
		}

		outName = m_sDisplayName;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	// Checks if a required item has been set in the Editor
	// If so, check if its in the users inventory/hands depending on settings set
	override bool CanBePerformedScript(IEntity user)
  	{
		if (!ApplyNodeFromCatalog())
			return false;

		if (!IsGatherRequestConfigured(m_GatherItemPrefab, m_GatherAmount))
			return false;

		// Stock model: this peer only knows the stock when it applies actions (a host). A client
		// of a dedicated server gets the server's toast on the attempt instead.
		if (m_Stock && m_Stock.Available(StockNowMs(user)) <= 0)
		{
			SetCannotPerformReason("#LF-Gather_Empty");
			return false;
		}

		if (m_GatherRegrowMs <= 0 && m_GatherAmountMax > 0 && m_iRemainingGathers <= 0 && m_NextRestock.Greater(user.GetWorld().GetTimestamp()))
		{
			int secondsLeft = m_NextRestock.DiffMilliseconds(user.GetWorld().GetTimestamp()) / 1000;
			SetCannotPerformReason(WidgetManager.Translate("#LF-Gather_Wait", secondsLeft + 1)); //+1 to avoid 0 seconds left.
			return false;
		}

		// Crops need no tool: the stage decides. Past the final stage the crop cannot be gathered.
		LF_BaseCrop baseCrop = LF_BaseCrop.Cast(GetOwner());
		if (baseCrop)
			return baseCrop.CanGather();

		if (!LF_HasGatherTool(user))
		{
			SetCannotPerformReason(LF_GatherTools.RefusalKey(m_Node, m_CheckInventoryForToolRequirement));
			return false;
		}

		m_fToolSpeed = ToolSpeedFor(user);
		return true;
 	}

	//------------------------------------------------------------------------------------------------
	//! A better tool shortens the hold: the engine adds the time slice to the progress, so a speed
	//! factor above 1 makes the same hold finish sooner (#2706).
	override float GetActionProgressScript(float fProgress, float timeSlice)
	{
		return fProgress + timeSlice * m_fToolSpeed;
	}

	//------------------------------------------------------------------------------------------------
	//! Speed factor of the tool the user holds for this node: 1 when the node needs no tool.
	protected float ToolSpeedFor(IEntity user)
	{
		if (!LF_GatherTools.NeedsTool(m_Node))
			return 1.0;
		float speed = LF_GatherTools.Speed(m_Node, LF_GatherTools.HeldTool(user, m_Node, m_CheckInventoryForToolRequirement));
		if (speed <= 0)
			return 1.0;
		return speed;
	}

	//------------------------------------------------------------------------------------------------
	//! Server-side gate mirroring the client checks: cooldown and remaining gathers first,
	//! then the tool requirement. A forged perform cannot skip these.
	protected bool LF_CanGatherServer(IEntity user)
	{
		if (m_GatherRegrowMs <= 0 && m_GatherAmountMax > 0 && m_iRemainingGathers <= 0 && m_NextRestock.Greater(user.GetWorld().GetTimestamp()))
			return false;

		LF_BaseCrop baseCrop = LF_BaseCrop.Cast(GetOwner());
		if (baseCrop)
			return baseCrop.CanGather();

		return LF_HasGatherTool(user);
	}

	//------------------------------------------------------------------------------------------------
	//! True if the user holds the configured tool (right hand, left gadget, or
	//! anywhere in inventory when m_CheckInventoryForToolRequirement is set).
	protected bool LF_HasGatherTool(IEntity user)
	{
		if (LF_GatherTools.NeedsTool(m_Node))
			return !LF_GatherTools.HeldTool(user, m_Node, m_CheckInventoryForToolRequirement).IsEmpty();
		return HasToolIn(user, m_GatherToolRequirement, m_CheckInventoryForToolRequirement);
	}

	//------------------------------------------------------------------------------------------------
	//! Tool check behind LF_HasGatherTool. An empty requirement passes.
	static bool HasToolIn(IEntity user, ResourceName tool, bool checkInventory)
	{
		if (!tool) return true;

		CharacterControllerComponent characterController = LF_Component<CharacterControllerComponent>.Find(user);
		if (characterController)
		{
			IEntity rightHandItem = characterController.GetRightHandItem();
			if (LF_Utils.SamePrefab(LF_Utils.GetPrefabName(rightHandItem), tool)) return true;

			IEntity leftHandItem = characterController.GetAttachedGadgetAtLeftHandSlot();
			if (LF_Utils.SamePrefab(LF_Utils.GetPrefabName(leftHandItem), tool)) return true;
		}

		if (checkInventory)
		{
			if (LF_InventoryUtils.GetAmount(user, tool) > 0) return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	//! Copies tool, yield, and cooldown from the chain config when this action
	//! names a node. A missing or broken row logs and leaves the action unable
	//! to grant anything. An empty id keeps the prefab attributes.
	protected bool ApplyNodeFromCatalog()
	{
		if (m_sNodeId.IsEmpty())
			return true;

		LF_ResourceNodeDef node = LF_ResourceChainCatalog.FindNode(m_sNodeId);
		if (!LF_ResourceChainCatalog.IsNodeUsable(node))
		{
			LF_Debug.Error("Gathering", string.Format("rejected gather: node %1 is missing or its item prefab does not resolve", m_sNodeId));
			m_GatherItemPrefab = ResourceName.Empty;
			m_GatherAmount = 0;
			return false;
		}

		m_Node = node;
		m_GatherItemPrefab = node.m_ItemPrefab;
		m_GatherAmount = node.m_iYield;
		m_GatherToolRequirement = node.m_ToolPrefab;
		m_GatherAmountMax = node.m_iMaxGathers;
		m_GatherTimeout = node.m_fCooldownMs;
		m_GatherRegrowMs = node.m_fRegrowMs;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Milliseconds on this spot's own clock: world time since the spot was first touched.
	protected float StockNowMs(IEntity any)
	{
		WorldTimestamp now = any.GetWorld().GetTimestamp();
		if (!m_StockEpoch)
			m_StockEpoch = now;
		return now.DiffMilliseconds(m_StockEpoch);
	}

	//------------------------------------------------------------------------------------------------
	//! Stock model (#2409): true when this gather may go ahead, and the stock drops by one.
	//! Spots on the cooldown model, or with no limit, always pass.
	protected bool TakeStock(IEntity owner)
	{
		if (m_GatherRegrowMs <= 0 || m_GatherAmountMax <= 0)
			return true;

		float nowMs = StockNowMs(owner);
		if (!m_Stock)
			m_Stock = LF_GatherStock.Create(m_GatherAmountMax, m_GatherRegrowMs, nowMs);

		if (!m_Stock.Take(nowMs))
			return false;

		LF_Debug.Info("Gathering", string.Format("%1 stock %2/%3", owner.GetName(), m_Stock.Available(nowMs), m_GatherAmountMax));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A grant that failed (full pockets) puts its gather back, so a refusal never eats stock.
	protected void GiveBackStock(IEntity owner)
	{
		if (m_Stock)
			m_Stock.GiveBack();
	}

	//------------------------------------------------------------------------------------------------
	//! The spot is empty: the player is told, with the time to the next gather.
	protected void RefuseEmpty(IEntity owner, IEntity user)
	{
		float nowMs = StockNowMs(owner);
		int minutes = Math.Ceil(m_Stock.MsUntilNext(nowMs) / 60000);
		if (minutes < 1)
			minutes = 1;

		LF_Debug.Warn("Gathering", string.Format("rejected gather: %1 is empty, next in %2 min", owner.GetName(), minutes));
		LF_Utils.NotifyEntity(user, "#LF-Gather_EmptyWait", "", 4.0, minutes.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! The job a gather family pays XP into, or "" when the family pays none (digging, foraging).
	static string FamilyJob(string family)
	{
		string action = LF_JobLevelSources.ActionForFamily(family);
		if (action.IsEmpty())
			return "";

		LF_JobsConfig config = LF_JobsConfig.Get();
		if (!config)
			return "";

		LF_JobXpAwardDef award = config.FindAward(action);
		if (!award)
			return "";

		return award.m_sJob;
	}

	//------------------------------------------------------------------------------------------------
	//! \return What a trader pays now for one unit of `good`, from the market config; 0 when the market
	//! does not list it. A live curve price wins over the base.
	protected static int MarketPrice(ResourceName good)
	{
		LF_MarketConfig config = LF_MarketConfig.Get();
		if (!config)
			return 0;

		LF_MarketGoodDef def = config.FindGood(good);
		if (!def)
			return 0;

		int live = LF_MarketBook.CurrentPrice(good);
		if (live > 0)
			return live;

		return def.m_iBase;
	}

	//------------------------------------------------------------------------------------------------
	//! \return What a trader pays for one `item`: its own market row, else the row of the first
	//! product a processing machine makes from it (the bar for an ore). 0 when no trader buys it.
	//! \param[out] soldAs The item the price is for; empty when the price is 0.
	static int TraderPrice(ResourceName item, out ResourceName soldAs)
	{
		soldAs = ResourceName.Empty;
		int price = MarketPrice(item);
		if (price > 0)
		{
			soldAs = item;
			return price;
		}

		LF_ResourceChainConfig chains = LF_ResourceChainCatalog.GetConfig();
		if (!chains || !chains.m_aMachines)
			return 0;

		foreach (LF_ResourceMachineDef machine : chains.m_aMachines)
		{
			if (!machine || !machine.m_aInputs || !machine.m_aOutputs)
				continue;

			bool takesItem = false;
			foreach (LF_ProcessingInput input : machine.m_aInputs)
			{
				if (input && LF_Utils.SamePrefab(input.m_InputPrefab, item))
					takesItem = true;
			}

			if (!takesItem)
				continue;

			foreach (LF_ProcessingOutput output : machine.m_aOutputs)
			{
				if (!output)
					continue;

				price = MarketPrice(output.m_OutputPrefab);
				if (price > 0)
				{
					soldAs = output.m_OutputPrefab;
					return price;
				}
			}
		}

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The item's display name in the running language.
	static string ItemName(ResourceName prefab)
	{
		string name = "#LF-Gather_Unknown";
		UIInfo uiInfo = LF_UIInfoUtils.GetInfo(prefab);
		if (uiInfo && !uiInfo.GetName().IsEmpty())
			name = uiInfo.GetName();

		return LF_Terms.Text(name);
	}

	//------------------------------------------------------------------------------------------------
	//! The toast sentence for a gather: what came in, the XP when some was paid, and what a trader
	//! pays when one buys it (left out when `price` is 0).
	static string ToastText(string itemName, int amount, int xp, int price, string soldAsName)
	{
		string text;
		if (xp > 0)
			text = LF_Terms.Text("#LF-Gather_ToastXp", itemName, amount.ToString(), xp.ToString());
		else
			text = LF_Terms.Text("#LF-Gather_Toast", itemName, amount.ToString());

		return text + PriceSentence(price, soldAsName);
	}

	//------------------------------------------------------------------------------------------------
	//! " The trader pays $50 for Iron bar." or "" when `price` is 0.
	static string PriceSentence(int price, string soldAsName)
	{
		if (price <= 0)
			return "";

		return " " + LF_Terms.Text("#LF-Gather_ToastPrice", price.ToString(), soldAsName);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: one toast to the player for a finished gather.
	protected static void NotifyGathered(IEntity user, ResourceName item, int amount, int xp)
	{
		ResourceName soldAs;
		int price = TraderPrice(item, soldAs);
		string soldAsName = "";
		if (price > 0)
			soldAsName = ItemName(soldAs);

		LF_Utils.NotifyEntity(user, ToastText(ItemName(item), amount, xp, price, soldAsName), "", 4.0);
	}

	//------------------------------------------------------------------------------------------------
	//! Reject malformed gather actions before an attempt is consumed.
	static bool IsGatherRequestConfigured(ResourceName itemPrefab, int amount)
	{
		if (itemPrefab.IsEmpty())
			return false;
		return amount > 0;
	}
}
