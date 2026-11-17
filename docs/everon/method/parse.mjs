import fs from "node:fs";
export function load(file){
  const rows=[]; const seen=new Set();
  for(const l of fs.readFileSync(file,"utf8").split("\n")){
    if(!l.startsWith("B|"))continue; const p=l.split("|");
    const key=p[1]+"@"+Math.round(p[2]*10)+","+Math.round(p[4]*10); if(seen.has(key))continue; seen.add(key);
    const g=/^\{([0-9A-F]+)\}(.*)$/.exec(p[1]);
    rows.push({guid:g[1],path:g[2],res:p[1],x:+p[2],y:+p[3],z:+p[4],yaw:+p[5],sx:+p[6],sy:+p[7],sz:+p[8],doors:+p[9],kids:+p[10],cls:p[11],miny:+p[12]});
  } return rows;
}
export function places(file){
  const out=[];
  for(const l of fs.readFileSync(file,"utf8").split("\n")){
    if(!l.startsWith("M|"))continue; const p=l.split("|");
    if(!p[1].startsWith("#AR-MapLocation_"))continue;
    out.push({key:p[1].replace("#AR-MapLocation_",""),type:+p[2],x:+p[4],z:+p[5],prefab:p[6]});
  } return out;
}
