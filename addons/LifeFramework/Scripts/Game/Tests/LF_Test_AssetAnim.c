// red-proof: 2026-11-14, with the last assertion expecting 2 stops, cli test --no-build --only LF_Test_AssetAnim:
//   [LFTEST]   - ending twice stops once (expected 2, got 1)
//   Restored; green.
// tier: WORLD
//! Asset animation (#340): the motion rules, and the drill as the first asset that uses them. The drill
//! prefab carries the component the asset spec generates (one spinning bone, the sparks), its model has
//! the bone and the socket, the server switches it and poses it, and LF_DrillScene holds it in front of
//! a robber and lays it back where it was.
class LF_Test_AssetAnim : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "assetanim/asset-anim";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_AssetAnim.ResetStatics();
		LF_DrillScene.ResetStatics();

		// The rules.
		vector spun = LF_AssetAnim.SpinAngles("0 0 1440", 0.3);
		ctx.EqualFloat(72, spun[2], 0.01, "1440 degrees a second for 0.3 s is 432, wrapped to 72");
		ctx.EqualFloat(0, spun[0], 0.01, "a roll spin leaves the pitch alone");
		ctx.EqualFloat(0.25, LF_AssetAnim.Progress(0, true, 0.5, 2), 0.001, "an active hinge moves towards open");
		ctx.EqualFloat(1, LF_AssetAnim.Progress(0.9, true, 0.5, 2), 0.001, "it stops at open");
		ctx.EqualFloat(0.5, LF_AssetAnim.Progress(0.75, false, 0.5, 2), 0.001, "an inactive hinge moves back");
		ctx.EqualFloat(0, LF_AssetAnim.Progress(0.1, false, 0.5, 2), 0.001, "it stops at closed");
		ctx.True(LF_AssetAnim.Settled(1, true) && LF_AssetAnim.Settled(0, false) && !LF_AssetAnim.Settled(0.5, true), "settled is the end it moves to");
		ctx.EqualFloat(0.5, LF_AssetAnim.Ease(0.5), 0.001, "the ease is symmetric");

		// The drill.
		IEntity drill = LF_Utils.SpawnEntityPrefab(LF_Bank.DRILL_PREFAB, "20 1 20");
		ctx.NotNull(drill, "the drill spawns");
		if (!drill)
			return;

		LF_AssetAnimComponent anim = LF_AssetAnimComponent.Of(drill);
		ctx.NotNull(anim, "the drill has the asset animation component");
		if (!anim)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(drill);
			return;
		}

		ctx.Equal(1, anim.GetBoneCount(), "one moving part: the chuck");
		ctx.Equal(1, anim.GetEffectCount(), "one effect: the sparks");
		Animation animation = drill.GetAnimation();
		ctx.True(animation && animation.GetBoneIndex("chuck") != -1, "the model has the chuck bone");
		ctx.True(animation && animation.GetBoneIndex("fx_bit") != -1, "the model has the socket the sparks come from");
		ctx.True(Resource.Load("{5702289A28BA229E}Particles/LF/Drill_fx_bit.ptc").IsValid(), "the sparks effect loads");

		ctx.False(anim.IsRunning(), "a drill starts switched off");
		ctx.True(anim.SetActive(true), "the server switches it on");
		ctx.True(anim.IsRunning(), "it runs");
		ctx.Equal(1, LF_AssetAnim.GetStarts(), "one start is counted");
		ctx.True(anim.SetActive(true), "switching it on twice is harmless");
		ctx.Equal(1, LF_AssetAnim.GetStarts(), "and is not counted twice");

		ctx.True(anim.Pose("21 2 21", "0 45 0"), "the server holds it in the air");
		ctx.True(anim.IsPosed() && vector.Distance(drill.GetOrigin(), "21 2 21") < 0.01, "it is where it was posed");
		anim.SetActive(false);
		anim.Release("20 1 20", "0 0 0");
		ctx.False(anim.IsRunning(), "it stops");
		ctx.True(!anim.IsPosed() && vector.Distance(drill.GetOrigin(), "20 1 20") < 0.01, "released, it lies where it was set down");
		ctx.Equal(1, LF_AssetAnim.GetStops(), "one stop is counted");

		// The scene: in front of a robber at chest height, then back where it lay.
		vector hold = LF_DrillScene.HoldPosition("10 5 10", "0 0 1");
		ctx.True(vector.Distance(hold, "10.135 6.23 10.63") < 0.001, "the hold spot is right, up and forward of the robber");
		hold = LF_DrillScene.HoldPosition("10 5 10", "1 0.5 0");
		ctx.True(vector.Distance(hold, "10.63 6.23 9.865") < 0.001, "facing east on a slope it is still level: forward is east, right is south");
		IEntity robber = LF_TestCharacterFixture.Spawn(ctx);
		if (robber)
		{
			vector rest = drill.GetOrigin();
			ctx.True(LF_DrillScene.Begin(robber, drill), "the scene starts");
			ctx.True(anim.IsRunning() && anim.IsPosed(), "the drill runs, held");
			ctx.True(drill.GetOrigin()[1] - robber.GetOrigin()[1] > 1, string.Format("at chest height (drill %1, robber %2)", drill.GetOrigin(), robber.GetOrigin()));
			ctx.Equal(1, LF_DrillScene.GetActiveCount(), "one scene runs");
			LF_DrillScene.End(robber.GetID(), "test");
			ctx.True(!anim.IsRunning() && !anim.IsPosed(), "the scene ends: off and released");
			ctx.True(vector.Distance(drill.GetOrigin(), rest) < 0.01, "the drill is back where it lay");
			ctx.Equal(0, LF_DrillScene.GetActiveCount(), "no scene is left");
			LF_DrillScene.End(robber.GetID(), "again");
			ctx.Equal(1, LF_DrillScene.GetStopCount(), "ending twice stops once");
			LF_TestCharacterFixture.Delete(robber);
		}

		SCR_EntityHelper.DeleteEntityAndChildren(drill);
		LF_AssetAnim.ResetStatics();
		LF_DrillScene.ResetStatics();
	}
}
