#!/usr/bin/env node
// Offline diagnostics only. Counts are already interval deltas.
// Health timings are callback peaks, not attributed time per capture cause.
// Usage: node tools/sampling_admission_summary.mjs Runtime.log
//        node tools/sampling_admission_summary.mjs --self-test
import {createReadStream} from "node:fs";
import {createInterface} from "node:readline";
const counters=["syncGraphAttempts","syncGraphCompleted","boundaryAttempts","boundaryCompleted","reuseHits","graphHostDemand","graphDisplayOnly","graphNoHost","graphCatalogue","graphRequestedPages","asyncJobsQueued"];
const healthKeys=["windowMs","observedCallbacks","controlPeakUs","scanPeakUs","worldPeakUs","controlOver16667Us"];
function parse(line,marker,keys){
  const at=line.indexOf(marker+" ");
  if(at<0)return null;
  const prefix=line.slice(0,at);
  const host=/(?:^|\s)host=(\d+)(?=\s|$)/.exec(prefix)?.[1]??"unknown";
  const required=new Set(["pid","match",...keys]);
  const values={};
  for(const token of line.slice(at+marker.length+1).trim().split(/\s+/)){
    const eq=token.indexOf("=");
    if(eq<1)continue;
    const key=token.slice(0,eq);
    if(!required.has(key))continue;
    const raw=token.slice(eq+1);
    const value=Number(raw);
    if(Object.hasOwn(values,key)||!/^\d+$/.test(raw)||!Number.isSafeInteger(value)||value>4294967295)return {invalid:true};
    values[key]=value;
  }
  if([...required].some(k=>!Object.hasOwn(values,k)))return {invalid:true};
  return {values,host,key:`${host}:${values.pid}:${values.match}`};
}
function createAnalyzer(){
  const groups=new Map(),pending=new Map();
  const result={records:0,warmup:0,malformed:0,malformedHealth:0,ignored:0,unpaired:0,groups:[]};
  function ingest(line){
    const h=parse(line,"sampling-health",healthKeys);
    if(h){
      if(h.invalid||!h.values.windowMs){result.malformedHealth++;pending.clear();return;}
      // One health record precedes its admission record in the producer.
      // Replace older unmatched windows instead of guessing their duration.
      pending.set(h.key,h.values);return;
    }
    const r=parse(line,"sampling-admission",["warmup",...counters]);
    if(!r){result.ignored++;return;}
    if(r.invalid||r.values.warmup>1){result.malformed++;pending.clear();return;}
    const v=r.values,hv=pending.get(r.key);pending.delete(r.key);
    if(v.warmup){result.warmup++;return;}
    result.records++;
    if(!groups.has(r.key))groups.set(r.key,{
      host:r.host,pid:v.pid,match:v.match,intervals:0,contextMismatch:0,
      totals:Object.fromEntries(counters.map(k=>[k,0])),
      paired:{intervals:0,windowMs:0,observedCallbacks:0,controlOver16667Us:0,controlPeakUs:0,scanPeakUs:0,worldPeakUs:0,syncGraphAttempts:0,boundaryAttempts:0,reuseHits:0}
    });
    const group=groups.get(r.key);group.intervals++;
    if(v.graphHostDemand+v.graphDisplayOnly+v.graphNoHost!==v.syncGraphAttempts)group.contextMismatch++;
    for(const k of counters)group.totals[k]+=v[k];
    if(!hv){result.unpaired++;return;}
    const p=group.paired;p.intervals++;
    for(const k of ["windowMs","observedCallbacks","controlOver16667Us"])p[k]+=hv[k];
    for(const k of ["controlPeakUs","scanPeakUs","worldPeakUs"])p[k]=Math.max(p[k],hv[k]);
    for(const k of ["syncGraphAttempts","boundaryAttempts","reuseHits"])p[k]+=v[k];
  }
  function finish(){
    result.groups=[...groups.values()].map(g=>{
      const p=g.paired;
      return {...g,paired:{...p,ratesPerSecond:p.windowMs?Object.fromEntries(["syncGraphAttempts","boundaryAttempts","reuseHits"].map(k=>[k,Number((1000*p[k]/p.windowMs).toFixed(3))])):null}};
    });
    return result;
  }
  return {ingest,finish};
}
function analyze(lines){const a=createAnalyzer();for(const line of lines)a.ingest(line);return a.finish();}
function selfTest(){
  const assert=(condition,message)=>{if(!condition)throw Error(message);};
  const line=(marker,fields,host=123)=>`2026-11-27 12:00:00.001 v1.0 host=${host} tid=7 ${marker} `+Object.entries(fields).map(([k,v])=>`${k}=${v}`).join(" ");
  const record=(overrides={},host=123)=>line("sampling-admission",{pid:42,match:1,warmup:0,...Object.fromEntries(counters.map(k=>[k,0])),...overrides},host);
  const health=(overrides={},host=123)=>line("sampling-health",{pid:42,match:1,windowMs:5000,observedCallbacks:90,controlPeakUs:7000,scanPeakUs:2000,worldPeakUs:1000,controlOver16667Us:0,...overrides},host);
  const result=analyze(["unrelated",health(),record({warmup:1,syncGraphAttempts:999}),health(),record({syncGraphAttempts:5,syncGraphCompleted:4,graphNoHost:3,graphHostDemand:2,graphCatalogue:5,graphRequestedPages:4}),health({windowMs:2000,controlPeakUs:19000,controlOver16667Us:1}),record({syncGraphAttempts:2,graphDisplayOnly:2,boundaryAttempts:9,reuseHits:11}),record({match:2,syncGraphAttempts:1}),health({},456),record({boundaryAttempts:8},456),record({warmup:2}),record().replace("reuseHits=0","reuseHits=oops"),record()+" reuseHits=2"]);
  const g=result.groups[0];
  assert(result.records===4&&result.warmup===1&&result.malformed===3&&result.ignored===1,"record accounting");
  assert(result.groups.length===3,"isolate host/pid/match");
  assert(g.totals.syncGraphAttempts===7&&g.totals.reuseHits===11,"sum existing deltas");
  assert(g.totals.graphCatalogue===5&&g.totals.graphRequestedPages===4,"preserve overlaps");
  assert(g.contextMismatch===0&&result.groups[1].contextMismatch===1,"non-atomic mismatch");
  assert(result.groups[2].totals.boundaryAttempts===8,"separate host");
  assert(g.paired.windowMs===7000&&g.paired.intervals===2,"exclude warmup window");
  assert(g.paired.ratesPerSecond.syncGraphAttempts===1,"duration-weighted rate");
  assert(g.paired.controlPeakUs===19000&&g.paired.controlOver16667Us===1,"max peak, sum over-budget observations");
  assert(result.unpaired===1&&result.groups[1].paired.ratesPerSecond===null,"no guessed rate");
  assert(analyze([health(),record({warmup:1}),record()]).unpaired===1,"do not reuse warmup timing");
  assert(analyze([health({},456),record()]).unpaired===1,"do not cross hosts");
  assert(analyze([health({windowMs:0}),record()]).malformedHealth===1,"reject zero duration");
  assert(analyze([]).records===0,"empty log");
  return "PASS: 14 offline diagnostic assertions";
}
const args=process.argv.slice(2);
if(args.length===1&&args[0]==="--self-test"){
  console.log(selfTest());
}else if(args.length!==1){
  console.error("Usage: node tools/sampling_admission_summary.mjs <logfile>");
  process.exitCode=2;
}else{
  try{
    const reader=createInterface({input:createReadStream(args[0],{encoding:"utf8"}),crlfDelay:Infinity});
    const analyzer=createAnalyzer();
    for await(const line of reader)analyzer.ingest(line);
    const result=analyzer.finish();
    console.log(JSON.stringify(result,null,2));
    if(!result.records){console.error("No usable post-warmup sampling intervals.");process.exitCode=1;}
    else if(result.malformed||result.malformedHealth){console.error("Malformed records were excluded.");process.exitCode=1;}
    if(result.unpaired)console.error("Some admission intervals lack paired health timing; rates cover paired windows only.");
  }catch(err){console.error(String(err));process.exitCode=2;}
}
