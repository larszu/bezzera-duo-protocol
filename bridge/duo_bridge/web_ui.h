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
    <div class="form" style="margin-top:8px"><label><input type="checkbox" id="f_flaechen" style="width:auto"> Tastenflächen zeigen</label></div>
    <p class="hinweis" id="dsp_hinweis">Nachbau aus Fotos. Ein Klick wirkt wie eine Berührung an dieser Stelle: Die Taste aus der Touch-Konfiguration des Displays (1070 Tasten) schreibt ihren Wert und wechselt die Seite, genau wie das Display selbst. Ist das Mainboard verbunden, liest es den Wert beim nächsten Abfragen.</p>
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
    <p class="hinweis">Tastendruck: Die nächsten n Antworten des Displays auf „VP lesen“ werden überschrieben, das Mainboard sieht den Wert wie einen Druck. Welche Taste welchen Wert schickt, steht in der Tastentabelle aus dem Display-Flash (docs/tasten.json); ein Klick auf das Display oben nutzt sie direkt.</p>
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
<script src="/tasten.js"></script>
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
// Farben nach den Bildschirmfotos im Bezzera-Handbuch (Matrix/Duo, 320x240):
// schwarzer Grund, tuerkise Linien und Schrift, graue Knoepfe.
const FARBE={bg1:"#000",bg2:"#000",rahmen:"#4fb8bf",cyan:"#5cc8cf",hell:"#9fe6ea",orange:"#ff9d45",rot:"#e3161b",text:"#f2f6f7",leise:"#8a9699"};
const DE=1, sprachIndex=s=>Math.min(2,Math.floor(Math.max(0,s)/100));
const T={ // [EN, DE, IT]
 start:["press to start","Für Start drücken","premere per accendere"],
 alarm:["Alarm","Alarm","Allarme"],
 2:["Loading time out\nrestart loading","Timeout Beladen\nNeustart","Timeout carico\nSpegnere e riaccendere"],
 3:["Please fill water tank","Bitte Tank füllen","Riempire serbatoio"],
 4:["NTC failure","Fehler Sonde NTC","Errore sonda NTC"],
 56:["Necessary maintenance","Wartung erforderlich","Necessaria manutenzione"],
 77:["No volumetric signal!\nDosage OFF","Kein volumetrisches Signal\nVolumetrische Dosierung OFF","Nessun segnale volumetrico\nDosaggio OFF"],
 89:["Change water filter","Wasserfilter wechseln","Sostituire filtro acqua"],
 91:["Attention!\nWashing process interrupted","Achtung!\nUnterbrochener Waschvorgang","Attenzione!\nProcesso di lavaggio interrotto"],
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
<linearGradient id="gbg" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#000"/><stop offset="1" stop-color="#000"/></linearGradient>
<linearGradient id="gtitel" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#3f8c91"/><stop offset=".55" stop-color="#16393c"/><stop offset="1" stop-color="#000"/></linearGradient>
<linearGradient id="gzeile" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#2b4447"/><stop offset=".55" stop-color="#101a1b"/><stop offset="1" stop-color="#000"/></linearGradient>
<linearGradient id="ghell" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#cdf3f5"/><stop offset=".45" stop-color="#4c9da2"/><stop offset="1" stop-color="#000"/></linearGradient>
<linearGradient id="gknopf" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#5e656a"/><stop offset=".5" stop-color="#2d3236"/><stop offset="1" stop-color="#1a1d20"/></linearGradient>
<linearGradient id="gok" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#6b7176"/><stop offset="1" stop-color="#23272b"/></linearGradient>
<linearGradient id="gleiste" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#9fe6ea"/><stop offset="1" stop-color="#1b4a4e"/></linearGradient>
<linearGradient id="gtrenn" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#5cc8cf" stop-opacity="0"/><stop offset=".5" stop-color="#bff0f2"/><stop offset="1" stop-color="#5cc8cf" stop-opacity="0"/></linearGradient>
<linearGradient id="gheiz" x1="0" y1="1" x2="0" y2="0"><stop offset="0" stop-color="#e3161b"/><stop offset=".5" stop-color="#ff9d2a"/><stop offset="1" stop-color="#ff9d2a" stop-opacity="0"/></linearGradient>
<radialGradient id="gscheibe"><stop offset="0" stop-color="#2f5b5f"/><stop offset=".7" stop-color="#0c1718"/><stop offset="1" stop-color="#000"/></radialGradient>
<radialGradient id="gglow"><stop offset="0" stop-color="#2f5b5f"/><stop offset="1" stop-color="#000"/></radialGradient>
<filter id="glow" x="-20%" y="-20%" width="140%" height="140%"><feGaussianBlur stdDeviation=".6" result="b"/><feMerge><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge></filter>
</defs>`;
const txt=(x,y,t,o={})=>`<text x="${x}" y="${y}" fill="${o.f||FARBE.text}" font-size="${o.s||12}" font-weight="${o.w||400}" text-anchor="${o.a||"start"}" font-family="Helvetica Neue,Helvetica,Arial,sans-serif" ${o.glow?'filter="url(#glow)"':""}>${t}</text>`;
const klick=()=>"";  // Klicks wertet die Tastentabelle aus (beruehre())

function rahmen(titel){
  return `<rect width="320" height="240" fill="url(#gbg)"/>
  <path d="M8 26 H312 V232 H8 Z" fill="none" stroke="${FARBE.rahmen}" stroke-width="3" rx="4" filter="url(#glow)"/>
  <rect x="8" y="8" width="304" height="18" fill="#1d3656"/>
  ${txt(14,21,titel,{s:13,glow:1})}`;
}
function okEcke(js=""){
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

// Wasserstand rechts neben dem Zeiger: nach rechts offenes "C" mit Doppellinie,
// "max" oben, "min" unten, zwei Balken in der Mitte (so auf den Fotos, Seite 101).
function wasserstand(){
  const cx=302, cy=131, r=40, bog=(rr,a0,a1)=>{const p=a=>[cx+Math.cos(a*Math.PI/180)*rr,cy+Math.sin(a*Math.PI/180)*rr];
    const [x0,y0]=p(a0),[x1,y1]=p(a1);return `M${x0.toFixed(1)} ${y0.toFixed(1)} A${rr} ${rr} 0 0 0 ${x1.toFixed(1)} ${y1.toFixed(1)}`};
  const strich=(a,t,dy)=>{const x0=cx+Math.cos(a*Math.PI/180)*(r+4), y0=cy+Math.sin(a*Math.PI/180)*(r+4);
    return `<path d="M${x0.toFixed(1)} ${y0.toFixed(1)} l-8 ${dy>0?6:-6}" stroke="#cfeeff" stroke-width="1.5"/>`+
      `<text x="${(x0-6).toFixed(1)}" y="${(y0+dy).toFixed(1)}" fill="#e6f6ff" font-size="7" font-weight="700" font-family="Arial,sans-serif" transform="rotate(${dy>0?35:-35} ${(x0-6).toFixed(1)} ${(y0+dy).toFixed(1)})">${t}</text>`};
  return `<g filter="url(#glow)" fill="none">
    <path d="${bog(r+3,250,110)}" stroke="#8fdcff" stroke-width="2"/>
    <path d="${bog(r-3,250,110)}" stroke="#8fdcff" stroke-width="2"/>
    <path d="${bog(r,250,110)}" stroke="#46d2ff" stroke-width="4" opacity=".45"/>
  </g>
  <path d="M${cx-r+2} ${cy-5} h26 M${cx-r+2} ${cy+4} h26" stroke="#e8f8ff" stroke-width="5" stroke-linecap="round" filter="url(#glow)"/>
  ${strich(245,"max",-4)}${strich(115,"min",10)}`;
}

function seiteStandby(s,j){
  return `<rect width="320" height="240" fill="url(#gbg)"/>${logo(160,92,1.2)}
  ${txt(160,128,"BEZZERA",{s:15,a:"middle",glow:1})}${txt(160,140,"Duo MN",{s:9,a:"middle"})}
  ${txt(160,166,tx("start",s),{s:12,a:"middle",glow:1})}${klick(60,40,200,140,"taste('Start')")}
  <rect x="262" y="96" width="34" height="30" fill="#33465f" stroke="${FARBE.rahmen}" stroke-width="1.5"/>
  <path d="M270 119 l12 -12 m-2 -4 a6 6 0 1 1 6 6" stroke="${FARBE.cyan}" stroke-width="3" fill="none" stroke-linecap="round"/>
  ${klick(262,96,34,30,`geh(${lang(s)+14})`)}${uhr(j.rtc)}`;
}
function seiteStart(s,j,k,d,einZeiger=false){
  const cx=160, cy=126, r=78;
  let skala="";
  // Skalen wie im Original: links 100 (oben) bis 10 (unten), rechts 2.5 bis 0.5 bar
  const links=[100,90,80,70,60,50,40,30,20,10], rechts=["2.5","2","1.5","1","0.5"];
  const pos=(grad,rr)=>[cx+Math.cos(grad*Math.PI/180)*rr, cy+Math.sin(grad*Math.PI/180)*rr+3];
  links.forEach((v,i)=>{const [x,y]=pos(255-i*(150/9),r+17); skala+=txt(x,y,v,{s:8,a:"middle",w:400,f:"#e6f6ff"})});
  if(!einZeiger) rechts.forEach((v,i)=>{const [x,y]=pos(285+i*(150/5),r+17); skala+=txt(x,y,v,{s:8,a:"middle",w:400,f:"#e6f6ff"})});
  const bogen=(a0,a1)=>{const p=(a)=>[cx+Math.cos(a*Math.PI/180)*r,cy+Math.sin(a*Math.PI/180)*r];const [x0,y0]=p(a0),[x1,y1]=p(a1);return `M${x0} ${y0} A${r} ${r} 0 0 1 ${x1} ${y1}`};
  return `<rect width="320" height="240" fill="url(#gbg)"/>
  <circle cx="${cx}" cy="${cy}" r="${r-8}" fill="url(#gglow)"/>
  <path d="${bogen(95,265)}" stroke="#e8f8ff" stroke-width="13" fill="none" filter="url(#glow)"/>
  ${einZeiger?"":`<path d="${bogen(275,445)}" stroke="#e8f8ff" stroke-width="13" fill="none" filter="url(#glow)"/>`}
  ${skala}${txt(cx,cy+r+14,"0 bar",{s:8,a:"middle",w:400})}
  ${einZeiger?`<g stroke="#dff6ff" stroke-width="2" fill="none"><path d="M${cx-8} ${cy-26} h16 v6 q-8 8 -16 0 z"/></g>${txt(cx,cy+10,k??"–",{s:26,a:"middle",glow:1})}`
  :`<g stroke="#dff6ff" stroke-width="2" fill="none"><path d="M${cx-30} ${cy-26} h16 v6 q-8 8 -16 0 z"/><path d="M${cx+18} ${cy-28} q-6 6 0 12 m6 -12 q-6 6 0 12"/></g>
  ${txt(cx-22,cy+10,k??"–",{s:26,a:"middle",glow:1})}${txt(cx+26,cy+10,d??"–",{s:26,a:"middle",glow:1})}`}
  <g fill="${FARBE.cyan}"><rect x="10" y="10" width="7" height="7" rx="2"/><rect x="10" y="20" width="7" height="7" rx="2"/><rect x="10" y="30" width="7" height="7" rx="2"/></g>
  ${klick(6,6,16,36,`geh(${lang(s)+14})`)}${logo(40,30,.45)}${txt(52,20,"BEZZERA",{s:12})}${txt(56,31,"Duo MN",{s:8})}
  ${einZeiger?"":wasserstand()}
  ${uhr(j.rtc)}`;
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

// ─── Baukasten für die übrigen Seiten (deutsche Nummern 100–195) ───────────
// Englische (0–95) und italienische (200–295) Seiten nutzen dieselbe Vorlage
// über seite%100+100; nur Titel, die in T stehen, erscheinen übersetzt.
let dspJ=null, dspFehlt=new Set(), dspGefragt=new Set();
function vpw(vp,i=0){const e=dspJ&&dspJ.vps.find(v=>v.vp==vp); if(!e||e.w.length<=i){dspFehlt.add(vp);return null} return e.w[i]}
function tastenRect(s){return tastenAuf(s).map(t=>({x0:Math.min(t[1],t[3]),y0:Math.min(t[2],t[4]),x1:Math.max(t[1],t[3]),y1:Math.max(t[2],t[4]),t}))}
function kn(r,label,sz=14,aktiv=false){const w=r.x1-r.x0,h=r.y1-r.y0;
  return `<rect x="${r.x0+2}" y="${r.y0+2}" width="${w-4}" height="${h-4}" rx="4" fill="${aktiv?"#bfe9ff":"url(#gknopf)"}" stroke="#9fc4e6" stroke-width="1.2"/>`+
  txt(r.x0+w/2,r.y0+h/2+sz*.36,label,{s:sz,a:"middle",f:aktiv?"#0b1a2e":FARBE.text})}
