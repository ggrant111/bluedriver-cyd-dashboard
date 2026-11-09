(function () {
  'use strict';
  var M = window.DashboardModel;
  var state = M.create();
  var screen = document.getElementById('screen');
  var view = 'dashboard';
  var cachedTitle = '';
  var previousFreshness = '';
  var start = performance.now();
  try {
    var saved = localStorage.getItem('bluedriver-cyd-simulator-v1');
    if (saved) M.restore(state, JSON.parse(saved));
  } catch (_) { /* file:// private mode may block storage; simulator still works */ }

  function persist() {
    try {
      localStorage.setItem('bluedriver-cyd-simulator-v1',
                           JSON.stringify(M.exportSettings(state)));
    } catch (_) {}
  }
  function markChanged() {persist();syncControls();renderFrame();}
  function button(name,label) {
    return '<button data-action="'+name+'" type="button">'+label+'</button>';
  }
  function header(title) {
    return '<div class="hw-header"><span class="hw-title">'+title+
           '</span><span class="demo">DEMO</span></div>';
  }
  function footer(a,b,c,d) {
    return '<div class="hw-footer">'+button(a[0],a[1])+button(b[0],b[1])+
           button(c[0],c[1])+button(d[0],d[1])+'</div>';
  }
  function tile(i) {
    return '<button type="button" class="tile '+state.layout+'-'+i+(i===0?' main':'')+
           '" data-action="gauge'+i+'" title="Change this gauge" aria-label="Cycle gauge '+(i+1)+'">'+
           '<span class="name">'+M.names[state.slots[state.page][i]][0]+'</span>'+
           '<span class="num">--</span><span class="u"></span><span class="fresh"></span>'+
           '</button>';
  }
  function renderFrame() {
    var html='';
    if (view==='dashboard') {
      html=header('BRONCO / OBD-II')+
           '<div class="grid-area">'+[0,1,2,3].map(tile).join('')+'</div>'+
           footer(['page','PAGE'],['ble','BLE'],['settings','SETUP'],
                  ['layout',state.layout==='grid'?'FOCUS':'GRID']);
    } else if (view==='settings') {
      var opts=[
        ['UNITS',state.units==='imperial'?'MPH / F':'KMH / C','units'],
        ['LAYOUT',state.layout==='grid'?'2 X 2 GRID':'FOCUS','layout'],
        ['SCENARIO',state.scenario.toUpperCase(),'scenario'],
        ['SIM DATA',state.paused?'PAUSED':'RUNNING','pause']
      ];
      html=header('SIMULATOR SETUP')+'<div class="setup-tiles">'+
           opts.map(function(x){
             return '<button type="button" data-action="'+x[2]+'"><span class="setting-label">'+
                    x[0]+'</span><span class="setting-value">'+x[1]+'</span></button>';
           }).join('')+'</div>'+
           footer(['dashboard','DASH'],['ble','BLE'],['page','PAGE'],['reset','RESET']);
    } else {
      html=header('BLUEDRIVER / BLE')+
           '<div class="ble-notice"><strong>Scanner unavailable in browser demo</strong>'+
           'This offline preview does not access Bluetooth, a scanner, or any vehicle. '+
           'The CYD firmware retains read-only BLE discovery through its separate diagnostics screen.'+
           '</div>'+
           footer(['dashboard','DASH'],['scan','SCAN'],['scan','ALL'],['settings','SETUP']);
    }
    screen.innerHTML=html;
    cachedTitle=view;
    previousFreshness='';
    refresh();
  }
  function refresh() {
    if (view!=='dashboard') return;
    var now=performance.now()-start;
    M.tick(state,now);
    for (var i=0;i<4;i++) {
      var id=state.slots[state.page][i], data=state.samples[id];
      var card=screen.querySelector('[data-action="gauge'+i+'"]');
      if (!card) continue;
      card.classList.remove('stale','delayed');
      var status=M.freshness(state,id,now);
      if (status==='stale') card.classList.add('stale');
      if (status==='delayed') card.classList.add('delayed');
      card.querySelector('.num').textContent=M.display(state,id);
      card.querySelector('.u').textContent=M.unit(state,id);
      card.querySelector('.fresh').textContent=status==='fresh'?'SIM':
                    status==='delayed'?'LAG':status==='stale'?'STALE':'WAIT';
    }
  }
  function syncControls() {
    document.getElementById('scenario').value=state.scenario;
    document.getElementById('pause').textContent=state.paused?'Resume readings':'Pause readings';
    document.getElementById('units').textContent=
      state.units==='imperial'?'Switch to metric':'Switch to imperial';
    document.getElementById('layout').textContent=
      state.layout==='grid'?'Focus layout':'Grid layout';
    var idx=Number(document.getElementById('slot').value);
    document.getElementById('metric').value=String(state.slots[state.page][idx]);
    document.getElementById('status').innerHTML=
      '<strong>DEMO ONLY.</strong> '+state.scenario.toUpperCase()+' scenario · '+
      (state.paused?'updates paused; gauges go stale':'simulated updates every 650 ms')+
      ' · page '+(state.page+1)+'/2 · no live OBD connection.';
  }
  function run(action) {
    if (action==='dashboard') view='dashboard';
    else if (action==='ble') view='ble';
    else if (action==='settings') view='settings';
    else if (action==='page') state.page=(state.page+1)%2;
    else if (action==='layout') state.layout=state.layout==='grid'?'focus':'grid';
    else if (action==='units') state.units=state.units==='imperial'?'metric':'imperial';
    else if (action==='pause') state.paused=!state.paused;
    else if (action==='scenario') {
      var seq=['idle','cruise','accelerate'];
      state.scenario=seq[(seq.indexOf(state.scenario)+1)%3];
    }
    else if (action==='reset') {state=M.create();view='dashboard';}
    else if (action.indexOf('gauge')===0) {
      var idx=Number(action.slice(5));
      M.cycle(state,idx);
    }
    else if (action==='scan') {
      document.getElementById('status').innerHTML=
        '<strong>NO BLE.</strong> Browser simulator intentionally has no scanner access.';
    }
    markChanged();
  }
  screen.addEventListener('click',function(event){
    var target=event.target.closest('[data-action]');
    if (target) run(target.dataset.action);
  });
  ['pause','units','layout','next','reset','dash'].forEach(function(id){
    document.getElementById(id).addEventListener('click',function(){
      run(id==='next'?'page':id==='dash'?'dashboard':id);
    });
  });
  document.getElementById('scenario').addEventListener('change',function(e){
    state.scenario=e.target.value;markChanged();
  });
  document.getElementById('slot').addEventListener('change',syncControls);
  var metric = document.getElementById('metric');
  M.names.forEach(function(item,i){
    var option=document.createElement('option');
    option.value=String(i);option.textContent=item[0];metric.appendChild(option);
  });
  metric.addEventListener('change',function(e){
    M.choose(state,state.page,Number(document.getElementById('slot').value),
             Number(e.target.value));
    markChanged();
  });
  function resize() {
    var outer=document.querySelector('.preview');
    var scale=outer.clientWidth/320;
    screen.style.transform='scale('+scale+')';
  }
  window.addEventListener('resize',resize);
  syncControls();
  renderFrame();
  resize();
  setInterval(function(){
    var now=performance.now()-start;
    M.tick(state,now);
    refresh();
  },250);
})();
