"""Type-check readonly C++ objects and private member access in actual generated factories."""
from pathlib import Path
import argparse, json, subprocess, tempfile

def main():
    parser=argparse.ArgumentParser()
    for name in ('build','compiler','core','color','gui'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--cxx',required=True)
    args=parser.parse_args();source_root=Path(__file__).resolve().parents[1]
    cases={
        'shared':('struct Node final {ii::core::Observable<int> count{3};};struct Backend final {std::shared_ptr<const Node> selected=std::make_shared<Node>();};','item.count',None),
        'pointer':('struct Node final {ii::core::Observable<int> count{3};};struct Backend final {Node owned;const Node* selected=&owned;};','item.count',None),
        'reference':('struct Node final {ii::core::Observable<int> count{3};};struct Backend final {Node owned;std::reference_wrapper<const Node> selected{owned};};','item.count',None),
        'record':('struct Node final {std::string title;int count;};struct Backend final {Node selected{"Record",3};};','item.count',None),
        'private_field':('class Node final {ii::core::Observable<int> private_count{3};};struct Backend final {std::shared_ptr<const Node> selected=std::make_shared<Node>();};','item.private_count','private_count'),
        'private_ancestor':('class Node final {struct Group final {ii::core::Observable<int> count{3};};Group hidden;};struct Backend final {std::shared_ptr<const Node> selected=std::make_shared<Node>();};','item.hidden.count','hidden'),
        'missing_field':('struct Node final {ii::core::Observable<int> count{3};};struct Backend final {std::shared_ptr<const Node> selected=std::make_shared<Node>();};','item.missing_field','missing_field'),
        'wrong_object':('struct Backend final {std::shared_ptr<const int> selected=std::make_shared<int>(3);};','item.count','count'),
        'mutable_only_observer':('struct Source final {ii::core::Observable<int> value{3};const int& get()const{return value.get();}std::uint64_t revision()const{return value.revision();}ii::core::ObservableSubscription observe(std::function<void(const int&,std::uint64_t)> callback){return value.observe(std::move(callback));}};struct Node final {Source count;};struct Backend final {std::shared_ptr<const Node> selected=std::make_shared<Node>();};','item.count','requires const observe'),
    }
    result={}
    with tempfile.TemporaryDirectory(prefix='readonly-types-',dir=args.build) as temporary:
        root=Path(temporary)
        includes=[root,source_root/'src',(args.core/'../../../include').resolve(),(args.color/'../../../include').resolve(),args.gui/'src',args.gui/'build/install/include']
        for name,(members,member,expected) in cases.items():
            (root/'Fixture.h').write_text('#pragma once\n#include <iiSDKCore/Core/Property/Observable.h>\n#include <string>\n#include <memory>\n#include <functional>\n'+members+'\n')
            (root/'View.lvui').write_text('import vm Backend\nlet item = vm.selected\nWindow { Label(text: '+member+') }\n')
            generation=subprocess.run([str(args.compiler),'--name','ReadonlyContract','--include','Fixture.h',str(root/'View.lvui')],text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            if generation.returncode:raise RuntimeError(f'Readonly generation failed for {name}: {generation.stderr}')
            (root/'View.generated.h').write_text(generation.stdout)
            (root/'probe.cpp').write_text('#include "View.generated.h"\nauto factory=&lvui_generated::ReadonlyContract::build;\n')
            msvc=Path(args.cxx).name.lower() in ('cl','cl.exe')
            command=[args.cxx,'/std:c++latest','/Zs','/utf-8'] if msvc else [args.cxx,'-std=c++23','-fsyntax-only']
            for include in includes:command.extend(['/I' if msvc else '-I',str(include)])
            compiled=subprocess.run([*command,str(root/'probe.cpp')],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
            if (expected is None and compiled.returncode) or (expected is not None and (compiled.returncode==0 or expected not in compiled.stdout)):
                raise RuntimeError(f'Unexpected C++ readonly contract for {name}: {compiled.stdout}')
            result[name]={'compiled':compiled.returncode==0,'expected_rejection':expected}
    report=args.build/'lvui-completion/native-readonly-type-validation.json'
    report.parent.mkdir(exist_ok=True);report.write_text(json.dumps(result,indent=2)+'\n')
    print(f'Native readonly C++ contracts: 4 valid, 5 rejected; report {report}')

if __name__=='__main__':main()
