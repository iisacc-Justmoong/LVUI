#!/usr/bin/env python3
"""Validate LVUI's offline design snapshot and generate its native token catalog."""
import argparse
import json
import math
import re
import sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
SNAPSHOT=ROOT/'catalog/lvui-style-tokens.json'
OUTPUT=ROOT/'src/Style/StyleTokens.generated.inc'
ALIASES=ROOT/'src/Style/StyleTokenAliases.generated.inc'
HEADER=ROOT/'src/Style/StyleTokens.h'
SEARCH_DEFAULTS=ROOT/'src/Style/SearchFieldTokens.generated.h'
SEARCH_MATERIAL_SOURCE=ROOT/'catalog/lvui-search-field-material.json'
SEARCH_MATERIAL=ROOT/'src/Style/SearchFieldMaterial.generated.h'
SEARCH_DISABLED_SOURCE=ROOT/'catalog/lvui-text-field-disabled-material.json'
SEARCH_CYLINDER_SOURCE=ROOT/'catalog/lvui-text-field-cylinder-geometry.json'
SEARCH_INLINE_SOURCE=ROOT/'catalog/lvui-text-field-inline-material.json'
SLIDER_TICKS_SOURCE=ROOT/'catalog/lvui-slider-ticks.json'
SLIDER_TICKS=ROOT/'src/Style/SliderTicks.generated.h'
SLIDER_CENTER_SOURCE=ROOT/'catalog/lvui-slider-center.json'
SLIDER_CENTER=ROOT/'src/Style/SliderCenter.generated.h'
SLIDER_FILLED_SOURCE=ROOT/'catalog/lvui-slider-filled.json'
SLIDER_FILLED=ROOT/'src/Style/SliderFilled.generated.h'
SLIDER_LABELED_SEGMENTED_SOURCE=ROOT/'catalog/lvui-slider-labeled-segmented.json'
SLIDER_SEGMENTED=ROOT/'src/Style/SliderSegmented.generated.h'
SLIDER_SYMBOL_SOURCE=ROOT/'catalog/lvui-slider-symbol.json'
SLIDER_SYMBOL=ROOT/'src/Style/SliderSymbol.generated.h'
SLIDER_ENDPOINT_SOURCE=ROOT/'catalog/lvui-slider-endpoints.json'
SLIDER_ENDPOINTS=ROOT/'src/Style/SliderEndpointTokens.generated.h'
ICON_BUTTON_SOURCE=ROOT/'catalog/lvui-icon-button-geometry.json'
ICON_BUTTON_DEFAULTS=ROOT/'src/Style/IconButtonDefaults.generated.h'
BUTTON_MOTION_SOURCE=ROOT/'catalog/lvui-button-motion.json'
BUTTON_MOTION_DEFAULTS=ROOT/'src/Style/ButtonMotionDefaults.generated.h'
DESIGN_ICON_SOURCE=ROOT/'catalog/lvui-design-icon-geometry.json'
DESIGN_ICON_DEFAULTS=ROOT/'src/Style/DesignIconDefaults.generated.h'
MATERIAL_RADIAL_SOURCE=ROOT/'catalog/lvui-material-radial.json'
MATERIAL_RADIAL_DEFAULTS=ROOT/'src/Style/MaterialRadialDefaults.generated.h'
SEGMENTED_SOURCE=ROOT/'catalog/lvui-segmented-geometry.json'
SEGMENTED_DEFAULTS=ROOT/'src/Style/SegmentedDefaults.generated.h'
BEGIN='// BEGIN GENERATED FIGMA COLOR DEFAULTS'
END='// END GENERATED FIGMA COLOR DEFAULTS'
TYPES={'FLOAT':'number','COLOR':'color','STRING':'string','BOOLEAN':'boolean'}

def member_name(text):
    words=re.findall(r'[A-Za-z0-9_]+',text)
    if not words:raise ValueError('Style member needs an explicit ASCII name: '+text)
    name=words[0][:1].lower()+words[0][1:]+''.join(w[:1].upper()+w[1:] for w in words[1:])
    return '_'+name if name[0].isdigit() else name

def selector(variable,collection):
    """Stable names depend on source paths, never on current leaf-name uniqueness."""
    kind=variable['type'];parts=variable['name'].split('/');name=collection['name']
    if kind=='COLOR':
        if name=='Theme Colors':
            if parts[0] in {'Panel','Action','Neutral'}:tail=[member_name(p) for p in parts[1:]]
            elif parts[0]=='Accent':tail=[member_name(parts[1])]+([] if parts[-1]=='Default' else [member_name(p) for p in parts[2:]])
            elif parts[0]=='Text':
                leaf=parts[-1]
                leaf='base' if leaf=='textTokenBase' else leaf[:-5] if leaf.endswith('Color') else leaf
                tail=['text',member_name(leaf)]
            else:tail=[member_name(p) for p in parts]
        elif name=='Material':tail=['material']+[member_name(p) for p in parts]
        elif name=='ColorPicker':tail=['picker',member_name(variable['name'].removesuffix(' color'))]
        else:tail=[member_name(name)]+[member_name(p) for p in parts]
        return 'Color.'+'.'.join(tail)
    if kind=='FLOAT':
        if name=='Theme Metrics':
            # Numeric leaves attach to their category: Space/8 -> space8.
            tail=member_name(parts[0])+''.join(p[:1].upper()+p[1:] for p in parts[1:])
        elif name=='Theme Typography':
            tail=member_name(parts[0])+parts[-1] if parts[0] in {'Weight','Tracking'} else member_name(parts[-1])
        else:tail=member_name(name)+''.join(p[:1].upper()+p[1:] for p in parts)
        return 'Metric.'+tail
    return ''

def selector_document(data):
    collections={c['id']:c for c in data['collections']}
    rows=sorted((selector(v,collections[v['collectionId']]),collections[v['collectionId']]['name']+'/'+v['name'])
                for v in data['variables'] if v['type'] in {'COLOR','FLOAT'})
    return ('# Color와 Metric 멤버 목록\n\n'
            '`tools/update_style_tokens.py`가 원본 Figma 스냅샷에서 생성하는 목록이다. '
            '각 멤버는 원본 토큰 ID를 참조하며 모드·별칭·앱별 값 변경을 유지한다.\n\n'
            '| 멤버 | Figma 컬렉션 / 변수 |\n|---|---|\n'+
            ''.join('| `'+name+'` | '+source+' |\n' for name,source in rows))

def literal(value):
    if isinstance(value,bool):return 'true' if value else 'false'
    if isinstance(value,(float,int)):
        if not math.isfinite(value):raise ValueError('Non-finite token')
        return repr(float(value))
    if not isinstance(value,str):raise ValueError('Unsupported C++ literal')
    # UTF-8 bytes avoid invalid C++ surrogate escapes for non-BMP Figma text.
    result='"'
    for byte in value.encode('utf-8'):
        if byte in (34,92):result+='\\'+chr(byte)
        elif 32<=byte<=126:result+=chr(byte)
        else:result+='\\'+format(byte,'03o')
    return result+'"'

def identity_aliases(item):
    aliases=[]
    for field in ('legacyAliases','sourceAliases'):
        values=item.get(field,[])
        if not isinstance(values,list) or any(not isinstance(v,str) or not v for v in values):
            raise ValueError('Invalid token identity aliases')
        aliases.extend(values)
    if len(set(aliases))!=len(aliases):raise ValueError('Duplicate token identity alias')
    return aliases

def default_color_name(collection, name):
    def snake(part):
        part=re.sub(r'([a-z0-9])([A-Z])',r'\1_\2',part)
        return re.sub(r'[^a-zA-Z0-9]+','_',part).strip('_').lower()
    prefix='' if collection=='Theme Colors' else snake(collection)+'_'
    return prefix+'_'.join(snake(part) for part in name.split('/'))

