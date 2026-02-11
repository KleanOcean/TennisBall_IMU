#pragma once
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>YoTB Dashboard</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#0a0e27;color:#fff;font-family:system-ui,-apple-system,sans-serif;min-height:100vh;overflow-x:hidden}
.nav{display:flex;align-items:center;padding:10px 20px;background:#0d1130;border-bottom:1px solid #1a1f44}
.logo{font-size:1.4rem;font-weight:800;color:#fff;letter-spacing:2px;margin-right:32px}
.logo span{color:#C8D820}
.tabs{display:flex;gap:4px}
.tab{padding:7px 18px;border:none;background:transparent;color:#667788;font-size:.78rem;font-weight:600;cursor:pointer;border-radius:6px;letter-spacing:1px;text-transform:uppercase;transition:all .2s}
.tab:hover{color:#aab}
.tab.active{background:#1a1f44;color:#C8D820}
.conn{margin-left:auto;display:flex;align-items:center;gap:7px;font-size:.78rem;color:#667788}
.conn-dot{width:8px;height:8px;border-radius:50%;transition:background .3s}
.conn-dot.on{background:#44FF44;box-shadow:0 0 6px #44FF44}
.conn-dot.off{background:#FF4444;box-shadow:0 0 6px #FF4444}
.view{display:none}.view.active{display:block}
.dash{display:grid;grid-template-columns:25% 40% 35%;gap:14px;padding:14px;max-width:1300px;margin:0 auto;min-height:calc(100vh - 52px)}
.col{display:flex;flex-direction:column;gap:14px}
.card{background:#111633;border-radius:12px;padding:16px}
.card-title{font-size:.7rem;color:#667788;letter-spacing:1.5px;text-transform:uppercase;margin-bottom:10px}
/* RPM Gauge */
.gauge-wrap{display:flex;flex-direction:column;align-items:center;justify-content:center;flex:1}
.gauge-cv{display:block}
.spin-label{margin-top:8px;font-size:.95rem;font-weight:600;color:#fff;padding:3px 14px;border-radius:20px;background:#1a1f44;text-align:center}
/* Ball */
.ball-card{flex:1;display:flex;align-items:center;justify-content:center}
.ball-card canvas{max-width:100%;height:auto}
.ball-toggle{position:absolute;top:8px;right:8px;z-index:2;background:#111633cc;border:none;border-radius:6px;padding:6px 10px;font-size:.75rem;color:#667788;cursor:pointer;font-weight:600;letter-spacing:1px;transition:color .2s}
.ball-toggle:hover{color:#C8D820}
/* Charts */
.chart-card{flex:1;display:flex;flex-direction:column}
.chart-cv{width:100%;flex:1;min-height:160px;display:block}
.tl-row{display:flex;gap:6px;align-items:center;padding:8px 4px;overflow-x:auto;min-height:36px}
.tl-dot{width:14px;height:14px;border-radius:50%;cursor:pointer;flex-shrink:0;border:2px solid rgba(255,255,255,.15);transition:transform .15s}
.tl-dot:hover{transform:scale(1.3)}
.tl-dot.lime{background:#C8D820;border-color:rgba(200,216,32,.5)}
.tl-dot.gray{background:#445;border-color:#556}
/* Controls bar */
.ctrl-bar{display:flex;align-items:center;gap:10px;padding:10px 20px;flex-wrap:wrap}
.btn{padding:6px 16px;border:none;border-radius:6px;font-size:.78rem;cursor:pointer;font-weight:600;transition:opacity .2s;color:#fff}
.btn:active{opacity:.7}
.btn-rec{background:#c0392b}.btn-rec.active{animation:pulse 1s infinite}
.btn-exp{background:#2563EB}.btn-rst{background:#555}.btn-clr{background:#d97706}
.rec-info{font-size:.75rem;color:#FF4444;display:flex;align-items:center;gap:5px}
.rec-dot{width:7px;height:7px;background:#FF4444;border-radius:50%;display:inline-block}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.5}}
/* Shots view */
.shots-view{padding:14px 20px;max-width:1100px;margin:0 auto}
.stat-row{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-bottom:14px}
.stat-box{background:#111633;border-radius:10px;padding:12px;text-align:center}
.stat-val{font-size:1.6rem;font-weight:700;color:#C8D820;font-family:'Courier New',monospace}
.stat-lbl{font-size:.65rem;color:#667788;margin-top:2px;text-transform:uppercase;letter-spacing:1px}
.timeline-scroll{overflow-x:auto;padding:8px 0}
.timeline-bar{position:relative;height:80px;min-width:100%;background:#111633;border-radius:10px}
.shot-mark{position:absolute;bottom:0;width:40px;transform:translateX(-20px);cursor:pointer;text-align:center;transition:opacity .2s}
.shot-mark:hover{opacity:.8}
.shot-dot2{width:12px;height:12px;border-radius:50%;margin:0 auto 2px;border:2px solid rgba(255,255,255,.2)}
.shot-rpm2{font-size:.7rem;color:#e0e0e0;font-family:'Courier New',monospace}
.shot-type2{font-size:.6rem;font-weight:600;margin-top:1px}
.spin-TOPSPIN{color:#FF6B6B}.spin-BACKSPIN{color:#4ECDC4}
.spin-SIDE_L,.spin-SIDE_R,.spin-SIDESPIN{color:#FFE66D}
.spin-SLICE{color:#A8E6CF}.spin-FLAT{color:#888}.spin-MIXED{color:#DDA0DD}
.dot-TOPSPIN{background:#FF6B6B}.dot-BACKSPIN{background:#4ECDC4}
.dot-SIDE_L,.dot-SIDE_R,.dot-SIDESPIN{background:#FFE66D}
.dot-SLICE{background:#A8E6CF}.dot-FLAT{background:#888}.dot-MIXED{background:#DDA0DD}
.shot-detail{background:#0d1130;border:1px solid #1a1f44;border-radius:10px;padding:14px;margin-top:12px;display:none}
.shot-detail.show{display:block}
.shot-detail h3{font-size:.9rem;color:#C8D820;margin-bottom:8px}
.sd-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:6px}
.sd-item{background:#111633;border-radius:6px;padding:6px 10px;text-align:center}
.sd-item .lbl{font-size:.6rem;color:#667788;display:block;text-transform:uppercase;letter-spacing:.5px}
.sd-item .val{font-size:1rem;font-weight:600;color:#fff;font-family:'Courier New',monospace}
/* Impact flash */
.impact-flash{position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(200,216,32,.12);pointer-events:none;opacity:0;transition:opacity .1s}
.impact-flash.active{opacity:1}
/* Analysis placeholder */
.analysis-ph{display:flex;align-items:center;justify-content:center;min-height:60vh;color:#334;font-size:1.1rem}
/* Mobile */
@media(max-width:768px){
.dash{grid-template-columns:1fr;padding:10px;gap:10px}
.dash .col:nth-child(2){order:-1}
.nav{padding:8px 12px;flex-wrap:wrap;gap:6px}
.logo{margin-right:auto;font-size:1.1rem}
.tabs{order:3;width:100%}
.conn{order:2}
.tab{padding:5px 10px;font-size:.7rem}
.stat-row{grid-template-columns:repeat(2,1fr)}
.sd-grid{grid-template-columns:repeat(2,1fr)}
}
</style>
</head>
<body>
<div class="impact-flash" id="impactFlash"></div>
<nav class="nav">
<div class="logo">Yo<span>TB</span></div>
<div class="tabs">
<button class="tab active" onclick="switchTab('dashboard')">Dashboard</button>
<button class="tab" onclick="switchTab('shots')">Shots</button>
<button class="tab" onclick="switchTab('analysis')">Analysis</button>
</div>
<div class="conn"><span class="conn-dot off" id="sDot"></span><span id="sTxt">Disconnected</span></div>
</nav>

<div class="view active" id="v_dashboard">
<div class="dash">
<div class="col">
<div class="card gauge-wrap">
<canvas class="gauge-cv" id="gaugeCv" width="220" height="180"></canvas>
<div class="spin-label" id="spinType">---</div>
</div>
<div class="card">
<div class="card-title">Controls</div>
<div class="ctrl-bar">
<button class="btn btn-rec" id="btnRec" onclick="toggleRec()">Rec</button>
<span class="rec-info" id="recInfo" style="display:none"><span class="rec-dot"></span><span id="recCnt">0</span></span>
<button class="btn btn-exp" onclick="exportCSV()">CSV</button>
<button class="btn btn-rst" onclick="resetBall()">Reset</button>
<button class="btn btn-clr" onclick="clearShots()">Clear</button>
</div>
</div>
</div>
<div class="col">
<div class="card ball-card" style="position:relative">
<canvas id="ballCv" width="350" height="350"></canvas>
<button id="ballToggle" class="ball-toggle" onclick="toggleBallMode()">WIRE</button>
</div>
</div>
<div class="col">
<div class="card chart-card">
<div class="card-title">Real Time RPM</div>
<canvas class="chart-cv" id="rpmCv"></canvas>
</div>
<div class="card">
<div class="card-title">Shot Timeline</div>
<div class="tl-row" id="tlRow"></div>
</div>
</div>
</div>
</div>

<div class="view" id="v_shots">
<div class="shots-view">
<div class="stat-row">
<div class="stat-box"><div class="stat-val" id="stTotal">0</div><div class="stat-lbl">Total Shots</div></div>
<div class="stat-box"><div class="stat-val" id="stAvg">0</div><div class="stat-lbl">Avg RPM</div></div>
<div class="stat-box"><div class="stat-val" id="stMax">0</div><div class="stat-lbl">Max RPM</div></div>
<div class="stat-box"><div class="stat-val" id="stG">0</div><div class="stat-lbl">Max G</div></div>
</div>
<div class="timeline-scroll"><div class="timeline-bar" id="timelineBar"></div></div>
<div class="shot-detail" id="shotDetail">
<h3 id="sdTitle">Shot #1</h3>
<div class="sd-grid" id="sdGrid"></div>
</div>
</div>
</div>

<div class="view" id="v_analysis">
<div class="analysis-ph">Analysis coming soon</div>
</div>

<script>
'use strict';
const HIST_LEN=250,MAX_RPM=3000;
const SEAM_N=72,SEAM_AMP=0.44;
const seamPts=[];
for(let i=0;i<SEAM_N;i++){const t=2*Math.PI*i/SEAM_N;const lat=SEAM_AMP*Math.sin(2*t);seamPts.push({x:Math.cos(lat)*Math.cos(t),y:Math.cos(lat)*Math.sin(t),z:Math.sin(lat)});}

let ws=null,connected=false;
let quat={w:1,x:0,y:0,z:0},rpm=0;
let ax=0,ay=0,az=0,gx=0,gy=0,gz=0;
let rpmHist=[];
let recording=false,recData=[];
let shots=[],firstShotTime=0;
let ballMode=localStorage.getItem('ballMode')||'wire';
const impactFlash=document.getElementById('impactFlash');
const sDot=document.getElementById('sDot'),sTxt=document.getElementById('sTxt');
const spinType=document.getElementById('spinType');
const recInfo=document.getElementById('recInfo'),recCnt=document.getElementById('recCnt');
const ballCv=document.getElementById('ballCv'),ballCtx=ballCv.getContext('2d');
const gaugeCv=document.getElementById('gaugeCv'),gaugeCtx=gaugeCv.getContext('2d');
const rpmCv=document.getElementById('rpmCv'),rpmCtx=rpmCv.getContext('2d');

/* Tab switching */
function switchTab(name){
document.querySelectorAll('.tab').forEach((t,i)=>{t.classList.toggle('active',['dashboard','shots','analysis'][i]===name);});
document.querySelectorAll('.view').forEach(v=>v.classList.remove('active'));
document.getElementById('v_'+name).classList.add('active');
if(name==='shots')updateTimeline();
if(name==='dashboard')resizeChart();
}

/* Resize RPM chart canvas */
function resizeChart(){
const r=rpmCv.getBoundingClientRect();
if(r.width<1)return;
rpmCv.width=r.width*devicePixelRatio;
rpmCv.height=r.height*devicePixelRatio;
rpmCtx.scale(devicePixelRatio,devicePixelRatio);
}
resizeChart();
window.addEventListener('resize',resizeChart);

/* Quaternion rotation */
function qrot(q,v){
const tx=2*(q.y*v.z-q.z*v.y),ty=2*(q.z*v.x-q.x*v.z),tz=2*(q.x*v.y-q.y*v.x);
return{x:v.x+q.w*tx+(q.y*tz-q.z*ty),y:v.y+q.w*ty+(q.z*tx-q.x*tz),z:v.z+q.w*tz+(q.x*ty-q.y*tx)};
}

function classifySpin(gx,gy,gz){
const agx=Math.abs(gx),agy=Math.abs(gy),agz=Math.abs(gz);
const mx=Math.max(agx,agy,agz);
if(mx<30)return'---';
if(agx>agy&&agx>agz)return'TOPSPIN';
if(agy>agx&&agy>agz)return'SIDESPIN';
return'GYRO';
}

/* WebSocket */
function connectWS(){
try{ws=new WebSocket('ws://'+window.location.hostname+':81');}catch(e){setTimeout(connectWS,2000);return;}
ws.onopen=()=>{connected=true;sDot.className='conn-dot on';sTxt.textContent='Connected';};
ws.onclose=()=>{connected=false;sDot.className='conn-dot off';sTxt.textContent='Disconnected';setTimeout(connectWS,2000);};
ws.onerror=()=>{ws.close();};
ws.onmessage=e=>{
try{
const d=JSON.parse(e.data);
ax=d.ax||0;ay=d.ay||0;az=d.az||0;
gx=d.gx||0;gy=d.gy||0;gz=d.gz||0;
quat={w:d.qw||1,x:d.qx||0,y:d.qy||0,z:d.qz||0};
rpm=d.rpm||0;
rpmHist.push(rpm);if(rpmHist.length>HIST_LEN)rpmHist.shift();
if(recording){recData.push({t:d.t||0,ax,ay,az,gx,gy,gz,qw:quat.w,qx:quat.x,qy:quat.y,qz:quat.z,rpm});recCnt.textContent=recData.length;}
if(d.imp===1){impactFlash.classList.add('active');setTimeout(()=>impactFlash.classList.remove('active'),150);}
if(d.spin){spinType.textContent=d.spin;spinType.className='spin-label spin-'+d.spin;}
if(d.event==='shot'){shots.push(d);if(shots.length===1)firstShotTime=d.t;updateTimeline();updateTlDots();}
}catch(err){}
};
}

/* Ball mode toggle */
function toggleBallMode(){
ballMode=(ballMode==='wire')?'solid':'wire';
localStorage.setItem('ballMode',ballMode);
document.getElementById('ballToggle').textContent=ballMode==='wire'?'WIRE':'SOLID';
}

/* Draw solid ball */
function drawBallSolid(){
const w=350,h=350,cx=w/2,cy=h/2,R=140;
const ctx=ballCtx;
ctx.clearRect(0,0,w,h);

/* Filled sphere with gradient */
const grd=ctx.createRadialGradient(cx-R*0.3,cy-R*0.3,R*0.05,cx,cy,R);
grd.addColorStop(0,'#E8F040');
grd.addColorStop(1,'#C8D820');
ctx.beginPath();
ctx.arc(cx,cy,R,0,2*Math.PI);
ctx.fillStyle=grd;
ctx.fill();

/* Thin dark outline */
ctx.beginPath();
ctx.arc(cx,cy,R,0,2*Math.PI);
ctx.strokeStyle='rgba(40,50,10,0.4)';
ctx.lineWidth=1.5;
ctx.stroke();

/* Seam curve */
const sPts=[];
for(let i=0;i<=SEAM_N;i++){
const idx=i%SEAM_N;
const rv=qrot(quat,seamPts[idx]);
sPts.push({sx:cx+rv.x*R,sy:cy-rv.y*R,z:rv.z});
}
for(let i=1;i<sPts.length;i++){
const p0=sPts[i-1],p1=sPts[i];
const zAvg=(p0.z+p1.z)/2;
if(zAvg<=-0.15)continue;
ctx.beginPath();
ctx.moveTo(p0.sx,p0.sy);
ctx.lineTo(p1.sx,p1.sy);
if(zAvg>0){
ctx.strokeStyle='#FFFFFF';
ctx.lineWidth=2.5;
}else{
ctx.strokeStyle='rgba(200,200,200,0.3)';
ctx.lineWidth=1.5;
}
ctx.stroke();
}
}

/* Draw wireframe ball */
function drawBall(){
if(ballMode==='solid'){drawBallSolid();return;}
const w=350,h=350,cx=w/2,cy=h/2,R=140;
const ctx=ballCtx;
ctx.clearRect(0,0,w,h);

/* Glow behind ball */
const grd=ctx.createRadialGradient(cx,cy,R*0.3,cx,cy,R*1.3);
grd.addColorStop(0,'rgba(200,216,32,0.08)');
grd.addColorStop(0.6,'rgba(200,216,32,0.03)');
grd.addColorStop(1,'rgba(200,216,32,0)');
ctx.fillStyle=grd;
ctx.fillRect(0,0,w,h);

const DEG=Math.PI/180;

/* Helper: draw line strip with depth-based opacity */
function drawStrip(pts,lw,base){
for(let i=1;i<pts.length;i++){
const p0=pts[i-1],p1=pts[i];
const zAvg=(p0.z+p1.z)/2;
const a=0.12+0.88*Math.max(0,zAvg/R);
ctx.beginPath();
ctx.moveTo(p0.sx,p0.sy);
ctx.lineTo(p1.sx,p1.sy);
ctx.strokeStyle=base?base:'rgba(200,216,32,'+a.toFixed(2)+')';
if(!base)ctx.strokeStyle='rgba(200,216,32,'+a.toFixed(2)+')';
ctx.lineWidth=lw;
ctx.stroke();
}
}

/* Longitude lines (12 lines, every 30 deg) */
for(let phi=0;phi<360;phi+=30){
const pts=[];
for(let th=0;th<=180;th+=8){
const tr=th*DEG,pr=phi*DEG;
const v={x:Math.sin(tr)*Math.cos(pr),y:Math.sin(tr)*Math.sin(pr),z:Math.cos(tr)};
const rv=qrot(quat,v);
pts.push({sx:cx+rv.x*R,sy:cy-rv.y*R,z:rv.z*R});
}
drawStrip(pts,0.8);
}

/* Latitude lines (5 rings at 30,60,90,120,150) */
for(let th=30;th<=150;th+=30){
const pts=[];
for(let phi=0;phi<=360;phi+=8){
const tr=th*DEG,pr=phi*DEG;
const v={x:Math.sin(tr)*Math.cos(pr),y:Math.sin(tr)*Math.sin(pr),z:Math.cos(tr)};
const rv=qrot(quat,v);
pts.push({sx:cx+rv.x*R,sy:cy-rv.y*R,z:rv.z*R});
}
drawStrip(pts,0.8);
}

/* Seam curve (thicker) */
const sPts=[];
for(let i=0;i<=SEAM_N;i++){
const idx=i%SEAM_N;
const rv=qrot(quat,seamPts[idx]);
sPts.push({sx:cx+rv.x*R,sy:cy-rv.y*R,z:rv.z*R});
}
for(let i=1;i<sPts.length;i++){
const p0=sPts[i-1],p1=sPts[i];
const zAvg=(p0.z+p1.z)/2;
const a=0.15+0.85*Math.max(0,zAvg/R);
ctx.beginPath();
ctx.moveTo(p0.sx,p0.sy);
ctx.lineTo(p1.sx,p1.sy);
ctx.strokeStyle='rgba(200,216,32,'+a.toFixed(2)+')';
ctx.lineWidth=2.5;
ctx.stroke();
}
}

/* Draw RPM gauge (semi-circular arc) */
function drawGauge(){
const w=220,h=180,cx=110,cy=140,R=85;
const ctx=gaugeCtx;
ctx.clearRect(0,0,w,h);
const startA=0.75*Math.PI;
const endA=2.25*Math.PI;
const sweep=endA-startA;

/* Track */
ctx.beginPath();
ctx.arc(cx,cy,R,startA,endA);
ctx.strokeStyle='#222244';
ctx.lineWidth=14;
ctx.lineCap='round';
ctx.stroke();

/* Fill */
const frac=Math.min(rpm/MAX_RPM,1);
if(frac>0.005){
ctx.beginPath();
ctx.arc(cx,cy,R,startA,startA+sweep*frac);
ctx.strokeStyle='#C8D820';
ctx.lineWidth=14;
ctx.lineCap='round';
ctx.shadowColor='rgba(200,216,32,0.4)';
ctx.shadowBlur=12;
ctx.stroke();
ctx.shadowBlur=0;
}

/* RPM text */
ctx.fillStyle='#C8D820';
ctx.font='bold 2.6rem system-ui';
ctx.textAlign='center';
ctx.textBaseline='middle';
ctx.fillText(Math.round(rpm),cx,cy-8);
ctx.fillStyle='#667788';
ctx.font='600 0.7rem system-ui';
ctx.fillText('RPM',cx,cy+22);

/* Scale labels */
ctx.fillStyle='#445';
ctx.font='0.55rem system-ui';
ctx.textAlign='center';
const labels=[0,1000,2000,3000];
for(let i=0;i<labels.length;i++){
const ang=startA+sweep*(labels[i]/MAX_RPM);
const lx=cx+Math.cos(ang)*(R+18);
const ly=cy+Math.sin(ang)*(R+18);
ctx.fillText(labels[i],lx,ly);
}
}

/* Draw RPM chart */
function drawRpmChart(){
const cw=rpmCv.width/devicePixelRatio,ch=rpmCv.height/devicePixelRatio;
if(cw<2)return;
const ctx=rpmCtx;
ctx.clearRect(0,0,cw*devicePixelRatio,ch*devicePixelRatio);
ctx.save();
const pad={l:40,r:10,t:8,b:20};
const pw=cw-pad.l-pad.r,ph=ch-pad.t-pad.b;

/* Determine Y range */
let yMax=200;
for(let i=0;i<rpmHist.length;i++)if(rpmHist[i]>yMax)yMax=rpmHist[i];
yMax=Math.ceil(yMax/100)*100+100;

/* Grid */
ctx.strokeStyle='rgba(255,255,255,0.04)';
ctx.lineWidth=1;
const nGrid=4;
for(let i=0;i<=nGrid;i++){
const y=pad.t+ph*(i/nGrid);
ctx.beginPath();ctx.moveTo(pad.l,y);ctx.lineTo(pad.l+pw,y);ctx.stroke();
const val=yMax-yMax*(i/nGrid);
ctx.fillStyle='#445';ctx.font='9px monospace';ctx.textAlign='right';
ctx.fillText(Math.round(val),pad.l-5,y+3);
}

/* Time labels on X axis */
ctx.fillStyle='#334';ctx.font='9px monospace';ctx.textAlign='center';
const secBack=HIST_LEN/50;
for(let i=0;i<=4;i++){
const x=pad.l+pw*(i/4);
const s=Math.round(secBack*(1-i/4));
ctx.fillText(s+'s',x,pad.t+ph+14);
}

/* RPM line */
if(rpmHist.length>1){
ctx.beginPath();
ctx.strokeStyle='#C8D820';
ctx.lineWidth=1.8;
ctx.shadowColor='rgba(200,216,32,0.3)';
ctx.shadowBlur=4;
for(let i=0;i<rpmHist.length;i++){
const x=pad.l+(i/(HIST_LEN-1))*pw;
const y=pad.t+ph*(1-rpmHist[i]/yMax);
i===0?ctx.moveTo(x,y):ctx.lineTo(x,y);
}
ctx.stroke();
ctx.shadowBlur=0;
}
ctx.restore();
}

/* Update shot timeline dots on dashboard */
function updateTlDots(){
const row=document.getElementById('tlRow');
row.innerHTML='';
shots.forEach((s,i)=>{
const d=document.createElement('div');
d.className='tl-dot '+(s.rpm>50?'lime':'gray');
d.title='#'+(i+1)+' '+Math.round(s.rpm)+' RPM '+s.type;
d.onclick=()=>{switchTab('shots');showShotDetail(i);};
row.appendChild(d);
});
}

/* Update shot stats and timeline bar in Shots tab */
function updateTimeline(){
const bar=document.getElementById('timelineBar');
const total=document.getElementById('stTotal');
const avgEl=document.getElementById('stAvg');
const maxEl=document.getElementById('stMax');
const maxGEl=document.getElementById('stG');
if(!shots.length){bar.innerHTML='';total.textContent='0';avgEl.textContent='0';maxEl.textContent='0';maxGEl.textContent='0';return;}
let sumR=0,mxR=0,mxG=0;
shots.forEach(s=>{sumR+=s.rpm;if(s.rpm>mxR)mxR=s.rpm;if(s.peakG>mxG)mxG=s.peakG;});
total.textContent=shots.length;
avgEl.textContent=Math.round(sumR/shots.length);
maxEl.textContent=Math.round(mxR);
maxGEl.textContent=mxG.toFixed(1);

const timeSpan=Math.max(shots[shots.length-1].t-firstShotTime,1000);
const barW=Math.max(shots.length*50,bar.parentElement.clientWidth);
bar.style.width=barW+'px';
bar.innerHTML='';
shots.forEach((s,i)=>{
const pct=(s.t-firstShotTime)/timeSpan;
const x=20+pct*(barW-40);
const mk=document.createElement('div');
mk.className='shot-mark';
mk.style.left=x+'px';
mk.onclick=()=>showShotDetail(i);
mk.innerHTML='<div class="shot-dot2 dot-'+s.type+'"></div><div class="shot-rpm2">'+Math.round(s.rpm)+'</div><div class="shot-type2 spin-'+s.type+'">'+s.type+'</div>';
bar.appendChild(mk);
});
updateTlDots();
}

function showShotDetail(idx){
const s=shots[idx];
const detail=document.getElementById('shotDetail');
const title=document.getElementById('sdTitle');
const grid=document.getElementById('sdGrid');
detail.classList.add('show');
title.textContent='Shot #'+(idx+1)+' - '+s.type;
title.style.color=getSpinColor(s.type);
grid.innerHTML=
'<div class="sd-item"><span class="lbl">RPM</span><span class="val">'+Math.round(s.rpm)+'</span></div>'+
'<div class="sd-item"><span class="lbl">Peak G</span><span class="val">'+s.peakG.toFixed(1)+'</span></div>'+
'<div class="sd-item"><span class="lbl">Type</span><span class="val">'+s.type+'</span></div>'+
'<div class="sd-item"><span class="lbl">Gyro X</span><span class="val">'+s.gx.toFixed(1)+'</span></div>'+
'<div class="sd-item"><span class="lbl">Gyro Y</span><span class="val">'+s.gy.toFixed(1)+'</span></div>'+
'<div class="sd-item"><span class="lbl">Gyro Z</span><span class="val">'+s.gz.toFixed(1)+'</span></div>';
}

function getSpinColor(type){
const c={'TOPSPIN':'#FF6B6B','BACKSPIN':'#4ECDC4','SIDE_L':'#FFE66D','SIDE_R':'#FFE66D','SIDESPIN':'#FFE66D','SLICE':'#A8E6CF','FLAT':'#888','MIXED':'#DDA0DD'};
return c[type]||'#fff';
}

/* Recording & export */
function toggleRec(){
recording=!recording;
const btn=document.getElementById('btnRec');
if(recording){btn.textContent='Stop';btn.classList.add('active');recData=[];recCnt.textContent='0';recInfo.style.display='flex';}
else{btn.textContent='Rec';btn.classList.remove('active');recInfo.style.display='none';}
}

function exportCSV(){
if(!recData.length&&!shots.length){alert('No data.');return;}
let csv='';
if(recData.length){
csv+='timestamp,ax,ay,az,gx,gy,gz,qw,qx,qy,qz,rpm\n';
recData.forEach(r=>{csv+=r.t+','+r.ax+','+r.ay+','+r.az+','+r.gx+','+r.gy+','+r.gz+','+r.qw+','+r.qx+','+r.qy+','+r.qz+','+r.rpm+'\n';});
}
if(shots.length){
csv+='\nShots\nid,timestamp,rpm,peakG,gx,gy,gz,type\n';
shots.forEach((s,i)=>{csv+=i+','+s.t+','+s.rpm+','+s.peakG+','+s.gx+','+s.gy+','+s.gz+','+s.type+'\n';});
}
const blob=new Blob([csv],{type:'text/csv'});
const a=document.createElement('a');a.href=URL.createObjectURL(blob);a.download='yotb_data.csv';a.click();URL.revokeObjectURL(a.href);
}

function resetBall(){if(ws&&ws.readyState===1)ws.send('reset');}
function clearShots(){
shots=[];firstShotTime=0;
updateTimeline();
document.getElementById('shotDetail').classList.remove('show');
if(ws&&ws.readyState===1)ws.send('clear_shots');
}

/* Main render loop */
let lastFrame=0;
function loop(ts){
requestAnimationFrame(loop);
if(ts-lastFrame<32)return;
lastFrame=ts;
drawBall();
drawGauge();
drawRpmChart();
if(!connected)spinType.textContent=classifySpin(gx,gy,gz);
}

document.getElementById('ballToggle').textContent=ballMode==='wire'?'WIRE':'SOLID';
connectWS();
requestAnimationFrame(loop);
</script>
</body>
</html>
)rawliteral";
