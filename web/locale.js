// Original locale lookups (FEAMER / OVAMER / CMNAMER, exported by tools/export_career.py).
// LOC files key their UTF-16 texts by the ELF-hash of the kT_ id string (tools/sam_ps2/loc_file.py name_hash);
// ids that the executable still names are looked up by name, the rest by their 32-bit hash.
export function nameHash(name){
 let h=0;
 for(let i=0;i<name.length;i++){h=((h<<4)+name.charCodeAt(i))>>>0;const g=h&0xf0000000;if(g)h=(h^(g>>>23)^g)>>>0;}
 return h>>>0;
}
const hex=h=>(h>>>0).toString(16).padStart(8,'0');
export class Locale {
 constructor(tables={}){this.tables=tables;}
 // key: 'kT_...' id, or a numeric hash. Files are searched overlay, front end, common.
 text(key,fallback=''){
  const h=hex(typeof key==='number'?key:nameHash(key));
  for(const file of ['OVAMER','FEAMER','CMNAMER']){const t=this.tables[file]?.[h];if(t!==undefined)return clean(t);}
  return fallback;
 }
 has(key){const h=hex(typeof key==='number'?key:nameHash(key));return ['OVAMER','FEAMER','CMNAMER'].some(f=>this.tables[f]?.[h]!==undefined);}
}
// '\\' (written as a doubled backslash in the LOC source) is the original line break; typographic quotes map to ASCII
// glyphs present in the bitmap fonts.
export function clean(text){return text.replace(/\\\\/g,'\n').replace(/[‘’]/g,"'").replace(/[“”]/g,'"').replace(/–|—/g,'-').replace(/…/g,'...');}
// printf-style %s / %d / %S substitution used by the original strings.
export function format(text,...args){let i=0;return text.replace(/%[sdS]/g,()=>String(args[i++]??''));}