def default_color_header(data, current):
    """Compile immutable native widget defaults from the same Figma color snapshot."""
    ids=set(data['colorReadback']['variableIds'])
    for capture in data['colorReadback'].get('additionalReadbacks', []):
        if not isinstance(capture,dict) or not isinstance(capture.get('variableIds'),list):
            raise ValueError('Invalid additional color readback')
        ids.update(capture['variableIds'])
    collections={c['id']:c for c in data['collections']}
    variables=[v for v in data['variables'] if v['id'] in ids]
    if len(variables)!=len(ids):raise ValueError('Missing captured color default')
    lines=[BEGIN,'namespace figma_defaults {',
           '[[nodiscard]] constexpr StyleColor with_alpha(StyleColor color, double alpha) noexcept {',
           '    color.alpha = alpha; return color;', '}']
    names=set()
    for v in sorted(variables,key=lambda item:(collections[item['collectionId']]['name'],item['name'])):
        collection=collections[v['collectionId']]
        value=v['valuesByMode'][collection['defaultModeId']]
        if v['type']!='COLOR' or not isinstance(value,dict) or set(value)!={'r','g','b','a'}:
            raise ValueError('A native color default must be a direct RGBA value: '+v['name'])
        name=default_color_name(collection['name'],v['name'])
        if name in names:raise ValueError('Duplicate native color default '+name)
        names.add(name)
        lines.append('inline constexpr StyleColor '+name+'{'+', '.join(literal(value[k]) for k in ('r','g','b','a'))+'};')
    lines+=['} // namespace figma_defaults',END]
    if current.count(BEGIN)!=1 or current.count(END)!=1:raise ValueError('Missing generated header markers')
    before,tail=current.split(BEGIN,1)
    _,after=tail.split(END,1)
    return before+'\n'.join(lines)+after

def validate_inline_snapshot(snapshot):
    states=snapshot.get('verifiedStates');modes=snapshot.get('verifiedModes')
    enabled=snapshot.get('enabled');disabled=snapshot.get('disabled')
    if (snapshot.get('schema')!=1 or snapshot.get('reference')!='TextField/Inline' or snapshot.get('cornerRadius')!=0 or
            not isinstance(modes,list) or len(modes)!=2 or any(not isinstance(mode,str) for mode in modes) or set(modes)!={'Text','Search'} or
            not isinstance(states,list) or len(states)!=6 or any(not isinstance(state,str) for state in states) or
            set(states)!={'Enabled','Disabled','Typing','Dragged','CursorActive','KeyboardFocus'} or
            not isinstance(enabled,dict) or not isinstance(disabled,dict) or enabled.get('strokes')!=[] or
            disabled.get('strokes')!=[] or disabled.get('effects')!=[] or enabled.get('fillGradient')!=disabled.get('fillGradient')):
        raise ValueError('Invalid inline text field snapshot')
    for value in (enabled,disabled):
        opacity=value.get('backgroundOpacity')
        if (not isinstance(value.get('backgroundVariableId'),str) or
                not isinstance(opacity,(int,float)) or isinstance(opacity,bool) or not math.isfinite(opacity) or not 0<=opacity<=1):
            raise ValueError('Invalid inline text field background')
    return enabled,disabled

def search_field_defaults(data, material=None, cylinder=None, inline=None):
    """Compile native search geometry and colors without a runtime catalog dependency."""
    if material is None:material=json.loads(SEARCH_MATERIAL_SOURCE.read_text(encoding='utf-8'))
    if cylinder is None:cylinder=json.loads(SEARCH_CYLINDER_SOURCE.read_text(encoding='utf-8'))
    if inline is None:inline=json.loads(SEARCH_INLINE_SOURCE.read_text(encoding='utf-8'))
    inline_enabled,inline_disabled=validate_inline_snapshot(inline)
    if material.get('schema')!=1 or material.get('reference')!='SearchField/Enabled':
        raise ValueError('Invalid search field component snapshot')
    opacity=material.get('backgroundOpacity')
    if not isinstance(opacity,(int,float)) or isinstance(opacity,bool) or not math.isfinite(opacity) or not 0<=opacity<=1:
        raise ValueError('Invalid search field background opacity')
    radius=cylinder.get('cornerRadius')
    states=cylinder.get('verifiedTextStates')
    modes=cylinder.get('verifiedModes')
    if (cylinder.get('schema')!=1 or cylinder.get('reference')!='TextField/Cylinder' or
            not isinstance(radius,(int,float)) or isinstance(radius,bool) or not math.isfinite(radius) or not 0<=radius<=8192 or
            not isinstance(modes,list) or len(modes)!=2 or any(not isinstance(mode,str) for mode in modes) or set(modes)!={'Text','Search'} or
            not isinstance(states,list) or len(states)!=6 or any(not isinstance(state,str) for state in states) or
            set(states)!={'Enabled','Disabled','Typing','Dragged','CursorActive','KeyboardFocus'}):
        raise ValueError('Invalid cylinder text field geometry')
    collections={c['name']:c for c in data['collections']}
    fields=[('height','Theme Metrics','Size/22','FLOAT'),
            ('icon_size','Theme Metrics','TextField/iconSize','FLOAT'),
            ('horizontal_padding','Theme Metrics','TextField/paddingHorizontal','FLOAT'),
            ('vertical_padding','Theme Metrics','TextField/paddingVertical','FLOAT'),
            ('border_width','Theme Metrics','TextField/borderWidth','FLOAT'),
            ('gap','Theme Metrics','Space/2','FLOAT'),('radius','Theme Metrics','Radius/8','FLOAT'),
            ('font_size','Theme Typography','Body/textBody','FLOAT'),
            ('line_height','Theme Typography','Body/textBodyLineHeight','FLOAT'),
            ('clear_line_height','Theme Metrics','Space/16','FLOAT'),
            ('background','Theme Colors','Panel/surfaceSolid','COLOR'),
            ('inline_background','Theme Colors','Panel/surfaceSolid','COLOR'),
            ('disabled_background','Material','TextField/Disabled','COLOR'),
            ('inline_disabled_background','Material','TextField/Inline','COLOR'),
            ('reflection','Material','TextField/Reflection','COLOR'),
            ('text','Theme Colors','Text/titleHeaderColor','COLOR'),
            ('disabled_text','Theme Colors','Text/disabledColor','COLOR'),
            ('description','Theme Colors','Text/descriptionColor','COLOR'),
            ('accent','Theme Colors','Accent/Blue/Default','COLOR'),
            ('font_family','Theme Typography','Font/fontBody','STRING')]
    lines=['#pragma once','// Generated by tools/update_style_tokens.py; do not edit.',
           '#include <LVUI/StyleTokens.h>','namespace lvui::search_field_defaults {']
    for name,group,path,kind in fields:
        collection=collections[group]
        matches=[v for v in data['variables'] if v['collectionId']==collection['id'] and v['name']==path]
        if len(matches)!=1 or matches[0]['type']!=kind:raise ValueError('Missing search field token '+group+'/'+path)
        if name=='background' and material.get('backgroundVariableId') not in [matches[0]['id'],*identity_aliases(matches[0])]:
            raise ValueError('Search field background is not bound to the LVUI surface token')
        if name=='inline_background' and inline_enabled['backgroundVariableId'] not in [matches[0]['id'],*identity_aliases(matches[0])]:
            raise ValueError('Inline field background is not bound to the LVUI surface token')
        if name=='inline_disabled_background' and inline_disabled['backgroundVariableId'] not in [matches[0]['id'],*identity_aliases(matches[0])]:
            raise ValueError('Disabled inline field background is not bound to the LVUI token')
        value=matches[0]['valuesByMode'].get(collection['defaultModeId'])
        if kind=='FLOAT':
            if not isinstance(value,(int,float)) or isinstance(value,bool) or not math.isfinite(value):raise ValueError('Invalid search field metric '+path)
            lines.append('inline constexpr double '+name+' = '+literal(value)+';')
        elif kind=='COLOR':
            if not isinstance(value,dict) or set(value)!={'r','g','b','a'} or any(not isinstance(v,(int,float)) or isinstance(v,bool) or not math.isfinite(v) or not 0<=v<=1 for v in value.values()):
                raise ValueError('Invalid search field color '+path)
            if name=='background':value={**value,'a':value['a']*opacity}
            if name=='inline_background':value={**value,'a':value['a']*inline_enabled['backgroundOpacity']}
            if name=='inline_disabled_background' and abs(value['a']-inline_disabled['backgroundOpacity'])>1e-7:
                raise ValueError('Disabled inline field opacity differs from its token')
            lines.append('inline constexpr StyleColor '+name+'{'+', '.join(literal(value[k]) for k in ('r','g','b','a'))+'};')
        else:
            if not isinstance(value,str) or '\0' in value:raise ValueError('Invalid search field font')
            lines.append('inline constexpr char '+name+'[] = '+literal(value)+';')
    lines.append('inline constexpr double cylinder_radius = '+literal(radius)+';')
    return '\n'.join(lines+['} // namespace lvui::search_field_defaults',''])

