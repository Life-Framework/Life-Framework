// Red-proof for validate-terms-shown: a {term} key shown without LF_Terms is caught on every path
// the rule covers (a literal, a key-returning call, a variable, SetText, a layout), and the
// LF_Terms path passes.
// inputs: imports
import { check } from "./validate-terms-shown.mjs";

const termed = new Set(["LF-Desk_Wanted"]);
const desk = `class LF_Desk
{
	static string AnswerKey(int o)
	{
		if (o == 1)
			return "#LF-Desk_Wanted";
		return "#LF-Desk_Ok";
	}
	static string CounterKey(int o)
	{
		return AnswerKey(o);
	}
}`;
const run = (body, layouts = []) => check(termed, [{ path: "Desk.c", text: desk }, { path: "Use.c", text: body }], layouts);
let failed = 0;
const expect = (name, failures, pattern) => {
  const ok = pattern ? failures.some((f) => pattern.test(f)) : failures.length === 0;
  if (!ok) { failed++; console.log(`FAIL ${name}: ${JSON.stringify(failures)}`); }
};

expect("a literal termed key in Translate fails", run(`string s = WidgetManager.Translate("#LF-Desk_Wanted");`), /Use\.c:1 .*LF-Desk_Wanted/);
expect("a call that returns a termed key fails (the gun counter slip)", run(`string s = WidgetManager.Translate(LF_Desk.CounterKey(o), arg);`), /LF-Desk_Wanted/);
expect("a variable assigned a termed key fails", run(`string key = LF_Desk.AnswerKey(o);\nreturn WidgetManager.Translate(key, x);`), /Use\.c:2/);
expect("SetText with a termed key fails", run(`w.SetText("#LF-Desk_Wanted");`), /SetText/);
expect("a layout text with a termed key fails", run(``, [{ path: "A.layout", text: `TextWidgetClass {\n Text "#LF-Desk_Wanted"\n}` }]), /A\.layout:2/);
expect("LF_Terms.Text passes", run(`string s = LF_Terms.Text(LF_Desk.CounterKey(o), arg);`));
expect("an untermed key in Translate passes", run(`string s = WidgetManager.Translate("#LF-Desk_Ok");`));

if (failed) process.exit(1);
console.log("terms-shown-selftest: OK");
