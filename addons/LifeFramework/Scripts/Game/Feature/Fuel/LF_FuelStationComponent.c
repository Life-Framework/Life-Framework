// Derived from Overthrow (https://github.com/ArmaOverthrow/Overthrow.Arma4), used with the author's permission, relicensed MIT.
// Source: Scripts/Components/OVT_FuelRequestComponent.c (a server fill that never trusts a client litre count or price,
// the vehicle's fuel nodes topped up through SCR_FuelManagerComponent); the station stock, the cash and the refusals are ours.
//------------------------------------------------------------------------------------------------
//! What one fill at a pump came to.
enum LF_EFuelSale
{
	OK,
	OFF,
	NO_STATION,
	NO_VEHICLE,
	NO_BUYER,
	TANK_FULL,
	EMPTY,
	NO_MONEY,
	FAILED
}

//------------------------------------------------------------------------------------------------
//! The outcome of `LF_FuelStationComponent.Sell`: the result, and the litres and cash that moved (zero when refused).
class LF_FuelSaleResult
{
	LF_EFuelSale m_eResult;
	int m_iLitres;
	int m_iCash;

	//------------------------------------------------------------------------------------------------
	bool IsOk()
	{
		return m_eResult == LF_EFuelSale.OK;
	}
}

//------------------------------------------------------------------------------------------------
class LF_FuelStationComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! A fuel pump that sells from its station's stock (#3328, framework #788). Put it on a pump prefab beside
//! `LF_FuelBuyAction`. While the fuel config is on, a fill draws the station's litres (`LF_FuelStock`, one per
//! station id, shared by every pump of that station) and the buyer's cash, and the vanilla free refuel at this
//! pump is off (`SCR_FuelSupportStationComponent.IsValid` below). While the config is off (the core default) the
//! component does nothing and the pump behaves as vanilla.
//!
//! Server-authoritative: the action names the pump (its owner); the server finds the vehicle next to it and
//! works out the litres from that vehicle's free tank room, the price from the config and the cash from the
//! buyer's pockets. A client never sends a litre count or a price. The stock is not saved yet (the `fuelstock`
//! section is a later card): a restart starts every station at its configured starting litres.
class LF_FuelStationComponent : ScriptComponent
{
	static const string FEATURE = "Fuel";
	//! How far from the pump a vehicle may stand to be filled, in metres (the vanilla pump area is 6 m).
	static const float REACH_M = 6; // lf-fixed: the vanilla pump area, not a gameplay knob

	[Attribute("", UIWidgets.EditBox, "Station id; every pump with the same id shares one tank. Matches an m_sId row of the fuel config overlay")]
	protected string m_sStationId;

	protected static ref map<string, ref LF_FuelStock> s_mStocks;
	protected static ref LF_FuelChargeLedger s_Ledger;

	protected static IEntity s_pFoundVehicle;
	protected static vector s_vOrigin;
	protected static float s_fBest;

	//------------------------------------------------------------------------------------------------
	string GetStationId()
	{
		return m_sStationId;
	}

