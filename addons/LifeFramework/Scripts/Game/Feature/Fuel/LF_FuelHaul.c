//------------------------------------------------------------------------------------------------
//! What one depot load or one pump-in came to (#3330).
enum LF_EFuelHaul
{
	OK,
	OFF,
	NO_HAULER,
	UNFIT,
	NO_TANKER,
	TANKER_FULL,
	TANKER_EMPTY,
	NO_STATION,
	STATION_FULL,
	NO_MONEY,
	NOT_BOUGHT,
	FAILED
}

//------------------------------------------------------------------------------------------------
//! The outcome of `LF_FuelHaul.Buy` or `PumpIn`: the result, the litres that moved, the cash that was charged
//! (a load) or paid (a pump-in), and the litres left in the tanker. Zeros when refused.
class LF_FuelHaulResult
{
	LF_EFuelHaul m_eResult;
	int m_iLitres;
	int m_iCash;
	int m_iCargoAfter;

	//------------------------------------------------------------------------------------------------
	bool IsOk()
	{
		return m_eResult == LF_EFuelHaul.OK;
	}
}

//------------------------------------------------------------------------------------------------
//! The tanker haul (#3330, framework #788): the hauler pays the depot's wholesale price for a load into the
//! tanker's cargo tank, drives it to a station and pumps it into the station's stock, where it is paid the
//! delivery price per litre that arrived. All of it runs on the server and works out litres and cash itself;
//! a client names nothing but the entity it used. The cargo tank is the tanker's fuel storage node (the
//! vanilla Ural tanker's cistern), never its engine tank.
class LF_FuelHaul
{
	static const string FEATURE = "Fuel";
	//! How far from a depot or a pump a tanker may stand, in metres: a Ural is long, the point is beside it.
	static const float REACH_M = 12; // lf-fixed: a truck's length, not a gameplay knob
	static const int MAX_DEPTH = 6; // lf-fixed: how deep the tanker's slotted parts are searched
	static const string CASH_REASON = "fuel-haul";

	protected static ref LF_FuelChargeLedger s_BuyLedger;
	protected static ref LF_FuelChargeLedger s_PayLedger;
	//! Litres bought at a depot and still in each tanker, by tanker. Only these are paid at a station: a vanilla
	//! Ural spawns with 900 l in its cistern and a vanilla pump refills it for free, and neither is a load.
	//! Unsaved by design: LF does not save vehicle fuel, so a tanker after a restart is a fresh spawn.
	protected static ref map<EntityID, int> s_mBought;
	protected static IEntity s_pFoundTanker;
	protected static vector s_vOrigin;
	protected static float s_fBest;

