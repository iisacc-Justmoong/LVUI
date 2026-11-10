"""Compile actual native query factories, including private getter dependencies and access failures."""
from pathlib import Path
from workspace_support import checked_workspace, provider
import argparse,json,subprocess,tempfile
def main():
    parser=argparse.ArgumentParser()
    for name in ('build','compiler','core','color','gui'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--workspace', type=Path, required=True)
    parser.add_argument('--gui-include', type=Path, required=True)
    parser.add_argument('--gui-export-include', type=Path, required=True)
    parser.add_argument('--cxx',required=True);args=parser.parse_args();source=Path(__file__).resolve().parents[1]
    args.workspace = checked_workspace(args.workspace)
    node='class Node final {ii::core::Observable<int> count_{3};public:int count()const{return count_.get();}};'
    cases={
        'value':(node+'struct Backend final {Node selected;};','vm.selected.count()',None),
        'shared_const':(node+'struct Backend final {std::shared_ptr<const Node> selected=std::make_shared<Node>();};','vm.selected.count()',None),
        'pointer_const':(node+'struct Backend final {Node owned;const Node* selected=&owned;};','(vm.selected).count()',None),
        'rvalue_receiver':('struct Node final {int count()&&{return 3;}};struct Backend final {Node make(){return {};}};','vm.make().count()',None),
        'lvalue_receiver':('struct Node final {int count()&{return 3;}};struct Backend final {Node selected;};','vm.selected.count()',None),
        'rvalue_method_on_lvalue':('struct Node final {int count()&&{return 3;}};struct Backend final {Node selected;};','vm.selected.count()','count'),
        'private_method':('class Node final {int hidden()const{return 3;}};struct Backend final {Node selected;};','vm.selected.hidden()','hidden'),
        'missing_method':(node+'struct Backend final {Node selected;};','vm.selected.missing()','missing'),
        'const_method':('struct Node final {int count(){return 3;}};struct Backend final {std::shared_ptr<const Node> selected=std::make_shared<Node>();};','vm.selected.count()','count'),
        'wrong_arity':('struct Node final {int count(int)const{return 3;}};struct Backend final {Node selected;};','vm.selected.count()','count'),
        'wrong_argument':('struct Node final {int count(std::string)const{return 3;}};struct Backend final {Node selected;};','vm.selected.count(2)','count'),
        'void_result':('struct Node final {void count()const{}};struct Backend final {Node selected;};','vm.selected.count()','void'),
        'private_ancestor':(node+'class Backend final {struct Group final {Node selected;};Group hidden;};','vm.hidden.selected.count()','hidden'),
    }
    result={}
    with provider.TemporaryDirectory(prefix='native-method-types-',dir=args.workspace) as temporary:
        root=Path(temporary);includes=[root,source/'src',args.build / 'include', args.build / 'lvui-generated-icons',(args.core/'../../../include').resolve(),(args.color/'../../../include').resolve(),args.gui/'src',args.gui_include, args.gui_export_include]
        for name,(members,expression,expected) in cases.items():
            provider.write_text(root/'Fixture.h', '#pragma once\n#include <iiSDKCore/Core/Property/Observable.h>\n#include <string>\n#include <memory>\n'+members+'\n')
            provider.write_text(root/'View.lvui', 'import vm Backend\nWindow { Label(text: '+expression+') }\n')
            generated=subprocess.run([str(args.compiler),'--name','MethodContract','--include','Fixture.h',str(root/'View.lvui')],text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            if generated.returncode:raise RuntimeError(f'Native method generation failed for {name}: {generated.stderr}')
            provider.write_text(root/'View.generated.h', generated.stdout);provider.write_text(root/'probe.cpp', '#include "View.generated.h"\nauto factory=&lvui_generated::MethodContract::build;\n')
            msvc=Path(args.cxx).name.lower() in ('cl','cl.exe');command=[args.cxx,'/std:c++latest','/Zs','/utf-8'] if msvc else [args.cxx,'-std=c++23','-fsyntax-only']
            for include in includes:command.extend(['/I' if msvc else '-I',str(include)])
            compiled=subprocess.run([*command,str(root/'probe.cpp')],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
            if (expected is None and compiled.returncode) or (expected is not None and (not compiled.returncode or expected not in compiled.stdout)):raise RuntimeError(f'Unexpected native method contract for {name}: {compiled.stdout}')
            result[name]={'compiled':compiled.returncode==0,'expected_rejection':expected}
    report=args.workspace/'lvui-completion/native-method-type-validation.json';provider.mkdir(report.parent, exist_ok=True);provider.write_text(report, json.dumps(result,indent=2)+'\n');print(f'Native C++ methods: 5 valid, 8 rejected; report {report}')
if __name__=='__main__':main()