	//------------------------------------------------------------------------------------------------
	//! True when this pump sells from stock: the fuel config is loaded and on. Off means vanilla.
	bool SellsFromStock()
	{
		LF_FuelConfig config = LF_FuelConfig.Get();
		return config && config.m_bEnabled;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the entity is a pump of a station that sells from stock (the vanilla refuel there is off).
	static bool ReplacesVanilla(IEntity pump)
	{
		if (!pump)
			return false;

		LF_FuelStationComponent station = LF_FuelStationComponent.Cast(pump.FindComponent(LF_FuelStationComponent));
		return station && station.SellsFromStock();
	}

	//------------------------------------------------------------------------------------------------
	//! The stock of this station id, made from the config the first time it is asked for. Null with no config.
	static LF_FuelStock StockFor(string stationId)
	{
		LF_FuelConfig config = LF_FuelConfig.Get();
		if (!config)
			return null;

		if (!s_mStocks)
			s_mStocks = new map<string, ref LF_FuelStock>();

		LF_FuelStock stock;
		if (s_mStocks.Find(stationId, stock))
			return stock;

		stock = config.NewStationFor(stationId);
		s_mStocks.Set(stationId, stock);
		LF_Debug.Info(FEATURE, string.Format("fuel station '%1' opened with %2 of %3 litres", stationId, stock.m_iLitres, stock.m_iCapacity));
		return stock;
	}

	//------------------------------------------------------------------------------------------------
	//! Litres of free room in the tanks of this vehicle that can take fuel (whole litres, rounded down).
	//! Zero for an entity with no fuel manager.
	static int VehicleRoom(IEntity vehicle)
	{
		if (!vehicle)
			return 0;

		SCR_FuelManagerComponent manager = SCR_FuelManagerComponent.Cast(vehicle.FindComponent(SCR_FuelManagerComponent));
		if (!manager)
			return 0;

		array<BaseFuelNode> nodes = {};
		manager.GetFuelNodesList(nodes);
		float room;
		foreach (BaseFuelNode node : nodes)
		{
			if (!CanTakeFuel(node))
				continue;

			room += node.GetMaxFuel() - node.GetFuel();
		}

		int litres = Math.Floor(room);
		return litres;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CanTakeFuel(BaseFuelNode node)
	{
		if (!node)
			return false;

		// A scripted node says whether it takes fuel; a plain node is a tank.
		SCR_FuelNode scripted = SCR_FuelNode.Cast(node);
		if (scripted && !scripted.CanReceiveFuel())
			return false;

		return node.GetFuel() < node.GetMaxFuel();
	}

	//------------------------------------------------------------------------------------------------
	//! Puts `litres` into the vehicle's tanks, one tank after the other. \return the litres that fit.
	protected static int FillVehicle(IEntity vehicle, int litres)
	{
		SCR_FuelManagerComponent manager = SCR_FuelManagerComponent.Cast(vehicle.FindComponent(SCR_FuelManagerComponent));
		if (!manager)
			return 0;

		array<BaseFuelNode> nodes = {};
		manager.GetFuelNodesList(nodes);
		float left = litres;
		foreach (BaseFuelNode node : nodes)
		{
			if (left <= 0)
				break;
			if (!CanTakeFuel(node))
				continue;

			float add = Math.Min(left, node.GetMaxFuel() - node.GetFuel());
			node.SetFuel(node.GetFuel() + add);
			left -= add;
		}

		int unfilled = Math.Round(left);
		return litres - unfilled;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sells `buyer` a fill of `vehicle` at the station `stationId`: as many litres as the tank has room
	//! for, cut by the station's stock and by the buyer's cash. Takes the cash, then fills the tank, then takes
	//! the litres out of the station; a refusal changes nothing.
	static LF_FuelSaleResult Sell(string stationId, IEntity vehicle, IEntity buyer)
	{
		LF_FuelSaleResult result = new LF_FuelSaleResult();
		LF_FuelConfig config = LF_FuelConfig.Get();
		if (!config || !config.m_bEnabled)
			return Refuse(result, LF_EFuelSale.OFF, "fuel stock is off on this server");
		if (!buyer)
			return Refuse(result, LF_EFuelSale.NO_BUYER, "no buyer");
		if (!vehicle)
			return Refuse(result, LF_EFuelSale.NO_VEHICLE, "no vehicle at the pump");

		LF_FuelStock stock = StockFor(stationId);
		if (!stock)
			return Refuse(result, LF_EFuelSale.NO_STATION, string.Format("station '%1' has no stock", stationId));

		int room = VehicleRoom(vehicle);
		if (room < 1)
			return Refuse(result, LF_EFuelSale.TANK_FULL, "the vehicle's tank has no room for a litre");

		if (!s_Ledger)
			s_Ledger = new LF_FuelChargeLedger();

		string key = LF_Utils.GetPlayerUid(buyer);
		LF_FuelPlan plan = LF_FuelStockRules.PlanSaleFor(config, stock, room, LF_MoneyUtils.GetCash(buyer), s_Ledger.Carry(key));
		if (!plan.IsOk())
		{
			if (plan.m_eResult == LF_EFuelResult.EMPTY)
				return Refuse(result, LF_EFuelSale.EMPTY, string.Format("station '%1' is dry", stationId));
			if (plan.m_eResult == LF_EFuelResult.NO_MONEY)
				return Refuse(result, LF_EFuelSale.NO_MONEY, "the buyer cannot pay for a litre");
			if (plan.m_eResult == LF_EFuelResult.OFF)
				return Refuse(result, LF_EFuelSale.OFF, "the fuel price is 0, sales are off");

			return Refuse(result, LF_EFuelSale.FAILED, string.Format("the sale plan was refused (%1)", plan.m_eResult));
		}

		if (plan.m_iCash > 0)
		{
			int taken = LF_MoneyUtils.RemoveCash(buyer, plan.m_iCash, "fuel");
			if (taken != plan.m_iCash)
			{
				// Short of the cash we counted a moment ago: give back what was taken and sell nothing.
				if (taken > 0)
					LF_MoneyUtils.AddCash(buyer, taken, "fuel");

				return Refuse(result, LF_EFuelSale.FAILED, string.Format("took %1 of %2 cash, sale undone", taken, plan.m_iCash));
			}
		}

		int filled = FillVehicle(vehicle, plan.m_iLitres);
		if (filled != plan.m_iLitres)
			LF_Debug.Warn(FEATURE, string.Format("fuel fill: planned %1 litres, the tank took %2", plan.m_iLitres, filled));

		LF_FuelStockRules.ApplySale(stock, plan);
		s_Ledger.Commit(key, plan);
		result.m_eResult = LF_EFuelSale.OK;
		result.m_iLitres = plan.m_iLitres;
		result.m_iCash = plan.m_iCash;
		LF_Debug.Info(FEATURE, string.Format("fuel sold at '%1': %2 litres for %3, %4 litres left", stationId, plan.m_iLitres, plan.m_iCash, stock.m_iLitres));
		return result;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_FuelSaleResult Refuse(LF_FuelSaleResult result, LF_EFuelSale code, string why)
	{
		result.m_eResult = code;
		LF_Debug.Info(FEATURE, string.Format("fuel sale refused: %1", why));
		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! The vehicle standing nearest to `pump` within reach, or null.
	static IEntity FindVehicleNear(IEntity pump)
	{
		if (!pump || !GetGame() || !GetGame().GetWorld())
			return null;

		s_pFoundVehicle = null;
		s_vOrigin = pump.GetOrigin();
		s_fBest = REACH_M;
		GetGame().GetWorld().QueryEntitiesBySphere(s_vOrigin, REACH_M, CollectVehicle, null, EQueryEntitiesFlags.ALL);
		IEntity found = s_pFoundVehicle;
		s_pFoundVehicle = null;
		return found;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool CollectVehicle(IEntity entity)
	{
		if (!entity)
			return true;

		Vehicle vehicle = Vehicle.Cast(entity);
		if (!vehicle)
			return true;

		float dist = vector.Distance(vehicle.GetOrigin(), s_vOrigin);
		if (dist < s_fBest)
		{
			s_fBest = dist;
			s_pFoundVehicle = vehicle;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a pump fill for `buyer` of the vehicle standing next to this pump. Tells the buyer how it went.
	LF_FuelSaleResult SellNearby(IEntity buyer)
	{
		if (!Replication.IsServer())
			return null;

		LF_FuelSaleResult result = Sell(m_sStationId, FindVehicleNear(GetOwner()), buyer);
		Tell(buyer, result);
		return result;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Tell(IEntity buyer, LF_FuelSaleResult result)
	{
		if (!buyer || !result)
			return;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;

		int playerId = playerManager.GetPlayerIdFromControlledEntity(buyer);
		if (playerId <= 0)
			return;

		string message = RefusalKey(result.m_eResult);
		LF_ENotificationType type = LF_ENotificationType.WARNING;
		string arg;
		if (result.IsOk())
		{
			message = "#LF-Fuel_Filled";
			type = LF_ENotificationType.INFO;
			arg = string.Format("%1 L, %2", result.m_iLitres, LF_MoneyFormat.FormatMoney(result.m_iCash));
		}

		LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-Fuel_Title", message, 5.0, type, arg);
	}

	//------------------------------------------------------------------------------------------------
	//! The localization key of the line a refused sale prints.
	static string RefusalKey(LF_EFuelSale code)
	{
		if (code == LF_EFuelSale.EMPTY)
			return "#LF-Fuel_Dry";
		if (code == LF_EFuelSale.NO_MONEY)
			return "#LF-Fuel_NoMoney";
		if (code == LF_EFuelSale.TANK_FULL)
			return "#LF-Fuel_TankFull";
		if (code == LF_EFuelSale.NO_VEHICLE)
			return "#LF-Fuel_NoVehicle";

		return "#LF-Fuel_Failed";
	}

	//------------------------------------------------------------------------------------------------
	//! World end and tests: forget every station's stock and every owed fraction.
	static void ResetStatics()
	{
		s_mStocks = null;
		s_Ledger = null;
		s_pFoundVehicle = null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_FuelStationComponent's statics.
class LF_FuelStationComponent_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_FuelStationComponent";
	}

	override void Apply()
	{
		LF_FuelStationComponent.ResetStatics();
	}
}

//------------------------------------------------------------------------------------------------
//! The vanilla free refuel is off at a pump that sells from stock.
modded class SCR_FuelSupportStationComponent
{
	//------------------------------------------------------------------------------------------------
	override bool IsValid(IEntity actionOwner, IEntity actionUser, SCR_BaseUseSupportStationAction action, vector actionPosition, out ESupportStationReasonInvalid reasonInvalid, out int supplyAmount)
	{
		if (LF_FuelStationComponent.ReplacesVanilla(GetOwner()))
		{
			reasonInvalid = ESupportStationReasonInvalid.DISABLED;
			return false;
		}

		return super.IsValid(actionOwner, actionUser, action, actionPosition, reasonInvalid, supplyAmount);
	}
}
