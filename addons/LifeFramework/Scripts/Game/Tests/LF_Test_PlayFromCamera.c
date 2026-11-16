// red-proof: observed 2026-11-16 with `tools\cli test --no-build --red LF_Test_PlayFromCamera`: `RED-OK spawn/play-from-camera`, failing `- Workbench, editor body replaced: spawn at the camera`.
// tier: LOGIC
//! Workbench "Play from camera" keeps the player at the camera: the spawn takes the replaced editor
//! body's pose only inside Workbench, only when such a body was replaced, and never with no position.
class LF_Test_PlayFromCamera : LF_Test
{
	override string GetName()
	{
		return "spawn/play-from-camera";
	}

	override void Run(LF_TestContext ctx)
	{
		vector camera = "40 15 40";
		bool inWorkbench = LF_PlayFromCamera.UseCameraPose(true, true, camera);
		if (RedProof())
			inWorkbench = false;

		ctx.True(inWorkbench, "Workbench, editor body replaced: spawn at the camera");
		ctx.True(!LF_PlayFromCamera.UseCameraPose(false, true, camera), "not Workbench (dedicated server, published build): the spawn point");
		ctx.True(!LF_PlayFromCamera.UseCameraPose(true, false, camera), "Workbench Play with no editor body: the spawn point");
		ctx.True(!LF_PlayFromCamera.UseCameraPose(true, true, vector.Zero), "an unknown body position never puts the player at the origin");
		ctx.True(!LF_PlayFromCamera.IsWorkbench(), "the test server is not a Workbench build, so its spawns never take the camera path");
	}
}
