// CPU copies of GPU data that the page drops once the GPU holds them (memory), and how to get them back for a new GPU device.
//
// three.js uploads a geometry's attribute arrays and a texture's texels once; after that only a new device (web/gpu-recovery.js:
// iOS Safari loses it in the background) reads them again. A module that drops such arrays registers a restore here; the device
// recovery awaits restoreGpuCopies() after the new device exists and before drawing resumes (pv gpuRestore), so the re-upload finds
// the data. The restores re-read the package files (HTTP cache, else the network: an evicted cache on iOS) or re-decode kept
// archives. An owner that has left the scene (course change, released location) is forgotten.
const entries = new Map(); // owner (Object3D) -> [async restore()]
export function registerGpuRestore(owner, restore) { if (!entries.has(owner)) entries.set(owner, []); entries.get(owner).push(restore); }
export function forgetGpuRestore(owner) { entries.delete(owner); }
export const gpuRestoreCount = () => [...entries.values()].reduce((n, l) => n + l.length, 0);
const attached = (o) => { for (let p = o; p; p = p.parent) if (p.isScene) return true; return false; };
export async function restoreGpuCopies() {
  const jobs = [];
  for (const [owner, list] of entries) { if (!attached(owner)) { entries.delete(owner); continue; } for (const restore of list) jobs.push(restore()); }
  await Promise.all(jobs);
  return jobs.length;
}
export async function fetchBuffer(url) {
  const r = await fetch(url);
  if (!r.ok) throw Error(`${url}: HTTP ${r.status}`);
  return r.arrayBuffer();
}

// pv gpuRelease: drop a world package's CPU copies once the GPU holds them (the vertex data shared by its batches, the colour / light-UV /
// vertex-alpha attributes, each uploaded batch's index array, each uploaded texture's texels), keeping whatever is not uploaded yet (a
// batch nothing has drawn or compiled: flag and MeshAnim pieces added later), and register the restore. Uploaded = three's backend
// holds a GPU buffer / texture for it (WebGPU; the WebGL fallback keeps everything). w: {group, backend, root, inter, colors, light,
// alpha, lightUvFrom, textureEntries: Map(source -> package entry), indices}. Returns the bytes released.
export function releaseWorldCopies(w) {
  const data = w.backend?.data; if (!(data instanceof WeakMap) || !w.backend.isWebGPUBackend) return 0;
  const onGpu = (x) => !!x && !!data.get(x)?.buffer, texOnGpu = (t) => !!data.get(t)?.texture;
  let bytes = 0; const drop = (holder, key = 'array') => { bytes += holder[key].byteLength; holder[key] = new holder[key].constructor(0); };
  const r = { inter: false, colors: false, light: [], alpha: [], indices: [], sources: [] };
  if (w.inter?.array.length && onGpu(w.inter)) { r.inter = true; r.count = w.inter.count; drop(w.inter); }
  if (w.colors?.array.length && onGpu(w.colors)) { r.colors = true; drop(w.colors); }
  for (const a of w.light || []) if (a.array.length && onGpu(a)) { r.light.push(a); drop(a); }
  for (const a of w.alpha || []) if (a.array.length && onGpu(a)) { r.alpha.push(a); drop(a); }
  const meshes = []; w.group.traverse((o) => { if (o.isMesh) meshes.push(o); }); for (const m of w.group.userData.hiddenMeshes || []) meshes.push(m);
  // a batch's own index copy, or the package's shared index buffer (pv sharedIndex: the whole indices.bin, once)
  if (w.indices) for (const m of meshes) { const ix = m.geometry?.index, first = ix?.ssxShared ? 0 : m.userData.indexFirst; if (first === undefined || !ix?.array.length || !onGpu(ix)) continue; r.indices.push([ix, first, ix.array.length]); drop(ix); }
  // textures: by source (wrap-mode clones share it); every texture drawing a source must be uploaded
  const bySource = new Map(); const see = (t) => { if (!t?.isTexture || !t.source) return; if (!bySource.has(t.source)) bySource.set(t.source, new Set()); bySource.get(t.source).add(t); };
  for (const m of meshes) for (const mat of [].concat(m.material || [])) { if (!mat) continue; for (const k in mat) see(mat[k]); for (const t of Object.values(mat.userData?.ps2Textures || {})) see(t); }
  for (const [src, set] of bySource) {
    const entry = w.textureEntries?.get(src), img = src.data; if (!entry || !img?.data?.byteLength) continue;
    if (![...set].every(texOnGpu)) continue;
    bytes += img.data.byteLength; const stub = { width: img.width, height: img.height, data: null }; src.data = stub; r.sources.push([src, entry, stub]);
  }
  if (!bytes) return 0;
  const { root, lightUvFrom } = w;
  registerGpuRestore(w.group, async () => {
    const need = (f) => (f ? fetchBuffer(root + f) : null);
    const [vb, cb, ab, ib] = await Promise.all([need(r.inter || r.light.length ? 'vertices.bin' : null), need(r.colors ? 'colors.bin' : null), need(r.alpha.length ? 'vertex-alpha.bin' : null), need(r.indices.length ? 'indices.bin' : null)]);
    if (vb) { const v = new Float32Array(vb); if (r.inter) { if (v.length !== r.count * w.inter.stride) throw Error(root + 'vertices.bin changed'); w.inter.array = v; } if (r.light.length) { const uv = lightUvFrom(v); for (const a of r.light) a.array = uv; } }
    if (cb) w.colors.array = new Float32Array(cb);
    if (ab) { const al = Float32Array.from(new Uint8Array(ab), (x) => x / 128); for (const a of r.alpha) a.array = al; }
    if (ib) { const all = new Uint32Array(ib); for (const [ix, first, n] of r.indices) ix.array = all.slice(first, first + n); }
    const { packageTexels } = await import('./texture-archive.js');
    await Promise.all(r.sources.map(async ([src, entry, stub]) => { if (src.data !== stub) return; const t = await packageTexels(root, entry); src.data = { width: t.width, height: t.height, data: t.data }; }));
  });
  return bytes;
}
