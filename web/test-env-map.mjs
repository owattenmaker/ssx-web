// pv envMap (web/world-material.js envPassMaterial; docs/visual-parity.md 43): the static-model env-map second pass.
// 1. From the code and data: ALPHA enums 2 / 17 (table 0x491FB0 -> 0x68 FIX 128 = Cs + Cd, 0x58 = Cs x Ad + Cd), VU1 program 3's UV mode 256
//    (0xCE8: xy = rows 10..13 x (n, 1)), the matrix at 0x504760 and the global gp+0x1404 in a race savestate, when present.
// 2. The packages: env batches are static-model instances whose second texture is in the texture table (after the re-export).
// 3. The material: an additive pass (One / One, alpha kept) without depth writes; mode 0x600000 reads the base texture's alpha.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
const web = path.dirname(fileURLToPath(import.meta.url)), root = path.resolve(web, '..');

// ---- 1. code / data ----
const elfPath = path.join(root, 'local/disc/SLUS_207.72');
if (fs.existsSync(elfPath)) {
  const elf = fs.readFileSync(elfPath), ph = elf.readUInt32LE(0x1c), n = elf.readUInt16LE(0x2c);
  const segs = Array.from({ length: n }, (_, i) => [0, 1, 2, 3, 4, 5].map((k) => elf.readUInt32LE(ph + i * 32 + k * 4)));
  const word = (va) => { for (const [, o, v, , fs_] of segs) if (va >= v && va < v + fs_) return elf.readUInt32LE(o + va - v); return null; };
  assert.equal(word(0x491fb0 + 2 * 4), 0x362568, 'ALPHA enum 2 -> 0x362568'); assert.equal(word(0x491fb0 + 17 * 4), 0x362588, 'ALPHA enum 17 -> 0x362588');
  assert.equal(word(0x362568), 0x24020080, 'enum 2: li v0, 128 (FIX)'); assert.equal(word(0x362570), 0x34420068, 'enum 2: ori 0x68 (A Cs, B 0, C FIX, D Cd)');
  assert.equal(word(0x362590), 0x24020058, 'enum 17: 0x58 (A Cs, B 0, C Ad, D Cd)');
  assert.equal(word(0x37f82c) & 0xffff, 0x100, '0x200000 case: ALPHA_2 enum 2 (word1 bits 7..11)'); assert.equal(word(0x37f8fc) & 0xffff, 0x880, '0x600000 case: ALPHA_2 enum 17');
  console.log('ELF: ALPHA_2 enum 2 = Cs x FIX(128) + Cd, enum 17 = Cs x Ad + Cd; the env cases of 37F2A4 set them');
} else console.log('skip ELF checks (local/disc/SLUS_207.72 not present)');
const prog = path.join(root, 'local/rider-lighting/vu/program3.bin');
if (fs.existsSync(prog)) {
  const r = spawnSync('python3', [path.join(root, 'tools/vudis.py'), prog, '0x2078', '0x20a8'], { encoding: 'utf8' }), mode = spawnSync('python3', [path.join(root, 'tools/vudis.py'), prog, '0xce8', '0xd40'], { encoding: 'utf8' });
  assert.match(r.stdout, /iaddiu vi4, vi0, 256[\s\S]*ibeq vi3, vi4, 0xce8/, 'UV mode 256 of the second texture -> 0xCE8');
  assert.match(mode.stdout, /lqi\.xyz vf10, \(vi13\+\+\)[\s\S]*itof15\.xyz vf10, vf10[\s\S]*mulax\.xy ACC\.xy, vf5, vf10x[\s\S]*madday\.xy ACC\.xy, vf6, vf10y[\s\S]*maddaz\.xy ACC\.xy, vf7, vf5z[\s\S]*maddw\.xy vf9, vf8, vf0w/, '0xCE8: uv = rows 10..13 x (n, 1)');
  console.log('VU1 program 3: UV mode 256 = rows 10..13 x (normal, 1)');
} else console.log('skip VU checks (program3.bin not present)');
const state = path.join(root, 'local/ps2-capture/runs/metro-event-race.p2s');
if (fs.existsSync(state)) {
  const py = `import zipfile,struct,sys\nee=zipfile.ZipFile(sys.argv[1]).read('eeMemory.bin')\nprint(list(struct.unpack_from('<16f',ee,0x504760)),struct.unpack_from('<I',ee,0x4A44F4)[0])`;
  const out = spawnSync('python3', ['-c', py, state], { encoding: 'utf8' }).stdout.trim().split(/\s(?=\d+$)/);
  assert.deepEqual(JSON.parse(out[0]), [0.5, 0, 0, 0, 0, -0.5, 0, 0, 0, 0, 0, 0, 0.5, 0.5, 1, 1], '0x504760: u = 0.5 n.x + 0.5, v = -0.5 n.y + 0.5');
  assert.equal(+out[1], 0, 'gp+0x1404 = 0: the env bits are kept (37F2A4)');
  console.log('RAM (metro-event-race): the UV matrix constant and the env switch as the port uses them');
} else console.log('skip RAM checks (metro-event-race.p2s not present)');

