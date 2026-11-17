// red-proof: RED-OK police/cuffs-server-refusals, observed 2026-11-17 with `tools\cli test --no-build --red
// LF_Test_CuffsServerRules,LF_Test_CuffsServerRefusals`: with the suspect left free, the hit did damage (expected 100,
// got 90) and every refusal line failed (driver seat, till, held till, vehicle, weapon, vault, lockpick, give cash,
// give item).
// tier: WORLD
//! police/cuffs-server-refusals (#2417): the server refuses a cuffed character's requests at their
//! server entry points, whatever its own machine sent. A hit it instigates does no damage; a driver's
//! seat is refused (a cargo seat is not); till, held-till, vehicle, weapon and vault robbery,
//! lockpicking and giving cash or an item are refused as cuffed. Talking (introduce) stays allowed.
class LF_Test_CuffsServerRefusals : LF_Test
{
	protected static const string UID = "lf-test-cuffs-server";
	protected static const ResourceName CAR = "{54C3CC22DEBD57BE}Prefabs/Vehicles/Wheeled/S105/S105_beige.et";

	override string GetName()
	{
		return "police/cuffs-server-refusals";
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity cuffed = LF_TestCharacterFixture.Spawn(ctx);
		IEntity victim = LF_TestCharacterFixture.Spawn(ctx);
		IEntity free = LF_TestCharacterFixture.Spawn(ctx);
		SCR_ChimeraCharacter chimera = SCR_ChimeraCharacter.Cast(cuffed);
		ctx.NotNull(chimera, "the suspect is a character");
		if (!chimera || !victim || !free)
		{
			Cleanup(cuffed, victim, free, null);
			return;
		}

		LF_CuffEnforcement.ResetStatics();
		if (!RedProof())
			chimera.LF_SetRestrained(true);

		Fire(ctx, cuffed, victim, free);
		IEntity car = Drive(ctx, cuffed);
		Rob(ctx, cuffed);
		Interact(ctx, cuffed, victim);

		Cleanup(cuffed, victim, free, car);
	}

	//------------------------------------------------------------------------------------------------
	protected void Fire(LF_TestContext ctx, IEntity cuffed, IEntity victim, IEntity free)
	{
		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(victim.FindComponent(SCR_CharacterDamageManagerComponent));
		ctx.NotNull(damage, "the victim has a damage manager");
		if (!damage)
			return;

		HitZone health = damage.GetDefaultHitZone();
		ctx.NotNull(health, "the victim has a health hit zone");
		if (!health)
			return;

		float before = health.GetHealth();
		damage.HandleDamage(Hit(victim, health, cuffed));
		ctx.EqualFloat(before, health.GetHealth(), 0.001, "a cuffed shooter's hit does no damage on the server");
		ctx.EqualStr("damage", LF_CuffEnforcement.LastRefused(), "and it was refused as cuffed");

		LF_CuffEnforcement.ResetStatics();
		damage.HandleDamage(Hit(victim, health, free));
		ctx.True(health.GetHealth() < before, "a free shooter's same hit does damage (the measure works)");
		ctx.EqualStr("", LF_CuffEnforcement.LastRefused(), "and nothing was refused");
	}