def slider_filled_defaults(data, snapshot):
    """Compile the four filled Slider size presets from the Figma state matrix."""
    if (not isinstance(snapshot,dict) or type(snapshot.get('schema')) is not int or snapshot['schema']!=1 or
        snapshot.get('reference')!='LVUI Slider/Filled' or
        not isinstance(snapshot.get('source'),str) or not snapshot['source'].startswith('https://www.figma.com/design/')):
        raise ValueError('Invalid filled slider snapshot')
    names=('Panel/surfaceSolid','Text/disabledColor','Accent/Blue/Default','Text/titleHeaderColor')
    sources=snapshot.get('sourceVariables')
    if not isinstance(sources,list) or len(sources)!=len(names):raise ValueError('Missing filled slider colors')
    for source,name in zip(sources,names):
        matches=[v for v in data['variables'] if v['name']==name and v['type']=='COLOR']
        if (not isinstance(source,dict) or source.get('name')!=name or len(matches)!=1 or
            source.get('id') not in matches[0].get('legacyAliases',[])):
            raise ValueError('Invalid filled slider color token')
        collection=next((c for c in data['collections'] if c['id']==matches[0]['collectionId']),None)
        selected=matches[0]['valuesByMode'].get(collection['defaultModeId']) if collection else None
        observed=list(source.get('values',{}).values()) if isinstance(source.get('values'),dict) else []
        if (not isinstance(selected,dict) or len(observed)!=1 or not isinstance(observed[0],dict) or
            set(selected)!={'r','g','b','a'} or set(observed[0])!=set(selected) or
            any(not isinstance(selected[k],(int,float)) or isinstance(selected[k],bool) or
                not isinstance(observed[0][k],(int,float)) or isinstance(observed[0][k],bool) or
                not math.isclose(selected[k],observed[0][k],abs_tol=1e-6) for k in selected)):
            raise ValueError('Filled slider color differs from its token')
    sizes=('Mini','Small','Regular','Large');states=('Idle','Hover','Pressed','Disabled')
    variants=snapshot.get('variants');width=snapshot.get('sampleWidth')
    def number(value):return isinstance(value,(int,float)) and not isinstance(value,bool) and math.isfinite(value) and 0<value<=8192
    if not number(width) or not isinstance(variants,list) or len(variants)!=16:raise ValueError('Incomplete filled slider matrix')
    by_size={};seen=set();nodes=set()
    for v in variants:
        if not isinstance(v,dict):raise ValueError('Invalid filled slider variant')
        size=v.get('size');state=v.get('state');node=v.get('node');height=v.get('height');track=v.get('trackHeight');inset=v.get('rangeInset');fill=v.get('sampleFillWidth');opacity=v.get('opacity')
        if (size not in sizes or state not in states or (size,state) in seen or not isinstance(node,str) or
            not re.fullmatch(r'\d+:\d+',node) or node in nodes or not all(number(x) for x in (height,track,inset,fill)) or
            height<track or not math.isclose(2*inset,track,abs_tol=1e-6) or
            not math.isclose(fill,(width+track)/2,abs_tol=1e-6) or type(v.get('symbolVisible')) is not bool or
            not isinstance(opacity,(int,float)) or isinstance(opacity,bool) or not math.isfinite(opacity) or
            not math.isclose(opacity,.32 if state=='Disabled' else 1,abs_tol=1e-6)):
            raise ValueError('Invalid filled slider geometry')
        thumb=v.get('thumbDiameter');ring=v.get('ringDiameter');stroke=v.get('ringStroke')
        if state in ('Hover','Pressed'):
            if not all(number(x) for x in (thumb,ring,stroke)) or thumb>track or ring<=thumb:raise ValueError('Invalid filled slider thumb')
        elif thumb is not None or ring is not None or stroke is not None:raise ValueError('Unexpected filled slider thumb')
        by_size.setdefault(size,{})[state]=v;seen.add((size,state));nodes.add(node)
    presets=[];outset=None;hover_stroke=None;pressed_stroke=None
    for size in sizes:
        group=by_size.get(size,{})
        if set(group)!=set(states):raise ValueError('Incomplete filled slider states')
        idle=group['Idle'];hover=group['Hover'];pressed=group['Pressed']
        for v in group.values():
            if any(v[field]!=idle[field] for field in ('height','trackHeight','rangeInset','symbolVisible')):raise ValueError('Filled slider geometry changes by state')
        if hover['thumbDiameter']!=pressed['thumbDiameter'] or hover['ringDiameter']!=pressed['ringDiameter']:
            raise ValueError('Filled slider thumb changes by state')
        delta=(hover['ringDiameter']-hover['thumbDiameter'])/2
        if outset is None:outset=delta;hover_stroke=hover['ringStroke'];pressed_stroke=pressed['ringStroke']
        if delta!=outset or hover['ringStroke']!=hover_stroke or pressed['ringStroke']!=pressed_stroke:
            raise ValueError('Inconsistent filled slider focus ring')
        presets.append((idle['height'],idle['trackHeight'],hover['thumbDiameter'],idle['rangeInset'],idle['symbolVisible']))
    track=snapshot.get('trackEffects');thumb=snapshot.get('thumbEffects')
    if (not isinstance(track,list) or len(track)!=3 or not all(isinstance(x,dict) and x.get('visible') is True for x in track) or
        [x.get('type') for x in track]!=['BACKGROUND_BLUR','INNER_SHADOW','INNER_SHADOW'] or
        not isinstance(thumb,list) or len(thumb)!=2 or not all(isinstance(x,dict) and x.get('visible') is True for x in thumb) or
        [x.get('type') for x in thumb]!=['DROP_SHADOW','INNER_SHADOW']):
        raise ValueError('Incomplete filled slider effect snapshot')
    blur=track[0].get('radius');highlight=track[2].get('color');offset=track[2].get('offset')
    if (not number(blur) or blur>64 or not isinstance(highlight,dict) or set(highlight)!={'r','g','b','a'} or
        any(not isinstance(highlight[k],(int,float)) or isinstance(highlight[k],bool) or not math.isfinite(highlight[k]) or
            not 0<=highlight[k]<=1 for k in highlight) or any(highlight[k]!=1 for k in ('r','g','b')) or
        not isinstance(offset,dict) or set(offset)!={'x','y'} or offset['x']!=0 or
        not isinstance(offset['y'],(int,float)) or isinstance(offset['y'],bool) or not math.isfinite(offset['y'])):
        raise ValueError('Invalid filled slider material effect')
    lines=['#pragma once','// Generated from the LVUI Figma component snapshot; do not edit.','#include <array>',
           'namespace lvui::slider_filled_defaults {',
           'struct SizePreset final { double height, track_height, thumb_diameter, range_inset; bool show_symbol; };',
           'inline constexpr std::array<SizePreset, 4> sizes{{']
    lines.extend('    SizePreset{'+', '.join(literal(x) for x in preset)+'},' for preset in presets)
    lines.extend(['}};','inline constexpr double ring_outset = '+literal(outset)+';',
                  'inline constexpr double hover_ring_stroke = '+literal(hover_stroke)+';',
                  'inline constexpr double pressed_ring_stroke = '+literal(pressed_stroke)+';',
                  'inline constexpr double blur_radius = '+literal(blur)+';',
                  'inline constexpr double highlight_opacity = '+literal(highlight['a'])+';',
                  'inline constexpr double highlight_offset_y = '+literal(offset['y'])+';',
                  '} // namespace lvui::slider_filled_defaults',''])
    return '\n'.join(lines)

