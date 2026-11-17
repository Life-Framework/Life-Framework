import fs from "node:fs"; import {load} from "./parse.mjs"; import {typeOf} from "./classify.mjs";
const B=JSON.parse(fs.readFileSync("tmp/everon/buildings.json","utf8"));
const en=new Map(B.prefabs.map(p=>[p.res,p]));
const rows=load("tmp/everon/raw/world.txt");
const C={}; for(const p of B.places.filter(p=>["city","town","village"].includes(p.kind))) C[p.name]=[p.x,p.z];
const res={};
for(const [n,[cx,cz]] of Object.entries(C)){ const c={houseInterior:0,houseNoInterior:0,shopHouse:0,pub:0,church:0,chapel:0,fuelStationBuilding:0,fuelStationRoof:0,industrialBuilding:0,industrialTankSilo:0,government:0,police:0,fire:0,hotel:0,garage:0,farmBuilding:0,kiosk:0,ruin:0,shed:0,pierQuayPieces:0,pierDeckBlocks:0,breakwater:0,military:0};
 for(const r of rows){ if(Math.hypot(r.x-cx,r.z-cz)>400)continue; const t=typeOf(r.path); if(!t)continue; const p=en.get(r.res);
  if(["house","townhouse","villa","farmhouse"].includes(t)){ if(p&&p.enterable)c.houseInterior++; else c.houseNoInterior++; }
  else if(t==="shop")c.shopHouse++; else if(t==="pub")c.pub++; else if(t==="church")c.church++; else if(t==="chapel")c.chapel++;
  else if(t==="fuel"){ if(/building/.test(r.path))c.fuelStationBuilding++; else if(/roof/.test(r.path))c.fuelStationRoof++; }
  else if(t==="industrial"&&p&&p.footprintM[0]*p.footprintM[1]>=20)c.industrialBuilding++; else if(t==="industrial-tank")c.industrialTankSilo++;
  else if(t==="government")c.government++; else if(t==="police")c.police++; else if(t==="fire")c.fire++; else if(t==="hotel")c.hotel++;
  else if(t==="garage")c.garage++; else if(t==="farm")c.farmBuilding++; else if(t==="kiosk")c.kiosk++; else if(t==="ruin")c.ruin++; else if(t==="shed")c.shed++; else if(t==="military")c.military++;
  else if(t==="port"){ if(/Breakwater/.test(r.path))c.breakwater++; else if(/Pier_0|PierConcrete|PierWooden_01_(10m|25m)/.test(r.path))c.pierQuayPieces++; } }
 res[n]=c; }
fs.writeFileSync("docs/everon/town-counts-400m.json",JSON.stringify({generated:"2026-11-16",radiusM:400,centres:C,counts:res},null,1));
console.log(Object.keys(res).length);
