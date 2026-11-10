"""Compile generated deferred references and check native receiver/signature contracts."""
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
    simple='struct Node final {void act()&{}};'
    cases={
        'root_method':('struct Backend final {void act(){}};','Window { PushButton(vm.act) }',True),
        'lvalue_noncopyable':('struct Node final {Node()=default;Node(const Node&)=delete;void act()&{}};struct Backend final {Node selected;};','Window { PushButton(vm.selected.act) }',True),
        'shared_const':('struct Node final {void act()const{}};struct Backend final {std::shared_ptr<const Node> selected=std::make_shared<Node>();};','Window { PushButton(vm.selected.act) }',True),
        'borrowed_pointer':(simple+'struct Backend final {Node owned;Node* selected=&owned;};','Window { PushButton((vm.selected).act) }',True),
        'standard_reference':(simple+'struct Backend final {Node owned;std::reference_wrapper<Node> selected{owned};};','Window { PushButton(vm.selected.act) }',True),
        'immovable_rvalue':('struct Node final {Node()=default;Node(const Node&)=delete;Node(Node&&)=delete;void act()&&{}};struct Backend final {Node make(){return {};}};','Window { PushButton(vm.make().act) }',True),
        'owned_unique_handle':(simple+'struct Backend final {std::unique_ptr<Node> make(){return std::make_unique<Node>();}};','Window { PushButton(vm.make().act) }',True),
        'typed_string_overload':('struct Node final {void act(std::string){};void act(double){}};struct Backend final {Node selected;};','let run: (String) -> Void = vm.selected.act\nWindow { InputField(onEdited: run) }',True),
        'typed_result':('struct Node final {std::string describe()const{return "Native";}};struct Backend final {Node selected;};','let render: () -> String = vm.selected.describe\nWindow { Label(text: render()) }',True),
        'immovable_parent_member':('struct Node final {void act()const{}};struct Parent final {Node child;Parent()=default;Parent(const Parent&)=delete;Parent(Parent&&)=delete;};struct Backend final {Parent make(){return {};}};','Window { PushButton(vm.make().child.act) }',True),
        'immovable_parent_getter':('struct Node final {void act()const{}};struct Parent final {Node child;Parent()=default;Parent(const Parent&)=delete;Parent(Parent&&)=delete;const Node& selected()const{return child;}};struct Backend final {Parent make(){return {};}};','Window { PushButton(vm.make().selected().act) }',True),
        'noncopyable_indexed':('struct Node final {Node()=default;Node(const Node&)=delete;void act(){}};struct Backend final {std::array<Node,1> rows;};','Window { PushButton(vm.rows[0].act) }',True),
        'private_method':('class Node final {void act(){}};struct Backend final {Node selected;};','Window { PushButton(vm.selected.act) }',False),
        'private_parent':(simple+'class Backend final {Node selected;};','Window { PushButton(vm.selected.act) }',False),
        'const_receiver':(simple+'struct Backend final {std::shared_ptr<const Node> selected=std::make_shared<Node>();};','Window { PushButton(vm.selected.act) }',False),
        'missing_method':(simple+'struct Backend final {Node selected;};','Window { PushButton(vm.selected.missing) }',False),
        'wrong_arity':('struct Node final {void act(int){}};struct Backend final {Node selected;};','Window { PushButton(vm.selected.act) }',False),
        'wrong_payload':('struct Node final {void act(bool){}};struct Backend final {Node selected;};','let run: (String) -> Void = vm.selected.act\nWindow { InputField(onEdited: run) }',False),
        'wrong_void_result':('struct Node final {int act(){return 1;}};struct Backend final {Node selected;};','let run: () -> Void = vm.selected.act\nWindow { PushButton(run) }',False),
        'wrong_receiver_category':('struct Node final {void act()&&{}};struct Backend final {Node selected;};','Window { PushButton(vm.selected.act) }',False),
        'void_receiver':('struct Backend final {void nothing(){}};','Window { PushButton(vm.nothing().act) }',False),
    }
    result={}
    with provider.TemporaryDirectory(prefix='native-method-reference-types-',dir=args.workspace) as temporary:
        root=Path(temporary);includes=[root,source/'src',args.build / 'include', args.build / 'lvui-generated-icons',(args.core/'../../../include').resolve(),(args.color/'../../../include').resolve(),args.gui/'src',args.gui_include, args.gui_export_include]
        for name,(members,body,valid) in cases.items():
            provider.write_text(root/'Fixture.h', '#pragma once\n#include <memory>\n#include <string>\n#include <functional>\n#include <array>\n#include <cstdint>\n'+members+'\n')
            provider.write_text(root/'View.lvui', 'import vm Backend\n'+body+'\n')
            generated=subprocess.run([str(args.compiler),'--name','ReferenceContract','--include','Fixture.h',str(root/'View.lvui')],text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            if generated.returncode:raise RuntimeError(f'Reference generation failed for {name}: {generated.stderr}')
            provider.write_text(root/'View.generated.h', generated.stdout);provider.write_text(root/'probe.cpp', '#include "View.generated.h"\nauto factory=&lvui_generated::ReferenceContract::build;\n')
            msvc=Path(args.cxx).name.lower() in ('cl','cl.exe');command=[args.cxx,'/std:c++latest','/Zs','/utf-8'] if msvc else [args.cxx,'-std=c++23','-fsyntax-only','-Werror=unused-parameter']
            for include in includes:command.extend(['/I' if msvc else '-I',str(include)])
            compiled=subprocess.run([*command,str(root/'probe.cpp')],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
            if (compiled.returncode==0)!=valid:raise RuntimeError(f'Unexpected deferred method contract for {name}: {compiled.stdout}')
            result[name]={'compiled':compiled.returncode==0,'valid':valid}
    report=args.workspace/'lvui-completion/native-method-reference-type-validation.json';provider.mkdir(report.parent, exist_ok=True)
    provider.write_text(report, json.dumps(result,indent=2)+'\n');print('Deferred method references: 12 valid native contracts, 9 rejected')
if __name__=='__main__':main()