def slider_segmented_defaults(data, snapshot, filled):
    """Validate the labeled/segmented Figma matrix and compile its tick geometry."""
    slider_filled_defaults(data, filled)
    if (not isinstance(snapshot,dict) or snapshot.get('schema')!=1 or
        snapshot.get('reference')!='LVUI Slider/Min-max labels and Segmented' or
        not isinstance(snapshot.get('source'),str) or not snapshot['source'].startswith('https://www.figma.com/design/')):
        raise ValueError('Invalid labeled/segmented slider snapshot')
    sets=snapshot.get('sets')
    if (not isinstance(sets,list) or len(sets)!=2 or not all(isinstance(s,dict) for s in sets) or
        [s.get('name') for s in sets]!=['Slider/Min-max labels','Slider/Segmented']):
        raise ValueError('Missing labeled/segmented slider families')
    sizes=('Mini','Small','Regular','Large');states=('Idle','Hover','Pressed','Disabled')
    filled_rows={(v['size'],v['state']):v for v in filled['variants']}
    track=next(v for v in data['variables'] if v['name']=='Panel/surfaceSolid')
    caption=next(v for v in data['variables'] if v['name']=='Text/captionColor')
    collection=next(c for c in data['collections'] if c['id']==caption['collectionId'])
    caption_value=caption['valuesByMode'][collection['defaultModeId']]
    def number(x):return isinstance(x,(int,float)) and not isinstance(x,bool) and math.isfinite(x) and 0<x<=8192
    nodes=set();default_count=None;diameters={};label_geometry=None
    for family in sets:
        segmented=family['name']=='Slider/Segmented';rows=family.get('variants');seen=set()
        if not isinstance(rows,list) or len(rows)!=16:raise ValueError('Incomplete labeled slider matrix')
        for row in rows:
            if not isinstance(row,dict):raise ValueError('Invalid labeled slider variant')
            size=row.get('size');state=row.get('state');key=(size,state);node=row.get('node')
            if (size not in sizes or state not in states or key in seen or not isinstance(node,str) or
                not re.fullmatch(r'\d+:\d+',node) or node in nodes):raise ValueError('Invalid labeled slider identity')
            seen.add(key);nodes.add(node);expected=filled_rows[key];thumb=filled_rows[(size,'Hover')]['thumbDiameter']
            opacity=row.get('opacity')
            if (not all(number(row.get(k)) for k in ('width','height','trackHeight','rangeInset','minimumWidth','maximumWidth','controlX','controlWidth')) or
                any(row[k]!=expected[k] for k in ('height','trackHeight','rangeInset')) or
                not isinstance(opacity,(int,float)) or isinstance(opacity,bool) or not math.isfinite(opacity) or
                not math.isclose(opacity,expected['opacity'],abs_tol=1e-6) or
                row.get('trackVariable') not in track.get('legacyAliases',[]) or row.get('symbolVisible') is not False):
                raise ValueError('Labeled slider geometry differs from its filled source')
            geometry=(row['minimumWidth'],row['maximumWidth'],row['controlX'],row['controlWidth'],row['width'])
            if label_geometry is None:label_geometry=geometry
            if geometry!=label_geometry or not math.isclose(row['controlX']-row['minimumWidth'],
                row['width']-row['controlX']-row['controlWidth']-row['maximumWidth'],abs_tol=1e-6):
                raise ValueError('Inconsistent labeled slider endpoints')
            should_show=segmented or state in ('Hover','Pressed')
            if row.get('thumbDiameter')!=(thumb if should_show else None):raise ValueError('Invalid labeled slider thumb')
            if not segmented:
                if any(row.get(k) is not None for k in ('tickCount','tickDiameter','tickColor','tickOpacity','tickVariable')):
                    raise ValueError('Unexpected labeled slider ticks')
                continue
            count=row.get('tickCount');diameter=row.get('tickDiameter');color=row.get('tickColor');opacity=row.get('tickOpacity')
            if (type(count) is not int or not 2<=count<=512 or not number(diameter) or diameter>row['trackHeight']/2 or
                row.get('tickVariable') not in caption.get('legacyAliases',[]) or not isinstance(color,dict) or
                set(color)!={'r','g','b'} or not all(isinstance(color[k],(int,float)) and not isinstance(color[k],bool) and
                    math.isfinite(color[k]) and math.isclose(color[k],caption_value[k],abs_tol=1e-6) for k in color) or
                not isinstance(opacity,(int,float)) or isinstance(opacity,bool) or not math.isclose(opacity,caption_value['a'],abs_tol=1e-6)):
                raise ValueError('Invalid segmented slider ticks')
            positions=row.get('tickPositions');tick_range=row.get('tickRangeWidth')
            if (not number(tick_range) or not math.isclose(tick_range,row['controlWidth']-2*row['rangeInset'],abs_tol=1e-6) or
                not isinstance(positions,list) or len(positions)!=count or not all(isinstance(position,(int,float)) and
                    not isinstance(position,bool) and math.isfinite(position) and
                    math.isclose(position,index/(count-1),abs_tol=1e-6) for index,position in enumerate(positions))):
                raise ValueError('Invalid segmented slider tick positions')
            if default_count is None:default_count=count
            if count!=default_count or (size in diameters and diameters[size]!=diameter):raise ValueError('Inconsistent segmented slider ticks')
            diameters[size]=diameter
    if set(diameters)!=set(sizes):raise ValueError('Missing segmented slider sizes')
    maximum_diameter=max(diameters.values())
    track_divisor=filled_rows[(sizes[0],'Idle')]['trackHeight']/diameters[sizes[0]]
    if any(not math.isclose(diameters[size],min(maximum_diameter,
        filled_rows[(size,'Idle')]['trackHeight']/track_divisor),abs_tol=1e-6) for size in sizes):
        raise ValueError('Segmented slider tick sizes do not share a track ratio')
    return '\n'.join(['#pragma once','// Generated from the LVUI Figma component snapshot; do not edit.',
        '#include <array>','#include <cstddef>','namespace lvui::slider_segmented_defaults {',
        'inline constexpr std::size_t default_segment_count = '+str(default_count)+';',
        'inline constexpr std::array<double, 4> tick_diameters{'+', '.join(literal(diameters[s]) for s in sizes)+'};',
        'inline constexpr double maximum_tick_diameter = '+literal(maximum_diameter)+';',
        'inline constexpr double tick_track_divisor = '+literal(track_divisor)+';',
        '} // namespace lvui::slider_segmented_defaults',''])

def slider_center_defaults(data, snapshot):
    """Compile center-mark geometry and its LVUI caption color from Figma."""
    if type(snapshot.get('schema')) is not int or snapshot.get('schema')!=1 or snapshot.get('reference')!='LVUI Slider/Center biased':
        raise ValueError('Invalid center slider snapshot')
    if not isinstance(snapshot.get('source'),str) or not snapshot['source'].startswith('https://www.figma.com/design/'):
        raise ValueError('Invalid center slider source')
    width=snapshot.get('centerMarkWidth');overhang=snapshot.get('centerMarkOverhang');source_radius=snapshot.get('sourceCornerRadius')
    def number(value):
        return isinstance(value,(int,float)) and not isinstance(value,bool) and math.isfinite(value) and 0<value<=256
    if (not number(width) or not number(overhang) or not isinstance(source_radius,(int,float)) or
        isinstance(source_radius,bool) or not math.isfinite(source_radius) or not 0<=source_radius<=8192):
        raise ValueError('Invalid center slider geometry')
    variants=snapshot.get('variants');sizes=('Mini','Small','Regular','Large');states=('Idle','Hover','Pressed','Disabled')
    if not isinstance(variants,list) or len(variants)!=len(sizes)*len(states):raise ValueError('Incomplete center slider variants')
    seen=set();nodes=set();heights={}
    for variant in variants:
        if not isinstance(variant,dict):raise ValueError('Invalid center slider variant')
        size=variant.get('size');state=variant.get('state');node=variant.get('node');track=variant.get('trackHeight')
        opacity=variant.get('opacity')
        if (size not in sizes or state not in states or (size,state) in seen or not isinstance(node,str) or
            not re.fullmatch(r'\d+:\d+',node) or node in nodes or not number(track) or
            not number(variant.get('markWidth')) or not number(variant.get('markHeight')) or not number(variant.get('markOverhang')) or
            not isinstance(opacity,(int,float)) or isinstance(opacity,bool) or not math.isfinite(opacity) or
            variant.get('markWidth')!=width or variant.get('markOverhang')!=overhang or
            variant.get('markCornerRadius')!=source_radius or isinstance(variant.get('markCornerRadius'),bool) or
            variant.get('markHeight')!=track+2*overhang or
            opacity!=(.32 if state=='Disabled' else 1)):
            raise ValueError('Invalid center slider variant geometry')
        if size in heights and heights[size]!=track:raise ValueError('Center slider track changed between states')
        heights[size]=track;seen.add((size,state));nodes.add(node)
    matches=[v for v in data['variables'] if v['id']==snapshot.get('fillVariableId')]
    if (len(matches)!=1 or matches[0]['type']!='COLOR' or matches[0]['name']!='Text/captionColor' or
        snapshot.get('sourceFillVariableId') not in matches[0].get('legacyAliases',[])):
        raise ValueError('Invalid center slider color token')
    collection=next((c for c in data['collections'] if c['id']==matches[0]['collectionId']),None)
    color=matches[0]['valuesByMode'].get(collection['defaultModeId']) if collection else None
    if not isinstance(color,dict) or set(color)!={'r','g','b','a'} or any(
            not isinstance(v,(int,float)) or isinstance(v,bool) or not math.isfinite(v) or not 0<=v<=1 for v in color.values()):
        raise ValueError('Invalid center slider color')
    return '\n'.join(['#pragma once','// Generated from the LVUI Figma component snapshot; do not edit.',
        '#include <LVUI/StyleTokens.h>','namespace lvui::slider_center_defaults {',
        'inline constexpr double width = '+literal(width)+';',
        'inline constexpr double overhang = '+literal(overhang)+';',
        'inline constexpr double radius = '+literal(min(source_radius,width/2))+';',
        'inline constexpr StyleColor color{'+', '.join(literal(color[k]) for k in ('r','g','b','a'))+'};',
        '} // namespace lvui::slider_center_defaults',''])

