#!/usr/bin/env python3
"""Generate a runtime JSON reader for original ground profiles/states (computer-rider seeds).

The human's grid seed is compiled in (tools/generate_event_seed.py). Computer riders are loaded at
run time from web/public/assets/<course>/npc-riders.json, so this emits the same field mapping
(engine/replay_io.mm, the one mapping every ground seed uses) as nlohmann::json readers instead of
literals: web/generated/npc_seed_reader.hpp.
"""
import re
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]


def generate(output):
    source = (ROOT / 'engine/replay_io.mm').read_text()
    lines = ['#pragma once', '#include "ground_motion.hpp"', '#include "json.hpp"', '#include <array>',
             'namespace ssx {',
             'inline GroundCurve browserJsonCurve(const nlohmann::json& v){GroundCurve c;if(v.size()!=c.size())throw std::runtime_error("Ground curve size");'
             'for(size_t i=0;i<c.size();++i)c[i]={v.at(i).at(0).get<float>(),v.at(i).at(1).get<float>()};return c;}',
             'inline GroundControlValue browserJsonControl(const nlohmann::json& v){return {v.at("current").get<float>(),v.at("rate").get<float>(),v.at("target").get<float>()};}',
             '// engine/replay_io.mm field mapping (original ground profile JSON from tools/reference_ground_profile.py).',
             'inline OriginalGroundProfile browserJsonGroundProfile(const nlohmann::json& p){OriginalGroundProfile profile;',
             'const auto& surface=p.at("surface");const auto& h=p.at("heading_profile");']
    fragment = source[source.index('ssx::OriginalGroundProfile profile;'):source.index('auto state=readOriginalPoseState(s);')]
    fields = re.findall(r'profile\.([\w.]+)=(value|curve)\((\w+),@"([^"]+)"\)', fragment)
    if len(fields) < 20:
        raise ValueError('Ground profile mapping changed')
    for field, kind, obj, key in fields:
        source_object = {'p': 'p', 'surface': 'surface', 'h': 'h'}[obj]
        reader = f'browserJsonCurve({source_object}.at("{key}"))' if kind == 'curve' else f'{source_object}.at("{key}").get<float>()'
        lines.append(f'profile.{field}={reader};')
    lines += ['profile.surface.id=surface.at("id").get<int>();',
              'profile.speedLimitTable=p.at("speed_limit_table").get<decltype(profile.speedLimitTable)>();',
              'return profile;}',
              'inline OriginalGroundState browserJsonGroundState(const nlohmann::json& s){OriginalGroundState state;']
    fragment = source[source.index('static ssx::OriginalGroundState readOriginalPoseState'):source.index('void initializeNativeReplay')]
    count = 0
    for field, kind, key in re.findall(r'state\.(\w+)=(vec|value|boolean)\(s,@"([^"]+)"\)', fragment):
        getter = {'vec': 'std::array<float,3>', 'value': 'float', 'boolean': 'bool'}[kind]
        lines.append(f'state.{field}=s.at("{key}").get<{getter}>();'); count += 1
    for field, key in re.findall(r'state\.(\w+)=control\(@"([^"]+)"\)', fragment):
        lines.append(f'state.{field}=browserJsonControl(s.at("{key}"));'); count += 1
    for field, kind, key in re.findall(r'state\.(\w+)=(signedInteger|unsigned32)\(s\[@"([^"]+)"\]', fragment):
        getter = 'uint32_t' if kind == 'unsigned32' else 'int64_t'
        lines.append(f'state.{field}=decltype(state.{field})(s.at("{key}").get<{getter}>());'); count += 1
    if count < 30:
        raise ValueError('Ground state mapping changed')
    lines += ['state.quaternion=s.at("quaternion").get<std::array<float,4>>();', 'return state;}', '}']
    Path(output).write_text('\n'.join(lines) + '\n')


if __name__ == '__main__':
    generate(ROOT / 'web/generated/npc_seed_reader.hpp')