function zeilenBox(x,y,w,h,aktiv){return `<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="6" fill="${aktiv?"url(#ghell)":"url(#gzeile)"}" stroke="#6fb8e6" stroke-width="1.2"/>`}
function mehrzeilig(x,y,t,o={}){return t.split("\n").map((z,i)=>txt(x,y+i*(o.zh||15),z,{s:12,a:"middle",glow:1,...o})).join("")}
function rahmenOhneTitel(){return `<rect width="320" height="240" fill="url(#gbg)"/><rect x="8" y="8" width="304" height="224" rx="3" fill="none" stroke="${FARBE.rahmen}" stroke-width="3" filter="url(#glow)"/>`}
function fussLinie(y=172){return `<path d="M16 ${y} H304" stroke="${FARBE.cyan}" stroke-width="2" opacity=".6"/>`}
const fmt=(v,div,st)=>v==null?"–":(v/div).toFixed(st);
// Tasten der Seite nach Art sortiert: Plus/Minus-Paare je VP, Tastencodes, Seitenwechsel
function tastenNach(s){const R=tastenRect(s);return {alle:R,
  pm:vp=>R.filter(r=>r.t[6]==2&&r.t[7]==vp).sort((a,b)=>a.t[8]-b.t[8]), // [minus, plus]
  code:(vp,w)=>R.find(r=>r.t[6]==5&&r.t[7]==vp&&(w===undefined||r.t[8]==w)),
  ascii:c=>R.find(r=>r.t[6]==0&&r.t[8]==undefined&&false)}}

// Icons
function hebel(x,y,richtung){ // Siebträgerhebel der Brühgruppe, stilisiert
  const pfeil=richtung>0?`<path d="M${x+28} ${y-30} a40 40 0 0 1 40 30" stroke="#cfeeff" stroke-width="3" fill="none"/><path d="M${x+62} ${y-6} l6 6 l4 -9" stroke="#cfeeff" stroke-width="3" fill="none"/>`
    :`<path d="M${x+68} ${y-30} a40 40 0 0 1 -10 40" stroke="#cfeeff" stroke-width="3" fill="none"/><path d="M${x+52} ${y+6} l6 6 l6 -6" stroke="#cfeeff" stroke-width="3" fill="none"/>`;
  return `<g stroke="#cfeeff" stroke-width="2.5" fill="none" filter="url(#glow)">
  <path d="M${x} ${y+40} V${y-20} h22 v10 h-6 v50 h44 l10 10 H${x}Z"/><circle cx="${x+12}" cy="${y+44}" r="7"/><path d="M${x+12} ${y+44} L${x+50} ${y-12}" stroke-width="4"/>${pfeil}</g>`;
}

