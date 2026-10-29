"""Compile actual AOT factories to verify native View input type and access contracts."""
from pathlib import Path
import argparse, json, subprocess, tempfile

def main():
    parser=argparse.ArgumentParser()
    for name in ('build','compiler','core','color','gui'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--cxx',required=True)
    args=parser.parse_args();source_root=Path(__file__).resolve().parents[1]
    record='struct Node final {std::string title="Native";};'
    cases={
        'value':(record+'struct Backend final {Node selected;};','item.title',None),
        'shared':(record+'struct Backend final {std::shared_ptr<const Node> selected=std::make_shared<Node>();};','item.title',None),
        'pointer':(record+'struct Backend final {Node owned;const Node* selected=&owned;};','item.title',None),
        'reference':(record+'struct Backend final {Node owned;std::reference_wrapper<const Node> selected{owned};};','item.title',None),
        'nested_owner':(record+'struct Backend final {std::shared_ptr<std::unique_ptr<const Node>> selected=std::make_shared<std::unique_ptr<const Node>>(std::make_unique<Node>());};','item.title',None),
        'wrong_value':(record+'struct Other final {std::string title;};struct Backend final {Other selected;};','item.title','does not match its declared payload type'),
        'wrong_pointee':(record+'struct Backend final {std::shared_ptr<const int> selected=std::make_shared<int>(3);};','item.title','does not match its declared payload type'),
        'unique_owner':(record+'struct Backend final {std::unique_ptr<Node> selected=std::make_unique<Node>();};','item.title','requires a copyable native value'),
        'noncopyable_value':('struct Node final {std::string title;Node()=default;Node(const Node&)=delete;};struct Backend final {Node selected;};','item.title','requires a copyable native value'),
        'throwing_move':('struct Node final {std::string title;Node()=default;Node(const Node&)=default;Node& operator=(Node&&)noexcept(false){return *this;}};struct Backend final {Node selected;};','item.title','requires a copyable native value'),
        'private_field':('class Node final {std::string hidden;};struct Backend final {Node selected;};','item.hidden','hidden'),
        'missing_field':(record+'struct Backend final {Node selected;};','item.missing','missing'),
    }
    result={}
    with tempfile.TemporaryDirectory(prefix='object-input-types-',dir=args.build) as temporary:
        root=Path(temporary)
        includes=[root,source_root/'src',(args.core/'../../../include').resolve(),(args.color/'../../../include').resolve(),args.gui/'src',args.gui/'build/install/include']
        for name,(members,member,expected) in cases.items():
            (root/'Fixture.h').write_text('#pragma once\n#include <iiSDKCore/Core/Property/Observable.h>\n#include <string>\n#include <memory>\n#include <functional>\n'+members+'\n')
            (root/'View.lvui').write_text('import class Node\nimport vm Backend\nlet Details(item: Node): View = Label(text: '+member+')\nWindow { Details(item: vm.selected) }\n')
            generation=subprocess.run([str(args.compiler),'--name','ObjectInputContract','--include','Fixture.h',str(root/'View.lvui')],text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            if generation.returncode:raise RuntimeError(f'Object input generation failed for {name}: {generation.stderr}')
            (root/'View.generated.h').write_text(generation.stdout)
            (root/'probe.cpp').write_text('#include "View.generated.h"\nauto factory=&lvui_generated::ObjectInputContract::build;\n')
            msvc=Path(args.cxx).name.lower() in ('cl','cl.exe')
            command=[args.cxx,'/std:c++latest','/Zs','/utf-8'] if msvc else [args.cxx,'-std=c++23','-fsyntax-only']
            for include in includes:command.extend(['/I' if msvc else '-I',str(include)])
            compiled=subprocess.run([*command,str(root/'probe.cpp')],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
            if (expected is None and compiled.returncode) or (expected is not None and (compiled.returncode==0 or expected not in compiled.stdout)):
                raise RuntimeError(f'Unexpected native input contract for {name}: {compiled.stdout}')
            result[name]={'compiled':compiled.returncode==0,'expected_rejection':expected}
    report=args.build/'lvui-completion/native-object-input-type-validation.json'
    report.parent.mkdir(exist_ok=True);report.write_text(json.dumps(result,indent=2)+'\n')
    print(f'Native object input contracts: 5 valid, 7 rejected; report {report}')

if __name__=='__main__':main()
