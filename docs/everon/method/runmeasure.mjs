import {spawnSync} from "node:child_process"; import fs from "node:fs";
const list=fs.readFileSync("tmp/everon/measure-list.txt","utf8").split("\n").filter(Boolean);
const out=[];
for(let i=0;i<list.length;i+=16){
  const r=spawnSync("node",["tools/cli.mjs","measure",...list.slice(i,i+16),"--surfaces","--json"],{encoding:"utf8",maxBuffer:1<<28});
  const txt=r.stdout; const j=txt.indexOf("["); 
  try{ out.push(...JSON.parse(txt.slice(j))); }catch(e){ console.error("batch",i,"fail",String(r.stderr).slice(0,200), txt.slice(0,200)); }
  console.error("done",i);
}
fs.writeFileSync("tmp/everon/measure.json",JSON.stringify(out));
console.log(out.length);