const SEITEN_DE={
 106:{typ:"start1"},183:{typ:"start1"},
 109:{typ:"prio"},111:{typ:"prio"},
 110:{typ:"wert",titel:"Vorbrühen",label:"Sekunden Vorbrühen",vp:0x5C,div:10,st:1,suffix:"″"},
 114:{typ:"menue"},163:{typ:"menue"},
 115:{typ:"info",icon:"sperre",text:"Bildschirm für 10 Sekunden gesperrt,\num das Display zu reinigen",rahmen:1},
 123:{typ:"pid",titel:"PID - Kessel Kaffee",vps:[0x76,0x77,0x78]},
 124:{typ:"band",titel:"PID - Kessel Kaffee",vp:0x79},
 125:{typ:"pid",titel:"PID - Kessel Services"},126:{typ:"band",titel:"PID - Kessel Services"},
 180:{typ:"pid",titel:"PID Gruppe"},181:{typ:"band",titel:"PID Gruppe"},
 127:{typ:"raster",titel:"Sondenempfindlichkeit",z:["50K","150K","400K","1M"],hell:0},
 128:{typ:"raster",titel:"Sondenempfindlichkeit",z:["50K","150K","400K","1M"],hell:1},
 129:{typ:"raster",titel:"Sondenempfindlichkeit",z:["50K","150K","400K","1M"],hell:2},
 130:{typ:"raster",titel:"Sondenempfindlichkeit",z:["50K","150K","400K","1M"],hell:3},
 131:{typ:"raster",titel:"Ladezeitlimit",z:["60 Sek","90 Sek","120 Sek"],hell:0},
 132:{typ:"raster",titel:"Ladezeitlimit",z:["60 Sek","90 Sek","120 Sek"],hell:1},
 133:{typ:"raster",titel:"Ladezeitlimit",z:["60 Sek","90 Sek","120 Sek"],hell:2},
 134:{typ:"frage",titel:"Achtung!",text:"Bestätigen,\num die Maschine zurückzusetzen"},
 135:{typ:"frage",text:"Drücken, um Passwort einzugeben"},144:{typ:"frage",text:"Drücken, um Passwort einzugeben"},
 136:{typ:"frage",text:"Falsches Passwort,\nzur Eingabe drücken"},145:{typ:"frage",text:"Falsches Passwort,\nzur Eingabe drücken"},
 140:{typ:"frage",text:"Möchten Sie das aktuelle\nPasswort zurücksetzen?\nOK drücken, um neues\nPasswort einzugeben"},
 141:{typ:"frage",text:"Möchten Sie das aktuelle\nPasswort zurücksetzen?\nOK drücken, um neues\nPasswort einzugeben"},
 142:{typ:"info",text:"Neues Passwort gespeichert",rahmen:1},194:{typ:"info",text:"Neues Passwort gespeichert",rahmen:1},
 192:{typ:"info",text:"Drücken,\num Passwort einzugeben",rahmen:1},
 193:{typ:"info",text:"Drücken,\num das neue Passwort\neinzugeben",rahmen:1},
 195:{typ:"info",text:"Falsches Passwort,\nzur Eingabe drücken",rahmen:1},
 137:{typ:"leer"},190:{typ:"leer"},196:{typ:"leer"},197:{typ:"leer"},198:{typ:"leer"},199:{typ:"leer"},
 138:{typ:"tastatur",esc:1},143:{typ:"tastatur"},
 139:{typ:"liste2",titel:"Passwort",z:["Einstellungen Kennwort","Techniker Kennwort"]},
 146:{typ:"sprache",titel:"Language",hell:1},147:{typ:"sprache",titel:"Lingua",hell:0},148:{typ:"sprache",titel:"Sprache",hell:2},
 149:{typ:"led"},150:{typ:"licht"},
 151:{typ:"frage",titel:"Tarierung Sensor",text:"Bitte den Tank entleeren und\ntrocknen"},
 152:{typ:"info",text:"Tarierung Sensor Tank...",rahmen:1},153:{typ:"info",text:"Tarierung OK",rahmen:1},
 154:{typ:"wert",titel:"Wartung",label:"Anzahl der Bezüge",vp:0x27,div:1,st:0},
 155:{typ:"wert",titel:"Wasser Filter",label:"Anzahl Tagen",vp:0x28,div:1,st:0,fuss:"ZURÜCKSETZEN"},
 164:{typ:"spuelen"},185:{typ:"spuelen"},
 165:{typ:"info",titel:"Waschen",text:"Blindfilter in Filterhalter einsetzen\ndrücken Sie um fortzufahren"},
 166:{typ:"hebel",titel:"Waschen",text:"Heben Sie den\nHebel an Drücken\nSie START",richtung:-1,knoepfe:["ESC","START"]},
 167:{typ:"info",titel:"Waschen",text:"Waschen..."},187:{typ:"info",titel:"Waschen",text:"Waschen..."},
 168:{typ:"hebel",titel:"Waschen",text:"Ziehen Sie nach\nunten und heben\nSie den Hebel.\nZum Waschen",richtung:1},
 169:{typ:"hebel",titel:"Waschen",text:"Waschen abge-\nschlossen. Den\nHebel herunter-\nziehen und den\nBlindfilter heraus-\nnehmen",richtung:1,knoepfe:["OK"]},
 186:{typ:"info",titel:"Waschen",text:"Blindfilter in Filterhalter\neinsetzen"},
 188:{typ:"info",titel:"Waschen",text:"Komplettes Waschen.\nEntfernen Sie den Blindfilter.\nDrücken Sie, um zu beenden."},
 184:{typ:"gruppe"},
};
for(let s=157;s<=162;s++) SEITEN_DE[s]={typ:"datum",hell:s-157};
for(let s=170;s<=176;s++) SEITEN_DE[s]={typ:"woche",tag:s-170};
LISTEN[79]=LISTEN[20];
LISTEN[82]=[["Auto on/off","Auto ON/OFF","Auto on off","OFF",70],["Password","Passwort","Password","OFF",39],["Group temp","Gruppentemperatur","Temperatura gruppo","",84]];
LISTE_TITEL[79]="tech"; LISTE_TITEL[82]="einst";

