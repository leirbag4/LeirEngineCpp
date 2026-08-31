// docs/sphinx/_static/leir_live_panel.js — panel flotante minimizable para pintar firmas C++ en vivo.
// Inyectado en TODAS las páginas api vía conf.py html_js_files. Minimizado por default: solo botón "setup".
(function(){
  function init(){
  // solo en api (opcional: si querés en todos los docs, sacá este if)
  if (!/\/api\//.test(location.pathname) && !/\/api$/.test(location.pathname)) return;
  if (document.getElementById('leir-live-panel')) return;
  const css = `
  #leir-live-panel{position:fixed;top:12px;right:12px;z-index:99999;background:#1e1e2e;border:1px solid #555;border-radius:10px;box-shadow:0 8px 32px rgba(0,0,0,0.6);width:360px;max-height:92vh;overflow:auto;font-family:system-ui,Segoe UI,Roboto,sans-serif;font-size:12px;color:#cdd6f4}
  #leir-live-panel.minimized{width:auto;max-height:auto;overflow:visible;background:transparent;border:0;box-shadow:none}
  #leir-live-panel.minimized .leir-live-body{display:none}
  #leir-live-panel.minimized .leir-live-header{border:0;background:transparent;padding:0;border-radius:10px;justify-content:flex-end}
  #leir-live-panel.minimized #leir-live-title{display:none}
  #leir-live-panel .leir-live-header{display:flex;align-items:center;justify-content:space-between;padding:10px 12px;border-bottom:1px solid #313244;position:sticky;top:0;background:#1e1e2e;border-radius:10px 10px 0 0}
  #leir-live-panel .leir-live-body{padding:10px 12px}
  #leir-live-panel input[type="text"]{background:#11111b;color:#cdd6f4;border:1px solid #45475a;border-radius:6px;padding:4px 6px}
  #leir-live-setup-btn{background:#1e1e2e;border:1px solid #555;color:#cdd6f4;padding:6px 14px;border-radius:8px;cursor:pointer;font-weight:700;box-shadow:0 4px 16px rgba(0,0,0,0.4)}
  #leir-live-setup-btn:hover{background:#313244}
  `;
  const s=document.createElement('style'); s.textContent=css; document.head.appendChild(s);
  const panel=document.createElement('div'); panel.id='leir-live-panel'; panel.className='minimized';
  panel.innerHTML=`
  <div class="leir-live-header">
    <b id="leir-live-title" style="font-size:13px;cursor:pointer;">Leir Live Paint <span style="opacity:0.6;font-weight:400;">— pintar</span></b>
    <div>
      <button id="leir-live-setup-btn" title="Abrir panel">setup</button>
      <button id="leir-live-expand" title="Expandir" style="background:#89b4fa;border:0;color:#11111b;padding:4px 10px;border-radius:6px;cursor:pointer;font-weight:700;display:none;">Expandir</button>
      <button id="leir-live-close2" title="Minimizar" style="background:#45475a;border:0;color:#cdd6f4;padding:4px 8px;border-radius:6px;cursor:pointer;margin-left:6px;display:none;">X</button>
    </div>
  </div>
  <div class="leir-live-body">
    <label style="display:block;margin-bottom:6px;opacity:0.9;">Template</label>
    <select id="leir-live-template" style="width:100%;background:#11111b;color:#cdd6f4;border:1px solid #45475a;border-radius:6px;padding:6px 8px;">
      <option value="custom">Custom (leir_theme #D09EF9)</option>
      <option value="monokai">monokai</option>
      <option value="dracula">dracula</option>
      <option value="github-dark">github-dark</option>
      <option value="native">native</option>
      <option value="one-dark">one-dark</option>
      <option value="solarized-dark">solarized-dark</option>
      <option value="pydata-monokai">pydata + monokai</option>
    </select>
    <div style="margin-top:10px;display:grid;grid-template-columns:1fr 44px 80px;gap:6px;align-items:center;">
      <span>Nombres <span style="opacity:0.6;">clase/func/ctor</span></span><input type="color" id="c_names"><input type="text" id="t_names">
      <span>Clase base link <span style="opacity:0.6;">UIElement</span></span><input type="color" id="c_base"><input type="text" id="t_base">
      <span>Keyword <span style="opacity:0.6;">class/public/inline</span></span><input type="color" id="c_kw"><input type="text" id="t_kw">
      <span>Type keyword <span style="opacity:0.6;">void/bool</span></span><input type="color" id="c_kt"><input type="text" id="t_kt">
      <span>Tipos/Namespace <span style="opacity:0.6;">Vector4/std</span></span><input type="color" id="c_n"><input type="text" id="t_n">
      <span>Parametros <span style="opacity:0.6;">text</span></span><input type="color" id="c_param"><input type="text" id="t_param">
      <span>Puntuacion <span style="opacity:0.6;">:: & * ( )</span></span><input type="color" id="c_p"><input type="text" id="t_p">
      <span>Strings</span><input type="color" id="c_s"><input type="text" id="t_s">
      <span>Propiedades</span><input type="color" id="c_prop"><input type="text" id="t_prop">
    </div>
    <div style="margin-top:10px;display:flex;gap:6px;">
      <button id="leir-live-copy" style="flex:1;background:#89b4fa;border:0;color:#11111b;padding:7px 8px;border-radius:6px;cursor:pointer;font-weight:600;">Copiar CSS</button>
      <button id="leir-live-export" style="flex:1;background:#a6e3a1;border:0;color:#11111b;padding:7px 8px;border-radius:6px;cursor:pointer;font-weight:600;">Ver CSS</button>
      <button id="leir-live-reset" style="background:#313244;border:0;color:#cdd6f4;padding:7px 10px;border-radius:6px;cursor:pointer;">Reset</button>
    </div>
    <pre id="leir-live-cssout" style="margin-top:8px;background:#11111b;border:1px solid #313244;border-radius:6px;padding:8px;white-space:pre-wrap;word-break:break-all;font-size:11px;max-height:160px;overflow:auto;display:none;"></pre>
  </div>`;
  document.body.appendChild(panel);
  let liveStyle=document.getElementById('leir-live-style');
  if(!liveStyle){ liveStyle=document.createElement('style'); liveStyle.id='leir-live-style'; document.head.appendChild(liveStyle); }
  const $=id=>document.getElementById(id);
  const inputs=[['c_names','t_names'],['c_base','t_base'],['c_kw','t_kw'],['c_kt','t_kt'],['c_n','t_n'],['c_param','t_param'],['c_p','t_p'],['c_s','t_s'],['c_prop','t_prop']];
  const map={c_names:'.sig .sig-name.descname .n .pre',c_base:'.sig a.reference.internal .n .pre',c_kw:'.sig .k, .sig .k .pre',c_kt:'.sig .kt, .sig .kt .pre',c_n:'.sig .n:not(.sig-param) .pre',c_param:'.sig .n.sig-param .pre',c_p:'.sig .p, .sig .p .pre',c_s:'.sig .s, .sig .s .pre, .sig .str, .sig .str .pre',c_prop:'.sig.c-property .sig-name.descname .n .pre'};
  const templates={custom:{c_names:'#D09EF9',c_base:'#D09EF9',c_kw:'#66d9ef',c_kt:'#f262bf',c_n:'#cdd6f4',c_param:'#f38ba8',c_p:'#ff4689',c_s:'#85d6ad',c_prop:'#D09EF9'},monokai:{c_names:'#a6e22e',c_base:'#a6e22e',c_kw:'#66d9ef',c_kt:'#66d9ef',c_n:'#f8f8f2',c_param:'#f8f8f2',c_p:'#f92672',c_s:'#e6db74',c_prop:'#a6e22e'},dracula:{c_names:'#bd93f9',c_base:'#bd93f9',c_kw:'#ff79c6',c_kt:'#8be9fd',c_n:'#f8f8f2',c_param:'#ffb86c',c_p:'#ff79c6',c_s:'#f1fa8c',c_prop:'#bd93f9'},'github-dark':{c_names:'#d2a8ff',c_base:'#d2a8ff',c_kw:'#ff7b72',c_kt:'#79c0ff',c_n:'#c9d1d9',c_param:'#ffa657',c_p:'#8b949e',c_s:'#a5d6ff',c_prop:'#d2a8ff'},native:{c_names:'#ffffff',c_base:'#ffffff',c_kw:'#6ab825',c_kt:'#6ab825',c_n:'#ffffff',c_param:'#bbbbbb',c_p:'#aaaaaa',c_s:'#ed9d13',c_prop:'#ffffff'},'one-dark':{c_names:'#61afef',c_base:'#61afef',c_kw:'#c678dd',c_kt:'#56b6c2',c_n:'#abb2bf',c_param:'#e06c75',c_p:'#abb2bf',c_s:'#98c379',c_prop:'#e5c07b'},'solarized-dark':{c_names:'#268bd2',c_base:'#268bd2',c_kw:'#859900',c_kt:'#2aa198',c_n:'#839496',c_param:'#cb4b16',c_p:'#586e75',c_s:'#2aa198',c_prop:'#b58900'},'pydata-monokai':{c_names:'#f8f8f2',c_base:'#f8f8f2',c_kw:'#66d9ef',c_kt:'#66d9ef',c_n:'#f8f8f2',c_param:'#f8f8f2',c_p:'#ff4689',c_s:'#e6db74',c_prop:'#f8f8f2'}};
  function hexToColorInput(v){return /^#[0-9a-fA-F]{6}$/.test(v)?v.toLowerCase():'#ffffff';}
  function apply(colors){ inputs.forEach(function(p){ const c=p[0],t=p[1]; const v=colors[c]||'#ffffff'; $(c).value=hexToColorInput(v); $(t).value=v; }); render(); }
  function render(){ let css=''; inputs.forEach(function(p){ const c=p[0],t=p[1]; let col=$(t).value.trim()||$(c).value; if(col[0]!=='#') col='#'+col; if(!/^#[0-9a-fA-F]{6}$/.test(col)) return; if(c==='c_names'){ css+='.sig .sig-name.descname .n .pre { color: '+col+' !important; }\n'; css+='.sig .sig-name.descname .n { color: '+col+' !important; }\n'; } else if(c==='c_base'){ css+='.sig a.reference.internal .n .pre { color: '+col+' !important; }\n'; css+='.sig a.reference.internal .n { color: '+col+' !important; }\n'; } else { css+=map[c]+' { color: '+col+' !important; }\n'; } }); if(true){ const propCol=$('t_prop').value; if(/^#[0-9a-fA-F]{6}$/.test(propCol)) css+='.sig.c-property .sig-name.descname .n .pre { color: '+propCol+' !important; }\n'; } liveStyle.textContent=css; const o=$('leir-live-cssout'); if(o&&o.style.display!=='none') o.textContent=css; }
  inputs.forEach(function(p){ const c=p[0],t=p[1]; $(c).addEventListener('input',function(){ $(t).value=$(c).value; render(); }); $(t).addEventListener('input',function(){ const v=$(t).value.trim(); if(/^#[0-9a-fA-F]{6}$/.test(v)) $(c).value=v.toLowerCase(); render(); }); });
  $('leir-live-template').addEventListener('change',function(e){ const k=e.target.value; if(templates[k]) apply(templates[k]); });
  $('leir-live-reset').addEventListener('click',function(){ apply(templates.custom); $('leir-live-template').value='custom'; });
  const panelEl=document.getElementById('leir-live-panel');
  function setExpanded(exp){
    panelEl.classList.toggle('minimized',!exp);
    $('leir-live-setup-btn').style.display=exp?'none':'';
    $('leir-live-close2').style.display=exp?'':'none';
    $('leir-live-expand').style.display='none';
  }
  $('leir-live-setup-btn').addEventListener('click',function(){ setExpanded(true); });
  $('leir-live-title').addEventListener('click',function(){ setExpanded(true); });
  $('leir-live-close2').addEventListener('click',function(){ setExpanded(false); });
  $('leir-live-copy').addEventListener('click',async function(){ const css=liveStyle.textContent||''; try{ await navigator.clipboard.writeText(css); const b=$('leir-live-copy'); const old=b.textContent; b.textContent='Copiado!'; setTimeout(function(){b.textContent=old;},1200);}catch(e){ const o=$('leir-live-cssout'); o.style.display='block'; o.textContent=css; }});
  $('leir-live-export').addEventListener('click',function(){ const o=$('leir-live-cssout'); o.style.display=o.style.display==='none'?'block':'none'; if(o.style.display==='block'){ o.textContent='/* leir_theme */\n'+liveStyle.textContent; }});
  apply(templates.custom);
  setExpanded(false);
  }
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }
})();
