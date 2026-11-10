"""Compile actual generated Window factories with valid and invalid native types."""
from pathlib import Path
from workspace_support import checked_workspace, provider
import argparse, json, subprocess, tempfile

def main():
    parser=argparse.ArgumentParser()
    for name in ('build','compiler','core','color','gui'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--cxx',required=True)
    parser.add_argument('--workspace', type=Path, required=True)
    parser.add_argument('--gui-include', type=Path, required=True)
    parser.add_argument('--gui-export-include', type=Path, required=True)
    args=parser.parse_args();source=Path(__file__).resolve().parents[1]
    args.workspace = checked_workspace(args.workspace)
    cases={
        'valid_geometry':('ii::core::Observable<double> width{720}; ii::core::Observable<std::string> title{std::string("Review")};','title: vm.title, width: vm.width',None),
        'valid_accent_enum':('ii::core::Observable<lvui::WindowAccent> accent{lvui::WindowAccent::green};','accent: vm.accent',None),
        'valid_accent_string':('ii::core::Observable<std::string> accent{std::string("green")};','accent: vm.accent',None),
        'wrong_width':('ii::core::Observable<bool> width{true};','width: vm.width','range'),
        'wrong_title':('ii::core::Observable<int> title{1};','title: vm.title','string_view'),
        'wrong_font':('ii::core::Observable<int> font{1};','fontFamily: vm.font','string_view'),
        'wrong_color':('ii::core::Observable<int> color{1};','backgroundColor: vm.color','color'),
        'wrong_accent_number':('ii::core::Observable<int> accent{1};','accent: vm.accent','window_accent'),
        'wrong_accent_enum':('enum class Other{blue};ii::core::Observable<Other> accent{Other::blue};','accent: vm.accent','window_accent'),
    }
    report={}
    with provider.TemporaryDirectory(prefix='window-types-',dir=args.workspace) as temporary:
        root=Path(temporary)
        includes=[root,source/'src',args.build / 'include', args.build / 'lvui-generated-icons',(args.core/'../../../include').resolve(),(args.color/'../../../include').resolve(),args.gui/'src',args.gui_include, args.gui_export_include]
        for name,(members,inputs,expected) in cases.items():
            provider.write_text(root/'Fixture.h', '#pragma once\n#include <LVUI/NativeWindowValues.h>\n#include <iiSDKCore/Core/Property/Observable.h>\n#include <string>\nstruct Backend final {'+members+'};\n')
            provider.write_text(root/'View.lvui', 'import vm Backend\nWindow('+inputs+') { Label() }\n')
            generation=subprocess.run([str(args.compiler),'--name','WindowContract','--include','Fixture.h',str(root/'View.lvui')],text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            if generation.returncode:raise RuntimeError(f'Window generation failed for {name}: {generation.stderr}')
            provider.write_text(root/'View.generated.h', generation.stdout)
            provider.write_text(root/'probe.cpp', '#include "View.generated.h"\nauto factory=&lvui_generated::WindowContract::build;\n')
            msvc=Path(args.cxx).name.lower() in ('cl','cl.exe')
            command=[args.cxx,'/std:c++latest','/Zs','/utf-8'] if msvc else [args.cxx,'-std=c++23','-fsyntax-only']
            for include in includes:command.extend(['/I' if msvc else '-I',str(include)])
            compiled=subprocess.run([*command,str(root/'probe.cpp')],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
            if (expected is None and compiled.returncode) or (expected is not None and (compiled.returncode==0 or expected not in compiled.stdout)):
                raise RuntimeError(f'Unexpected C++ Window contract for {name}: {compiled.stdout}')
            report[name]={'compiled':compiled.returncode==0,'expected_rejection':expected}
    path=args.workspace/'lvui-completion/native-window-independent-type-validation.json';provider.mkdir(path.parent, exist_ok=True)
    provider.write_text(path, json.dumps(report,indent=2)+'\n');print(f'Native Window C++ contracts: 3 valid, 6 rejected; report {path}')

if __name__=='__main__':main()
