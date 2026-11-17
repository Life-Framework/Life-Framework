import fs from "node:fs"; import {load} from "./parse.mjs"; import {typeOf} from "./classify.mjs";
const B=JSON.parse(fs.readFileSync("tmp/everon/buildings.json","utf8")); const pf=new Map(B.prefabs.map(p=>[p.res,p]));
const ms=JSON.parse(fs.readFileSync("tmp/everon/measure.json","utf8")); const mb=new Map(ms.map(m=>[m.ref,m.bounds]));
const all=load("tmp/everon/raw/saintphilippe.txt"); // within 450m: buildings + props
const SP=[4503,10772];
const bld=[],props=[];
for(const r of all){ const t=typeOf(r.path); const p=pf.get(r.res); if(t&&p&&!["waterworks","port"].includes(t)&&!/pole|Pole/.test(r.path)&&mb.get(r.res)) bld.push({...r,type:t,p}); else if(!/^Prefabs\/Structures\/(Walls|Signs|BuildingParts|Infrastructure)/.test(r.path)) props.push(r); }
console.log("bld",bld.length,"props",props.length);
function inside(b,r,sign){ const bb=mb.get(b.res); const dx=r.x-b.x,dz=r.z-b.z; const a=sign*b.yaw*Math.PI/180; const lx=dx*Math.cos(a)-dz*Math.sin(a), lz=dx*Math.sin(a)+dz*Math.cos(a); return lx>=bb.min[0]&&lx<=bb.max[0]&&lz>=bb.min[2]&&lz<=bb.max[2]&&r.y>=b.y-3&&r.y<=b.y+bb.max[1]; }
const big=bld.filter(b=>b.p.areaM2>=60&&["townhouse","house","shop","pub","villa","police","government","fire","church"].includes(b.type));
for(const sign of [1,-1]){ let n=0; for(const b of big) for(const r of props) if(/Furniture/.test(r.path)&&inside(b,r,sign))n++; console.log("sign",sign,"furniture inside",n); }

// ---- full Saint Philippe output
const short=p=>p.split("/").pop().replace(".et","");
const rows=[];
for(const b of bld){ if(Math.hypot(b.x-SP[0],b.z-SP[1])>450)continue;
  const fur={}; let nf=0; for(const r of props){ if(inside(b,r,1)){ const k=short(r.path); fur[k]=(fur[k]||0)+1; nf++; } }
  const top=Object.entries(fur).sort((a,c)=>c[1]-a[1]).slice(0,4).map(e=>e[0]+"×"+e[1]).join(", ");
  rows.push({type:b.type,prefab:short(b.path),res:b.res,x:+b.x.toFixed(1),z:+b.z.toFixed(1),yaw:Math.round(b.yaw),distM:Math.round(Math.hypot(b.x-SP[0],b.z-SP[1])),footprintM:b.p.footprintM,floors:b.p.floors,enterable:b.p.enterable,doorChildren:b.p.doorChildren,propsInside:nf,topProps:top}); }
rows.sort((a,b)=>a.distM-b.distM);
// port pieces
const port=all.filter(r=>typeOf(r.path)==="port"&&!/PierCover|pole|SeaBollard/.test(r.path)).map(r=>({prefab:short(r.path),x:Math.round(r.x),z:Math.round(r.z)}));
// ascii map: 30 m cells
const CH={townhouse:"T",house:"H",shop:"S",pub:"P",church:"C",chapel:"c",fuel:"F",government:"G",police:"X",fire:"R",kiosk:"K",farm:"B","farm-small":"b",industrial:"I","industrial-tank":"i",garage:"g",shed:"s",military:"M",villa:"V",farmhouse:"H",ruin:"r",recreation:"o",hotel:"L",airport:"A"};
const cell=30,N=30; const grid=Array.from({length:N},()=>Array(N).fill("."));
const prio="CGXRPSFTVHcKBIgMiooLbsr";
const put=(x,z,ch)=>{const gx=Math.floor((x-(SP[0]-450))/cell),gz=Math.floor((SP[1]+450-z)/cell); if(gx<0||gz<0||gx>=N||gz>=N)return; const cur=grid[gz][gx]; if(cur==="."||prio.indexOf(ch)<prio.indexOf(cur)||prio.indexOf(cur)<0)grid[gz][gx]=ch;};
for(const b of bld)if(CH[b.type])put(b.x,b.z,CH[b.type]);
for(const p of port)put(p.x,p.z,"~");
const ascii=grid.map(r=>r.join("")).join("\n");
const counts={}; for(const r of rows)counts[r.type]=(counts[r.type]||0)+1;
const propCounts={}; for(const r of props){ if(/Vegetation|Rocks|Powerline/.test(r.path))continue; const k=r.res; propCounts[k]=(propCounts[k]||0)+1; }
const topProps=Object.entries(propCounts).sort((a,b)=>b[1]-a[1]).slice(0,40).map(([res,n])=>({res,count:n}));
fs.writeFileSync("tmp/everon/saint-philippe.json",JSON.stringify({centre:{x:SP[0],z:SP[1],mapKey:"StPhillipe"},radiusM:450,counts,buildings:rows,pierQuayPieces:port,topProps,asciiMap:{cellM:cell,originX:SP[0]-450,topZ:SP[1]+450,legend:CH,rows:ascii.split("\n")}},null,1));
console.log(counts); console.log(ascii);
