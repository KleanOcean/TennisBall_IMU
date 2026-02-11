#pragma once
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>TennisBall IMU</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{background:#1a1a2e;color:#e0e0e0;font-family:system-ui,-apple-system,sans-serif;min-height:100vh;overflow-x:hidden}
.header{display:flex;justify-content:space-between;align-items:center;padding:12px 20px;background:#0f3460;border-bottom:2px solid #1a5276}
.header h1{font-size:1.3rem;color:#C8D820;letter-spacing:1px}
.status{display:flex;align-items:center;gap:8px;font-size:0.85rem}
.status-dot{width:10px;height:10px;border-radius:50%;transition:background 0.3s}
.status-dot.on{background:#44FF44;box-shadow:0 0 6px #44FF44}
.status-dot.off{background:#FF4444;box-shadow:0 0 6px #FF4444}
.main{display:grid;grid-template-columns:1fr 1fr;gap:16px;padding:16px;max-width:1200px;margin:0 auto}
@media(max-width:768px){.main{grid-template-columns:1fr;padding:10px;gap:10px}}
.card{background:#16213e;border-radius:12px;padding:16px;box-shadow:0 4px 12px rgba(0,0,0,0.3)}
.card h2{font-size:0.95rem;color:#8ab4f8;margin-bottom:10px}
.ball-wrap{display:flex;flex-direction:column;align-items:center;gap:10px}
.ball-wrap canvas{max-width:100%;height:auto}
.rpm-display{text-align:center}
.rpm-val{font-size:2.8rem;font-weight:700;color:#C8D820;font-family:'Courier New',monospace}
.rpm-label{font-size:0.8rem;color:#999;margin-top:2px}
.spin-type{font-size:1rem;font-weight:600;color:#e0e0e0;margin-top:4px;padding:3px 14px;border-radius:20px;background:#0f3460;display:inline-block}
.chart-canvas{width:100%;height:150px;display:block}
.data-grid{display:grid;grid-template-columns:1fr 1fr;gap:6px}
.data-item{display:flex;justify-content:space-between;padding:5px 10px;background:#1a1a2e;border-radius:6px;font-size:0.82rem}
.data-item .lbl{color:#8ab4f8}.data-item .val{font-family:'Courier New',monospace;color:#e0e0e0}
.controls{display:flex;justify-content:center;align-items:center;gap:12px;padding:12px 16px;flex-wrap:wrap}
.btn{padding:8px 20px;border:none;border-radius:8px;font-size:0.85rem;cursor:pointer;font-weight:600;transition:opacity 0.2s}
.btn:active{opacity:0.7}
.btn-rec{background:#c0392b;color:#fff}
.btn-rec.active{animation:pulse 1s infinite}
.btn-export{background:#2980b9;color:#fff}
.btn-reset{background:#7f8c8d;color:#fff}
.rec-info{font-size:0.8rem;color:#FF4444;display:flex;align-items:center;gap:5px}
.rec-dot{width:8px;height:8px;background:#FF4444;border-radius:50%;display:inline-block}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:0.5}}
.timeline-wrap{padding:12px 16px}
.timeline-card{background:#16213e;border-radius:12px;padding:16px;box-shadow:0 4px 12px rgba(0,0,0,0.3)}
.timeline-card h2{font-size:0.95rem;color:#8ab4f8;margin-bottom:10px}
.shot-stats{display:flex;gap:16px;margin-bottom:12px;flex-wrap:wrap}
.stat-box{background:#1a1a2e;border-radius:8px;padding:8px 14px;text-align:center;min-width:80px}
.stat-val{font-size:1.4rem;font-weight:700;color:#C8D820;font-family:'Courier New',monospace}
.stat-lbl{font-size:0.7rem;color:#888;margin-top:2px}
.timeline-scroll{overflow-x:auto;padding:8px 0}
.timeline-bar{position:relative;height:80px;min-width:100%;background:#1a1a2e;border-radius:8px}
.shot-mark{position:absolute;bottom:0;width:36px;transform:translateX(-18px);cursor:pointer;text-align:center;transition:opacity 0.2s}
.shot-mark:hover{opacity:0.8}
.shot-dot{width:12px;height:12px;border-radius:50%;margin:0 auto 2px;border:2px solid rgba(255,255,255,0.3)}
.shot-rpm{font-size:0.7rem;color:#e0e0e0;font-family:'Courier New',monospace}
.shot-type{font-size:0.6rem;font-weight:600;margin-top:1px}
.spin-TOPSPIN{color:#FF6B6B}.spin-BACKSPIN{color:#4ECDC4}
.spin-SIDE_L,.spin-SIDE_R,.spin-SIDESPIN{color:#FFE66D}
.spin-SLICE{color:#A8E6CF}.spin-FLAT{color:#888}
.spin-MIXED{color:#DDA0DD}
.dot-TOPSPIN{background:#FF6B6B}.dot-BACKSPIN{background:#4ECDC4}
.dot-SIDE_L,.dot-SIDE_R,.dot-SIDESPIN{background:#FFE66D}
.dot-SLICE{background:#A8E6CF}.dot-FLAT{background:#888}
.dot-MIXED{background:#DDA0DD}
.shot-detail{background:#0f3460;border-radius:10px;padding:14px;margin-top:10px;display:none}
.shot-detail.show{display:block}
.shot-detail h3{font-size:0.9rem;color:#C8D820;margin-bottom:8px}
.shot-detail-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:6px}
.shot-detail-item{background:#1a1a2e;border-radius:6px;padding:6px 10px;text-align:center}
.shot-detail-item .lbl{font-size:0.65rem;color:#8ab4f8;display:block}
.shot-detail-item .val{font-size:1rem;font-weight:600;color:#e0e0e0;font-family:'Courier New',monospace}
.impact-flash{position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(255,100,100,0.15);pointer-events:none;opacity:0;transition:opacity 0.1s}
.impact-flash.active{opacity:1}
.btn-clear{background:#e67e22;color:#fff}
</style>
</head>
<body>
<div class="impact-flash" id="impactFlash"></div>
<div class="header">
<h1>TennisBall IMU</h1>
<div class="status"><span class="status-dot off" id="sDot"></span><span id="sTxt">Disconnected</span></div>
</div>
<div class="main">
<div class="card">
<div class="ball-wrap">
<canvas id="ballCv" width="300" height="300"></canvas>
<div class="rpm-display">
<div class="rpm-val" id="rpmVal">0</div>
<div class="rpm-label">RPM</div>
<div class="spin-type" id="spinType">---</div>
</div>
</div>
</div>
<div style="display:flex;flex-direction:column;gap:16px">
<div class="card">
<h2>Gyroscope (&deg;/s)</h2>
<canvas id="gyroCv" class="chart-canvas"></canvas>
</div>
<div class="card">
<h2>Accelerometer (g)</h2>
<canvas id="accelCv" class="chart-canvas"></canvas>
</div>
<div class="card">
<h2>Current Values</h2>
<div class="data-grid" id="dataGrid"></div>
</div>
</div>
</div>
<div class="controls">
<button class="btn btn-rec" id="btnRec" onclick="toggleRec()">Record</button>
<span class="rec-info" id="recInfo" style="display:none"><span class="rec-dot"></span>Recording... <span id="recCnt">0</span></span>
<button class="btn btn-export" onclick="exportCSV()">Export CSV</button>
<button class="btn btn-reset" onclick="resetBall()">Reset Ball</button>
<button class="btn btn-clear" onclick="clearShots()">Clear Shots</button>
</div>
<div class="timeline-wrap">
<div class="timeline-card">
<h2>Shot Timeline</h2>
<div class="shot-stats" id="shotStats">
<div class="stat-box"><div class="stat-val" id="statTotal">0</div><div class="stat-lbl">Shots</div></div>
<div class="stat-box"><div class="stat-val" id="statAvgRPM">0</div><div class="stat-lbl">Avg RPM</div></div>
<div class="stat-box"><div class="stat-val" id="statMaxRPM">0</div><div class="stat-lbl">Max RPM</div></div>
<div class="stat-box"><div class="stat-val" id="statMaxG">0</div><div class="stat-lbl">Max G</div></div>
</div>
<div class="timeline-scroll"><div class="timeline-bar" id="timelineBar"></div></div>
<div class="shot-detail" id="shotDetail">
<h3 id="shotDetailTitle">Shot #1</h3>
<div class="shot-detail-grid" id="shotDetailGrid"></div>
</div>
</div>
</div>
<script>
'use strict';
const HIST_LEN=250;
const SEAM_N=72,SEAM_AMP=0.44;
const seamPts=[];
for(let i=0;i<SEAM_N;i++){const t=2*Math.PI*i/SEAM_N;const lat=SEAM_AMP*Math.sin(2*t);seamPts.push({x:Math.cos(lat)*Math.cos(t),y:Math.cos(lat)*Math.sin(t),z:Math.sin(lat)});}

let ws=null,connected=false;
let quat={w:1,x:0,y:0,z:0},rpm=0;
let ax=0,ay=0,az=0,gx=0,gy=0,gz=0;
let gyroH={x:[],y:[],z:[]},accelH={x:[],y:[],z:[]};
let recording=false,recData=[];
let shots=[];
let firstShotTime=0;
const impactFlash=document.getElementById('impactFlash');

const sDot=document.getElementById('sDot'),sTxt=document.getElementById('sTxt');
const rpmVal=document.getElementById('rpmVal'),spinType=document.getElementById('spinType');
const recInfo=document.getElementById('recInfo'),recCnt=document.getElementById('recCnt');
const ballCv=document.getElementById('ballCv'),ballCtx=ballCv.getContext('2d');
const gyroCv=document.getElementById('gyroCv'),gyroCtx=gyroCv.getContext('2d');
const accelCv=document.getElementById('accelCv'),accelCtx=accelCv.getContext('2d');
const dataGrid=document.getElementById('dataGrid');

const dataFields=['ax','ay','az','gx','gy','gz'];
const dataEls={};
dataFields.forEach(f=>{const d=document.createElement('div');d.className='data-item';d.innerHTML='<span class="lbl">'+f+'</span><span class="val" id="dv_'+f+'">0.00</span>';dataGrid.appendChild(d);dataEls[f]=d.querySelector('.val');});

function resizeCharts(){
[gyroCv,accelCv].forEach(c=>{const r=c.getBoundingClientRect();c.width=r.width*devicePixelRatio;c.height=r.height*devicePixelRatio;c.getContext('2d').scale(devicePixelRatio,devicePixelRatio);});
}
resizeCharts();
window.addEventListener('resize',resizeCharts);

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

function connectWS(){
try{ws=new WebSocket('ws://'+window.location.hostname+':81');}catch(e){setTimeout(connectWS,2000);return;}
ws.onopen=()=>{connected=true;sDot.className='status-dot on';sTxt.textContent='Connected';};
ws.onclose=()=>{connected=false;sDot.className='status-dot off';sTxt.textContent='Disconnected';setTimeout(connectWS,2000);};
ws.onerror=()=>{ws.close();};
ws.onmessage=e=>{
try{
const d=JSON.parse(e.data);
ax=d.ax||0;ay=d.ay||0;az=d.az||0;
gx=d.gx||0;gy=d.gy||0;gz=d.gz||0;
quat={w:d.qw||1,x:d.qx||0,y:d.qy||0,z:d.qz||0};
rpm=d.rpm||0;
pushHist(gyroH,gx,gy,gz);
pushHist(accelH,ax,ay,az);
if(recording){recData.push({t:d.t||0,ax,ay,az,gx,gy,gz,qw:quat.w,qx:quat.x,qy:quat.y,qz:quat.z,rpm});recCnt.textContent=recData.length;}
// Impact flash
if(d.imp===1){
    impactFlash.classList.add('active');
    setTimeout(()=>impactFlash.classList.remove('active'),150);
}
// Live spin type from firmware
if(d.spin){spinType.textContent=d.spin;spinType.className='spin-type spin-'+d.spin;}
// Shot event from firmware
if(d.event==='shot'){
    shots.push(d);
    if(shots.length===1)firstShotTime=d.t;
    updateTimeline();
}
}catch(err){}
};
}

function pushHist(h,x,y,z){h.x.push(x);h.y.push(y);h.z.push(z);if(h.x.length>HIST_LEN){h.x.shift();h.y.shift();h.z.shift();}}

function drawBall(){
const w=300,h=300,cx=w/2,cy=h/2,R=120;
ballCtx.clearRect(0,0,w,h);
const grad=ballCtx.createRadialGradient(cx-30,cy-30,20,cx,cy,R);
grad.addColorStop(0,'#D8E840');grad.addColorStop(1,'#C8D820');
ballCtx.beginPath();ballCtx.arc(cx,cy,R,0,2*Math.PI);ballCtx.fillStyle=grad;ballCtx.fill();

let prev=null,prevR=null;
for(let i=0;i<=SEAM_N;i++){
const idx=i%SEAM_N;
const rp=qrot(quat,seamPts[idx]);
const sx=cx+rp.x*R,sy=cy-rp.y*R;
if(prev!==null){
if(rp.z>0){ballCtx.beginPath();ballCtx.moveTo(prev.sx,prev.sy);ballCtx.lineTo(sx,sy);ballCtx.strokeStyle='#fff';ballCtx.lineWidth=2.2;ballCtx.stroke();}
else if(rp.z>-0.15){ballCtx.beginPath();ballCtx.moveTo(prev.sx,prev.sy);ballCtx.lineTo(sx,sy);ballCtx.strokeStyle='rgba(180,180,180,0.35)';ballCtx.lineWidth=1.5;ballCtx.stroke();}
}
prev={sx,sy};prevR=rp;
}

ballCtx.beginPath();ballCtx.arc(cx,cy,R,0,2*Math.PI);ballCtx.strokeStyle='rgba(0,0,0,0.25)';ballCtx.lineWidth=2;ballCtx.stroke();
}

function drawChart(ctx,cv,hist,colors){
const w=cv.width/devicePixelRatio,h=cv.height/devicePixelRatio;
ctx.clearRect(0,0,w*devicePixelRatio,h*devicePixelRatio);
ctx.save();
const pad={l:45,r:10,t:5,b:18};
const cw=w-pad.l-pad.r,ch=h-pad.t-pad.b;

let mn=Infinity,mx=-Infinity;
['x','y','z'].forEach(k=>{hist[k].forEach(v=>{if(v<mn)mn=v;if(v>mx)mx=v;});});
if(mn===Infinity){mn=-1;mx=1;}
const range=mx-mn||1;
mn-=range*0.1;mx+=range*0.1;
const yr=mx-mn;

ctx.strokeStyle='rgba(255,255,255,0.07)';ctx.lineWidth=1;
for(let i=0;i<=4;i++){
const y=pad.t+ch*(i/4);
ctx.beginPath();ctx.moveTo(pad.l,y);ctx.lineTo(pad.l+cw,y);ctx.stroke();
const val=mx-yr*(i/4);
ctx.fillStyle='#888';ctx.font='10px monospace';ctx.textAlign='right';
ctx.fillText(val.toFixed(1),pad.l-4,y+3);
}

['x','y','z'].forEach((k,ki)=>{
const arr=hist[k];if(!arr.length)return;
ctx.beginPath();ctx.strokeStyle=colors[ki];ctx.lineWidth=1.5;
for(let i=0;i<arr.length;i++){
const px=pad.l+(i/(HIST_LEN-1))*cw;
const py=pad.t+ch*(1-(arr[i]-mn)/yr);
i===0?ctx.moveTo(px,py):ctx.lineTo(px,py);
}
ctx.stroke();
});
ctx.restore();
}

function updateData(){
dataEls.ax.textContent=ax.toFixed(3);dataEls.ay.textContent=ay.toFixed(3);dataEls.az.textContent=az.toFixed(3);
dataEls.gx.textContent=gx.toFixed(1);dataEls.gy.textContent=gy.toFixed(1);dataEls.gz.textContent=gz.toFixed(1);
rpmVal.textContent=Math.round(rpm);
if(!connected)spinType.textContent=classifySpin(gx,gy,gz);
}

let lastFrame=0;
function loop(ts){
requestAnimationFrame(loop);
if(ts-lastFrame<32)return;
lastFrame=ts;
drawBall();
drawChart(gyroCtx,gyroCv,gyroH,['#FF4444','#44FF44','#4444FF']);
drawChart(accelCtx,accelCv,accelH,['#FF4444','#44FF44','#4444FF']);
updateData();
}

function toggleRec(){
recording=!recording;
const btn=document.getElementById('btnRec');
if(recording){btn.textContent='Stop';btn.classList.add('active');recData=[];recCnt.textContent='0';recInfo.style.display='flex';}
else{btn.textContent='Record';btn.classList.remove('active');recInfo.style.display='none';}
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
const a=document.createElement('a');a.href=URL.createObjectURL(blob);a.download='imu_data.csv';a.click();URL.revokeObjectURL(a.href);
}

function resetBall(){if(ws&&ws.readyState===1)ws.send('reset');}

function updateTimeline(){
    const bar=document.getElementById('timelineBar');
    const total=document.getElementById('statTotal');
    const avgRPM=document.getElementById('statAvgRPM');
    const maxRPM=document.getElementById('statMaxRPM');
    const maxG=document.getElementById('statMaxG');

    if(!shots.length){bar.innerHTML='';total.textContent='0';avgRPM.textContent='0';maxRPM.textContent='0';maxG.textContent='0';return;}

    // Stats
    let sumRPM=0,mxRPM=0,mxG=0;
    shots.forEach(s=>{sumRPM+=s.rpm;if(s.rpm>mxRPM)mxRPM=s.rpm;if(s.peakG>mxG)mxG=s.peakG;});
    total.textContent=shots.length;
    avgRPM.textContent=Math.round(sumRPM/shots.length);
    maxRPM.textContent=Math.round(mxRPM);
    maxG.textContent=mxG.toFixed(1);

    // Timeline markers
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
        mk.innerHTML='<div class="shot-dot dot-'+s.type+'"></div><div class="shot-rpm">'+Math.round(s.rpm)+'</div><div class="shot-type spin-'+s.type+'">'+s.type+'</div>';
        bar.appendChild(mk);
    });
}

function showShotDetail(idx){
    const s=shots[idx];
    const detail=document.getElementById('shotDetail');
    const title=document.getElementById('shotDetailTitle');
    const grid=document.getElementById('shotDetailGrid');
    detail.classList.add('show');
    title.textContent='Shot #'+(idx+1)+' - '+s.type;
    title.className='';title.style.color=getSpinColor(s.type);
    grid.innerHTML=
        '<div class="shot-detail-item"><span class="lbl">RPM</span><span class="val">'+Math.round(s.rpm)+'</span></div>'+
        '<div class="shot-detail-item"><span class="lbl">Peak G</span><span class="val">'+s.peakG.toFixed(1)+'</span></div>'+
        '<div class="shot-detail-item"><span class="lbl">Type</span><span class="val">'+s.type+'</span></div>'+
        '<div class="shot-detail-item"><span class="lbl">Gyro X</span><span class="val">'+s.gx.toFixed(1)+'</span></div>'+
        '<div class="shot-detail-item"><span class="lbl">Gyro Y</span><span class="val">'+s.gy.toFixed(1)+'</span></div>'+
        '<div class="shot-detail-item"><span class="lbl">Gyro Z</span><span class="val">'+s.gz.toFixed(1)+'</span></div>';
}

function getSpinColor(type){
    const c={'TOPSPIN':'#FF6B6B','BACKSPIN':'#4ECDC4','SIDE_L':'#FFE66D','SIDE_R':'#FFE66D','SIDESPIN':'#FFE66D','SLICE':'#A8E6CF','FLAT':'#888','MIXED':'#DDA0DD'};
    return c[type]||'#e0e0e0';
}

function clearShots(){
    shots=[];firstShotTime=0;
    updateTimeline();
    document.getElementById('shotDetail').classList.remove('show');
    if(ws&&ws.readyState===1)ws.send('clear_shots');
}

connectWS();
requestAnimationFrame(loop);
</script>
</body>
</html>
)rawliteral";