def slider_tick_defaults(data, snapshot):
    """Capture unbound tick geometry and its bound LVUI color from Figma."""
    if snapshot.get('schema')!=1 or snapshot.get('reference')!='Slider/Regular/Ticks':
        raise ValueError('Invalid slider tick snapshot')
    gap=snapshot.get('gapBelowTrack');size=snapshot.get('tickSize');count=snapshot.get('defaultCount')
    if any(not isinstance(v,(int,float)) or isinstance(v,bool) or not math.isfinite(v) or not 0<=v<=256 for v in (gap,size)) or size==0:
        raise ValueError('Invalid slider tick geometry')
    if not isinstance(count,int) or isinstance(count,bool) or not 2<=count<=512:
        raise ValueError('Invalid slider tick count')
    matches=[v for v in data['variables'] if v['id']==snapshot.get('fillVariableId')]
    if len(matches)!=1 or matches[0]['type']!='COLOR':raise ValueError('Invalid slider tick color token')
    collection=next((c for c in data['collections'] if c['id']==matches[0]['collectionId']),None)
    color=matches[0]['valuesByMode'].get(collection['defaultModeId']) if collection else None
    if not isinstance(color,dict) or set(color)!={'r','g','b','a'} or any(not isinstance(v,(int,float)) or isinstance(v,bool) or not math.isfinite(v) or not 0<=v<=1 for v in color.values()):
        raise ValueError('Invalid slider tick color')
    return '\n'.join(['#pragma once','// Generated from the LVUI Figma component snapshot; do not edit.',
        '#include <LVUI/StyleTokens.h>','#include <cstddef>','namespace lvui::slider_tick_defaults {',
        'inline constexpr double gap = '+literal(gap)+';',
        'inline constexpr double size = '+literal(size)+';',
        'inline constexpr std::size_t count = '+str(count)+';',
        'inline constexpr StyleColor color{'+', '.join(literal(color[k]) for k in ('r','g','b','a'))+'};',
        '} // namespace lvui::slider_tick_defaults',''])

def slider_symbol_defaults(snapshot):
    if snapshot.get('schema')!=1 or snapshot.get('reference')!='Slider/Thumb/Symbol':
        raise ValueError('Invalid slider symbol snapshot')
    size=snapshot.get('symbolSize');icon=snapshot.get('defaultIcon');legacy=snapshot.get('legacyIcon')
    if not isinstance(size,(int,float)) or isinstance(size,bool) or not math.isfinite(size) or not 1<=size<=256:
        raise ValueError('Invalid slider symbol size')
    if not isinstance(icon,str) or not re.fullmatch(r'[A-Za-z][A-Za-z0-9_.-]{0,127}',icon):
        raise ValueError('Invalid slider symbol icon')
    if not isinstance(legacy,str) or not re.fullmatch(r'[A-Za-z][A-Za-z0-9_.-]{0,127}',legacy) or legacy==icon:
        raise ValueError('Invalid slider symbol legacy icon')
    if not isinstance(snapshot.get('canonicalSource'),str) or not snapshot['canonicalSource'].startswith('https://www.figma.com/design/'):
        raise ValueError('Invalid slider symbol canonical source')
    return '\n'.join(['#pragma once','// Generated from the LVUI Figma component snapshot; do not edit.',
        'namespace lvui::slider_symbol_defaults {','inline constexpr double size = '+literal(size)+';',
        'inline constexpr char icon[] = '+literal(icon)+';',
        'inline constexpr char legacy_icon[] = '+literal(legacy)+';','} // namespace lvui::slider_symbol_defaults',''])

def segmented_defaults(data,snapshot):
    """Compile geometry and validate the background and optional border fallback."""
    if type(snapshot.get('schema')) is not int or snapshot.get('schema')!=1 or snapshot.get('reference')!='LVUI segmented control defaults':
        raise ValueError('Invalid segmented control snapshot')
    source=snapshot.get('source')
    if not isinstance(source,str) or not source.startswith('https://www.figma.com/design/'):
        raise ValueError('Invalid segmented control source')
    def number(owner,name,maximum):
        value=owner.get(name)
        if not isinstance(value,(int,float)) or isinstance(value,bool) or not math.isfinite(value) or not 0<=value<=maximum:
            raise ValueError('Invalid segmented control geometry: '+name)
        return value
    variants={}
    for name in ('label','icon'):
        variant=snapshot.get(name)
        if not isinstance(variant,dict) or not re.fullmatch(r'[a-f0-9]{40}',str(variant.get('componentKey',''))):
            raise ValueError('Invalid segmented control component: '+name)
        count=variant.get('verifiedVariantCount')
        if type(count) is not int or count<1:raise ValueError('Invalid segmented control variant count')
        variants[name]=(number(variant,'horizontalPadding',256),number(variant,'verticalPadding',256))
    variables={v['name']:v for v in data['variables']}
    for field,name in (('backgroundVariableKey','Panel/subSurface'),('borderVariableKey','Panel/surfaceRaised')):
        if snapshot.get(field) not in variables[name].get('legacyAliases',[]):
            raise ValueError('Segmented control color binding changed: '+field)
    spacing=number(snapshot,'spacing',256);border=number(snapshot,'borderWidth',256);radius=number(snapshot,'cornerRadius',256)
    radius_variable=variables['Radius/8']
    radius_values=radius_variable.get('valuesByMode',{})
    if (snapshot.get('cornerRadiusVariableKey') not in radius_variable.get('legacyAliases',[]) or
        radius_variable.get('type')!='FLOAT' or not radius_values or
        any(value!=radius for value in radius_values.values())):
        raise ValueError('Segmented control radius binding changed')
    return '\n'.join(['#pragma once','// Generated from the LVUI Figma segmented control snapshots; do not edit.',
        'namespace lvui::segmented_defaults {',
        'inline constexpr double label_horizontal_padding = '+literal(variants['label'][0])+';',
        'inline constexpr double label_vertical_padding = '+literal(variants['label'][1])+';',
        'inline constexpr double icon_horizontal_padding = '+literal(variants['icon'][0])+';',
        'inline constexpr double icon_vertical_padding = '+literal(variants['icon'][1])+';',
        'inline constexpr double spacing = '+literal(spacing)+';',
        'inline constexpr double border_width = '+literal(border)+';',
        'inline constexpr double corner_radius = '+literal(radius)+';',
        '} // namespace lvui::segmented_defaults',''])