// ---- 2. packages ----
let envBatches = 0, packages = 0;
for (const dir of fs.readdirSync(path.join(web, 'public/assets'))) {
  const f = path.join(web, 'public/assets', dir, 'world.json'); if (!fs.existsSync(f) || dir.startsWith('RIDER_')) continue;
  const d = JSON.parse(fs.readFileSync(f)); packages++;
  for (const b of d.batches) {
    if (!b.env) continue; envBatches++;
    assert.equal(b.instance, true, `${dir}: env batch on terrain`); assert.ok([0x200000, 0x600000].includes(b.env[1]), `${dir}: env mode ${b.env[1]}`);
    assert.ok(d.textures['9-' + b.env[0]], `${dir}: env texture 9-${b.env[0]} missing from the texture table`);
  }
}
console.log(`packages: ${packages} checked, ${envBatches} env batches${envBatches ? '' : ' (not re-exported yet)'}`);

// ---- 3. the material (a fresh process) ----
const code = `globalThis.location={search:'',href:'http://x/'};
  const T=await import('three/webgpu'),W=await import('./world-material.js'),tsl=await import('three/tsl');
  const tex=()=>{const t=new T.DataTexture(new Uint8Array(4),1,1);t.needsUpdate=true;return t;};const textures={'9-50':tex()},map=tex();
  const add=W.envPassMaterial({instance:true,env:[50,0x200000]},map,textures,tsl.uv()),ad=W.envPassMaterial({instance:true,env:[50,0x600000]},map,textures,tsl.uv());
  const miss=W.envPassMaterial({instance:true,env:[51,0x200000]},map,textures,tsl.uv());
  const d=m=>({blending:m.blending,src:m.blendSrc,dst:m.blendDst,srcA:m.blendSrcAlpha,dstA:m.blendDstAlpha,transparent:m.transparent,depthWrite:m.depthWrite,combine:m.userData.originalWorldCombine});
  console.log(JSON.stringify({add:d(add),ad:d(ad),miss,key:W.envPassKey({instance:true,env:[50,0x200000]}),none:W.envPassKey({instance:true})}));`;
const p = spawnSync(process.execPath, ['--input-type=module', '-e', code], { cwd: web, encoding: 'utf8' });
assert.equal(p.status, 0, p.stderr);
const m = JSON.parse(p.stdout.trim().split('\n').pop());
const additive = { blending: 5, src: 201, dst: 201, srcA: 200, dstA: 201, transparent: true, depthWrite: false };
assert.deepEqual({ ...m.add, combine: undefined }, { ...additive, combine: undefined }); assert.equal(m.add.combine, 'instance-env-add'); assert.equal(m.ad.combine, 'instance-env-Ad');
assert.equal(m.miss, null, 'no second texture in the table: no pass'); assert.equal(m.key, ':e50_2097152'); assert.equal(m.none, '');
console.log('envPassMaterial: Cs + Cd / Cs x Ad + Cd additive pass, alpha kept, no depth writes');
