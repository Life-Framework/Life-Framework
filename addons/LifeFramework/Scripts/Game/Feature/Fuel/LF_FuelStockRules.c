// Derived from Overthrow (https://github.com/ArmaOverthrow/Overthrow.Arma4), used with the author's permission, relicensed MIT.
// Source: Scripts/Game/Data/OVT_FuelPricing.c and Scripts/Game/Data/OVT_FuelChargeLedger.c (the shape: a fill plan clamped
// by stock and by money, price 0 as the kill switch, a fractional-cost accumulator); the station stock and delivery rules are ours.
//------------------------------------------------------------------------------------------------
//! Fuel station stock rules (#3327, framework #788). Pure ints, no world, no entity: the pump, the delivery
//! drop-off and the job pay (later cards) call these and only these to decide how many litres move and what
//! it costs. Prices are in hundredths of a cash unit per litre (`PRICE_SCALE`), so 0.30 a litre is 30 and a
//! sub-unit price is carried (`LF_FuelChargeLedger`) instead of rounded away or charged a whole unit per litre.
//------------------------------------------------------------------------------------------------
enum LF_EFuelResult
{
	OK,
	OFF,
	BAD_INPUT,
	EMPTY,
	FULL,
	NO_MONEY
}

//------------------------------------------------------------------------------------------------
//! One station's litres, never above its capacity and never below zero.
class LF_FuelStock
{
	int m_iCapacity;
	int m_iLitres;

	//------------------------------------------------------------------------------------------------
	//! A station of `capacity` litres (1..MAX_LITRES) holding `litres` (clamped to 0..capacity).
	static LF_FuelStock Make(int capacity, int litres)
	{
		LF_FuelStock stock = new LF_FuelStock();
		stock.m_iCapacity = LF_Utils.ClampInt(capacity, 1, LF_FuelStockRules.MAX_LITRES);
		stock.m_iLitres = LF_Utils.ClampInt(litres, 0, stock.m_iCapacity);
		return stock;
	}

	//------------------------------------------------------------------------------------------------
	//! Litres a delivery can still add.
	int Room()
	{
		return LF_Utils.MaxInt(m_iCapacity - m_iLitres, 0);
	}
}

//------------------------------------------------------------------------------------------------
//! What one sale or delivery would do. `m_iLitres` and `m_iCash` are what actually moves and is charged or paid
//! (zero when refused); `m_iCarryAfter` is the fractional cost (0..99 hundredths) the caller keeps for next time.
class LF_FuelPlan
{
	LF_EFuelResult m_eResult;
	int m_iLitres;
	int m_iCash;
	int m_iCarryAfter;

	//------------------------------------------------------------------------------------------------
	bool IsOk()
	{
		return m_eResult == LF_EFuelResult.OK;
	}
}

//------------------------------------------------------------------------------------------------
class LF_FuelStockRules
{
	static const int PRICE_SCALE = 100;
	//! Largest balance the rules read: keeps `balance * PRICE_SCALE` inside an int.
	static const int MAX_CASH = 10000000;	// lf-fixed: int overflow guard for money sums
	//! Largest price in hundredths per litre (100 cash a litre): with MAX_LITRES keeps litres times price inside an int.
	static const int MAX_PRICE = 10000;	// lf-fixed: int overflow guard
	static const int MAX_LITRES = 100000;	// lf-fixed: int overflow guard

	//------------------------------------------------------------------------------------------------
	protected static LF_FuelPlan Refuse(LF_EFuelResult result, string why)
	{
		LF_FuelPlan plan = new LF_FuelPlan();
		plan.m_eResult = result;
		LF_Debug.Info("Fuel", string.Format("fuel plan refused: %1", why));
		return plan;
	}

	//------------------------------------------------------------------------------------------------
	//! Fills `plan` for `litres` at `priceCenti` on top of `carry` hundredths already owed: the whole cash
	//! due now (floor) and the hundredths left to carry.
	protected static void Charge(LF_FuelPlan plan, int litres, int priceCenti, int carry)
	{
		int totalCenti = carry + litres * priceCenti;
		plan.m_iLitres = litres;
		plan.m_iCash = totalCenti / PRICE_SCALE;
		plan.m_iCarryAfter = totalCenti % PRICE_SCALE;
	}

