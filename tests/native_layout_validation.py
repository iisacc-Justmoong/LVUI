"""Type-check native C++ layout source values in actual generated factories."""
from pathlib import Path
import argparse, json, subprocess, tempfile

def main():
    parser=argparse.ArgumentParser()
    for name in ('build','compiler','core','color','gui'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--cxx',required=True)
    args=parser.parse_args();source_root=Path(__file__).resolve().parents[1]
    cases={
        'valid_axes':('ii::core::Observable<double> width{200},flex{1}; ii::core::Observable<ii::gui::StackAlignmentAxes> alignment{ii::gui::StackAlignmentAxes{}};',
                      'VStack(width: vm.width, flex: vm.flex, alignment: vm.alignment) {}',None),
        'valid_text':('ii::core::Observable<std::string> alignment{std::string("bottomTrailing")};',
                      'ZStack(alignment: vm.alignment) {}',None),
        'wrong_width':('ii::core::Observable<std::string> width{std::string("200")};','VStack(width: vm.width) {}','range'),
        'wrong_flex':('ii::core::Observable<bool> flex{true};','VStack(flex: vm.flex) {}','range'),
        'wrong_alignment':('ii::core::Observable<int> alignment{0};','ZStack(alignment: vm.alignment) {}','stack_alignment'),
        'wrong_alignment_name':('ii::core::Observable<bool> name{false};','VStack(alignmentName: vm.name) {}','string_view'),
    }
    result={}
    with tempfile.TemporaryDirectory(prefix='layout-types-',dir=args.build) as temporary:
        root=Path(temporary)
        includes=[root,source_root/'src',(args.core/'../../../include').resolve(),(args.color/'../../../include').resolve(),args.gui/'src',args.gui/'build/install/include']
        for name,(members,view,expected) in cases.items():
            (root/'Fixture.h').write_text('#pragma once\n#include <iiSDKCore/Core/Property/Observable.h>\n#include <string>\nstruct Backend final {'+members+'};\n')
            (root/'View.lvui').write_text('import vm Backend\nWindow { '+view+' }\n')
            generation=subprocess.run([str(args.compiler),'--name','LayoutContract','--include','Fixture.h',str(root/'View.lvui')],text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            if generation.returncode:raise RuntimeError(f'Layout generation failed for {name}: {generation.stderr}')
            (root/'View.generated.h').write_text(generation.stdout)
            (root/'probe.cpp').write_text('#include "View.generated.h"\nauto factory=&lvui_generated::LayoutContract::build;\n')
            msvc=Path(args.cxx).name.lower() in ('cl','cl.exe')
            command=[args.cxx,'/std:c++latest','/Zs','/utf-8'] if msvc else [args.cxx,'-std=c++23','-fsyntax-only']
            for include in includes:command.extend(['/I' if msvc else '-I',str(include)])
            compiled=subprocess.run([*command,str(root/'probe.cpp')],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
            if (expected is None and compiled.returncode) or (expected is not None and (compiled.returncode==0 or expected not in compiled.stdout)):
                raise RuntimeError(f'Unexpected C++ layout contract for {name}: {compiled.stdout}')
            result[name]={'compiled':compiled.returncode==0,'expected_rejection':expected}
    report=args.build/'lvui-completion/native-layout-type-validation.json'
    report.parent.mkdir(exist_ok=True);report.write_text(json.dumps(result,indent=2)+'\n')
    print(f'Native layout C++ contracts: 2 valid, 4 rejected; report {report}')

if __name__=='__main__':main()
