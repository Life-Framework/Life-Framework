import fs from "node:fs";
import {load,places} from "./parse.mjs"; import {typeOf} from "./classify.mjs";
const W="tmp/everon/raw/world.txt";
const rows=load(W); const pl=places(W);
const ms=JSON.parse(fs.readFileSync("tmp/everon/measure.json","utf8")); const mByRes=new Map(ms.map(m=>[m.ref,m]));
const EXCL=/railing|_roof\.|_platform|fuelsign|tankpistol|_panels?\.|paneling|BackWall|LeftWall|RightWall|_walls\.|Terrace|furniture_01|\/Dst\/|PierCover|PierWooden_01_pole|Pier_Wood_Long|SeaBollard|StatueAnchor|ShedMetal_0\d_benches|\/GateCollective/;
function floors(m){ if(!m||!m.bounds)return null; const fx=(m.bounds.max[0]-m.bounds.min[0])*(m.bounds.max[2]-m.bounds.min[2]);
  const lv=[]; for(const s of (m.surfaces||[]).slice().sort((a,b)=>a.y-b.y)){let l=lv.find(l=>Math.abs(l.y-s.y)<1.0); if(!l){l={y:s.y,area:0,n:0};lv.push(l);} l.area+=s.area;}
  const big=lv.filter(l=>l.area>=Math.max(12,fx*0.15)); return {levelsY:big.map(l=>+l.y.toFixed(1)), above:big.filter(l=>l.y>-1.0).length, below:big.filter(l=>l.y<=-1.0).length};}
// places
const PL=pl.map(p=>{let kind="site",rad=300; if(p.type===59){kind="city";rad=800;} else if(p.type===61){kind="town";rad=550;} else if(p.type===60){kind="village";rad=380;} else if(p.type===62){kind="hamlet";rad=230;} else if(p.type===39){kind="castle";rad=350;} else if(p.type!==58)return null; return {...p,kind,rad};}).filter(Boolean);
for(const p of pl) if(p.type===39) {}
// building instances
const inst=[]; const prefabs=new Map();
for(const r of rows){ const t=typeOf(r.path); if(!t)continue; if(EXCL.test(r.path))continue; if(r.sx*r.sy*r.sz===0&&!/_walls\.et|nowalls/.test(r.path))continue; if(t==="port"||t==="waterworks"||t==="industrial-tower"||t==="castle"){ }
  inst.push({...r,type:t});
  let p=prefabs.get(r.res); if(!p){p={res:r.res,guid:r.guid,path:r.path,type:t,n:0,sx:r.sx,sy:r.sy,sz:r.sz,doors:r.doors,kids:r.kids,miny:r.miny,towns:{}};prefabs.set(r.res,p);} p.n++; }
for(const b of inst){ let best=null,bs=9; for(const p of PL){const d=Math.hypot(b.x-p.x,b.z-p.z); const s=d/p.rad; if(s<1&&s<bs){bs=s;best=p;}} b.place=best?best.key:null; b.placeKind=best?best.kind:null; b.d=best?Math.hypot(b.x-best.x,b.z-best.z):null; const p=prefabs.get(b.res); const k=best?best.key:"(rural)"; p.towns[k]=(p.towns[k]||0)+1; }
const NAME={StPhillipe:"Saint Philippe",StPierre:"Saint Pierre",LeMoule:"Le Moule",MontfortCastle:"Montfort Castle",EntreDeux:"Entre-Deux",LesCreux:"Les Creux",LaValette:"La Valette",LeBosc:"Le Bosc"};
const nice=k=>NAME[k]||k.replace(/([a-z])([A-Z])/g,"$1 $2");
// prefab output
const out=[];
for(const p of prefabs.values()){ const m=mByRes.get(p.res); const fl=floors(m); 
  const interior=(p.doors>0&&fl&&fl.above>0)||(p.doors>0&&!fl&&p.kids>3); 
  out.push({res:p.res,guid:p.guid,path:p.path,type:p.type,instances:p.n,
   footprintM:[+p.sx.toFixed(1),+p.sz.toFixed(1)],areaM2:Math.round(p.sx*p.sz),heightM:m&&m.bounds?+m.bounds.max[1].toFixed(1):+(p.sy+p.miny).toFixed(1),
   basementM:m&&m.bounds&&m.bounds.min[1]<-2?+(-m.bounds.min[1]).toFixed(1):0,
   floors:fl?fl.above:null,floorLevelsY:fl?fl.levelsY:null,doorChildren:Math.round(p.doors),
   enterable:!!interior, measured:!!m,
   towns:Object.fromEntries(Object.entries(p.towns).sort((a,b)=>b[1]-a[1]).slice(0,12).map(([k,v])=>[k==="(rural)"?k:nice(k),v]))});
}
out.sort((a,b)=>a.type.localeCompare(b.type)||b.instances-a.instances);
// place table
const placeOut=[];
for(const p of PL){ const mine=inst.filter(b=>b.place===p.key); const c={}; for(const b of mine)c[b.type]=(c[b.type]||0)+1; placeOut.push({key:p.key,name:nice(p.key),kind:p.kind,x:+p.x.toFixed(0),z:+p.z.toFixed(0),radiusM:p.rad,buildings:c,total:mine.length}); }
// non-site places (natural) just coordinates
const nat=pl.filter(p=>![58,59,60,61,62,39].includes(p.type)).map(p=>({key:p.key,name:nice(p.key),mapType:p.type,x:+p.x.toFixed(0),z:+p.z.toFixed(0)}));
fs.writeFileSync("tmp/everon/buildings.json",JSON.stringify({prefabs:out,places:placeOut,naturalPlaces:nat},null,1));
console.log("prefabs",out.length,"instances",inst.length,"places",placeOut.length,"nat",nat.length);
const bt={}; for(const b of inst)bt[b.type]=(bt[b.type]||0)+1; console.log(bt);
for(const p of placeOut.filter(p=>p.kind!=="site"&&p.kind!=="hamlet").sort((a,b)=>b.total-a.total)) console.log(p.name,p.kind,p.x,p.z,p.total,JSON.stringify(p.buildings));