	//------------------------------------------------------------------------------------------------
	//! A sale of up to `wantedLitres` to a buyer holding `balance` cash, at `priceCenti` per litre, with `carry`
	//! hundredths owed from earlier sales. Litres are clamped by stock first, then by what the buyer can pay
	//! (the cash due, carry included, never exceeds the balance); the charge is derived from the litres that
	//! move. Price 0 is the kill switch (OFF); stock under `minStock` (at least 1) is EMPTY; cash that cannot
	//! cover one litre is NO_MONEY. Does not change the station.
	static LF_FuelPlan PlanSale(LF_FuelStock station, int wantedLitres, int balance, int priceCenti, int carry, int minStock)
	{
		if (!station)
			return Refuse(LF_EFuelResult.BAD_INPUT, "no station");
		if (priceCenti <= 0)
			return Refuse(LF_EFuelResult.OFF, "price 0, fuel sales are off");
		if (wantedLitres <= 0 || balance < 0)
			return Refuse(LF_EFuelResult.BAD_INPUT, string.Format("wanted %1 litres with balance %2", wantedLitres, balance));

		priceCenti = LF_Utils.MinInt(priceCenti, MAX_PRICE);
		carry = LF_Utils.ClampInt(carry, 0, PRICE_SCALE - 1);
		minStock = LF_Utils.MaxInt(minStock, 1);
		if (station.m_iLitres < minStock)
			return Refuse(LF_EFuelResult.EMPTY, string.Format("station holds %1 litres, needs %2 to sell", station.m_iLitres, minStock));

		int litres = LF_Utils.MinInt(LF_Utils.MinInt(wantedLitres, MAX_LITRES), station.m_iLitres);
		// Most litres whose cash due fits the balance: carry + n * price < (balance + 1) * scale.
		int cash = LF_Utils.MinInt(balance, MAX_CASH);
		int affordable = (cash * PRICE_SCALE + PRICE_SCALE - 1 - carry) / priceCenti;
		if (affordable <= 0)
			return Refuse(LF_EFuelResult.NO_MONEY, string.Format("balance %1 does not cover one litre at %2", balance, priceCenti));

		litres = LF_Utils.MinInt(litres, affordable);
		LF_FuelPlan plan = new LF_FuelPlan();
		plan.m_eResult = LF_EFuelResult.OK;
		Charge(plan, litres, priceCenti, carry);
		return plan;
	}

	//------------------------------------------------------------------------------------------------
	//! `PlanSale` with the config's price and minimum stock; a disabled config is OFF.
	static LF_FuelPlan PlanSaleFor(LF_FuelConfig config, LF_FuelStock station, int wantedLitres, int balance, int carry)
	{
		if (!config)
			return Refuse(LF_EFuelResult.BAD_INPUT, "no fuel config");
		if (!config.m_bEnabled)
			return Refuse(LF_EFuelResult.OFF, "fuel stock is disabled");

		return PlanSale(station, wantedLitres, balance, config.m_iPricePerLitre, carry, config.m_iMinStockToSell);
	}

	//------------------------------------------------------------------------------------------------
	//! A delivery of `offeredLitres` to a station: only the room left lands, and the payout (`payCenti` per
	//! litre, plus `carry` owed from earlier drops) is for the litres that landed. A full station is FULL.
	//! Does not change the station.
	static LF_FuelPlan PlanDelivery(LF_FuelStock station, int offeredLitres, int payCenti, int carry)
	{
		if (!station)
			return Refuse(LF_EFuelResult.BAD_INPUT, "no station");
		if (offeredLitres <= 0 || payCenti < 0)
			return Refuse(LF_EFuelResult.BAD_INPUT, string.Format("offered %1 litres at pay %2", offeredLitres, payCenti));

		int room = station.Room();
		if (room <= 0)
			return Refuse(LF_EFuelResult.FULL, string.Format("station is at capacity %1", station.m_iCapacity));

		int litres = LF_Utils.MinInt(LF_Utils.MinInt(offeredLitres, MAX_LITRES), room);
		LF_FuelPlan plan = new LF_FuelPlan();
		plan.m_eResult = LF_EFuelResult.OK;
		Charge(plan, litres, LF_Utils.MinInt(payCenti, MAX_PRICE), LF_Utils.ClampInt(carry, 0, PRICE_SCALE - 1));
		return plan;
	}

	//------------------------------------------------------------------------------------------------
	//! Removes a planned sale's litres from the station. A refused plan moves nothing; stock stays at 0 or above.
	static void ApplySale(LF_FuelStock station, LF_FuelPlan plan)
	{
		if (!station || !plan || !plan.IsOk())
			return;

		station.m_iLitres = LF_Utils.MaxInt(station.m_iLitres - plan.m_iLitres, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Adds a planned delivery's litres to the station, never past its capacity. A refused plan moves nothing.
	static void ApplyDelivery(LF_FuelStock station, LF_FuelPlan plan)
	{
		if (!station || !plan || !plan.IsOk())
			return;

		station.m_iLitres = LF_Utils.MinInt(station.m_iLitres + plan.m_iLitres, station.m_iCapacity);
	}
}

//------------------------------------------------------------------------------------------------
//! The fractional cost owed per key (a persistent player id, a job id). Deliberately unsaved and never settled
//! on its own: a restart forgets under one cash unit per key, which is the cheaper mistake than saving a row
//! per customer. The caller reads `Carry` into the plan and `Commit`s the plan after it charged the player.
class LF_FuelChargeLedger
{
	protected ref map<string, int> m_mCarry = new map<string, int>();

	//------------------------------------------------------------------------------------------------
	//! Hundredths owed by `key`, 0 when none.
	int Carry(string key)
	{
		int carry;
		if (m_mCarry.Find(key, carry))
			return carry;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Stores the plan's carry for `key`; a refused plan stores nothing, a carry of 0 drops the row.
	void Commit(string key, LF_FuelPlan plan)
	{
		if (!plan || !plan.IsOk() || key.IsEmpty())
			return;

		if (plan.m_iCarryAfter <= 0)
			m_mCarry.Remove(key);
		else
			m_mCarry.Set(key, plan.m_iCarryAfter);
	}

	//------------------------------------------------------------------------------------------------
	int Count()
	{
		return m_mCarry.Count();
	}

	//------------------------------------------------------------------------------------------------
	void Clear()
	{
		m_mCarry.Clear();
	}
}
