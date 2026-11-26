#!/usr/bin/env node
// Offline diagnostic only: consumes existing sampling-admission log lines.
// No game connection, memory access, commands, or runtime configuration.
// Usage: node tools/sampling_admission_summary.mjs Runtime.log
//        node tools/sampling_admission_summary.mjs --self-test
import {createReadStream} from "node:fs";
import {createInterface} from "node:readline";
const counters=["syncGraphAttempts","syncGraphCompleted","boundaryAttempts","boundaryCompleted","reuseHits","graphHostDemand","graphDisplayOnly","graphNoHost","graphCatalogue","graphRequestedPages","asyncJobsQueued"];
function analyze(lines){
  const groups=new Map();
  let malformed=0,warmup=0,records=0,ignored=0;
  for(const line of lines){
    const marker="sampling-admission ";
    const at=line.indexOf(marker);
    if(at<0){ignored++;continue;}
    const values={};let invalid=false;
    for(const token of line.slice(at+marker.length).trim().split(/\s+/)){
      const match=/^([A-Za-z][A-Za-z0-9]*)=(\d+)$/.exec(token);
      if(!match)continue;
      const [,key,raw]=match;
      if(Object.hasOwn(values,key)){invalid=true;break;}
      const value=Number(raw);
      if(!Number.isSafeInteger(value)||value>4294967295){invalid=true;break;}
      values[key]=value;
    }
    if(invalid||!["pid","match","warmup",...counters].every(k=>Object.hasOwn(values,k))||values.warmup>1){malformed++;continue;}
    if(values.warmup){warmup++;continue;}
    records++;
    const key=values.pid+":"+values.match;
    if(!groups.has(key))groups.set(key,{pid:values.pid,match:values.match,intervals:0,contextMismatch:0,totals:Object.fromEntries(counters.map(k=>[k,0]))});
    const group=groups.get(key);group.intervals++;
    // Counts are already interval deltas. Subscription contexts partition
    // attempts approximately; catalogue and page-request tags may overlap.
    if(values.graphHostDemand+values.graphDisplayOnly+values.graphNoHost!==values.syncGraphAttempts)group.contextMismatch++;
    for(const k of counters)group.totals[k]+=values[k];
  }
  return {records,warmup,malformed,ignored,groups:[...groups.values()]};
}
function selfTest(){
  const assert=(condition,message)=>{if(!condition)throw Error(message);};
  const record=(overrides={})=>"sampling-admission "+Object.entries({pid:42,match:1,warmup:0,...Object.fromEntries(counters.map(k=>[k,0])),...overrides}).map(([k,v])=>`${k}=${v}`).join(" ")+" scope=sync-graph-attempts;subscription-context-exclusive;catalogue-requests-overlap";
  const result=analyze(["unrelated",record({warmup:1,syncGraphAttempts:999}),record({syncGraphAttempts:5,syncGraphCompleted:4,graphNoHost:3,graphHostDemand:2,graphCatalogue:5,graphRequestedPages:4}),record({syncGraphAttempts:2,graphDisplayOnly:2,boundaryAttempts:9,reuseHits:11}),record({match:2,syncGraphAttempts:1,graphNoHost:0}),record({pid:7,match:1,boundaryAttempts:8}),record({warmup:2}),record().replace("reuseHits=0","reuseHits=oops"),record()+" reuseHits=2"]);
  assert(result.records===4&&result.warmup===1&&result.malformed===3&&result.ignored===1,"record accounting");
  assert(result.groups.length===3,"isolate pid/match");
  assert(result.groups[0].totals.syncGraphAttempts===7&&result.groups[0].totals.reuseHits===11,"sum deltas, do not difference twice");
  assert(result.groups[0].totals.graphCatalogue===5&&result.groups[0].totals.graphRequestedPages===4,"preserve overlapping contexts");
  assert(result.groups[0].contextMismatch===0&&result.groups[1].contextMismatch===1,"flag non-atomic counter mismatch");
  assert(result.groups[2].totals.boundaryAttempts===8,"separate process");
  assert(analyze([]).records===0,"empty log");
  return "PASS: 7 offline sampling diagnostic assertions";
}
const args=process.argv.slice(2);
if(args.length===1&&args[0]==="--self-test"){
  console.log(selfTest());
}else if(args.length!==1){
  console.error("Usage: node tools/sampling_admission_summary.mjs <logfile>");
  process.exitCode=2;
}else{
  try{
    const stream=createReadStream(args[0],{encoding:"utf8"});
    const reader=createInterface({input:stream,crlfDelay:Infinity});
    // Read only matching lines, rather than retaining unrelated log contents.
    const lines=[];let other=0;
    for await(const line of reader){
      if(line.includes("sampling-admission "))lines.push(line);else other++;
    }
    const result=analyze(lines);result.ignored+=other;
    console.log(JSON.stringify(result,null,2));
    if(!result.records){console.error("No usable post-warmup sampling intervals.");process.exitCode=1;}
    else if(result.malformed){console.error("Some records were malformed and excluded.");process.exitCode=1;}
  }catch(err){console.error(String(err));process.exitCode=2;}
}
