"""Compile actual native query factories, including private getter dependencies and access failures."""
from pathlib import Path
import argparse,json,subprocess,tempfile
def main():
    parser=argparse.ArgumentParser()
    for name in ('build','compiler','core','color','gui'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--cxx',required=True);args=parser.parse_args();source=Path(__file__).resolve().parents[1]
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
    with tempfile.TemporaryDirectory(prefix='native-method-types-',dir=args.build) as temporary:
        root=Path(temporary);includes=[root,source/'src',(args.core/'../../../include').resolve(),(args.color/'../../../include').resolve(),args.gui/'src',args.gui/'build/install/include']
        for name,(members,expression,expected) in cases.items():
            (root/'Fixture.h').write_text('#pragma once\n#include <iiSDKCore/Core/Property/Observable.h>\n#include <string>\n#include <memory>\n'+members+'\n')
            (root/'View.lvui').write_text('import vm Backend\nWindow { Label(text: '+expression+') }\n')
            generated=subprocess.run([str(args.compiler),'--name','MethodContract','--include','Fixture.h',str(root/'View.lvui')],text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            if generated.returncode:raise RuntimeError(f'Native method generation failed for {name}: {generated.stderr}')
            (root/'View.generated.h').write_text(generated.stdout);(root/'probe.cpp').write_text('#include "View.generated.h"\nauto factory=&lvui_generated::MethodContract::build;\n')
            msvc=Path(args.cxx).name.lower() in ('cl','cl.exe');command=[args.cxx,'/std:c++latest','/Zs','/utf-8'] if msvc else [args.cxx,'-std=c++23','-fsyntax-only']
            for include in includes:command.extend(['/I' if msvc else '-I',str(include)])
            compiled=subprocess.run([*command,str(root/'probe.cpp')],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
            if (expected is None and compiled.returncode) or (expected is not None and (not compiled.returncode or expected not in compiled.stdout)):raise RuntimeError(f'Unexpected native method contract for {name}: {compiled.stdout}')
            result[name]={'compiled':compiled.returncode==0,'expected_rejection':expected}
    report=args.build/'lvui-completion/native-method-type-validation.json';report.parent.mkdir(exist_ok=True);report.write_text(json.dumps(result,indent=2)+'\n');print(f'Native C++ methods: 5 valid, 8 rejected; report {report}')
if __name__=='__main__':main()