	//------------------------------------------------------------------------------------------------
	protected BaseDamageContext Hit(IEntity victim, HitZone zone, IEntity shooter)
	{
		BaseDamageContext hit = new BaseDamageContext();
		hit.damageType = EDamageType.KINETIC;
		hit.damageValue = 10;
		hit.hitEntity = victim;
		hit.struckHitZone = zone;
		hit.instigator = Instigator.CreateInstigator(shooter);
		hit.hitPosition = victim.GetOrigin();
		return hit;
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity Drive(LF_TestContext ctx, IEntity cuffed)
	{
		Resource res = Resource.Load(CAR);
		ctx.True(res && res.IsValid(), "the test car resolves");
		if (!res || !res.IsValid())
			return null;

		EntitySpawnParams params();
		params.TransformMode = ETransformMode.WORLD;
		cuffed.GetTransform(params.Transform);
		params.Transform[3] = params.Transform[3] + "0 0 20";
		IEntity car = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		ctx.NotNull(car, "the test car spawned");
		if (!car)
			return null;

		BaseCompartmentManagerComponent seats = BaseCompartmentManagerComponent.Cast(car.FindComponent(BaseCompartmentManagerComponent));
		ctx.NotNull(seats, "the car has seats");
		if (!seats)
			return car;

		array<BaseCompartmentSlot> slots = {};
		seats.GetCompartments(slots);
		BaseCompartmentSlot pilot;
		BaseCompartmentSlot cargo;
		foreach (BaseCompartmentSlot slot : slots)
		{
			if (!pilot && PilotCompartmentSlot.Cast(slot))
				pilot = slot;
			else if (!cargo && !PilotCompartmentSlot.Cast(slot))
				cargo = slot;
		}

		ctx.True(pilot != null && cargo != null, "the car has a driver's seat and a passenger seat");
		if (!pilot || !cargo)
			return car;

		ctx.True(LF_CuffEnforcement.RefuseSeat(cuffed, pilot), "the server refuses a cuffed character the driver's seat");
		ctx.EqualStr("driver seat", LF_CuffEnforcement.LastRefused(), "as cuffed");
		LF_CuffEnforcement.ResetStatics();
		ctx.False(LF_CuffEnforcement.RefuseSeat(cuffed, cargo), "a passenger seat is not refused");
		return car;
	}

	//------------------------------------------------------------------------------------------------
	protected void Rob(LF_TestContext ctx, IEntity cuffed)
	{
		float last;
		ctx.Equal(0, LF_RobAction.CommitRobbery(UID, cuffed, 100, 5, 0, 0, 0, 0, last), "a till robbery pays nothing");
		ctx.EqualStr(LF_RobAction.REFUSED_CUFFED, LF_RobAction.GetLastRefusal(), "the till robbery is refused as cuffed");

		ctx.EqualStr(LF_RobAction.REFUSED_CUFFED, LF_RobHold.Refusal(cuffed, null), "a held robbery is refused as cuffed");

		LF_CuffEnforcement.ResetStatics();
		ctx.True(LF_RobVehicleAction.CommitVehicleRobbery(UID, cuffed, "") == null, "a vehicle robbery spawns nothing");
		ctx.EqualStr("vehicle robbery", LF_CuffEnforcement.LastRefused(), "the vehicle robbery is refused as cuffed");

		LF_CuffEnforcement.ResetStatics();
		ctx.False(LF_RobWeaponAction.CommitWeaponRobbery(UID, cuffed, ""), "a weapon robbery hands nothing over");
		ctx.EqualStr("weapon robbery", LF_CuffEnforcement.LastRefused(), "the weapon robbery is refused as cuffed");

		LF_CuffEnforcement.ResetStatics();
		ctx.False(LF_Bank.Rob("lf-test-cuffs-vault", UID, cuffed), "a vault robbery does not start");
		ctx.EqualStr("vault robbery", LF_CuffEnforcement.LastRefused(), "the vault robbery is refused as cuffed");

		LF_CuffEnforcement.ResetStatics();
		ctx.False(LF_Lockpick.BeginAs(cuffed, cuffed, UID), "a lockpick does not start");
		ctx.EqualStr("lockpick", LF_CuffEnforcement.LastRefused(), "the lockpick is refused as cuffed");
	}

	//------------------------------------------------------------------------------------------------
	protected void Interact(LF_TestContext ctx, IEntity cuffed, IEntity other)
	{
		LF_CuffEnforcement.ResetStatics();
		ctx.False(LF_PlayerInteraction.OpenGiveCash(cuffed, other), "the give-cash sheet does not open");
		ctx.EqualStr("give cash", LF_CuffEnforcement.LastRefused(), "giving cash is refused as cuffed");

		LF_CuffEnforcement.ResetStatics();
		ctx.False(LF_PlayerInteraction.OfferItemInHand(cuffed, other), "no item is handed over");
		ctx.EqualStr("give item", LF_CuffEnforcement.LastRefused(), "giving an item is refused as cuffed");

		LF_CuffEnforcement.ResetStatics();
		LF_PlayerInteraction.Introduce(cuffed, other, 0);
		ctx.EqualStr("", LF_CuffEnforcement.LastRefused(), "talking is not refused as cuffed");
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity cuffed, IEntity victim, IEntity free, IEntity car)
	{
		SCR_ChimeraCharacter chimera = SCR_ChimeraCharacter.Cast(cuffed);
		if (chimera)
			chimera.LF_SetRestrained(false);

		LF_CuffEnforcement.ResetStatics();
		LF_TestCharacterFixture.Delete(cuffed);
		LF_TestCharacterFixture.Delete(victim);
		LF_TestCharacterFixture.Delete(free);
		if (car)
			SCR_EntityHelper.DeleteEntityAndChildren(car);
	}
}