function seiteBaukasten(s,j,k,d){
  const b=s%100, def=SEITEN_DE[100+b]; if(!def||!def.typ) return null;
  const T_=tastenNach(s), R=T_.alle, titel=t=>rahmen(t);
  switch(def.typ){
  case "leer": return `<rect width="320" height="240" fill="#e9eef6"/>`;
  case "alarm": return rahmen(tx("alarm",s))+icon(def.icon,160,86)+mehrzeilig(160,132,def.text,{s:13})+fussLinie(176)+R.map(r=>kn(r,"OK",18)).join("");
  case "info": return dialog(def.titel||"")+(def.icon=="sperre"?SYM.tuch(160,80,1.8):"")+
      mehrzeilig(160,def.titel?120:118,def.text,{s:12});
  case "frage": { // Meldung mit ESC/OK: rechte Taste OK, linke ESC
    const kn2=R.slice().sort((a,b)=>a.x0-b.x0);
    return dialog(def.titel||"")+mehrzeilig(160,def.titel?96:92,def.text,{s:12})+fussLinie(172)+
      kn2.map((r,i)=>kn(r,kn2.length>1&&i==0?"ESC":"OK",13)).join("");
  }
  case "prio": {
    const v=vpw(0x5A), namen=["Kaffee","Services","Kein"];
    const felder=R.filter(r=>r.t[7]==0x5A).sort((a,b)=>a.x0-b.x0);
    return rahmen("Priorität Kessel")+felder.map((r,i)=>{const cx=(r.x0+r.x1)/2;
      return `<circle cx="${cx}" cy="86" r="14" fill="none" stroke="#dff6ff" stroke-width="3" filter="url(#glow)"/>`+(v==i?`<circle cx="${cx}" cy="86" r="8" fill="#dff6ff" filter="url(#glow)"/>`:"")+
        txt(cx,128,namen[i],{s:12,a:"middle",glow:1})}).join("")+fussLinie(172)+R.filter(r=>r.t[7]==0).map(r=>kn(r,"OK",16)).join("");
  }
  case "wert": {
    const [m,p]=T_.pm(def.vp), v=vpw(def.vp);
    return rahmen(def.titel)+txt(40,80,def.label,{s:12,f:FARBE.cyan})+txt(150,142,fmt(v,def.div,def.st)+(def.suffix||""),{s:34,a:"middle",glow:1})+
      (m?kn(m,"–",22):"")+(p?kn(p,"+",22):"")+(def.fuss?txt(18,224,def.fuss,{s:10}):"")+okEcke();
  }
  case "pid": {
    const zeilen=[["P",1,10],["I",2,100],["D",1,10]];
    const plusMinus=R.filter(r=>r.t[6]==2), vps=[...new Set(plusMinus.map(r=>r.t[7]))].sort((a,b)=>a-b);
    return rahmen(def.titel)+zeilen.map((z,i)=>{const y=30+i*57, vp=vps[i]; const [m,p]=vp!=null?T_.pm(vp):[];
      return zeilenBox(12,y,258,48)+txt(24,y+30,z[0],{s:13})+txt(96,y+33,vp!=null?fmt(vpw(vp),z[2],z[1]):"–",{s:22,a:"middle",glow:1})+(m?kn(m,"–",18):"")+(p?kn(p,"+",18):"")}).join("")+
      txt(24,226,"Band",{s:9,f:FARBE.leise})+kn({x0:274,y0:28,x1:314,y1:104},"▲",14)+kn({x0:274,y0:118,x1:314,y1:192},"▼",14)+okEcke();
  }
  case "band": {
    const pm=R.filter(r=>r.t[6]==2), vp=pm.length?pm[0].t[7]:null, [m,p]=vp!=null?T_.pm(vp):[];
    return rahmen(def.titel)+zeilenBox(12,28,258,48)+txt(24,58,"Band",{s:12})+txt(96,61,vp!=null?fmt(vpw(vp),1,0):"–",{s:22,a:"middle",glow:1})+(m?kn(m,"–",18):"")+(p?kn(p,"+",18):"")+
      kn({x0:274,y0:28,x1:314,y1:104},"▲",14)+kn({x0:274,y0:118,x1:314,y1:192},"▼",14)+okEcke();
  }
  case "raster": {
    const pos=[[8,31,141,70],[159,31,305,70],[8,81,141,120],[159,81,305,120]];
    return rahmen(def.titel)+def.z.map((z,i)=>{const [x0,y0,x1,y1]=pos[i];
      return zeilenBox(x0,y0,x1-x0,y1-y0,i==def.hell)+txt(x0+12,y0+25,z,{s:13,f:i==def.hell?"#0b1a2e":FARBE.text})}).join("")+okEcke();
  }
  case "tastatur": {
    const ziff=R.filter(r=>r.t[6]==0&&r.t[0]==s);
    const lab=r=>{const c=TASTEN.find(t=>t===r.t); return null};
    let o=rahmenOhneTitel()+`<rect x="16" y="10" width="220" height="34" rx="4" fill="#0e1628" stroke="#6fb8e6"/>`+
      `<rect x="242" y="9" width="66" height="34" rx="4" fill="url(#gknopf)" stroke="#9fc4e6"/>`+txt(275,33,"←",{s:20,a:"middle"});
    const rechteck=[[10,59,61,115],[70,58,121,114],[134,58,185,114],[196,58,247,114],[260,58,311,114],[8,124,59,180],[70,124,121,180],[134,124,185,180],[195,124,246,180],[260,124,311,180]];
    rechteck.forEach((q,i)=>{o+=kn({x0:q[0],y0:q[1],x1:q[2],y1:q[3]},String((i+1)%10),24)});
    o+=fussLinie(190)+(def.esc?kn({x0:173,y0:196,x1:241,y1:233},"ESC",13):"")+kn({x0:250,y0:196,x1:315,y1:233},"OK",13);
    return o;
  }
  case "liste2": return rahmen(def.titel)+def.z.map((z,i)=>zeilenBox(9,31+i*49,274,39)+txt(22,56+i*49,z,{s:13,glow:1})).join("")+okEcke();
  case "sprache": {
    const flagge=(x,y,i)=>i==0?`<rect x="${x}" y="${y}" width="8" height="16" fill="#009246"/><rect x="${x+8}" y="${y}" width="8" height="16" fill="#fff"/><rect x="${x+16}" y="${y}" width="8" height="16" fill="#ce2b37"/>`
      :i==1?`<rect x="${x}" y="${y}" width="24" height="16" fill="#012169"/><path d="M${x} ${y} L${x+24} ${y+16} M${x+24} ${y} L${x} ${y+16}" stroke="#fff" stroke-width="3.2"/><path d="M${x} ${y} L${x+24} ${y+16} M${x+24} ${y} L${x} ${y+16}" stroke="#c8102e" stroke-width="1.2"/><path d="M${x+12} ${y} V${y+16} M${x} ${y+8} H${x+24}" stroke="#fff" stroke-width="5"/><path d="M${x+12} ${y} V${y+16} M${x} ${y+8} H${x+24}" stroke="#c8102e" stroke-width="2.6"/>`
      :`<rect x="${x}" y="${y}" width="24" height="5.4" fill="#000"/><rect x="${x}" y="${y+5.3}" width="24" height="5.4" fill="#dd0000"/><rect x="${x}" y="${y+10.6}" width="24" height="5.4" fill="#ffce00"/>`;
    return rahmen(def.titel)+["Italiano","English","Deutsch"].map((z,i)=>{const y=28+i*50;
      return zeilenBox(8,y,196,40,i==def.hell)+flagge(16,y+12,i)+txt(48,y+26,z,{s:15,f:i==def.hell?"#0b1a1c":FARBE.text})}).join("")+okEcke();
  }
  case "led": case "licht": {
    const vpH=def.typ=="led"?0x21:0x29, v=vpw(vpH), [m,p]=T_.pm(vpH), y=def.typ=="led"?108:92;
    let o=rahmen(def.typ=="led"?"Led Körper - RGB":"Lichter");
    if(def.typ=="led"){o+=txt(16,46,"LED Farbe",{s:11})+["R","G","B"].map((c,i)=>{const vp=0x22+i, r=T_.pm(vp)[0]; const an=vpw(vp)==1;
      return r?`<rect x="${r.x0+4}" y="${r.x1?r.y0+4:0}" width="${r.x1-r.x0-8}" height="${r.y1-r.y0-8}" rx="4" fill="${an?"#bfe9ff":"none"}" stroke="#9fc4e6"/>`+txt((r.x0+r.x1)/2,r.y0+30,c,{s:20,a:"middle",f:an?"#0b1a2e":FARBE.text}):""}).join("")+
      `<path d="M16 72 H304" stroke="${FARBE.cyan}" stroke-width="2" opacity=".6"/>`+txt(16,96,"LED Leuchtdichte",{s:11})}
    const anteil=v==null?0.2:Math.max(0,Math.min(1,(v-1)/4));
    o+=`<rect x="16" y="${y+4}" width="176" height="36" rx="6" fill="#5a6c86"/><rect x="16" y="${y+4}" width="${Math.max(24,176*anteil)}" height="36" rx="6" fill="#e8f4ff"/>`+(m?kn(m,"–",20):"")+(p?kn(p,"+",20):"")+okEcke();
    return o;
  }
  case "datum": {
    const r=j.rtc||"2000-01-01 00:00:00", felder=[r.slice(8,10),r.slice(5,7),r.slice(0,4),r.slice(11,13),r.slice(14,16)];
    const pos=[[15,66,52,110],[61,67,98,111],[108,67,158,111],[184,66,224,110],[240,66,290,110]];
    const hell=[0,1,2,3,4,0][def.hell];
    let o=rahmen("Datum und Uhrzeit")+`<rect x="12" y="30" width="154" height="158" rx="4" fill="none" stroke="#6fb8e6"/><rect x="170" y="30" width="138" height="158" rx="4" fill="none" stroke="#6fb8e6"/>`+
      txt(18,46,"Datum",{s:10})+txt(176,46,"Uhrzeit",{s:10});
    pos.forEach((q,i)=>{o+=`<rect x="${q[0]}" y="${q[1]}" width="${q[2]-q[0]}" height="${q[3]-q[1]}" rx="3" fill="${i==hell?"#bfe9ff":"#1c2f4a"}"/>`+txt((q[0]+q[2])/2,q[1]+28,felder[i],{s:15,a:"middle",f:i==hell?"#0b1a2e":FARBE.text})});
    const pm=R.filter(r=>r.t[6]==2);
    return o+kn({x0:40,y0:120,x1:90,y1:165},"–",22)+kn({x0:100,y0:120,x1:150,y1:165},"+",22)+pm.map(r=>kn(r,r.t[8]>0?"+":"–",22)).join("")+okEcke();
  }
  case "woche": {
    const tage=["SON","MON","DIE","MIT","DON","FRI","SAM"], x=[10,54,97,141,183,226,271];
    let o=rahmen("Auto ON/OFF")+tage.map((t,i)=>`<rect x="${x[i]}" y="24" width="39" height="39" rx="3" fill="${i==def.tag?"#bfe9ff":"#1c2f4a"}" stroke="#6fb8e6"/>`+txt(x[i]+19.5,48,t,{s:9,a:"middle",f:i==def.tag?"#0b1a2e":FARBE.text})).join("");
    [[0x07,72,"ON"],[0x0B,134,"OFF"]].forEach(([vp,y,l])=>{const v=vpw(vp); const [m,p]=T_.pm(vp);
      o+=txt(20,y+28,l,{s:12,f:FARBE.cyan})+`<rect x="70" y="${y+2}" width="62" height="40" rx="3" fill="#1c2f4a"/><rect x="138" y="${y+2}" width="62" height="40" rx="3" fill="#1c2f4a"/>`+
        txt(101,y+30,v==null?"00":String(v).padStart(2,"0"),{s:18,a:"middle"})+txt(169,y+30,"00",{s:18,a:"middle"})+(m?kn(m,"–",20):"")+(p?kn(p,"+",20):"")});
    return o+txt(14,222,"ZURÜCKSETZEN",{s:10})+okEcke();
  }
  case "spuelen": {
    const kurz=T_.code(6,0), komplett=T_.code(6,1), esc=R.find(r=>r.t[7]==0);
    return dialog("Waschen")+txt(160,62,"Rückspülen der Brühgruppe",{s:12,a:"middle",glow:1})+(kurz?kn(kurz,"KURZ",15):"")+(komplett?kn(komplett,"COMPLET",15):"")+fussLinie(172)+(esc?kn(esc,"ESC",14):"");
  }
  case "hebel": {
    const k=R.slice().sort((a,b)=>a.x0-b.x0).filter(r=>r.x1-r.x0<150);
    return dialog(def.titel)+hebel(24,120,def.richtung)+mehrzeilig(230,def.knoepfe&&def.knoepfe.length==2?60:56,def.text,{s:11,zh:13})+
      (def.knoepfe?fussLinie(172)+k.map((r,i)=>kn(r,def.knoepfe[i]||"OK",13)).join(""):"");
  }
  case "gruppe": {
    const pm=R.filter(r=>r.t[6]==2), vp=pm.length?pm[0].t[7]:null, [m,p]=vp!=null?T_.pm(vp):[];
    return rahmen("Gruppentemperatur")+txt(18,48,"GRUPPEN",{s:10})+txt(76,48,"OFF",{s:13,f:FARBE.cyan})+`<path d="M16 60 H304" stroke="${FARBE.cyan}" stroke-width="2" opacity=".6"/>`+
      txt(18,82,"Temperatur",{s:12,f:FARBE.cyan})+(vp!=null?txt(110,132,fmt(vpw(vp),1,0),{s:34,a:"middle",glow:1}):"")+(m?kn(m,"–",22):kn({x0:206,y0:104,x1:246,y1:142},"–",22))+(p?kn(p,"+",22):kn({x0:252,y0:104,x1:292,y1:142},"+",22))+okEcke();
  }
  case "start1": return seiteStart(s,j,k,null,true);
  case "menue": return seiteMenue(s,j,k,d);
  }
  return null;
}
function seiteMenue(s,j,k,d){
  const o=seiteStart(s,j,k,d).replace('<rect width="320" height="240" fill="url(#gbg)"/>','<rect width="320" height="240" fill="url(#gbg)"/><g opacity=".35">')+"</g>";
  const knopf=(y,icon)=>`<rect x="4" y="${y}" width="66" height="44" rx="6" fill="#16304f" stroke="#6fb8e6" stroke-width="1.2"/>${icon}`;
  const ic=(y,p)=>`<g transform="translate(37,${y+22})" stroke="#dff6ff" stroke-width="2.5" fill="none" filter="url(#glow)">${p}</g>`;
  return o+`<rect x="0" y="0" width="76" height="240" fill="#0c1a2e" opacity=".92"/>`+
    `<g fill="${FARBE.cyan}"><rect x="10" y="10" width="7" height="7" rx="2"/><rect x="10" y="20" width="7" height="7" rx="2"/><rect x="10" y="30" width="7" height="7" rx="2"/></g>`+
    knopf(45,ic(45,`<path d="M-12 -8 h14 v10 h-14z M2 -4 h8 v10 h-8"/>`))+knopf(94,ic(94,`<circle r="8"/><path d="M0 -13 v4 M0 9 v4 M-13 0 h4 M9 0 h4 M-9 -9 l3 3 M6 6 l3 3 M-9 9 l3 -3 M6 -6 l3 -3"/>`))+
    knopf(144,ic(144,`<path d="M-12 6 q6 -14 16 -6 q6 4 10 -2 M-6 10 h14"/>`))+knopf(193,ic(193,`<path d="M-8 -6 a11 11 0 1 0 16 0 M0 -13 v10"/>`));
}
function seiteSonst(s){
  return rahmen(seitenname(s))+txt(160,120,"Seite "+s,{s:14,a:"middle",f:FARBE.leise})+txt(160,140,"noch nicht nachgebaut",{s:10,a:"middle",w:400,f:FARBE.leise});
}
function zeigeDisplay(j,k,d){
  const s=j.seite; dspSeite=s;
  if(s<0){$("dsp").innerHTML=defs+`<rect width="320" height="240" fill="#000"/>`+txt(160,124,"keine Seite bekannt",{s:12,a:"middle",f:FARBE.leise});return}
  const b=s%100; let inhalt; dspJ=j;
  if(b==0) inhalt=seiteStandby(s,j);
  else if([1,5,78].includes(b)) inhalt=seiteStart(s,j,k,d);
  else if((inhalt=seiteBaukasten(s,j,k,d))) {}
  else if(ALARM_ICON[b]) inhalt=seiteAlarm(s,j);
  else if(LISTEN[b]) inhalt=seiteListe(s);
  else if([7,8,12,13].includes(b)) inhalt=seiteTemperatur(s,j);
  else if(b==90) inhalt=`<rect width="320" height="240" fill="url(#gbg)"/>${logo(160,100,1.2)}${txt(160,140,"BEZZERA",{s:15,a:"middle",glow:1})}${txt(40,226,"TFT 2.0",{s:14,f:FARBE.cyan})}${txt(200,226,"FW: "+(((j.vps.find(x=>x.vp==0x63)||{w:[0]}).w[0])/10).toFixed(1),{s:14,f:FARBE.orange})}`;
  else inhalt=seiteSonst(s);
  $("dsp").innerHTML=defs+inhalt;
  // fehlende Werte der Seite einmal beim Display abfragen (Antwort landet in der VP-Tabelle)
  for(const vp of dspFehlt){ if(dspGefragt.has(s+":"+vp)) continue; dspGefragt.add(s+":"+vp);
    cmd(`d c6 a5 04 83 ${(vp>>8).toString(16).padStart(2,"0")} ${(vp&255).toString(16).padStart(2,"0")} 01`); }
  dspFehlt.clear();
}
</script>


