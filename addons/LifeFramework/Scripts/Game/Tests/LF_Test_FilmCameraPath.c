// red-proof: 2026-11-11 (#392 orbit azimuth), azimuth-180 centre z 190 -> 210, cli test --no-build --only LF_Test_FilmCameraPath:
//   [LFTEST]   - azimuth 180: ... at the radius on the -z side (expected 210, got 190)
// Restored; green.
// red-proof: 2026-11-10 (follow smoothing, #394 polish), SmoothAlpha(0.25, 0.25) == 0.5 -> 0.4, cli test --no-build --only LF_Test_FilmCameraPath:
//   [LFTEST]   - dt equal to tau moves half way
// Restored; green.
// red-proof: 2026-11-08, quarter-time assertion 15.625 -> 15.0, cli test --no-build --tier fast:
//   [LFTEST] FAIL film/camera-path
//   [LFTEST]   - quarter time eases in (smoothstep 0.25 = 0.15625) (expected 15, got 15.625)
// Restored; the fast suite is green.

// tier: LOGIC
class LF_Test_FilmCameraPath : LF_Test
{
	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "film/camera-path";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ctx.True(LF_FilmCameraPath.SmoothAlpha(0, 0.25) == 0, "no time passed: the follow point does not move");
		ctx.True(LF_FilmCameraPath.SmoothAlpha(1, 0) == 1, "no smoothing: it jumps to the target");
		ctx.True(LF_FilmCameraPath.SmoothAlpha(0.25, 0.25) == 0.5, "dt equal to tau moves half way");
		ctx.True(LF_FilmCameraPath.SmoothAlpha(0.033, 0.25) < LF_FilmCameraPath.SmoothAlpha(0.1, 0.25), "a longer frame moves further");

		LF_FilmCameraPath empty = new LF_FilmCameraPath();
		vector position;
		vector lookAt;
		float fov;
		ctx.False(empty.Sample(1, position, lookAt, fov), "an empty path has nothing to sample");

		LF_FilmCameraPath path = new LF_FilmCameraPath();
		ctx.True(path.Add(0, "0 10 0", "0 0 100", 60), "first key accepted");
		ctx.True(path.Add(10, "100 10 0", "100 0 100", 40), "later key accepted");
		ctx.False(path.Add(10, "5 5 5", "0 0 0", 60), "a key at the same time is rejected");
		ctx.False(path.Add(4, "5 5 5", "0 0 0", 60), "an earlier key is rejected");
		ctx.Equal(2, path.Count(), "rejected keys are not stored");
		ctx.EqualFloat(10, path.GetDuration(), 0.001, "duration is the last key time");

		ctx.True(path.Sample(5, position, lookAt, fov), "mid-path sample");
		ctx.EqualFloat(50, position[0], 0.001, "smoothstep midpoint is halfway");
		ctx.EqualFloat(10, position[1], 0.001, "height holds between equal keys");
		ctx.EqualFloat(50, fov, 0.001, "fov eases with the position");

		path.Sample(2.5, position, lookAt, fov);
		ctx.EqualFloat(15.625, position[0], 0.001, "quarter time eases in (smoothstep 0.25 = 0.15625)");
		ctx.EqualFloat(15.625, lookAt[0], 0.001, "look-at follows the same ease");

		path.Sample(-3, position, lookAt, fov);
		ctx.EqualFloat(0, position[0], 0.001, "before the first key clamps to it");
		path.Sample(99, position, lookAt, fov);
		ctx.EqualFloat(100, position[0], 0.001, "after the last key clamps to it");
		ctx.EqualFloat(40, fov, 0.001, "after the last key keeps its fov");

		// Orbit in any direction (#392): the arc is centred on the azimuth, 0 = +z, 90 = +x, 180 = -z.
		vector centre = "100 0 200";
		LF_FilmCameraPath orbit = new LF_FilmCameraPath();
		orbit = LF_FilmCameraPath.BuildOrbit(centre, 10, 12, 180, 90, 4);
		orbit.Sample(6, position, lookAt, fov);
		ctx.EqualFloat(100, position[0], 0.01, "azimuth 180: the middle of the arc is straight -z of the point (x)");
		ctx.EqualFloat(190, position[2], 0.01, "azimuth 180: ... at the radius on the -z side");
		ctx.EqualFloat(4, position[1], 0.01, "orbit height is the camera height above the point");
		orbit.Sample(0, position, lookAt, fov);
		ctx.EqualFloat(1, Math.Round(vector.Distance(Vector(position[0], 0, position[2]), centre) / 10), 0.001, "every key sits on the radius");
		orbit = LF_FilmCameraPath.BuildOrbit(centre, 10, 12, 90, 0, 4);
		orbit.Sample(6, position, lookAt, fov);
		ctx.EqualFloat(110, position[0], 0.01, "azimuth 90 is the +x side");
		orbit = LF_FilmCameraPath.BuildOrbit(centre, 10, 12);
		orbit.Sample(6, position, lookAt, fov);
		ctx.EqualFloat(210, position[2], 0.01, "default orbit keeps the old +z side");

		LF_FilmCameraPath fixedShot = LF_FilmCameraPath.BuildStatic("1 2 3", "4 5 6", 5, 45);
		fixedShot.Sample(2.5, position, lookAt, fov);
		ctx.True(position == "1 2 3" && lookAt == "4 5 6", "a static camera does not move");
		ctx.EqualFloat(5, fixedShot.GetDuration(), 0.001, "a static shot lasts its seconds");

		LF_FilmCameraPath drone = LF_FilmCameraPath.BuildDrone("0 3 0", "0 0 10", "0 13 0", "50 0 10", 4, 60);
		drone.Sample(2, position, lookAt, fov);
		ctx.EqualFloat(8, position[1], 0.001, "a drone is half way up half way through");
		ctx.EqualFloat(25, lookAt[0], 0.001, "the drone look-at travels too");
		drone.Sample(0.4, position, lookAt, fov);
		ctx.True(position[1] < 3 + 10 * 0.1, "a drone eases out of the start");

		array<float> nums;
		ctx.True(LF_FilmCamera.Numbers("1 2.5 -3", 3, nums) && nums.Count() == 3 && nums[1] == 2.5 && nums[2] == -3, "camera args parse as numbers");
		ctx.False(LF_FilmCamera.Numbers("1 two 3", 3, nums), "a word in camera args is refused");
		ctx.False(LF_FilmCamera.Numbers("1 2", 3, nums), "too few camera args are refused");
		ctx.True(LF_FilmCamera.Command("static", "1 2 3").StartsWith("error:"), "a short static command names its error");
		ctx.True(LF_FilmCamera.Command("zoom", "") == "error:bad_camera_mode", "an unknown mode is refused");
	}
}
