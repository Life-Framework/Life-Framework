// tools/prs-schedule.mjs - `tools\cli prs schedule print|install|status|remove`.
//
// One Windows Task Scheduler entry, "LF-PRs-Heal", runs `prs heal --watch --once` every 10 minutes
// from the main checkout. It starts through `conhost.exe --headless`, so no console window ever
// appears (same rule as tools/lib/quiet-launch.mjs); output goes to tmp/logs/prs-heal-watch.log.
// `print` shows what `install` would register and changes nothing. The task runs as the current
// user, only while logged on (it needs that user's gh login), and never overlaps itself.
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import * as wt from "./wt.mjs";

export const TASK_NAME = "LF-PRs-Heal";
const PS = join(process.env.SystemRoot || "C:\Windows", "System32", "WindowsPowerShell", "v1.0", "powershell.exe");

export function registerScript(root, minutes = 10) {
  const main = wt.mainRootOf(root);
  const cli = join(main, "tools", "cli.mjs");
  const q = (s) => `'${String(s).replace(/'/g, "''")}'`;
  return [
    `$a = New-ScheduledTaskAction -Execute 'conhost.exe' -Argument ${q(`--headless "${process.execPath}" "${cli}" prs heal --watch --once`)} -WorkingDirectory ${q(main)}`,
    `$t = New-ScheduledTaskTrigger -Once -At (Get-Date).AddMinutes(1) -RepetitionInterval (New-TimeSpan -Minutes ${minutes})`,
    `$s = New-ScheduledTaskSettingsSet -MultipleInstances IgnoreNew -ExecutionTimeLimit (New-TimeSpan -Minutes 45) -StartWhenAvailable -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -Hidden`,
    `Register-ScheduledTask -TaskName ${q(TASK_NAME)} -Action $a -Trigger $t -Settings $s -Description 'Life Framework: merge origin/main into conflicting ws/* PRs when only generated files conflict (prs heal --watch)' -Force | Out-Null`,
    `Write-Host "registered ${TASK_NAME}"`,
  ].join("\n");
}

function ps(script) {
  return wt.run(PS, ["-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-Command", script]);
}

export function cmdSchedule(root, args) {
  const sub = args[0] ?? "print";
  if (sub === "print") {
    console.log(`Task ${TASK_NAME} (nothing registered by print). Install registers exactly:\n`);
    console.log(registerScript(root));
    console.log(`\nInstall: tools\\cli prs schedule install   Remove: tools\\cli prs schedule remove`);
    return 0;
  }
  if (process.platform !== "win32") {
    console.log("FAIL  prs schedule uses Windows Task Scheduler; elsewhere run `prs heal --watch` under cron or systemd");
    return 1;
  }
  let res;
  if (sub === "install") res = ps(registerScript(root));
  else if (sub === "remove") res = ps(`Unregister-ScheduledTask -TaskName '${TASK_NAME}' -Confirm:$false; Write-Host 'removed ${TASK_NAME}'`);
  else if (sub === "status") res = ps(`$t = Get-ScheduledTask -TaskName '${TASK_NAME}' -ErrorAction SilentlyContinue; if (-not $t) { Write-Host 'not registered' } else { $i = Get-ScheduledTaskInfo -TaskName '${TASK_NAME}'; Write-Host "$($t.State) last run $($i.LastRunTime) result $($i.LastTaskResult) next $($i.NextRunTime)" }`);
  else {
    console.log("usage: tools\\cli prs schedule print|install|status|remove");
    return 1;
  }
  process.stdout.write(res.stdout + res.stderr);
  return res.status === 0 ? 0 : 1;
}
