import fs from "node:fs";
const B=JSON.parse(fs.readFileSync("tmp/everon/buildings.json","utf8"));
import {load} from "./parse.mjs"; import {typeOf} from "./classify.mjs";
const rows=load("tmp/everon/raw/world.txt"); const SP=[4503,10772];
const want=["house","townhouse","villa","farmhouse","shop","pub","kiosk","church","chapel","fuel","government","police","fire","hotel","garage","farm","industrial","military","airport","recreation","port"];
const plan=[];
for(const t of want){
  let cand=rows.filter(r=>typeOf(r.path)===t && r.sx*r.sz>=(t==="port"?10:12) && !/BuildingAddons|HouseAddons\/.*(roof|platform)|PierCover|pole|SeaBollard|Breakwater|nowalls|Terrace|Furniture\/|HouseRuins/.test(r.path));
  if(t==="port")cand=cand.filter(r=>/Pier_01|PierConcrete_01_20m/.test(r.path));
  if(t==="industrial")cand=cand.filter(r=>/Warehouse_01\.|FactoryHall|Office_E_01\.|Sawmill|Workshop_02\.|Garage_E_02/.test(r.path));
  const cnt={}; for(const r of cand)cnt[r.res]=(cnt[r.res]||0)+1;
  const top=Object.entries(cnt).sort((a,b)=>b[1]-a[1]).slice(0,3).map(e=>e[0]);
  for(const res of top){ const insts=cand.filter(r=>r.res===res).sort((a,b)=>Math.hypot(a.x-SP[0],a.z-SP[1])-Math.hypot(b.x-SP[0],b.z-SP[1])); const r=insts[0]; if(!r)continue;
   plan.push({type:t,res,x:+r.x.toFixed(1),y:+r.y.toFixed(1),z:+r.z.toFixed(1),yaw:+r.yaw.toFixed(0),distFromSaintPhilippeM:Math.round(Math.hypot(r.x-SP[0],r.z-SP[1])),instancesOnEveron:cnt[res],sizeM:[+r.sx.toFixed(1),+r.sz.toFixed(1),+r.sy.toFixed(1)]}); }
}
fs.writeFileSync("tmp/everon/pics-plan.json",JSON.stringify(plan,null,1)); console.log(plan.length); for(const p of plan.filter((p,i)=>true)) console.log(p.type,p.res.split("/").pop(),p.x,p.z,p.distFromSaintPhilippeM);