def design_icon_defaults(snapshot):
    """Generate the intrinsic geometry of an existing LVUI Figma icon."""
    if type(snapshot.get('schema')) is not int or snapshot.get('schema')!=1 or snapshot.get('reference')!='LVUI design icon default geometry':
        raise ValueError('Invalid design icon snapshot')
    source=snapshot.get('source');key=snapshot.get('componentKey')
    if not isinstance(source,str) or not source.startswith('https://www.figma.com/design/') or not isinstance(key,str) or not re.fullmatch(r'[a-f0-9]{40}',key):
        raise ValueError('Invalid design icon source')
    def dimension(name):
        value=snapshot.get(name)
        if not isinstance(value,(int,float)) or isinstance(value,bool) or not math.isfinite(value) or not 1<=value<=256:
            raise ValueError('Invalid design icon geometry: '+name)
        return value
    width=dimension('width');height=dimension('height')
    if width!=height:raise ValueError('Design icon geometry must be square')
    return '\n'.join(['#pragma once','// Generated from the LVUI Figma icon geometry snapshot; do not edit.',
        'namespace lvui::design_icon_defaults {','inline constexpr double extent = '+literal(width)+';',
        '} // namespace lvui::design_icon_defaults',''])

def material_radial_defaults(snapshot):
    """Generate common radial fields and stops from the LVUI Figma snapshot."""
    if snapshot.get('schema')!=1 or snapshot.get('reference')!='LVUI material radial fields' or snapshot.get('colorProfile')!='SRGB':
        raise ValueError('Invalid material radial snapshot')
    if not isinstance(snapshot.get('source'),str) or not snapshot['source'].startswith('https://www.figma.com/design/'):
        raise ValueError('Invalid material radial source')
    def number(value,low,high):
        if not isinstance(value,(int,float)) or isinstance(value,bool) or not math.isfinite(value) or not low<=value<=high:
            raise ValueError('Invalid material radial number')
        return value
    stops=snapshot.get('stops');fields=snapshot.get('fields')
    if not isinstance(stops,list) or len(stops)!=4 or not all(isinstance(s,dict) for s in stops) or \
       not isinstance(fields,list) or len(fields)!=8 or not all(isinstance(f,dict) for f in fields):
        raise ValueError('Invalid material radial fields or stops')
    positions=[number(s.get('position'),0,1) for s in stops]
    opacities=[number(s.get('opacity'),0,1) for s in stops]
    if positions[0]!=0 or positions[-1]!=1 or not positions[0]<positions[1]<positions[2]<positions[3] or opacities[0]!=1 or opacities[-1]!=0:
        raise ValueError('Invalid material radial stop order')
    if stops[0].get('color')!='primary' or stops[-1].get('color')!=[0,0,0] or stops[1].get('color')!=stops[2].get('color'):
        raise ValueError('Unsupported material radial stop colors')
    edge=stops[1].get('color')
    if not isinstance(edge,list) or len(edge)!=3:raise ValueError('Invalid material radial edge color')
    edge=[number(c,0,1) for c in edge]
    rows=[]
    for field in fields:
        x=field.get('centerX');y=field.get('centerY');radius=field.get('radius');strength=field.get('strength')
        if not all(isinstance(pair,list) and len(pair)==2 for pair in (x,y,radius)) or strength not in {'intense','faint'}:
            raise ValueError('Invalid material radial field')
        values=[number(x[0],0,1),number(x[1],-8192,8192),number(y[0],0,1),number(y[1],-8192,8192),
                number(radius[0],.01,8192),number(radius[1],.01,8192)]
        rows.append('    {'+', '.join(literal(v) for v in values)+', '+literal(strength=='intense')+'},')
    return '\n'.join(['#pragma once','#include <array>',
        '// Generated from the LVUI Figma radial material snapshot; do not edit.',
        'namespace lvui::material_radial_defaults {',
        'struct Field final { double x_factor,x_offset,y_factor,y_offset,radius_x,radius_y; bool intense; };',
        'inline constexpr double first_position = '+literal(positions[1])+';',
        'inline constexpr double second_position = '+literal(positions[2])+';',
        'inline constexpr double first_opacity = '+literal(opacities[1])+';',
        'inline constexpr double second_opacity = '+literal(opacities[2])+';',
        'inline constexpr std::array<double,3> edge_rgb{{'+', '.join(literal(v) for v in edge)+'}};',
        'inline constexpr std::array<Field,8> fields{{',*rows,'}};',
        '} // namespace lvui::material_radial_defaults',''])

def button_motion_defaults(snapshot):
    """Generate button interaction timing from the inspected Figma prototype."""
    if snapshot.get('schema') != 1 or snapshot.get('reference') != 'LVUI button prototype transitions':
        raise ValueError('Invalid button motion snapshot')
    sources=snapshot.get('sources')
    if (not isinstance(sources,list) or len(sources)!=2 or
            not all(isinstance(source,str) and source.startswith('https://www.figma.com/design/') and
                    '?node-id=' in source for source in sources)):
        raise ValueError('Invalid button motion sources')
    total=snapshot.get('verifiedVariantCount');enabled=snapshot.get('enabledVariantCount')
    if (not isinstance(total,int) or isinstance(total,bool) or total<1 or
            not isinstance(enabled,int) or isinstance(enabled,bool) or not 1<=enabled<=total or
            snapshot.get('easing')!='EASE_OUT'):
        raise ValueError('Invalid button motion variants or easing')
    durations=[]
    for field,name in [('hoverMilliseconds','hover'),('pressMilliseconds','press'),
                       ('releaseMilliseconds','release'),('postReleaseMilliseconds','post_release')]:
        value=snapshot.get(field)
        if not isinstance(value,int) or isinstance(value,bool) or not 1<=value<=10000:
            raise ValueError('Invalid button motion duration: '+field)
        durations.append('inline constexpr std::chrono::milliseconds '+name+'{'+str(value)+'};')
    return '\n'.join(['#pragma once','// Generated from the LVUI Figma button motion snapshot; do not edit.',
        '#include <chrono>','namespace lvui::button_motion_defaults {',*durations,
        '} // namespace lvui::button_motion_defaults',''])

def icon_button_defaults(snapshot):
    """Generate the shared fixed geometry of the Figma IconButton component."""
    if snapshot.get('schema')!=1 or snapshot.get('reference')!='PushButton / IconButton, borderless, default':
        raise ValueError('Invalid icon button snapshot')
    count=snapshot.get('verifiedVariantCount')
    if not isinstance(count,int) or isinstance(count,bool) or count<1:
        raise ValueError('Invalid icon button variant count')
    def number(name,minimum,maximum):
        value=snapshot.get(name)
        if not isinstance(value,(int,float)) or isinstance(value,bool) or not math.isfinite(value) or not minimum<=value<=maximum:
            raise ValueError('Invalid icon button geometry: '+name)
        return value
    width=number('width',1,256);height=number('height',1,256)
    icon_width=number('iconWidth',1,256);icon_height=number('iconHeight',1,256)
    left=number('paddingLeft',0,128);right=number('paddingRight',0,128)
    top=number('paddingTop',0,128);bottom=number('paddingBottom',0,128)
    radius=number('cornerRadius',0,128)
    if (width!=height or icon_width!=icon_height or left!=right or left!=top or left!=bottom or
            width!=icon_width+left+right or height!=icon_height+top+bottom or radius>width/2):
        raise ValueError('Inconsistent icon button geometry')
    return '\n'.join(['#pragma once','// Generated from the LVUI Figma IconButton geometry snapshot; do not edit.',
        'namespace lvui::icon_button_defaults {',
        'inline constexpr double extent = '+literal(width)+';',
        'inline constexpr double icon_size = '+literal(icon_width)+';',
        'inline constexpr double horizontal_padding = '+literal(left)+';',
        'inline constexpr double corner_radius = '+literal(radius)+';',
        '} // namespace lvui::icon_button_defaults',''])