	//------------------------------------------------------------------------------------------------
	//! Fills `nodes` with the cargo tanks of `entity` and every part slotted into it.
	static void CargoNodes(IEntity entity, notnull array<SCR_FuelNode> nodes, int depth = 0)
	{
		if (!entity || depth > MAX_DEPTH)
			return;

		SCR_FuelManagerComponent manager = SCR_FuelManagerComponent.Cast(entity.FindComponent(SCR_FuelManagerComponent));
		if (manager)
		{
			array<BaseFuelNode> all = {};
			manager.GetFuelNodesList(all);
			foreach (BaseFuelNode node : all)
			{
				SCR_FuelNode scripted = SCR_FuelNode.Cast(node);
				if (scripted && scripted.HasAllTypeFlags(SCR_EFuelNodeTypeFlag.IS_FUEL_STORAGE))
					nodes.Insert(scripted);
			}
		}

		IEntity child = entity.GetChildren();
		while (child)
		{
			CargoNodes(child, nodes, depth + 1);
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Whole litres in the tanker's cargo tanks (rounded down); 0 for a vehicle with no cargo tank.
	static int CargoLitres(IEntity tanker)
	{
		array<SCR_FuelNode> nodes = {};
		CargoNodes(tanker, nodes);
		float litres;
		foreach (SCR_FuelNode node : nodes)
		{
			litres += node.GetFuel();
		}

		int whole = Math.Floor(litres);
		return whole;
	}

	//------------------------------------------------------------------------------------------------
	//! Whole litres of free room in the tanker's cargo tanks (rounded down).
	static int CargoRoom(IEntity tanker)
	{
		array<SCR_FuelNode> nodes = {};
		CargoNodes(tanker, nodes);
		float room;
		foreach (SCR_FuelNode node : nodes)
		{
			room += node.GetMaxFuel() - node.GetFuel();
		}

		int whole = Math.Floor(room);
		return whole;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts up to `litres` into the cargo tanks, one after the other. \return the litres that went in.
	static int AddCargo(IEntity tanker, int litres)
	{
		array<SCR_FuelNode> nodes = {};
		CargoNodes(tanker, nodes);
		float left = litres;
		foreach (SCR_FuelNode node : nodes)
		{
			if (left <= 0)
				break;

			float add = Math.Min(left, node.GetMaxFuel() - node.GetFuel());
			if (add <= 0)
				continue;

			node.SetFuel(node.GetFuel() + add);
			left -= add;
		}

		int unfilled = Math.Round(left);
		return litres - unfilled;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes up to `litres` out of the cargo tanks. \return the litres that came out.
	static int TakeCargo(IEntity tanker, int litres)
	{
		array<SCR_FuelNode> nodes = {};
		CargoNodes(tanker, nodes);
		float left = litres;
		foreach (SCR_FuelNode node : nodes)
		{
			if (left <= 0)
				break;

			float take = Math.Min(left, node.GetFuel());
			if (take <= 0)
				continue;

			node.SetFuel(node.GetFuel() - take);
			left -= take;
		}

		int missing = Math.Round(left);
		return litres - missing;
	}

	//------------------------------------------------------------------------------------------------
	//! Litres of `tanker`'s load that were bought at a depot (0 for a tanker that never loaded).
	static int BoughtLitres(IEntity tanker)
	{
		if (!tanker || !s_mBought)
			return 0;

		int litres;
		if (!s_mBought.Find(tanker.GetID(), litres))
			return 0;

		return litres;
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddBought(IEntity tanker, int litres)
	{
		if (!tanker)
			return;

		if (!s_mBought)
			s_mBought = new map<EntityID, int>();

		int now = LF_Utils.MaxInt(0, BoughtLitres(tanker) + litres);
		if (now > 0)
			s_mBought.Set(tanker.GetID(), now);
		else
			s_mBought.Remove(tanker.GetID());
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_FuelHaulResult Refuse(LF_EFuelHaul code, string why)
	{
		LF_FuelHaulResult result = new LF_FuelHaulResult();
		result.m_eResult = code;
		LF_Debug.Info(FEATURE, string.Format("fuel haul refused: %1", why));
		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! The refusals every haul step shares: fuel stock off, no hauler, a dead, downed or cuffed hauler,
	//! no tanker. Null when the step may go ahead.
	protected static LF_FuelHaulResult CheckHaul(LF_FuelConfig config, IEntity tanker, IEntity hauler)
	{
		if (!config || !config.m_bEnabled)
			return Refuse(LF_EFuelHaul.OFF, "fuel stock is off on this server");
		if (!hauler)
			return Refuse(LF_EFuelHaul.NO_HAULER, "no hauler");
		// A dead, downed or cuffed hauler hauls nothing (a modified client can send the action anyway).
		if (!LF_Utils.IsAliveState(hauler) || LF_Downed.IsDowned(hauler) || LF_Restraint.IsRestrained(hauler))
			return Refuse(LF_EFuelHaul.UNFIT, "the hauler is dead, downed or cuffed");
		if (!tanker)
			return Refuse(LF_EFuelHaul.NO_TANKER, "no tanker at the point");

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: loads `tanker` at the depot for `hauler`: the configured litres per delivery, cut by the
	//! tanker's free cargo room and by the hauler's cash, charged at the wholesale price for the litres
	//! that went in. Takes the cash first, then fills the tank; a refusal changes nothing.
	static LF_FuelHaulResult Buy(IEntity tanker, IEntity hauler)
	{
		LF_FuelConfig config = LF_FuelConfig.Get();
		LF_FuelHaulResult refused = CheckHaul(config, tanker, hauler);
		if (refused)
			return refused;

		// Another hauler's tanker (they bought its load and hold its wheel) is not yours to load.
		LF_VehicleLockComponent heldLock = LF_Component<LF_VehicleLockComponent>.Find(tanker);
		int heldBy = -1;
		if (heldLock)
			heldBy = heldLock.GetJobDriverPlayerId();
		// A holder who left the server holds nothing.
		if (heldBy > 0 && !GetGame().GetPlayerManager().GetPlayerControlledEntity(heldBy))
			heldBy = -1;
		if (heldBy > 0 && heldBy != LF_PlayerInteraction.PlayerIdOf(hauler))
			return Refuse(LF_EFuelHaul.NO_TANKER, string.Format("the tanker is held by player %1's haul", heldBy));

		int room = CargoRoom(tanker);
		if (room < 1)
		{
			if (CargoNodesCount(tanker) < 1)
				return Refuse(LF_EFuelHaul.NO_TANKER, "the vehicle has no cargo tank");

			return Refuse(LF_EFuelHaul.TANKER_FULL, "the tanker has no room for a litre");
		}

		if (!s_BuyLedger)
			s_BuyLedger = new LF_FuelChargeLedger();

		// The depot never runs dry: a stock at the rules' largest size stands for it.
		LF_FuelStock depot = LF_FuelStock.Make(LF_FuelStockRules.MAX_LITRES, LF_FuelStockRules.MAX_LITRES);
		int wanted = LF_Utils.MinInt(config.m_iLitresPerDelivery, room);
		string key = LF_Utils.GetPlayerUid(hauler);
		int cash = LF_MoneyUtils.GetCash(hauler);
		// No cash at all buys nothing (the rules would let a litre ride on the carried fraction).
		if (cash < 1)
			return Refuse(LF_EFuelHaul.NO_MONEY, "the hauler has no cash");

		LF_FuelPlan plan = LF_FuelStockRules.PlanSale(depot, wanted, cash, config.m_iWholesalePerLitre, s_BuyLedger.Carry(key), 1);
		if (!plan.IsOk())
		{
			if (plan.m_eResult == LF_EFuelResult.NO_MONEY)
				return Refuse(LF_EFuelHaul.NO_MONEY, "the hauler cannot pay for a litre");
			if (plan.m_eResult == LF_EFuelResult.OFF)
				return Refuse(LF_EFuelHaul.OFF, "the wholesale price is 0, loads are off");

			return Refuse(LF_EFuelHaul.FAILED, string.Format("the load plan was refused (%1)", plan.m_eResult));
		}

		if (plan.m_iCash > 0)
		{
			int taken = LF_MoneyUtils.RemoveCash(hauler, plan.m_iCash, CASH_REASON);
			if (taken != plan.m_iCash)
			{
				// Short of the cash we counted a moment ago: give back what was taken and load nothing.
				if (taken > 0)
					LF_MoneyUtils.AddCash(hauler, taken, CASH_REASON);

				return Refuse(LF_EFuelHaul.FAILED, string.Format("took %1 of %2 cash, load undone", taken, plan.m_iCash));
			}
		}

		int added = AddCargo(tanker, plan.m_iLitres);
		if (added < 1)
		{
			if (plan.m_iCash > 0)
				LF_MoneyUtils.AddCash(hauler, plan.m_iCash, CASH_REASON);

			return Refuse(LF_EFuelHaul.FAILED, "the cargo tank took nothing, load undone");
		}

		if (added != plan.m_iLitres)
			LF_Debug.Warn(FEATURE, string.Format("fuel load: planned %1 litres, the tank took %2", plan.m_iLitres, added));

		s_BuyLedger.Commit(key, plan);
		AddBought(tanker, added);
		// No free cars: a bought load gives the (locked) depot tanker to this hauler; nobody else gets in.
		LF_VehicleLockComponent tankerLock = LF_Component<LF_VehicleLockComponent>.Find(tanker);
		if (tankerLock)
			tankerLock.SetJobDriver(LF_PlayerInteraction.PlayerIdOf(hauler));
		LF_FuelHaulResult result = new LF_FuelHaulResult();
		result.m_eResult = LF_EFuelHaul.OK;
		result.m_iLitres = added;
		result.m_iCash = plan.m_iCash;
		result.m_iCargoAfter = CargoLitres(tanker);
		LF_Debug.Info(FEATURE, string.Format("fuel loaded: %1 litres for %2, %3 litres in the tanker", added, plan.m_iCash, result.m_iCargoAfter));
		return result;
	}

	//------------------------------------------------------------------------------------------------
	protected static int CargoNodesCount(IEntity tanker)
	{
		array<SCR_FuelNode> nodes = {};
		CargoNodes(tanker, nodes);
		return nodes.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: pumps the tanker's load into the station `stationId`. Only the station's free room lands, the
	//! hauler is paid the delivery price for the litres that landed, and the rest stays in the tanker. A full
	//! station and an empty tanker are refused and change nothing.
	static LF_FuelHaulResult PumpIn(string stationId, IEntity tanker, IEntity hauler)
	{
		LF_FuelConfig config = LF_FuelConfig.Get();
		LF_FuelHaulResult refused = CheckHaul(config, tanker, hauler);
		if (refused)
			return refused;

		int cargo = CargoLitres(tanker);
		if (cargo < 1)
			return Refuse(LF_EFuelHaul.TANKER_EMPTY, "the tanker holds no fuel");

		// Only litres bought at a depot are a load: the spawn fill and a free vanilla refill are not paid.
		int offered = LF_Utils.MinInt(cargo, BoughtLitres(tanker));
		if (offered < 1)
			return Refuse(LF_EFuelHaul.NOT_BOUGHT, string.Format("none of the tanker's %1 litres was bought at a depot", cargo));

		LF_FuelStock stock = LF_FuelStationComponent.StockFor(stationId);
		if (!stock)
			return Refuse(LF_EFuelHaul.NO_STATION, string.Format("station '%1' has no stock", stationId));

		if (!s_PayLedger)
			s_PayLedger = new LF_FuelChargeLedger();

		string key = LF_Utils.GetPlayerUid(hauler);
		LF_FuelPlan plan = LF_FuelStockRules.PlanDelivery(stock, offered, config.m_iDeliveryPayPerLitre, s_PayLedger.Carry(key));
		if (!plan.IsOk())
		{
			if (plan.m_eResult == LF_EFuelResult.FULL)
				return Refuse(LF_EFuelHaul.STATION_FULL, string.Format("station '%1' is full", stationId));

			return Refuse(LF_EFuelHaul.FAILED, string.Format("the delivery plan was refused (%1)", plan.m_eResult));
		}

		// Take the litres out first, then pay for what actually came out.
		int taken = TakeCargo(tanker, plan.m_iLitres);
		if (taken < 1)
			return Refuse(LF_EFuelHaul.FAILED, "the cargo tank gave nothing, delivery undone");

		if (taken != plan.m_iLitres)
		{
			LF_Debug.Warn(FEATURE, string.Format("fuel delivery: planned %1 litres, the tank gave %2", plan.m_iLitres, taken));
			plan = LF_FuelStockRules.PlanDelivery(stock, taken, config.m_iDeliveryPayPerLitre, s_PayLedger.Carry(key));
			if (!plan.IsOk())
			{
				AddCargo(tanker, taken);
				return Refuse(LF_EFuelHaul.FAILED, "the delivery could not be re-planned, litres put back");
			}
		}

		// Taken came out of the bought load: forget those litres before anything is paid. Litres the re-plan did
		// not land go back into the tanker as bought.
		AddBought(tanker, -taken);
		if (taken > plan.m_iLitres)
		{
			int back = AddCargo(tanker, taken - plan.m_iLitres);
			AddBought(tanker, back);
		}

		LF_FuelStockRules.ApplyDelivery(stock, plan);
		if (plan.m_iCash > 0)
			LF_MoneyUtils.AddCash(hauler, plan.m_iCash, CASH_REASON);

		s_PayLedger.Commit(key, plan);
		LF_FuelHaulResult result = new LF_FuelHaulResult();
		result.m_eResult = LF_EFuelHaul.OK;
		result.m_iLitres = plan.m_iLitres;
		result.m_iCash = plan.m_iCash;
		result.m_iCargoAfter = CargoLitres(tanker);
		LF_Debug.Info(FEATURE, string.Format("fuel delivered to '%1': %2 litres paid %3, %4 litres in the station, %5 left in the tanker", stationId, plan.m_iLitres, plan.m_iCash, stock.m_iLitres, result.m_iCargoAfter));
		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! The tanker (a vehicle with a cargo tank) standing nearest to `point` within reach, or null.
	static IEntity FindTankerNear(IEntity point)
	{
		if (!point || !GetGame() || !GetGame().GetWorld())
			return null;

		s_pFoundTanker = null;
		s_vOrigin = point.GetOrigin();
		s_fBest = REACH_M;
		GetGame().GetWorld().QueryEntitiesBySphere(s_vOrigin, REACH_M, CollectTanker, null, EQueryEntitiesFlags.ALL);
		IEntity found = s_pFoundTanker;
		s_pFoundTanker = null;
		return found;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CollectTanker(IEntity entity)
	{
		Vehicle vehicle = Vehicle.Cast(entity);
		if (!vehicle)
			return true;

		float dist = vector.Distance(vehicle.GetOrigin(), s_vOrigin);
		if (dist >= s_fBest || CargoNodesCount(vehicle) < 1)
			return true;

		s_fBest = dist;
		s_pFoundTanker = vehicle;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The localization key of the line a refused haul step prints.
	static string RefusalKey(LF_EFuelHaul code)
	{
		if (code == LF_EFuelHaul.NO_MONEY)
			return "#LF-Fuel_NoMoney";
		if (code == LF_EFuelHaul.NO_TANKER)
			return "#LF-FuelHaul_NoTanker";
		if (code == LF_EFuelHaul.TANKER_FULL)
			return "#LF-FuelHaul_TankerFull";
		if (code == LF_EFuelHaul.TANKER_EMPTY)
			return "#LF-FuelHaul_TankerEmpty";
		if (code == LF_EFuelHaul.STATION_FULL)
			return "#LF-FuelHaul_StationFull";
		if (code == LF_EFuelHaul.NOT_BOUGHT)
			return "#LF-FuelHaul_NotBought";

		return "#LF-Fuel_Failed";
	}

	//------------------------------------------------------------------------------------------------
	//! Tells the hauler how a step went. `okKey` is the line for a success, `%1` the litres and cash.
	static void Tell(IEntity hauler, LF_FuelHaulResult result, string okKey)
	{
		if (!hauler || !result || !GetGame())
			return;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;

		int playerId = playerManager.GetPlayerIdFromControlledEntity(hauler);
		if (playerId <= 0)
			return;

		string message = RefusalKey(result.m_eResult);
		LF_ENotificationType type = LF_ENotificationType.WARNING;
		string arg;
		if (result.IsOk())
		{
			message = okKey;
			type = LF_ENotificationType.INFO;
			arg = string.Format("%1 L, %2", result.m_iLitres, LF_MoneyFormat.FormatMoney(result.m_iCash));
		}

		LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Fuel_Title", message, 5.0, type, arg);
	}

	//------------------------------------------------------------------------------------------------
	//! World end and tests: forget every owed fraction.
	static void ResetStatics()
	{
		s_BuyLedger = null;
		s_PayLedger = null;
		s_mBought = null;
		s_pFoundTanker = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_FuelHaul's statics.
class LF_FuelHaul_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_FuelHaul";
	}

	override void Apply()
	{
		LF_FuelHaul.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! "Pump into the station tanks" on a pump of a station that sells from stock (#3330). The server finds the
//! tanker beside the pump and does the whole delivery in `LF_FuelHaul.PumpIn`; the action carries no litres.
class LF_FuelHaulPumpAction : ScriptedUserAction
{
	protected LF_FuelStationComponent m_Station;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Station = LF_FuelStationComponent.Cast(pOwnerEntity.FindComponent(LF_FuelStationComponent));
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return m_Station != null && m_Station.SellsFromStock();
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return m_Station != null && m_Station.SellsFromStock();
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "#LF-FuelHaul_PumpIn";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		if (!m_Station)
		{
			LF_Debug.Error(LF_FuelHaul.FEATURE, "fuel pump-in action on an entity with no LF_FuelStationComponent");
			return;
		}

		LF_FuelHaulResult result = LF_FuelHaul.PumpIn(m_Station.GetStationId(), LF_FuelHaul.FindTankerNear(pOwnerEntity), pUserEntity);
		LF_FuelHaul.Tell(pUserEntity, result, "#LF-FuelHaul_Delivered");
	}
}