<script>
// ─── Stil nach dem Handbuch: Rahmen, Knöpfe, Symbole, Startbildschirm ──────
// Überschreibt die gleichnamigen Funktionen des ersten Nachbaus. Vorlage sind
// die Bildschirmfotos (320×240) im Bezzera-Handbuch „Matrix Duo“ (2018/2020).
const TK=FARBE.cyan;
T.start=["press to start","Für Start drücken","premere per accendere"];
T.dal=["Dal 1901","Dal 1901","Dal 1901"];

// Symbole, alle in Türkis, Mittelpunkt x/y, Größe ~ s
const SYM={
 tasse:(x,y,s=1)=>`<g transform="translate(${x},${y}) scale(${s})" fill="none" stroke="${TK}" stroke-width="2.2"><path d="M-9 -6 H7 V2 Q7 9 -1 9 Q-9 9 -9 2 Z"/><path d="M7 -3 Q13 -3 13 1 Q13 5 7 4"/><path d="M-11 11 H9" stroke-width="1.5"/></g>`,
 dampf:(x,y,s=1)=>`<g transform="translate(${x},${y}) scale(${s})" fill="none" stroke="${TK}" stroke-width="2"><path d="M-8 9 Q-12 5 -8 1 Q-10 -4 -4 -5 Q-2 -10 3 -7 Q9 -8 8 -2 Q13 1 9 6 Q8 10 3 9 Z"/><path d="M-6 -10 Q-9 -14 -5 -16"/></g>`,
 wasser:(x,y,s=1)=>`<g transform="translate(${x},${y}) scale(${s})" fill="${TK}"><text x="-9" y="-2" font-size="8" fill="${TK}" font-family="Arial">1</text><path d="M-2 -8 q-3 4 0 5 q3 -1 0 -5z"/><path d="M4 -10 q-3 4 0 5 q3 -1 0 -5z"/><path d="M-6 3 q2 -2 4 0 t4 0 t4 0 M-6 7 q2 -2 4 0 t4 0 t4 0" fill="none" stroke="${TK}" stroke-width="1.4"/></g>`,
 zahnrad:(x,y,s=1)=>{let z="";for(let i=0;i<8;i++){z+=`<rect x="-2.6" y="-13" width="5.2" height="6" rx="1" transform="rotate(${i*45})"/>`}return `<g transform="translate(${x},${y}) scale(${s})" fill="${TK}">${z}<circle r="9"/><circle r="3.6" fill="#000"/></g>`},
 dusche:(x,y,s=1)=>`<g transform="translate(${x},${y}) scale(${s})"><path d="M-12 -12 Q-4 -16 0 -8" fill="none" stroke="${TK}" stroke-width="3"/><path d="M-4 -8 L6 -12 L10 -2 L0 2 Z" fill="${TK}"/><g stroke="${TK}" stroke-width="1.6">${[[2,5],[6,3],[10,1],[4,9],[8,7],[12,5],[6,13],[10,11],[14,9]].map(([a,b])=>`<path d="M${a} ${b} l1.5 1.5"/>`).join("")}</g></g>`,
 power:(x,y,s=1)=>`<g transform="translate(${x},${y}) scale(${s})" fill="none" stroke="${TK}" stroke-width="3" stroke-linecap="round"><path d="M-5 -9 A11 11 0 1 0 5 -9" stroke-dasharray="40 3 3 3 3 3"/><path d="M0 -14 V-2"/></g>`,
 tuch:(x,y,s=1)=>`<g transform="translate(${x},${y}) scale(${s})"><path d="M-12 -4 L2 -13 L12 1 L-2 11 Z" fill="${TK}"/><path d="M2 -13 L5 -6 L12 1" fill="#2d6f73"/><path d="M-4 2 q-2 -6 2 -7 q1 -4 4 -2 q2 -3 4 0 q3 -1 3 2 l1 7 q0 5 -6 6 q-6 0 -8 -6z" fill="#dff7f8" stroke="#000" stroke-width=".8"/></g>`,
 schluessel:(x,y,s=1)=>`<g transform="translate(${x},${y}) scale(${s}) rotate(45)" fill="${TK}"><path d="M-2.5 -4 H2.5 V13 Q2.5 16 0 16 Q-2.5 16 -2.5 13 Z"/><path d="M-7 -10 A8 8 0 1 0 7 -10 L3 -10 L3 -4 L-3 -4 L-3 -10 Z"/></g>`,
 sprache:(x,y,s=1)=>`<g transform="translate(${x},${y}) scale(${s})" fill="none" stroke="${TK}" stroke-width="1.6"><rect x="-11" y="-9" width="12" height="9" rx="2" fill="${TK}"/><text x="-8" y="-2" font-size="7" fill="#000" stroke="none" font-weight="700" font-family="Arial">A</text><rect x="-2" y="-3" width="13" height="10" rx="2" fill="#000"/><path d="M1 7 l-2 4 l5 -4"/><text x="1.5" y="5" font-size="7" fill="${TK}" stroke="none" font-family="Arial">あ</text></g>`,
 winkel:(x,y,s=1)=>`<g transform="translate(${x},${y}) scale(${s})" fill="${TK}"><path d="M-10 10 V-10 L10 10 Z"/><path d="M-6 6 V-1 L1 6 Z" fill="#000"/></g>`,
 birne:(x,y,s=1)=>`<g transform="translate(${x},${y}) scale(${s})"><circle cy="-3" r="9" fill="${TK}"/><circle cx="-3" cy="-5" r="1.4" fill="#000"/><circle cx="3" cy="-5" r="1.4" fill="#000"/><circle cy="0" r="1.4" fill="#000"/><rect x="-4.5" y="6" width="9" height="2" fill="${TK}"/><rect x="-4" y="9" width="8" height="2" fill="${TK}"/></g>`,
 tank:(x,y,s=1,f=TK)=>`<g transform="translate(${x},${y}) scale(${s})"><path d="M-11 -9 V8 H11 V-9" fill="none" stroke="${f}" stroke-width="2.4"/><path d="M-11 -3 H-7 M7 -3 H11" stroke="${f}" stroke-width="2"/><path d="M0 -7 q-5 6 0 9 q5 -3 0 -9z" fill="${f}"/></g>`,
 tropfen:(x,y,s=1)=>`<g transform="translate(${x},${y}) scale(${s})" fill="${TK}"><path d="M-4 -11 q-6 7 0 10 q6 -3 0 -10z"/><path d="M4 -1 q-6 7 0 10 q6 -3 0 -10z"/></g>`,
 hahn:(x,y,s=1)=>`<g transform="translate(${x},${y}) scale(${s})" fill="none" stroke="${TK}" stroke-width="2.4"><path d="M-8 8 V-6 H6 V-1"/><path d="M6 3 q-2 3 0 4 q2 -1 0 -4z" fill="${TK}" stroke="none"/></g>`,
 kalender:(x,y,s=1,uhr=false)=>`<g transform="translate(${x},${y}) scale(${s})"><rect x="-10" y="-8" width="20" height="18" rx="2" fill="${TK}"/><rect x="-8" y="-3" width="16" height="11" fill="#000"/>${[-6,-1,4].map(a=>`<rect x="${a}" y="-1" width="3" height="3" fill="${TK}"/><rect x="${a}" y="4" width="3" height="3" fill="${TK}"/>`).join("")}<rect x="-6" y="-11" width="2" height="5" fill="${TK}"/><rect x="4" y="-11" width="2" height="5" fill="${TK}"/>${uhr?`<circle cx="8" cy="8" r="6" fill="#000" stroke="${TK}" stroke-width="2"/><path d="M8 5 V8 H10" stroke="${TK}" stroke-width="1.5" fill="none"/>`:`<path d="M3 6 l3 3 l6 -7" stroke="#fff" stroke-width="2" fill="none"/>`}</g>`,
 filter:(x,y,s=1,f=TK)=>`<g transform="translate(${x},${y}) scale(${s})" fill="${f}">${Array.from({length:10},(_,i)=>`<circle cx="${(10*Math.cos(i*Math.PI/5)).toFixed(1)}" cy="${(10*Math.sin(i*Math.PI/5)).toFixed(1)}" r="1.8"/>`).join("")}<text x="0" y="4" font-size="11" text-anchor="middle" font-weight="700" font-family="Arial">S</text></g>`,
 warn:(x,y,s=1)=>`<g transform="translate(${x},${y}) scale(${s})"><path d="M0 -16 L18 14 H-18 Z" fill="none" stroke="${FARBE.rot}" stroke-width="3.2" stroke-linejoin="round"/><path d="M0 -4 V5" stroke="#fff" stroke-width="3" stroke-linecap="round"/><circle cy="9.5" r="1.7" fill="#fff"/></g>`,
 thermo:(x,y,s=1)=>`<g transform="translate(${x},${y}) scale(${s})"><rect x="-2.5" y="-11" width="5" height="14" rx="2.5" fill="none" stroke="${TK}" stroke-width="1.8"/><circle cy="6" r="4.5" fill="${TK}"/></g>`,
 key:(x,y,s=1)=>`<g transform="translate(${x},${y}) scale(${s}) rotate(-35)"><circle cx="-6" r="5" fill="none" stroke="${TK}" stroke-width="3"/><path d="M-1 0 H12 M8 0 V4 M11 0 V3" stroke="${TK}" stroke-width="3"/></g>`,
};

