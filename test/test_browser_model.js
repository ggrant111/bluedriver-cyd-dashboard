// Node regression tests for browser-only dashboard model.
// Run: node test/test_browser_model.js
'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const M = require('../simulator/model.js');

const model = M.create();
assert.equal(M.names.length,9);
assert.deepEqual(model.slots,[[0,1,2,3],[4,5,6,7]]);
assert.equal(M.freshness(model,0,0),'missing');
assert.equal(M.tick(model,0),true);
assert.equal(M.tick(model,100),false);
assert.equal(M.display(model,1),'0');
assert.equal(M.unit(model,1),'mph');
assert.equal(M.tick(model,650),true);
assert.equal(M.freshness(model,0,2149),'fresh');
assert.equal(M.freshness(model,0,2150),'delayed');
assert.equal(M.freshness(model,0,4150),'stale');

model.paused=true;
assert.equal(M.tick(model,10000),false);
assert.equal(M.freshness(model,0,10000),'stale');
model.paused=false;
assert.equal(M.tick(model,10000),true);
assert.equal(M.freshness(model,0,10000),'fresh');
model.scenario='cruise';
assert.equal(M.tick(model,10700),true);
assert.ok(model.samples[1].raw>80);
const cruiseSpeed=model.samples[1].raw;
assert.equal(M.unit(model,1),'mph');
assert.ok(M.converted(model,1)<cruiseSpeed);
model.units='metric';
assert.equal(M.converted(model,1),cruiseSpeed);
assert.equal(M.unit(model,1),'km/h');
const rawCoolant=model.samples[2].raw;
model.units='imperial';
assert.equal(M.converted(model,2),rawCoolant*1.8+32);
model.page=1;M.cycle(model,0);
assert.equal(model.slots[1][0],5);
assert.equal(M.choose(model,1,0,7),true);
assert.equal(M.choose(model,1,0,9),false);
assert.equal(model.slots[1][0],7);
model.layout='focus';
const settings=M.exportSettings(model);
const restored=M.create();
M.restore(restored,settings);
assert.equal(restored.layout,'focus');
assert.equal(restored.slots[1][0],7);
assert.equal(restored.samples[0].at,null); // never restore data as live
assert.equal(restored.paused,false);
M.restore(restored,{units:'nonsense',layout:'INVALID',scenario:'oops',page:999,
  slots:[[200,-1,'bad',0],[0,0,0,0]]});
assert.equal(restored.units,'imperial');
assert.equal(restored.layout,'grid');
assert.equal(restored.scenario,'idle');
assert.equal(restored.page,0);
assert.equal(restored.slots[0][0],0); // invalid saved metric not accepted
const html=fs.readFileSync('simulator/index.html','utf8');
const app=fs.readFileSync('simulator/app.js','utf8');
assert.match(html,/width:320px;height:240px/);
assert.match(html,/SYNTHETIC TELEMETRY ONLY/);
assert.match(html,/model\.js/);
assert.match(html,/app\.js/);
assert.match(app,/data-action/);
assert.match(app,/SIM/);
assert.doesNotMatch(app,/navigator\.bluetooth|fetch\(|XMLHttpRequest/);
console.log('PASS: browser simulator model, saved settings, stale detection, units, offline guardrails');