def slider_endpoint_defaults(data, snapshot):
    if snapshot.get('schema')!=1 or snapshot.get('reference')!='Slider/Endpoints':
        raise ValueError('Invalid slider endpoint snapshot')
    collections={c['name']:c for c in data['collections']}
    def token(group,name,kind):
        collection=collections.get(group)
        if not collection:raise ValueError('Missing slider endpoint token collection')
        items=[v for v in data['variables'] if v['collectionId']==collection['id'] and v['name']==name]
        if len(items)!=1 or items[0]['type']!=kind:raise ValueError('Missing slider endpoint token '+name)
        return items[0]['valuesByMode'].get(collection['defaultModeId'])
    def number(value,minimum,maximum):
        if not isinstance(value,(int,float)) or isinstance(value,bool) or not math.isfinite(value) or not minimum<=value<=maximum:
            raise ValueError('Invalid slider endpoint metric')
        return literal(value)
    outer=number(token('Theme Metrics','Space/8','FLOAT'),0,256)
    inner=number(token('Theme Metrics','Space/4','FLOAT'),0,256)
    font=number(token('Theme Typography','Body/textBody','FLOAT'),1,256)
    line=number(token('Theme Typography','Body/textBodyLineHeight','FLOAT'),0,1024)
    weight=token('Theme Typography','Weight/Medium','FLOAT')
    if weight not in (400,500,600,700) or isinstance(weight,bool):raise ValueError('Invalid slider endpoint font weight')
    minimum=number(snapshot.get('minimumControlWidth'),1,8192)
    color=token('Theme Colors','Text/bodyColor','COLOR')
    if not isinstance(color,dict) or set(color)!={'r','g','b','a'} or any(not isinstance(v,(int,float)) or isinstance(v,bool) or not math.isfinite(v) or not 0<=v<=1 for v in color.values()):
        raise ValueError('Invalid slider endpoint color')
    labels=[snapshot.get('minimumLabel'),snapshot.get('maximumLabel')]
    if any(not isinstance(v,str) or '\0' in v or len(v.encode('utf-8'))>65536 for v in labels):
        raise ValueError('Invalid slider endpoint label')
    return '\n'.join(['#pragma once','// Generated from LVUI Figma tokens and the LVRS endpoint geometry snapshot; do not edit.',
        '#include <LVUI/StyleTokens.h>','namespace lvui::slider_endpoint_defaults {',
        'inline constexpr double outer_gap = '+outer+';','inline constexpr double inner_gap = '+inner+';',
        'inline constexpr double minimum_control_width = '+minimum+';',
        'inline constexpr double font_size = '+font+';','inline constexpr double line_height = '+line+';',
        'inline constexpr unsigned font_weight = '+str(int(weight))+';',
        'inline constexpr StyleColor label_color{'+', '.join(literal(color[k]) for k in ('r','g','b','a'))+'};',
        'inline constexpr char minimum_label[] = '+literal(labels[0])+';',
        'inline constexpr char maximum_label[] = '+literal(labels[1])+';',
        '} // namespace lvui::slider_endpoint_defaults',''])

def search_material_defaults(data, disabled=None, inline=None):
    """Preserve the unbound paint/effect data captured from the LVUI component."""
    if data.get('schema') != 1 or data.get('reference') != 'SearchField/Enabled':
        raise ValueError('Invalid search material snapshot')
    if disabled is None:disabled=json.loads(SEARCH_DISABLED_SOURCE.read_text(encoding='utf-8'))
    if inline is None:inline=json.loads(SEARCH_INLINE_SOURCE.read_text(encoding='utf-8'))
    inline_enabled,inline_disabled=validate_inline_snapshot(inline)
    if (disabled.get('schema')!=1 or disabled.get('reference')!='TextField/Disabled' or
            disabled.get('strokes')!=[] or disabled.get('effects')!=[]):
        raise ValueError('Invalid disabled text field material snapshot')
    def number(value,minimum,maximum):
        if not isinstance(value,(int,float)) or isinstance(value,bool) or not math.isfinite(value) or not minimum<=value<=maximum:
            raise ValueError('Invalid search material number')
        return literal(value)
    def color(value):
        if not isinstance(value,dict) or set(value)!={'r','g','b','a'}:
            raise ValueError('Invalid search material color')
        return '{'+', '.join(number(value[k],0,1) for k in ('r','g','b','a'))+'}'
    def gradient(name,source=data):
        value=source[name]
        if (value.get('type')!='GRADIENT_LINEAR' or value.get('gradientTransform')!=[[0,1,0],[-1,0,1]] or
            value.get('visible') is not True or value.get('opacity')!=1 or value.get('blendMode')!='NORMAL'):
            raise ValueError('Unsupported search material gradient')
        stops=value['gradientStops']
        if len(stops)<2 or stops[0]['position']!=0 or stops[-1]['position']!=1:
            raise ValueError('Invalid search material gradient stops')
        positions=[stop['position'] for stop in stops]
        if any(a>=b for a,b in zip(positions,positions[1:])):
            raise ValueError('Search material stops must increase')
        return ['    {'+number(stop['position'],0,1)+', '+color(stop['color'])+'},' for stop in stops]
    def checked_effects(effects):
        if (not isinstance(effects,list) or len(effects)!=3 or effects[0].get('type')!='BACKGROUND_BLUR' or
            effects[0].get('blurType')!='NORMAL' or any(e.get('visible') is not True for e in effects) or
            any(e.get('type')!='INNER_SHADOW' for e in effects[1:])):
            raise ValueError('Unsupported text field material effects')
        return effects
    effects=checked_effects(data['effects']);inline_effects=checked_effects(inline_enabled.get('effects'))
    lines=['#pragma once','// Generated from the LVUI Figma component snapshot; do not edit.',
           '#include <LVUI/StyleTokens.h>','namespace lvui::search_field_material {',
           'struct SearchGradientStop final { double position; StyleColor color; };',
           'struct SearchShadow final { double radius, offset_x, offset_y; StyleColor color; };',
           'inline constexpr double blur_radius = '+number(effects[0]['radius'],0,128)+';',
           'inline constexpr double inline_blur_radius = '+number(inline_effects[0]['radius'],0,128)+';',
           'inline constexpr SearchGradientStop fill_gradient[] = {']
    lines+=gradient('fillGradient')+['};','inline constexpr SearchGradientStop stroke_gradient[] = {']
    lines+=gradient('strokeGradient')+['};','inline constexpr SearchGradientStop disabled_fill_gradient[] = {']
    lines+=gradient('fillGradient',disabled)+['};','inline constexpr SearchGradientStop inline_fill_gradient[] = {']
    lines+=gradient('fillGradient',inline_enabled)+['};','inline constexpr SearchShadow inner_shadows[] = {']
    def shadows(items):
        result=[]
        for effect in items[1:]:
            if effect.get('spread')!=0 or effect.get('blendMode')!='NORMAL':
                raise ValueError('Unsupported search shadow')
            result.append('    {'+', '.join([number(effect['radius'],0.001,128),number(effect['offset']['x'],-128,128),
                number(effect['offset']['y'],-128,128),color(effect['color'])])+'},')
        return result
    lines+=shadows(effects)+['};','inline constexpr SearchShadow inline_inner_shadows[] = {']
    lines+=shadows(inline_effects)
    return '\n'.join(lines+['};','} // namespace lvui::search_field_material',''])

