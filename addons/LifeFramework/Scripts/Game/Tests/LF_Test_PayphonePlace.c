// red-proof: 2026-11-21, `tools\cli test --no-build --red LF_Test_PayphonePlace`: RED-OK map/payphone-place (4 assertions fail with the hook on); green without it.
// red-proof: RedProof() skips the spawn, so there is no payphone place and the checks fail.
//! map/payphone-place (rc-map-places): a payphone prefab registers itself as a "Payphone" place so the map
//! can name it; a station telephone (it answers a service) does not, it sits inside its building.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PayphonePlace : LF_Test
{
	static const ResourceName PAYPHONE = "{8F2C6E1D4B7A5093}Prefabs/Comms/LF_Payphone.et";
	static const ResourceName STATION = "{9BDAE42023AB3357}Prefabs/Comms/LF_DispatchPhone.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "map/payphone-place";
	}

	//------------------------------------------------------------------------------------------------
	protected bool Holds(IEntity entity)
	{
		array<IEntity> places = {};
		LF_Places.FindAll(LF_PhoneComponent.PLACE_KIND, places);
		return places.Find(entity) != -1;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity caller = LF_TestCharacterFixture.Spawn(ctx);
		if (!caller)
			return;

		vector at = caller.GetOrigin();
		IEntity payphone;
		if (!RedProof())
			payphone = LF_Utils.SpawnEntityPrefab(PAYPHONE, at + "3 0 0");

		IEntity station = LF_Utils.SpawnEntityPrefab(STATION, at + "9 0 0");
		ctx.NotNull(payphone, "the payphone prefab spawns");
		ctx.NotNull(station, "the station telephone prefab spawns");
		if (payphone)
			ctx.True(Holds(payphone), "a payphone is a place of kind Payphone");

		if (station)
			ctx.False(Holds(station), "a station telephone is not a payphone place");

		LF_TestLooseSweep.DeleteBuilding(payphone);
		LF_TestLooseSweep.DeleteBuilding(station);
		LF_TestCharacterFixture.Delete(caller);
	}
}
