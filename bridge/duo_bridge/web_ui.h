// Weboberflaeche als eine HTML-Datei, ausgeliefert von web.ino.
// Seitennamen: ../../docs/seiten.md (englische Nummern, +100 DE, +200 IT).
#pragma once

static const char WEB_UI[] PROGMEM = R"HTML(<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Duo Bridge</title>
<style>
:root{--bg:#0b1220;--karte:#131d31;--linie:#22304d;--text:#e6edf7;--leise:#8b9bb8;--akzent:#4cc9f0;
--kaffee:#f4a261;--dampf:#4cc9f0;--warn:#f94144;--ok:#43aa8b}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:15px/1.4 system-ui,-apple-system,sans-serif}
header{display:flex;flex-wrap:wrap;gap:8px 16px;align-items:center;padding:12px 16px;border-bottom:1px solid var(--linie)}
header h1{font-size:17px;margin:0;font-weight:600}
.lampe{display:inline-flex;align-items:center;gap:6px;color:var(--leise);font-size:13px}
.lampe i{width:9px;height:9px;border-radius:50%;background:#555;display:inline-block}
.lampe.an i{background:var(--ok)}
main{display:grid;gap:14px;padding:14px 16px;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));max-width:1200px}
section{background:var(--karte);border:1px solid var(--linie);border-radius:10px;padding:14px}
h2{font-size:13px;text-transform:uppercase;letter-spacing:.06em;color:var(--leise);margin:0 0 10px;font-weight:600}
.gross{display:flex;gap:18px;flex-wrap:wrap}
.wert{font-size:44px;font-weight:600;font-variant-numeric:tabular-nums;line-height:1}
.wert small{font-size:18px;color:var(--leise);font-weight:400}
.einheit{font-size:13px;color:var(--leise);margin-top:4px}
#seitenname{font-size:20px;font-weight:600}
.alarm{color:var(--warn)}
#banner{display:none;background:#3a1216;border:1px solid var(--warn);color:#ffd6d6;padding:8px 12px;border-radius:8px;margin:12px 16px 0}
canvas{width:100%;height:170px;display:block}
table{width:100%;border-collapse:collapse;font-variant-numeric:tabular-nums;font-size:13px}
td,th{padding:4px 6px;border-bottom:1px solid var(--linie);text-align:left;vertical-align:top}
th{color:var(--leise);font-weight:500}
.form{display:flex;flex-wrap:wrap;gap:6px;align-items:center;margin-bottom:10px}
input,select,button{font:inherit;background:#0e1628;color:var(--text);border:1px solid var(--linie);border-radius:6px;padding:6px 8px}
input{width:90px}
input.breit{flex:1;min-width:160px}
button{background:#1c3355;border-color:#2c4d7d;cursor:pointer}
button:hover{background:#244170}
#log{font:12px/1.5 ui-monospace,monospace;max-height:260px;overflow:auto;white-space:pre-wrap;color:var(--leise)}
#katalog{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:4px;max-height:300px;overflow:auto;font-size:12px}
#katalog button{text-align:left;padding:4px 6px;font-size:12px}
.hinweis{font-size:12px;color:var(--leise);margin:4px 0 0}
</style>
</head>
<body>
<header>
  <h1>Bezzera Duo · Bridge</h1>
  <span class="lampe" id="l_mb"><i></i>Mainboard</span>
  <span class="lampe" id="l_dp"><i></i>Display</span>
  <span class="lampe" id="uhr"></span>
</header>
<div id="banner">Mainboard ist verbunden: Eingriffe unten wirken auf die echte Maschine.</div>
<main>
  <section style="grid-column:1/-1">
    <h2>Display</h2>
    <div id="dsp_wrap" style="max-width:640px;margin:0 auto"><svg id="dsp" viewBox="0 0 320 240" style="width:100%;display:block;border-radius:6px;box-shadow:0 0 0 6px #000,0 6px 30px #0008"></svg></div>
    <p class="hinweis" id="dsp_hinweis">Nachbau aus Fotos. Klick auf Menüpunkte schaltet die Seite wie das Display. Tasten, deren Code noch unbekannt ist, werden nur protokolliert.</p>
  </section>

  <section>
    <h2>Anzeige</h2>
    <div id="seitenname">–</div>
    <div class="einheit" id="seitennr"></div>
    <div class="gross" style="margin-top:14px">
      <div><div class="wert" style="color:var(--kaffee)" id="t_kaffee">–</div><div class="einheit">Kaffeekessel °C</div></div>
      <div><div class="wert" style="color:var(--dampf)" id="t_dampf">–</div><div class="einheit">Servicekessel °C</div></div>
    </div>
  </section>

  <section>
    <h2>Temperaturverlauf</h2>
    <canvas id="kurve" width="600" height="170"></canvas>
    <p class="hinweis">Nur solange diese Seite offen ist. <span style="color:var(--kaffee)">■</span> Kaffee <span style="color:var(--dampf)">■</span> Service</p>
  </section>

  <section>
    <h2>Steuerung</h2>
    <div class="form">Seite <input id="f_seite" type="number" min="0" max="299" value="101"><button onclick="cmd('p '+v('f_seite'))">zeigen</button></div>
    <div class="form">Tastendruck: VP <select id="f_ovp"><option value="0x0000">0x0000</option><option value="0x0001">0x0001</option></select>
      Wert <input id="f_owert" value="5"> ×<input id="f_oanz" value="1" style="width:50px">
      <button onclick="cmd('o '+v('f_ovp')+' '+v('f_owert')+' '+v('f_oanz'))">senden</button></div>
    <div class="form">VP <input id="f_vp" value="0x0050"> Worte <input id="f_w" class="breit" value="10 11 12 13 14 15 16 17 18"><button onclick="cmd('w '+v('f_vp')+' '+v('f_w'))">schreiben</button></div>
    <div class="form">Roh <select id="f_ziel"><option value="d">→ Display</option><option value="m">→ Mainboard</option></select>
      <input id="f_roh" class="breit" value="c6 a5 03 81 03 02"><button onclick="cmd(v('f_ziel')+' '+v('f_roh'))">senden</button></div>
    <p class="hinweis">Tastendruck: Die nächsten n Antworten des Displays auf „VP lesen“ werden überschrieben, das Mainboard sieht den Wert wie einen Druck. Welche Werte welche Taste sind, ist noch nicht vollständig bekannt.</p>
  </section>

  <section>
    <h2>Variablen</h2>
    <table><thead><tr><th>VP</th><th>Worte</th><th>Alter</th></tr></thead><tbody id="vps"></tbody></table>
  </section>

  <section>
    <h2>Seiten</h2>
    <div class="form">Sprache <select id="f_sprache"><option value="100">Deutsch</option><option value="0">Englisch</option><option value="200">Italienisch</option></select></div>
    <div id="katalog"></div>
  </section>

  <section>
    <h2>Ereignisse</h2>
    <div id="log"></div>
  </section>
</main>
<script>
const SEITEN={0:"Standby",1:"Startbildschirm",2:"Alarm: Ladezeit überschritten",3:"Alarm: Tank füllen",4:"Alarm: NTC-Fehler",
5:"Startbildschirm (Var.)",6:"Startbildschirm, ein Zeiger",7:"Einstellungen Kaffee",8:"Einstellungen Tee",9:"Kesselpriorität",10:"Vorbrühen",
11:"Kesselpriorität (Var.)",12:"Einstellungen Kaffee (leer)",13:"Einstellungen Tee (leer)",14:"Startbildschirm mit Menü",15:"Reinigungssperre 10 s",
16:"Einstellungen 1",17:"Einstellungen 2",18:"Einstellungen 3",19:"Einstellungen 4",20:"Technikmenü 1",21:"Technikmenü 2",22:"Technikmenü 3",
23:"PID Kaffeekessel",24:"Technikmenü (Var.) / DE: PID Kaffee Band",25:"Technikmenü (Var.) / DE: PID Services",26:"Technikmenü (Var.) / DE: PID Services Band",27:"Sonden 50K",28:"Sonden 150K",29:"Sonden 400K",
30:"Sonden 1M",31:"Ladezeit 60 s",32:"Ladezeit 90 s",33:"Ladezeit 120 s",34:"Maschine zurücksetzen?",35:"Passwort eingeben",36:"Falsches Passwort",
37:"DGUS-Standardbild",38:"Zifferntastatur",39:"Passwörter",40:"Passwort zurücksetzen?",41:"Passwort zurücksetzen? (Var.)",42:"Neues Passwort gespeichert",
43:"Zifferntastatur",44:"Passwort eingeben",45:"Falsches Passwort",46:"Sprache",47:"Sprache (Var.)",48:"Sprache (Var.)",49:"LED Gehäuse RGB",50:"Licht",
51:"Sensor kalibrieren",52:"Kalibrierung läuft",53:"Kalibrierung OK",54:"Wartung: Bezüge",55:"Wasserfilter: Tage",56:"Alarm: Wartung nötig",57:"leer",
58:"Datum/Uhrzeit",59:"Datum/Uhrzeit",60:"Datum/Uhrzeit",61:"Datum/Uhrzeit",62:"Datum/Uhrzeit",63:"Startbildschirm mit Menü",64:"Spülen: Art wählen",
65:"Spülen: Blindsieb",66:"Spülen: Hebel hoch",67:"Spülen läuft",68:"Spülen: Hebel",69:"Spülen fertig",70:"Auto Ein/Aus",71:"Auto Ein/Aus",72:"Auto Ein/Aus",
73:"Auto Ein/Aus",74:"Auto Ein/Aus",75:"Auto Ein/Aus",76:"Auto Ein/Aus",77:"Alarm: kein Volumensignal",78:"Startbildschirm (Var.)",79:"Technikmenü (Var.)",
80:"PID Gruppe",81:"PID Gruppe Band",82:"Einstellungen (Gruppe)",83:"Startbildschirm, ein Zeiger",84:"Kappe einsetzen",85:"Spülen: Art wählen",
86:"Spülen: Blindsieb",87:"Spülen läuft",88:"Spülen fertig",89:"Alarm: Wasserfilter wechseln",90:"Startbild",91:"Warnung: Spülen abgebrochen",
92:"Passwort: aktuelles",93:"Passwort: neues",94:"Passwort gespeichert",95:"Passwort falsch"};
const ALARM=new Set([2,3,4,56,77,89,91]);
const SPRACHE=["Englisch","Deutsch","Italienisch"];
const $=id=>document.getElementById(id), v=id=>$(id).value.trim();
let seit=0, verlauf=[];

function seitenname(s){if(s<0)return "–";const b=s%100;return SEITEN[b]||("Seite "+b)}
async function cmd(z){await fetch("/api/cmd",{method:"POST",body:z});}
function hex(n){return "0x"+n.toString(16).toUpperCase().padStart(4,"0")}

function katalog(){
  const off=+v("f_sprache"), k=$("katalog"); k.innerHTML="";
  for(const [b,n] of Object.entries(SEITEN)){
    const s=off+ +b, e=document.createElement("button");
    e.textContent=s+" · "+n; if(ALARM.has(+b)) e.className="alarm";
    e.onclick=()=>cmd("p "+s); k.appendChild(e);
  }
}
$("f_sprache").onchange=katalog; katalog();

function zeichne(){
  const c=$("kurve"), g=c.getContext("2d"), W=c.width=c.clientWidth*devicePixelRatio, H=c.height=170*devicePixelRatio;
  g.clearRect(0,0,W,H); if(verlauf.length<2) return;
  const alle=verlauf.flatMap(p=>[p.k,p.d]).filter(x=>x!=null);
  let lo=Math.min(...alle)-2, hi=Math.max(...alle)+2; if(hi-lo<10){hi=lo+10}
  const t0=verlauf[0].t, t1=verlauf[verlauf.length-1].t||t0+1;
  const X=t=>(t-t0)/(t1-t0||1)*(W-40*devicePixelRatio)+34*devicePixelRatio, Y=y=>H-(y-lo)/(hi-lo)*(H-16)-8;
  g.fillStyle="#8b9bb8"; g.font=(11*devicePixelRatio)+"px system-ui";
  for(let i=0;i<=4;i++){const y=lo+(hi-lo)*i/4; g.fillText(Math.round(y),2,Y(y)+4); g.strokeStyle="#22304d"; g.beginPath(); g.moveTo(34*devicePixelRatio,Y(y)); g.lineTo(W,Y(y)); g.stroke()}
  for(const [key,col] of [["k","#f4a261"],["d","#4cc9f0"]]){
    g.strokeStyle=col; g.lineWidth=2*devicePixelRatio; g.beginPath(); let an=false;
    for(const p of verlauf){if(p[key]==null)continue; an?g.lineTo(X(p.t),Y(p[key])):g.moveTo(X(p.t),Y(p[key])); an=true}
    g.stroke();
  }
}

async function hole(){
  try{
    const r=await fetch("/api/status?seit="+seit), j=await r.json();
    const mb=j.mainboard.still_ms>=0&&j.mainboard.still_ms<2000, dp=j.display.still_ms>=0&&j.display.still_ms<2000;
    $("l_mb").classList.toggle("an",mb); $("l_dp").classList.toggle("an",dp); $("banner").style.display=mb?"block":"none";
    $("uhr").textContent=j.rtc?("Display-Uhr "+j.rtc):"";
    $("seitenname").textContent=seitenname(j.seite);
    $("seitenname").className=ALARM.has(j.seite%100)?"alarm":"";
    $("seitennr").textContent=j.seite>=0?("Seite "+j.seite+" · "+SPRACHE[Math.floor(j.seite/100)]):"";
    const v50=j.vps.find(x=>x.vp==0x50);
    const k=v50?v50.w[3]:null, d=v50?v50.w[4]:null;
    $("t_kaffee").textContent=k??"–"; $("t_dampf").textContent=d??"–";
    if(v50){verlauf.push({t:Date.now()/1000,k,d}); if(verlauf.length>1800) verlauf.shift(); zeichne()}
    $("vps").innerHTML=j.vps.sort((a,b)=>a.vp-b.vp).map(x=>`<tr><td>${hex(x.vp)}</td><td>${x.w.join(" ")}</td><td>${(x.alter_ms/1000).toFixed(1)} s</td></tr>`).join("");
    if(j.ereignisse.length){const l=$("log"); l.textContent+=j.ereignisse.join("\n")+"\n"; l.scrollTop=l.scrollHeight}
    seit=j.ereignis_nr;
    zeigeDisplay(j,k,d);
  }catch(e){$("l_mb").classList.remove("an");$("l_dp").classList.remove("an")}
}
setInterval(hole,700); hole(); addEventListener("resize",zeichne);
</script>

<script>
// ─── Display-Nachbau (320×240 wie das DMT32240) ────────────────────────────
const FARBE={bg1:"#0c1424",bg2:"#05080f",rahmen:"#cfeeff",cyan:"#46d2ff",orange:"#ff9d45",text:"#f2f8ff",leise:"#7f93b3"};
const DE=1, sprachIndex=s=>Math.min(2,Math.floor(Math.max(0,s)/100));
const T={ // [EN, DE, IT]
 start:["press to start","Für Start drücken","premere per accendere"],
 alarm:["Alarm","Alarm","Allarme"],
 2:["Loading time out","Timeout Beladen","Timeout carico"],
 3:["Please fill water tank","Bitte Tank füllen","Riempire serbatoio"],
 4:["NTC failure","Fehler Sonde NTC","Errore sonda NTC"],
 56:["Necessary maintenance","Wartung erforderlich","Necessaria manutenzione"],
 77:["No volumetric signal","Kein volumetrisches Signal","Nessun segnale volumetrico"],
 89:["Change water filter","Wasserfilter wechseln","Sostituire filtro acqua"],
 91:["Washing process interrupted","Unterbrochener Waschvorgang","Processo di lavaggio interrotto"],
 einst:["Settings","Einstellungen","Impostazioni"], tech:["Technician menu","Technisches Menü","Menu tecnico"],
 kaffee:["Coffee settings","Einstellungen Kaffee","Impostazioni caffè"], tee:["Tea settings","Tee Einstellungen","Impostazioni The"],
 temp:["Temperature","Temperatur","Temperatura"], kessel:["BOILER","KESSEL","CALDAIA"], prio:["PRIORITY","PRIORITÄT","PRIORITA'"],
 gruppe:["GROUP","GRUPPE","GRUPPO"], vorb:["WETTING","VORBRÜHEN","PREINFUS."],
};
const tx=(k,s)=>(T[k]||[k,k,k])[sprachIndex(s)];
// Menülisten: [Text EN, DE, IT, rechts, Zielseite (englisch) oder null]
const LISTEN={
 16:[["Language","Sprache","Lingua","ENG",46],["Units","Einheiten","Unità","°C | °F",null],["Led body RGB","Led Körper RGB","Led carrozzeria RGB","OFF",49],["Lights","Lichter","Luci","",50]],
 17:[["Lights","Lichter","Luci","OFF",50],["Sensor calibration","Sensor","Sensore","",51],["Maintenance","Wartung","Manutenzione","",54],["Water filter","Wasser Filter","Filtro acqua","",55]],
 18:[["Water filter","Wasser Filter","Filtro acqua","",55],["Water source","Wasser Eingang","Ingresso acqua","",null],["Date & time","Datum und Uhrzeit","Data e ora","",58],["Auto on/off","Auto ON/OFF","Auto on off","",70]],
 19:[["Auto on/off","Auto ON/OFF","Auto on off","OFF",70],["Password","Passwort","Password","OFF",39]],
 20:[["Machine layout","Layout Gerät","Layout macchina","E61 | BZ",null],["PID group","PID-Gruppe","PID-Gruppo","",80],["PID coffee boiler","PID-Kessel Kaffee","PID-Caldaia caffè","",23],["PID service boiler","PID-Kessel Services","PID-Caldaia servizi","",25]],
 21:[["PID service boiler","PID-Kessel Services","PID-Caldaia servizi","",25],["Level probes","Füllstandsonden","Sonde di livello","",27],["Password","Passwort","Password","",39],["Loading time out","Timeout Beladen","Time out di carico","",31]],
 22:[["Loading time out","Timeout Beladen","Time out di carico","",31],["Total brewings","Abgaben gesamt","Erogazioni totali","",null],["Reset","Reset","Reset","",34]],
};
const LISTE_TITEL={16:"einst",17:"einst",18:"einst",19:"einst",20:"tech",21:"tech",22:"tech"};
const ALARM_ICON={2:"warn",3:"tank",4:"warn",56:"wrench",77:"warn",89:"sun",91:"warn"};

let dspSeite=-1, dspDaten={};
function lang(s){return Math.floor(Math.max(0,s)/100)*100}
function geh(s){cmd("p "+s)}
function taste(name){const e=$("log"); e.textContent+=`Taste „${name}“ auf Seite ${dspSeite}: Code noch unbekannt, nichts gesendet\n`; e.scrollTop=e.scrollHeight}

const defs=`<defs>
<linearGradient id="gbg" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="${FARBE.bg1}"/><stop offset="1" stop-color="${FARBE.bg2}"/></linearGradient>
<linearGradient id="gzeile" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#2a4b73"/><stop offset=".5" stop-color="#132a47"/><stop offset="1" stop-color="#0b1a2e"/></linearGradient>
<linearGradient id="gknopf" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#5b6f8c"/><stop offset="1" stop-color="#26344a"/></linearGradient>
<radialGradient id="gglow"><stop offset="0" stop-color="#bff0ff" stop-opacity=".9"/><stop offset=".45" stop-color="#46d2ff" stop-opacity=".35"/><stop offset="1" stop-color="#46d2ff" stop-opacity="0"/></radialGradient>
<filter id="glow" x="-20%" y="-20%" width="140%" height="140%"><feGaussianBlur stdDeviation="1.6" result="b"/><feMerge><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge></filter>
</defs>`;
const txt=(x,y,t,o={})=>`<text x="${x}" y="${y}" fill="${o.f||FARBE.text}" font-size="${o.s||12}" font-weight="${o.w||700}" text-anchor="${o.a||"start"}" font-family="Arial Rounded MT Bold,Arial,sans-serif" ${o.glow?'filter="url(#glow)"':""}>${t}</text>`;
const klick=(x,y,w,h,js)=>`<rect x="${x}" y="${y}" width="${w}" height="${h}" fill="transparent" style="cursor:pointer" onclick="${js}"/>`;

function rahmen(titel){
  return `<rect width="320" height="240" fill="url(#gbg)"/>
  <path d="M8 26 H312 V232 H8 Z" fill="none" stroke="${FARBE.rahmen}" stroke-width="3" rx="4" filter="url(#glow)"/>
  <rect x="8" y="8" width="304" height="18" fill="#1d3656"/>
  ${txt(14,21,titel,{s:13,glow:1})}`;
}
function okEcke(js){
  return `<path d="M222 232 L238 206 H312 V232 Z" fill="url(#gknopf)" stroke="${FARBE.rahmen}" stroke-width="2"/>
  ${txt(276,228,"OK",{s:22,a:"middle",glow:1})}${klick(222,206,90,26,js)}`;
}
function knopf(x,y,w,h,t,js,s=14){return `<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="3" fill="url(#gknopf)" stroke="#9fb6d6" stroke-width="1"/>${txt(x+w/2,y+h/2+s*.36,t,{s,a:"middle"})}${klick(x,y,w,h,js)}`}
function uhr(r,x=12,y=222){
  if(!r) return "";
  const hm=r.slice(11,16), dat=r.slice(8,10)+"/"+r.slice(5,7)+"/"+r.slice(0,4);
  return txt(x,y-10,hm,{s:22,f:"#dff6ff",glow:1})+txt(x,y+4,dat,{s:9,f:"#bfe9ff"});
}
function logo(x,y,sk=1){ // stilisierte Schlange + Schriftzug, wie auf dem Standby
  return `<g transform="translate(${x},${y}) scale(${sk})" fill="none" stroke="${FARBE.orange}" stroke-width="3" stroke-linecap="round" filter="url(#glow)">
  <path d="M-12 -34 q8 -6 16 0 q8 6 16 0 M-14 -22 q14 -8 28 0 q-14 8 -28 0 M-12 -8 q12 -8 24 0 q-12 8 -24 0 M-8 6 q8 -6 16 0 q-8 6 -16 0 M-4 16 q4 6 0 12"/>
  <circle cx="-14" cy="-36" r="2.5" fill="${FARBE.orange}"/></g>`;
}
function icon(name,x,y){
  const o=`fill="none" stroke="${FARBE.orange}" stroke-width="3" stroke-linejoin="round" stroke-linecap="round" filter="url(#glow)"`;
  if(name==="tank") return `<g transform="translate(${x},${y})" ${o}><path d="M-16 -14 V10 H16 V-14 M-16 -6 H-8 M8 -6 H16"/><path d="M0 -8 q-6 8 0 12 q6 -4 0 -12z" fill="${FARBE.orange}"/></g>`;
  if(name==="wrench") return `<g transform="translate(${x},${y}) rotate(-45)" ${o}><path d="M-3 -14 V10 M-8 -18 q8 -8 16 0 M-5 10 q5 6 10 0"/></g>`;
  if(name==="sun") return `<g transform="translate(${x},${y})" ${o}><circle r="6"/>${[0,45,90,135,180,225,270,315].map(a=>`<path d="M0 -10 V-15" transform="rotate(${a})"/>`).join("")}</g>`;
  return `<g transform="translate(${x},${y})" ${o}><path d="M0 -16 L17 12 H-17 Z"/><path d="M0 -5 V3"/><circle cy="7.5" r="1" fill="${FARBE.orange}"/></g>`;
}

function seiteStandby(s,j){
  return `<rect width="320" height="240" fill="url(#gbg)"/>${logo(160,92,1.2)}
  ${txt(160,128,"BEZZERA",{s:15,a:"middle",glow:1})}${txt(160,140,"Duo MN",{s:9,a:"middle"})}
  ${txt(160,166,tx("start",s),{s:12,a:"middle",glow:1})}${klick(60,40,200,140,"taste('Start')")}
  <rect x="262" y="96" width="34" height="30" fill="#33465f" stroke="${FARBE.rahmen}" stroke-width="1.5"/>
  <path d="M270 119 l12 -12 m-2 -4 a6 6 0 1 1 6 6" stroke="${FARBE.cyan}" stroke-width="3" fill="none" stroke-linecap="round"/>
  ${klick(262,96,34,30,`geh(${lang(s)+14})`)}${uhr(j.rtc)}`;
}
function seiteStart(s,j,k,d){
  const cx=160, cy=126, r=78;
  let skala="";
  // Skalen wie im Original: links 100 (oben) bis 10 (unten), rechts 2.5 bis 0.5 bar
  const links=[100,90,80,70,60,50,40,30,20,10], rechts=["2.5","2","1.5","1","0.5"];
  const pos=(grad,rr)=>[cx+Math.cos(grad*Math.PI/180)*rr, cy+Math.sin(grad*Math.PI/180)*rr+3];
  links.forEach((v,i)=>{const [x,y]=pos(255-i*(150/9),r+17); skala+=txt(x,y,v,{s:8,a:"middle",w:400,f:"#e6f6ff"})});
  rechts.forEach((v,i)=>{const [x,y]=pos(285+i*(150/5),r+17); skala+=txt(x,y,v,{s:8,a:"middle",w:400,f:"#e6f6ff"})});
  const bogen=(a0,a1)=>{const p=(a)=>[cx+Math.cos(a*Math.PI/180)*r,cy+Math.sin(a*Math.PI/180)*r];const [x0,y0]=p(a0),[x1,y1]=p(a1);return `M${x0} ${y0} A${r} ${r} 0 0 1 ${x1} ${y1}`};
  return `<rect width="320" height="240" fill="url(#gbg)"/>
  <circle cx="${cx}" cy="${cy}" r="${r-8}" fill="url(#gglow)"/>
  <path d="${bogen(95,265)}" stroke="#e8f8ff" stroke-width="13" fill="none" filter="url(#glow)"/>
  <path d="${bogen(275,445)}" stroke="#e8f8ff" stroke-width="13" fill="none" filter="url(#glow)"/>
  ${skala}${txt(cx,cy+r+14,"0 bar",{s:8,a:"middle",w:400})}
  <g stroke="#dff6ff" stroke-width="2" fill="none"><path d="M${cx-30} ${cy-26} h16 v6 q-8 8 -16 0 z"/><path d="M${cx+18} ${cy-28} q-6 6 0 12 m6 -12 q-6 6 0 12"/></g>
  ${txt(cx-22,cy+10,k??"–",{s:26,a:"middle",glow:1})}${txt(cx+26,cy+10,d??"–",{s:26,a:"middle",glow:1})}
  <g fill="${FARBE.cyan}"><rect x="10" y="10" width="7" height="7" rx="2"/><rect x="10" y="20" width="7" height="7" rx="2"/><rect x="10" y="30" width="7" height="7" rx="2"/></g>
  ${klick(6,6,16,36,`geh(${lang(s)+14})`)}${logo(40,30,.45)}${txt(52,20,"BEZZERA",{s:12})}${txt(56,31,"Duo MN",{s:8})}
  <path d="M268 88 a40 40 0 0 1 0 76" stroke="${FARBE.cyan}" stroke-width="6" fill="none" opacity=".7"/>
  ${klick(cx-60,cy-60,120,120,"taste('Zeiger')")}${uhr(j.rtc)}`;
}
function seiteAlarm(s,j){
  const b=s%100;
  return rahmen(tx("alarm",s))+icon(ALARM_ICON[b],160,90)+txt(160,140,tx(b,s),{s:13,a:"middle",glow:1})+
    `<path d="M16 180 H304" stroke="${FARBE.cyan}" stroke-width="2" opacity=".6"/>`+
    knopf(238,190,68,34,"OK","taste('OK')",20);
}
function seiteListe(s){
  const b=s%100, L=LISTEN[b], i=sprachIndex(s)+0;
  let o=rahmen(tx(LISTE_TITEL[b],s));
  L.forEach((z,n)=>{const y=32+n*48;
    o+=`<rect x="14" y="${y}" width="248" height="40" rx="6" fill="url(#gzeile)" stroke="#6fb8e6" stroke-width="1.2"/>
    <circle cx="32" cy="${y+20}" r="9" fill="#bfe9ff" opacity=".85"/>`+txt(50,y+25,z[i],{s:13,glow:1})+txt(252,y+25,z[3],{s:11,a:"end"})+
    klick(14,y,248,40,z[4]!=null?`geh(${lang(s)+z[4]})`:`taste('${z[1]}')`)});
  const erste=[16,20].includes(b), letzte=[19,22].includes(b);
  o+=knopf(270,32,38,40,"▲",erste?"":`geh(${s-1})`,16)+knopf(270,130,38,40,"▼",letzte?"":`geh(${s+1})`,16);
  return o+okEcke(`geh(${lang(s)+(b<20?14:0)})`);
}
function seiteTemperatur(s,j){
  const b=s%100, v50=j.vps.find(x=>x.vp==0x50);
  return rahmen(tx(b==7||b==12?"kaffee":"tee",s))+
    txt(18,48,tx("kessel",s),{s:10})+txt(66,48,"OFF",{s:13,f:FARBE.cyan})+txt(130,48,tx("prio",s),{s:10})+txt(196,48,"Kaffee",{s:12,f:FARBE.cyan})+
    `<path d="M16 60 H304" stroke="${FARBE.cyan}" stroke-width="2" opacity=".6"/>`+txt(18,82,tx("temp",s),{s:12,f:FARBE.cyan})+
    (b==7||b==8?txt(110,132,"0",{s:40,a:"middle",glow:1}):"")+
    knopf(206,104,40,38,"–","taste('Temperatur –')",22)+knopf(252,104,40,38,"+","taste('Temperatur +')",22)+
    (b==7||b==12?txt(18,226,tx("gruppe",s)+"  OFF",{s:10})+txt(120,226,tx("vorb",s),{s:10}):"")+okEcke("taste('OK')");
}
function seiteSonst(s){
  return rahmen(seitenname(s))+txt(160,120,"Seite "+s,{s:14,a:"middle",f:FARBE.leise})+txt(160,140,"noch nicht nachgebaut",{s:10,a:"middle",w:400,f:FARBE.leise});
}
function zeigeDisplay(j,k,d){
  const s=j.seite; dspSeite=s;
  if(s<0){$("dsp").innerHTML=defs+`<rect width="320" height="240" fill="#000"/>`+txt(160,124,"keine Seite bekannt",{s:12,a:"middle",f:FARBE.leise});return}
  const b=s%100; let inhalt;
  if(b==0) inhalt=seiteStandby(s,j);
  else if([1,5,6,78,83].includes(b)) inhalt=seiteStart(s,j,k,d);
  else if(ALARM_ICON[b]) inhalt=seiteAlarm(s,j);
  else if(LISTEN[b]) inhalt=seiteListe(s);
  else if([7,8,12,13].includes(b)) inhalt=seiteTemperatur(s,j);
  else if(b==90) inhalt=`<rect width="320" height="240" fill="url(#gbg)"/>${logo(160,100,1.2)}${txt(160,140,"BEZZERA",{s:15,a:"middle",glow:1})}${txt(40,226,"TFT 2.0",{s:14,f:FARBE.cyan})}${txt(200,226,"FW: "+(((j.vps.find(x=>x.vp==0x63)||{w:[0]}).w[0])/10).toFixed(1),{s:14,f:FARBE.orange})}`;
  else inhalt=seiteSonst(s);
  $("dsp").innerHTML=defs+inhalt;
}
</script>
</body>
</html>)HTML";