def generate(data):
    if data.get('schema')!=1:raise ValueError('Unsupported token snapshot schema')
    collections=data['collections'];variables=data['variables']
    if not collections or not variables:raise ValueError('Empty style token snapshot')
    by_id={v['id']:v for v in variables};by_collection={c['id']:c for c in collections}
    if len(by_id)!=len(variables) or len(by_collection)!=len(collections):raise ValueError('Duplicate Figma identity')
    names=set();selectors=set();identities=set();collection_identities=set();code=['// Generated by tools/update_style_tokens.py; do not edit.','static const StyleTokenCatalog catalog({']
    for c in collections:
        for identity in [c['id'],c['key'],c['name'],*identity_aliases(c)]:
            if identity in collection_identities:raise ValueError('Colliding collection identity '+identity)
            collection_identities.add(identity)
        mode_ids={m['modeId'] for m in c['modes']}
        if len(mode_ids)!=len(c['modes']):raise ValueError('Duplicate collection mode')
        if c['defaultModeId'] not in mode_ids:raise ValueError('Missing default mode')
        if c['count']!=sum(v['collectionId']==c['id'] for v in variables):raise ValueError('Incomplete collection '+c['name'])
        mode_identities=set()
        for m in c['modes']:
            for identity in [m['modeId'],m['name'],*identity_aliases(m)]:
                if identity in mode_identities:raise ValueError('Colliding mode identity '+identity)
                mode_identities.add(identity)
        modes=', '.join('{'+literal(m['modeId'])+', '+literal(m['name'])+'}' for m in c['modes'])
        code.append('    {'+', '.join(literal(c[k]) for k in ['id','key','name','defaultModeId'])+', {'+modes+'}, '+literal(c['hiddenFromPublishing'])+'},')
    code.append('}, {')
    for v in variables:
        collection=by_collection[v['collectionId']];kind=TYPES[v['type']]
        css=v['cssName']
        if not re.fullmatch(r'--lvui-[a-z0-9_-]+',css):raise ValueError('Invalid canonical CSS name: '+v['name'])
        member=selector(v,collection)
        if member and v.get('codeSyntax',{}).get('WEB')!=member:raise ValueError('Figma code syntax differs from LVUI member: '+v['name'])
        if css in names:raise ValueError('Duplicate CSS token '+css)
        names.add(css)
        for identity in [v['id'],v['key'],css,*identity_aliases(v)]:
            if identity in identities:raise ValueError('Colliding token identity '+identity)
            identities.add(identity)
        values=[]
        if set(v['valuesByMode'])!={m['modeId'] for m in collection['modes']}:raise ValueError('Incomplete token modes')
        for mode,value in v['valuesByMode'].items():
            if isinstance(value,dict) and value.get('type')=='VARIABLE_ALIAS':
                if value['id'] not in by_id or by_id[value['id']]['type']!=v['type']:raise ValueError('Invalid alias '+v['name'])
                payload='StyleTokenAlias{'+literal(value['id'])+'}'
            elif kind=='color':
                if set(value)!={'r','g','b','a'} or not all(isinstance(x,(int,float)) and not isinstance(x,bool) and math.isfinite(x) and 0<=x<=1 for x in value.values()):raise ValueError('Invalid RGBA token')
                payload='StyleTokenValue{StyleColor{'+', '.join(literal(value[k]) for k in ['r','g','b','a'])+'}}'
            else:
                expected={'number':(int,float),'string':str,'boolean':bool}[kind]
                if not isinstance(value,expected) or (kind=='number' and isinstance(value,bool)):raise ValueError('Token type mismatch')
                if kind=='string' and ('\0' in value or len(value.encode('utf-8'))>4096):raise ValueError('Invalid token String')
                payload='StyleTokenValue{'+('std::string{'+literal(value)+'}' if kind=='string' else literal(value))+'}'
            values.append('{'+literal(mode)+', '+payload+'}')
        # Validate the complete alias graph, including otherwise-unused tokens.
        def visit(identifier,path,selected):
            if identifier in path:raise ValueError('Alias cycle: '+identifier)
            if len(path)>=64:raise ValueError('Alias chain limit')
            target=by_id[identifier];collection_id=target['collectionId']
            for mode,value in target['valuesByMode'].items():
                if collection_id in selected and selected[collection_id]!=mode:continue
                if isinstance(value,dict) and value.get('type')=='VARIABLE_ALIAS':visit(value['id'],path+[identifier],dict(selected,**{collection_id:mode}))
        visit(v['id'],[],{})
        fields=[literal(v[k]) for k in ['id','key','name','collectionId']]+[literal(css),literal(v['description']),'StyleTokenType::'+kind]
        fields+=['{'+', '.join(values)+'}','{'+', '.join(literal(s) for s in v['scopes'])+'}',literal(v['hiddenFromPublishing'])]
        if member:
            if member in selectors or not re.fullmatch(r'(Color|Metric)\.[A-Za-z_][A-Za-z0-9_]*(\.[A-Za-z_][A-Za-z0-9_]*)*',member):raise ValueError('Invalid or colliding style selector '+member)
            selectors.add(member)
        fields.append(literal(member))
        code.append('    {'+', '.join(fields)+'},')
    code.append('});')
    return '\n'.join(code)+'\n'

def compatibility(data):
    """Keep alternate design identities without changing public C++ token layouts."""
    lines=['// Generated by tools/update_style_tokens.py; do not edit.',
           'struct BuiltinTokenAlias { std::string_view id, key, alias; };',
           'struct BuiltinModeAlias { std::string_view collection_id, collection_key, mode_id, alias; };']
    tokens=[(v['id'],v['key'],a) for v in data['variables'] for a in identity_aliases(v)]
    collections=[(c['id'],c['key'],a) for c in data['collections'] for a in identity_aliases(c)]
    modes=[(c['id'],c['key'],m['modeId'],a) for c in data['collections'] for m in c['modes'] for a in identity_aliases(m)]
    for name,kind,entries in [('builtin_token_aliases','BuiltinTokenAlias',tokens),
                              ('builtin_collection_aliases','BuiltinTokenAlias',collections),
                              ('builtin_mode_aliases','BuiltinModeAlias',modes)]:
        lines.append('static constexpr std::array<'+kind+', '+str(len(entries)) + '> '+name+'{{')
        lines.extend('    {'+', '.join(literal(v) for v in entry)+'},' for entry in entries)
        lines.append('}};')
    return '\n'.join(lines)+'\n'

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--check',action='store_true');args=parser.parse_args()
    data=json.loads(SNAPSHOT.read_text(encoding='utf-8'))
    material=json.loads(SEARCH_MATERIAL_SOURCE.read_text(encoding='utf-8'))
    disabled_material=json.loads(SEARCH_DISABLED_SOURCE.read_text(encoding='utf-8'))
    cylinder=json.loads(SEARCH_CYLINDER_SOURCE.read_text(encoding='utf-8'))
    inline=json.loads(SEARCH_INLINE_SOURCE.read_text(encoding='utf-8'))
    slider_ticks=json.loads(SLIDER_TICKS_SOURCE.read_text(encoding='utf-8'))
    slider_center=json.loads(SLIDER_CENTER_SOURCE.read_text(encoding='utf-8'))
    slider_filled=json.loads(SLIDER_FILLED_SOURCE.read_text(encoding='utf-8'))
    slider_labeled_segmented=json.loads(SLIDER_LABELED_SEGMENTED_SOURCE.read_text(encoding='utf-8'))
    slider_symbol=json.loads(SLIDER_SYMBOL_SOURCE.read_text(encoding='utf-8'))
    slider_endpoints=json.loads(SLIDER_ENDPOINT_SOURCE.read_text(encoding='utf-8'))
    icon_button=json.loads(ICON_BUTTON_SOURCE.read_text(encoding='utf-8'))
    button_motion=json.loads(BUTTON_MOTION_SOURCE.read_text(encoding='utf-8'))
    design_icon=json.loads(DESIGN_ICON_SOURCE.read_text(encoding='utf-8'))
    material_radial=json.loads(MATERIAL_RADIAL_SOURCE.read_text(encoding='utf-8'))
    segmented=json.loads(SEGMENTED_SOURCE.read_text(encoding='utf-8'))
    outputs={OUTPUT:generate(data),ALIASES:compatibility(data),SEARCH_DEFAULTS:search_field_defaults(data,material,cylinder,inline),SEARCH_MATERIAL:search_material_defaults(material,disabled_material,inline),SLIDER_TICKS:slider_tick_defaults(data,slider_ticks),SLIDER_CENTER:slider_center_defaults(data,slider_center),SLIDER_FILLED:slider_filled_defaults(data,slider_filled),SLIDER_SEGMENTED:slider_segmented_defaults(data,slider_labeled_segmented,slider_filled),SLIDER_SYMBOL:slider_symbol_defaults(slider_symbol),SLIDER_ENDPOINTS:slider_endpoint_defaults(data,slider_endpoints),ICON_BUTTON_DEFAULTS:icon_button_defaults(icon_button),BUTTON_MOTION_DEFAULTS:button_motion_defaults(button_motion),DESIGN_ICON_DEFAULTS:design_icon_defaults(design_icon),MATERIAL_RADIAL_DEFAULTS:material_radial_defaults(material_radial),SEGMENTED_DEFAULTS:segmented_defaults(data,segmented),ROOT/'docs/StyleTokenMembers.md':selector_document(data),
             HEADER:default_color_header(data,HEADER.read_text(encoding='utf-8'))}
    for path,generated in outputs.items():
        if args.check:
            if not path.exists() or path.read_text(encoding='utf-8')!=generated:raise SystemExit('Style token output is stale: '+str(path))
        elif not path.exists() or path.read_text(encoding='utf-8')!=generated:
            # File creation is delegated to the workspace's file provider.
            sys.path.insert(0,str(Path.home()/'.local/SDK/iiFileProvider/share/iiFileProvider/python'))
            import ii_file_provider
            ii_file_provider.write_text(path,generated,encoding='utf-8')
    print('LVUI_STYLE_TOKENS_OK variables='+str(len(json.loads(SNAPSHOT.read_text(encoding='utf-8'))['variables'])))

if __name__=='__main__':main()