function schlange(x,y,sk,farbe){ // Bezzera-„Biscione“, vereinfachte Kontur
  return `<g transform="translate(${x},${y}) scale(${sk})" fill="none" stroke="${farbe}" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
  <path d="M-4 -26 q10 -6 14 2 q-4 6 -12 5 q-12 -2 -12 8 q0 9 13 8 q13 -1 13 9 q0 10 -13 9 q-12 -1 -12 7 q0 7 10 7"/>
  <path d="M-4 -26 l-6 -4 M-4 -26 l-10 1 M-13 -32 l6 12 M-18 -26 l12 0"/><circle cx="6" cy="-24" r="1.2" fill="${farbe}"/></g>`;
}
function kopfLogo(x,y,f="#fff"){return schlange(x+6,y+12,.38,f)+txt(x+16,y+11,"BEZZERA",{s:13,w:800,f})+txt(x+27,y+22,"Dal 1901",{s:8,w:700,f})}
function menueQuadrate(){return `<g fill="${TK}"><rect x="2" y="3" width="8" height="8"/><rect x="2" y="13" width="8" height="8"/><rect x="2" y="23" width="8" height="8"/></g>`}
function uhr(r){
  const hm=r?r.slice(11,16):"--:--", dat=r?r.slice(8,10)+"/"+r.slice(5,7)+"/"+r.slice(0,4):"";
  return `<rect x="0" y="198" width="4" height="42" fill="url(#gleiste)"/>`+txt(8,219,hm,{s:19,w:300,f:TK})+txt(8,231,dat,{s:8,f:TK});
}
// Seite mit Titel oben links, türkisem Rahmen und OK-Reiter unten rechts
function rahmen(titel){
  return `<rect width="320" height="240" fill="#000"/>`+txt(6,15,titel,{s:13,f:FARBE.text})+
   `<path d="M2 20 H317 V196 M2 20 V237 H230" fill="none" stroke="${FARBE.rahmen}" stroke-width="1.6"/>`;
}
function okEcke(){
  return `<path d="M230 237 L250 196 H317" fill="none" stroke="${FARBE.rahmen}" stroke-width="1.6"/>`+
   `<path d="M236 238 L254 200 H318 V238 Z" fill="url(#gok)"/>`+txt(283,231,"OK",{s:28,w:700,a:"middle",f:"#d5f4f6"});
}
// Dialogfenster (Alarm, Rückfragen): voller Rahmen, Titelbalken
function dialog(titel){
  return `<rect width="320" height="240" fill="#000"/><rect x="2" y="2" width="316" height="236" fill="none" stroke="${FARBE.rahmen}" stroke-width="3"/>`+
   (titel?`<rect x="3.5" y="3.5" width="313" height="30" fill="url(#gtitel)"/>`+txt(12,26,titel,{s:20,f:FARBE.text}):"");
}
function rahmenOhneTitel(){return dialog("")}
function fussLinie(y=172){return `<rect x="8" y="${y}" width="304" height="2" fill="url(#gtrenn)"/>`}
function knopf(x,y,w,h,t,js,s=14){return `<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="3" fill="url(#gknopf)" stroke="#50585d"/>`+txt(x+w/2,y+h/2+s*.36,t,{s,a:"middle",w:700,f:"#e9f7f8"})}
function kn(r,label,sz=14,aktiv=false){const w=r.x1-r.x0,h=r.y1-r.y0, x=r.x0+2,y=r.y0+2;
  if(label=="–"||label=="+"){ // ± wie im Handbuch: dunkles Feld, dicke helle Zeichen
    const cx=x+(w-4)/2, cy=y+(h-4)/2, l=Math.min(w,h)*.22;
    return `<rect x="${x}" y="${y}" width="${w-4}" height="${h-4}" rx="3" fill="url(#gknopf)" stroke="#50585d"/><path d="M${cx-l} ${cy} H${cx+l}${label=="+"?` M${cx} ${cy-l} V${cy+l}`:""}" stroke="#dff5f7" stroke-width="${Math.max(3,l*.45)}"/>`;
  }
  return `<rect x="${x}" y="${y}" width="${w-4}" height="${h-4}" rx="3" fill="${aktiv?"#bfe9ec":"url(#gknopf)"}" stroke="#50585d"/>`+
    txt(r.x0+w/2,r.y0+h/2+sz*.36,label,{s:sz,a:"middle",w:700,f:aktiv?"#0b1a1c":"#e9f7f8"});
}
function zeilenBox(x,y,w,h,aktiv){return `<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="2" fill="${aktiv?"url(#ghell)":"url(#gzeile)"}" stroke="${FARBE.rahmen}" stroke-width=".8"/>`}
function mehrzeilig(x,y,t,o={}){return t.split("\n").map((z,i)=>txt(x,y+i*(o.zh||19),z,{s:15,a:"middle",f:FARBE.text,...o})).join("")}
function schalter(x,y,text,an){ // „| OFF“ mit Unterstrich: grün = aktiv
  return `<rect x="${x-6}" y="${y-13}" width="1.5" height="15" fill="#bfe9ec"/>`+txt(x,y,text,{s:15,f:FARBE.text})+`<rect x="${x}" y="${y+5}" width="${text.length*9}" height="2.5" fill="${an?"#39d353":"#777"}"/>`;
}
function scrollleiste(stufe,stufen){
  const th=Math.max(20,120/stufen), ty=70+(120-th)*(stufen>1?stufe/(stufen-1):0);
  return `<rect x="274" y="22" width="41" height="170" rx="3" fill="#0d1516" stroke="#2c4a4c"/>`+
   `<rect x="276" y="24" width="37" height="42" rx="3" fill="url(#gknopf)"/><path d="M285 52 L294.5 38 L304 52" fill="none" stroke="${TK}" stroke-width="3"/>`+
   `<rect x="276" y="146" width="37" height="44" rx="3" fill="url(#gknopf)"/><path d="M285 160 L294.5 174 L304 160" fill="none" stroke="${TK}" stroke-width="3"/>`+
   `<rect x="279" y="${Math.min(ty,120)}" width="31" height="16" rx="3" fill="#9aa3a6"/>`;
}

