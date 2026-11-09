/* Offline-only browser model. Mirrors include/dashboard_model.h design.
   Runs from file:// without a build server. No network/Bluetooth/OBD access. */
(function (root) {
  'use strict';
  var names = [
    ['ENGINE RPM', 'rpm'], ['SPEED', 'speed'], ['COOLANT', 'temperature'],
    ['ECU VOLT', 'V'], ['ENGINE LOAD', '%'], ['THROTTLE', '%'],
    ['INTAKE AIR', 'temperature'], ['FUEL LEVEL', '%'], ['MAP ABS', 'kPa']
  ];
  var defaults = [[0,1,2,3],[4,5,6,7]];
  var UPDATE = 650, DELAYED = 1500, STALE = 3500;
  function create() {
    return {
      page: 0, layout: 'grid', units: 'imperial', scenario: 'idle',
      paused: false, slots: defaults.map(function(a) {return a.slice();}),
      samples: names.map(function() {return {raw: null, at: null};}),
      lastTick: null
    };
  }
  function validIndex(i) {return Number.isInteger(i) && i >= 0 && i < names.length;}
  function choose(state,page,slot,id) {
    if (!validIndex(id) || page < 0 || page > 1 || slot < 0 || slot > 3) return false;
    state.slots[page][slot] = id; return true;
  }
  function cycle(state,slot) {
    if (!Number.isInteger(slot) || slot < 0 || slot > 3) return;
    state.slots[state.page][slot] = (state.slots[state.page][slot] + 1) % names.length;
  }
  function tick(state,now) {
    if (state.paused || (state.lastTick !== null && now - state.lastTick < UPDATE)) return false;
    state.lastTick = now;
    var step = Math.floor(now / UPDATE) >>> 0;
    var rpm, speed, load, throttle, map;
    if (state.scenario === 'idle') {
      rpm=800+20*(step%7); speed=0; load=14+2*(step%6);
      throttle=11+step%4; map=39+step%5;
    } else if (state.scenario === 'cruise') {
      rpm=1700+33*(step%12); speed=89+step%6; load=38+2*(step%8);
      throttle=23+step%9; map=58+step%14;
    } else {
      rpm=1200+155*(step%25); speed=8+5*(step%21); load=45+2*(step%22);
      throttle=30+3*(step%22); map=65+3*(step%20);
    }
    var vals=[rpm,speed,84+step%11,13.7+.1*(step%3),load,throttle,24+step%12,73-step%8,map];
    vals.forEach(function(value,i) {state.samples[i] = {raw:value,at:now};});
    return true;
  }
  function freshness(state,id,now) {
    var data=state.samples[id];
    if (!data || data.at === null) return 'missing';
    var age=Math.max(0,now-data.at);
    return age>=STALE?'stale':age>=DELAYED?'delayed':'fresh';
  }
  function unit(state,id) {
    var kind = names[id][1];
    if (kind==='speed') return state.units==='imperial'?'mph':'km/h';
    if (kind==='temperature') return state.units==='imperial'?'F':'C';
    return kind;
  }
  function converted(state,id) {
    var sample=state.samples[id], raw=sample&&sample.raw;
    if (raw===null||raw===undefined) return null;
    if (state.units==='imperial' && id===1) return raw*.621371192;
    if (state.units==='imperial' && (id===2||id===6)) return raw*1.8+32;
    return raw;
  }
  function display(state,id) {
    var val=converted(state,id);
    return val===null?'--':val.toFixed(id===3?1:0);
  }
  function restore(state,saved) {
    if (!saved || typeof saved!=='object') return;
    state.units = saved.units==='metric'?'metric':'imperial';
    state.layout = saved.layout==='focus'?'focus':'grid';
    state.scenario = ['idle','cruise','accelerate'].indexOf(saved.scenario)>=0?saved.scenario:'idle';
    state.page=saved.page===1?1:0;
    if (Array.isArray(saved.slots) && saved.slots.length===2)
      saved.slots.forEach(function(arr,p) {
        if (Array.isArray(arr) && arr.length===4)
          arr.forEach(function(id,s) {choose(state,p,s,id);});
      });
    // Intentionally do not restore samples, stale timestamps, or paused state.
  }
  function exportSettings(state) {
    return {
      units:state.units,layout:state.layout,scenario:state.scenario,
      page:state.page,slots:state.slots.map(function(a){return a.slice();})
    };
  }
  var api={
    names:names, create:create, choose:choose, cycle:cycle,
    tick:tick, freshness:freshness, unit:unit, converted:converted,
    display:display, restore:restore, exportSettings:exportSettings,
    UPDATE:UPDATE, DELAYED:DELAYED, STALE:STALE
  };
  if (typeof module!=='undefined' && module.exports) module.exports=api;
  root.DashboardModel=api;
})(typeof globalThis!=='undefined'?globalThis:this);