// Startbildschirm: links Pumpendruck 0–10 bar, rechts Druck Servicekessel
// 0–2,5 bar, in der Mitte Temperaturen; rechts der Wasserstand.
function seiteStart(s,j,k,d,einZeiger=false){
  const cx=158, cy=120, R=84, rad=a=>a*Math.PI/180, P=(a,r)=>[cx+Math.cos(rad(a))*r, cy+Math.sin(rad(a))*r];
  const bogen=(a0,a1,r,sweep)=>{const [x0,y0]=P(a0,r),[x1,y1]=P(a1,r);return `M${x0.toFixed(1)} ${y0.toFixed(1)} A${r} ${r} 0 0 ${sweep} ${x1.toFixed(1)} ${y1.toFixed(1)}`};
  let o=`<rect width="320" height="240" fill="#000"/><circle cx="${cx}" cy="${cy}" r="${R-8}" fill="url(#gscheibe)"/>`;
  // linker Bogen: Pumpendruck, 0 unten (95°) bis 10 oben (265°)
  const LA=v=>95+v/10*170;
  o+=`<path d="${bogen(95,265,R,1)}" stroke="${TK}" stroke-width="15" fill="none"/>`;
  for(let v=0;v<=10;v+=.5){const [x0,y0]=P(LA(v),R-7.5),[x1,y1]=P(LA(v),R+(v%1?-3:7.5));o+=`<path d="M${x0.toFixed(1)} ${y0.toFixed(1)} L${x1.toFixed(1)} ${y1.toFixed(1)}" stroke="#000" stroke-width="${v%1?1:1.6}"/>`}
  ["1.0","2.0","3.0","4.0","5.0","6.0","7.0","8.0","9.0","10"].forEach((t,i)=>{const [x,y]=P(i==9?LA(9.55):LA(i+1),R+17);o+=txt(x.toFixed(1),(y+3).toFixed(1),t,{s:i==9||i==4?13:8,a:"middle",f:"#dfe7e8"})});
  o+=`<path d="${bogen(LA(6),LA(9.8),R-11,1)}" stroke="#e3161b" stroke-width="2.5" fill="none" stroke-opacity=".9"/><path d="${bogen(LA(4.5),LA(6.2),R-11,1)}" stroke="#ff9d2a" stroke-width="2.5" fill="none" stroke-opacity=".6"/>`;
  if(!einZeiger){
    const RA=v=>85-v/2.5*170;
    o+=`<path d="${bogen(RA(2.5),RA(0),R,1)}" stroke="${TK}" stroke-width="15" fill="none"/>`;
    for(let v=0;v<=2.5;v+=.125){const g=Math.abs(v*2-Math.round(v*2))<1e-6;const [x0,y0]=P(RA(v),R-7.5),[x1,y1]=P(RA(v),R+(g?7.5:-3));o+=`<path d="M${x0.toFixed(1)} ${y0.toFixed(1)} L${x1.toFixed(1)} ${y1.toFixed(1)}" stroke="#000" stroke-width="${g?1.6:1}"/>`}
    ["0.5","1","1.5","2","2.5"].forEach((t,i)=>{const [x,y]=P(i==4?RA(2.38):RA((i+1)/2),R+16);o+=txt(x.toFixed(1),(y+3).toFixed(1),t,{s:i==4?13:9,a:"middle",f:"#dfe7e8"})});
    o+=`<path d="${bogen(RA(2.45),RA(1.3),R-11,1)}" stroke="#e3161b" stroke-width="2.5" fill="none" stroke-opacity=".9"/><path d="${bogen(RA(1.35),RA(0.9),R-11,1)}" stroke="#ff9d2a" stroke-width="2.5" fill="none" stroke-opacity=".6"/>`;
  }
  // Trennstriche oben und unten, Zeiger auf 0
  o+=`<path d="M${cx} ${cy-R-9} V${cy-R+9} M${cx} ${cy+R-9} V${cy+R+9}" stroke="#000" stroke-width="3"/>`;
  const zeiger=a=>{const [x0,y0]=P(a,R-8),[x1,y1]=P(a,R+8);return `<path d="M${x0.toFixed(1)} ${y0.toFixed(1)} L${x1.toFixed(1)} ${y1.toFixed(1)}" stroke="#fff" stroke-width="4"/>`};
  o+=zeiger(LA(0.15)); if(!einZeiger) o+=zeiger(85-0.15/2.5*170);
  o+=txt(cx,cy+R+13,"0",{s:12,a:"middle",f:"#dfe7e8"})+txt(cx+6,cy+R+13,"bar",{s:8,f:"#dfe7e8"});
  // Mitte
  if(einZeiger){
    o+=SYM.tasse(cx,cy-30,1)+txt(cx,cy+12,k??"–",{s:30,a:"middle",f:"#fff"});
  }else{
    o+=SYM.tasse(cx-26,cy-28,1)+SYM.dampf(cx+20,cy-26,.95)+SYM.wasser(cx+40,cy-28,.9);
    o+=txt(cx-24,cy+12,k??"–",{s:24,a:"middle",f:"#fff"})+txt(cx+2,cy-2,"°C",{s:9,a:"middle",f:"#fff"})+txt(cx+28,cy+12,d??"–",{s:24,a:"middle",f:"#fff"});
    o+=`<rect x="${cx-44}" y="${cy+18}" width="34" height="3" fill="#7a8285"/><rect x="${cx+10}" y="${cy+18}" width="34" height="3" fill="#7a8285"/>`;
    // Wasserstand, rechts angeschnitten
    const wx=304, wy=120, wr=34;
    const seg=(a0,a1,voll)=>{const q=a=>[wx+Math.cos(rad(a))*wr, wy+Math.sin(rad(a))*wr];const [x0,y0]=q(a0),[x1,y1]=q(a1);
      return `<path d="M${x0.toFixed(1)} ${y0.toFixed(1)} A${wr} ${wr} 0 0 0 ${x1.toFixed(1)} ${y1.toFixed(1)}" stroke="${TK}" stroke-width="7" fill="none" ${voll?"":`stroke-opacity=".25"`}/>`};
    o+=`<circle cx="${wx}" cy="${wy}" r="${wr-6}" fill="#0c1718"/>`+seg(248,215,false)+seg(211,178,true)+seg(174,141,true)+seg(137,104,true);
    o+=txt(wx-12,wy-2,"water",{s:8,a:"middle",f:TK})+txt(wx-12,wy+8,"level",{s:8,a:"middle",f:TK});
    o+=`<g transform="translate(${wx-26},${wy-38}) rotate(-58)">`+txt(0,0,"max",{s:9,f:"#cfd8d9"})+`</g><g transform="translate(${wx-38},${wy+36}) rotate(58)">`+txt(0,0,"min",{s:9,f:"#cfd8d9"})+`</g>`;
  }
  return o+menueQuadrate()+kopfLogo(14,2)+uhr(j.rtc);
}
function seiteStandby(s,j){
  return `<rect width="320" height="240" fill="#000"/>`+kopfLogo(4,0)+`<rect x="0" y="30" width="170" height="1.5" fill="url(#gtrenn)"/>`+
   schlange(160,92,1.25,FARBE.rot)+txt(160,160,"BEZZERA",{s:13,w:800,a:"middle",f:"#fff"})+txt(160,173,"Dal 1901",{s:9,w:700,a:"middle",f:"#fff"})+
   txt(190,207,tx("start",s),{s:12,a:"middle",f:"#d9dfe0"})+
   `<path d="M272 108 H318 V148 H266 V114 Z" fill="#2c3b3d"/>`+SYM.schluessel(292,128,.9)+uhr(j.rtc);
}
function seiteMenue(s,j,k,d){
  const bild=seiteStart(s,j,k,d).replace('<rect width="320" height="240" fill="#000"/>','<rect width="320" height="240" fill="#000"/><g opacity=".38">')+"</g>";
  const kachel=(y,icon)=>`<path d="M6 ${y} H66 L72 ${y+6} V${y+43} H12 L6 ${y+37} Z" fill="#162224" stroke="#355a5d"/>${icon}`;
  return bild+`<rect x="0" y="0" width="78" height="240" fill="#000" opacity=".85"/><rect x="74" y="36" width="3" height="204" fill="url(#gleiste)"/>`+
    menueQuadrate()+kopfLogo(14,2)+kachel(45,SYM.tuch(39,68,.95))+kachel(94,SYM.zahnrad(39,117,1))+kachel(144,SYM.dusche(39,167,1))+kachel(193,SYM.power(39,216,1));
}
function seiteAlarm(s,j){
  const b=s%100, ic={2:"warn",3:"tank",4:"warn",56:"schluessel",77:"warn",89:"filter",91:"warn"}[b];
  const symbol=ic=="warn"?SYM.warn(160,70,1.1):ic=="tank"?SYM.tank(160,72,1.6,FARBE.orange):ic=="filter"?SYM.filter(160,72,1.4,FARBE.orange):`<g>${SYM.schluessel(160,72,1.3).replaceAll(TK,FARBE.orange)}</g>`;
  const R=tastenRect(s);
  return dialog(tx("alarm",s))+symbol+mehrzeilig(160,126,tx(b,s),{s:16})+fussLinie(178)+R.map(r=>kn(r,"OK",18)).join("");
}
// Menülisten: Symbol, Text, Wert/Schalter; Scrollleiste; OK-Reiter
const LISTEN_SYM={Sprache:"sprache",Language:"sprache",Lingua:"sprache",Einheiten:"winkel",Units:"winkel","Unità":"winkel"};
function zeilenSymbol(z){
  const t=z[0].toLowerCase();
  if(t.startsWith("language")) return SYM.sprache(24,0,.9);
  if(t.startsWith("units")) return SYM.winkel(24,0,.8);
  if(t.startsWith("led")||t.startsWith("lights")) return SYM.birne(24,0,.9);
  if(t.startsWith("sensor")) return SYM.tank(24,0,.8);
  if(t.startsWith("maint")) return SYM.schluessel(24,0,.7);
  if(t.startsWith("water filter")) return SYM.filter(24,0,.8);
  if(t.startsWith("water source")) return SYM.tropfen(24,0,.8);
  if(t.startsWith("date")) return SYM.kalender(24,0,.8);
  if(t.startsWith("auto")) return SYM.kalender(24,0,.8,true);
  if(t.startsWith("password")) return SYM.key(24,0,.8);
  if(t.startsWith("group temp")) return SYM.thermo(24,0,1);
  return "";
}
function seiteListe(s){
  const b=s%100, L=LISTEN[b], i=sprachIndex(s), tech=LISTE_TITEL[b]=="tech";
  let o=rahmen(tx(LISTE_TITEL[b],s));
  L.forEach((z,n)=>{const y=27+n*50, sym=tech?"":zeilenSymbol(z);
    o+=zeilenBox(8,y,258,40)+(sym?`<g transform="translate(0,${y+20})">${sym}</g>`:"")+txt(tech?16:44,y+26,z[i],{s:15,f:FARBE.text});
    const w=z[3];
    if(w=="OFF"||w=="ON") o+=schalter(222,y+26,w,w=="ON");
    else if(w=="°C | °F") o+=txt(206,y+26,"°C",{s:15,f:FARBE.text})+`<rect x="206" y="${y+31}" width="22" height="2.5" fill="#39d353"/>`+schalter(236,y+26,"°F",false);
    else if(w=="E61 | BZ") o+=txt(196,y+26,"E61",{s:15,f:FARBE.text})+`<rect x="196" y="${y+31}" width="28" height="2.5" fill="#39d353"/>`+schalter(234,y+26,"BZ",false);
    else if(w) o+=txt(258,y+26,w,{s:15,a:"end",f:FARBE.text});
    if(!tech&&z[0].toLowerCase().startsWith("water source")) o+=SYM.tank(214,y+19,.7)+`<rect x="206" y="${y+31}" width="18" height="2.5" fill="#39d353"/>`+SYM.hahn(248,y+20,.8);
  });
  const erste=[16,20].includes(b), stufe={16:0,17:1,18:2,19:3,20:0,21:1,22:2,79:0,82:0}[b]||0, stufen=[16,17,18,19].includes(b)?4:3;
  return o+scrollleiste(stufe,stufen)+okEcke();
}
function seiteTemperatur(s,j){
  const b=s%100, kaffee=b==7||b==12;
  let o=rahmen(tx(kaffee?"kaffee":"tee",s))+txt(10,48,tx("kessel",s),{s:12,f:FARBE.text})+txt(70,48,"OFF",{s:15,f:TK})+`<rect x="70" y="53" width="30" height="3" fill="#777"/>`+
    txt(134,48,tx("prio",s)+":",{s:12,f:FARBE.text})+txt(208,48,["coffee","Kaffee","caffè"][sprachIndex(s)],{s:15,f:TK})+`<rect x="8" y="62" width="300" height="2" fill="url(#gtrenn)"/>`+txt(12,88,tx("temp",s),{s:16,f:TK});
  if(b==7||b==8) o+=txt(66,136,"–",{s:50,w:200,a:"middle",f:"#8fd8dc"})+txt(128,104,"°C",{s:22,w:200,f:"#8fd8dc"});
  o+=tastenRect(s).filter(r=>r.t[6]==5&&r.t[7]==0x15).map(r=>kn(r,r.t[8]==2?"+":"–")).join("");
  if(kaffee) o+=`<rect x="4" y="202" width="2" height="18" fill="#bfe9ec"/>`+txt(10,217,"CRONO",{s:12,f:FARBE.text})+txt(62,217,"OFF",{s:12,f:TK})+`<rect x="62" y="221" width="22" height="2" fill="#777"/>`+
    `<rect x="142" y="202" width="2" height="18" fill="#bfe9ec"/>`+txt(150,217,tx("vorb",s),{s:12,f:FARBE.text});
  return o+okEcke();
}
// Spülablauf: Brühgruppe von der Seite mit Hebel und Pfeil
function hebel(x,y,richtung){
  return `<g transform="translate(${x},${y})" fill="none" stroke="${TK}" stroke-width="1.8">
   <rect x="0" y="-28" width="20" height="52" rx="2"/><rect x="4" y="-36" width="12" height="8"/><rect x="7" y="-42" width="6" height="6"/>
   <path d="M20 -10 H48 V20 H20"/><path d="M48 6 H62 L66 12 H48"/>
   <circle cx="10" cy="30" r="7"/><circle cx="10" cy="30" r="2" fill="${TK}"/>
   <path d="M12 26 L36 ${richtung>0?-18:-22}" stroke-width="5"/>
   ${richtung>0?`<path d="M30 -30 A34 34 0 0 1 58 -4" stroke-width="2"/><path d="M52 -8 l6 4 l1 -7" stroke-width="2"/>`
     :`<path d="M52 -8 A34 34 0 0 0 26 -32" stroke-width="2"/><path d="M31 -34 l-6 2 l3 6" stroke-width="2"/>`}</g>`;
}
</script>
<script>
// ─── Berührung wie am Display: Tastentabelle aus 13.bin ────────────────────
let letzterStatus=null;
if(location.hash=="#flaechen") $("f_flaechen").checked=true;
function tastenAuf(s){return (typeof TASTEN!=="undefined"?TASTEN:[]).filter(t=>t[0]==s)}
function trifft(t,x,y){const x0=Math.min(t[1],t[3]),x1=Math.max(t[1],t[3]),y0=Math.min(t[2],t[4]),y1=Math.max(t[2],t[4]);return x>=x0&&x<=x1&&y>=y0&&y<=y1&&x1>x0}
function protokoll(z){const l=$("log"); l.textContent+=z+"\n"; l.scrollTop=l.scrollHeight}
async function vpWert(vp){
  let e=letzterStatus&&letzterStatus.vps.find(v=>v.vp==vp);
  if(!e){ // beim Display nachfragen, Antwort landet in der VP-Tabelle
    await cmd(`d c6 a5 04 83 ${(vp>>8).toString(16).padStart(2,"0")} ${(vp&255).toString(16).padStart(2,"0")} 01`);
    await new Promise(r=>setTimeout(r,250));
    const j=await (await fetch("/api/status")).json(); e=j.vps.find(v=>v.vp==vp);
  }
  return e&&e.w.length?e.w[0]:0;
}
async function beruehre(x,y){
  const s=dspSeite, t=tastenAuf(s).find(t=>trifft(t,x,y));
  if(!t){protokoll(`Berührung (${x},${y}) auf Seite ${s}: keine Taste`);return}
  const [,, , , ,folge,typ,vp]=t, hx="0x"+vp.toString(16);
  if(typ==5){await cmd(`w ${hx} ${t[8]}`); protokoll(`Taste Seite ${s}: VP ${hx} = ${t[8]}`+(folge>=0?`, → Seite ${folge}`:""))}
  else if(typ==2){
    const [,,,,,,,,rich,schritt,min,max,kreis]=t; let v=await vpWert(vp)+rich*schritt;
    if(v>max) v=kreis?min:max; if(v<min) v=kreis?max:min;
    await cmd(`w ${hx} ${v}`); protokoll(`${rich>0?"+":"–"} Seite ${s}: VP ${hx} = ${v} [${min}..${max}]`);
  }
  else if(typ==1){
    const v=prompt(`Wert für VP ${hx} (${t[8]}..${t[9]})`); if(v===null) return;
    const n=Math.max(t[8],Math.min(t[9],parseInt(v,10)||0)); await cmd(`w ${hx} ${n}`); protokoll(`Eingabe Seite ${s}: VP ${hx} = ${n}`);
  }
  else protokoll(`Taste Seite ${s}`+(folge>=0?`: → Seite ${folge}`:": ohne Funktion"));
  if(folge>=0) await cmd(`p ${folge}`);
}
$("dsp").addEventListener("click",ev=>{
  const r=$("dsp").getBoundingClientRect();
  beruehre(Math.round((ev.clientX-r.left)/r.width*320),Math.round((ev.clientY-r.top)/r.height*240));
});
$("dsp").style.cursor="pointer";
const _zeige=zeigeDisplay;
zeigeDisplay=function(j,k,d){
  letzterStatus=j; _zeige(j,k,d);
  if($("f_flaechen").checked){
    $("dsp").innerHTML+=tastenAuf(j.seite).map(t=>{const x0=Math.min(t[1],t[3]),y0=Math.min(t[2],t[4]);
      return `<rect x="${x0}" y="${y0}" width="${Math.abs(t[3]-t[1])}" height="${Math.abs(t[4]-t[2])}" fill="#ff9d4522" stroke="#ff9d45" stroke-width="1" stroke-dasharray="3 2" pointer-events="none"/>`}).join("");
  }
};
</script>
</body>
</html>)HTML";
