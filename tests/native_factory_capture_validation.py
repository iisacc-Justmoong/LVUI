"""Compile real generated factories for native callable factory lifetime and argument contracts."""
from pathlib import Path
import argparse, json, subprocess, tempfile


def main():
    parser = argparse.ArgumentParser()
    for name in ('build', 'compiler', 'core', 'color', 'gui'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--cxx', required=True)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1]
    cases = {
        'owned_function': ('struct Backend {auto make(){return []{};}};', 'Window { PushButton(vm.make()) }', True),
        'borrowed_noncopyable': ('struct Fn {Fn()=default;Fn(const Fn&)=delete;void operator()()&{}};struct Backend {Fn fn;Fn& make(){return fn;}};', 'Window { PushButton(vm.make()) }', True),
        'const_borrowed': ('struct Fn {void operator()()const{}};struct Backend {const Fn fn;const Fn& make(){return fn;}};', 'Window { PushButton(vm.make()) }', True),
        'function_reference': ('inline void action(){};struct Backend {auto& make(){return action;}};', 'Window { PushButton(vm.make()) }', True),
        'temporary_borrowed': ('struct Fn {void operator()(){}};struct Node {Fn fn;Node()=default;Node(const Node&)=delete;Node(Node&&)=delete;Fn& make(){return fn;}};struct Backend {Node temporary(){return {};}};', 'Window { PushButton(vm.temporary().make()) }', True),
        'immovable_function': ('struct Fn {Fn()=default;Fn(const Fn&)=delete;Fn(Fn&&)=delete;void operator()()&{}};struct Backend {Fn make(){return {};}};', 'Window { PushButton(vm.make()) }', True),
        'shared_handler': ('struct Fn {Fn()=default;Fn(const Fn&)=delete;void operator()(){}};struct Backend {auto make(){return std::make_shared<Fn>();}};', 'Window { PushButton(vm.make()) }', True),
        'temporary_argument': ('struct Arg {Arg()=default;Arg(const Arg&)=delete;Arg(Arg&&)=delete;};struct Backend {Arg argument(){return {};}auto make(const Arg&){return []{};}};', 'Window { PushButton(vm.make(vm.argument())) }', True),
        'rvalue_argument': ('struct Arg {};struct Backend {Arg argument(){return {};}auto make(Arg&&){return []{};}void make(Arg&)=delete;};', 'Window { PushButton(vm.make(vm.argument())) }', True),
        'lvalue_argument': ('struct Arg {Arg()=default;Arg(const Arg&)=delete;Arg(Arg&&)=delete;};struct Backend {Arg argument;auto make(Arg&){return []{};}void make(Arg&&)=delete;};', 'Window { PushButton(vm.make(vm.argument)) }', True),
        'immovable_value_argument': ('struct Arg {Arg()=default;Arg(const Arg&)=delete;Arg(Arg&&)=delete;};struct Backend {Arg argument(){return {};}auto make(Arg){return []{};}};', 'Window { PushButton(vm.make(vm.argument())) }', True),
        'mixed_immovable_arguments': ('struct Arg {Arg()=default;Arg(const Arg&)=delete;Arg(Arg&&)=delete;};struct Node {auto make(const Arg&,Arg){return []{};}};struct Backend {Node node;Arg argument(){return {};}};', 'Window { PushButton(vm.node.make(vm.argument(),vm.argument())) }', True),
        'result_function': ('struct Backend {auto make(){return []{return std::string("Native");};}};', 'let render: () -> String = vm.make()\nWindow { Label(text: render()) }', True),
        'moved_result': ('struct Fn {Fn()=default;Fn(const Fn&)=delete;Fn(Fn&&)=default;void operator()()&{}};struct Backend {Fn fn;Fn&& make(){return std::move(fn);}};', 'Window { PushButton(vm.make()) }', True),
        'void_factory': ('struct Backend {void make(){}};', 'Window { PushButton(vm.make()) }', False),
        'numeric_factory': ('struct Backend {int make(){return 1;}};', 'Window { PushButton(vm.make()) }', False),
        'private_factory': ('class Backend {auto make(){return []{};}};', 'Window { PushButton(vm.make()) }', False),
        'wrong_payload': ('struct Backend {auto make(){return [](bool){};}};', 'let run: (String) -> Void = vm.make()\nWindow { InputField(onEdited: run) }', False),
        'wrong_result': ('struct Backend {auto make(){return []{return 1;};}};', 'let run: () -> Void = vm.make()\nWindow { PushButton(run) }', False),
        'void_argument': ('struct Backend {void argument(){};auto make(int){return []{};}};', 'Window { PushButton(vm.make(vm.argument())) }', False),
    }
    report = {}
    unexpected = []
    with tempfile.TemporaryDirectory(prefix='native-factory-capture-types-', dir=args.build) as temporary:
        root = Path(temporary)
        includes = [root, source / 'src', args.build / 'lvui-generated-icons', (args.core / '../../../include').resolve(), (args.color / '../../../include').resolve(), args.gui / 'src', args.gui / 'build/install/include']
        for name, (members, body, valid) in cases.items():
            (root / 'Fixture.h').write_text('#pragma once\n#include <iiSDKCore/Core/Property/Observable.h>\n#include <memory>\n#include <functional>\n#include <string>\n#include <cstdint>\n' + members + '\n')
            (root / 'View.lvui').write_text('import vm Backend\n' + body + '\n')
            generated = subprocess.run([str(args.compiler), '--name', 'FactoryCaptureContract', '--include', 'Fixture.h', str(root / 'View.lvui')], text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if generated.returncode:
                raise RuntimeError(f'Factory generation failed for {name}: {generated.stderr}')
            (root / 'View.generated.h').write_text(generated.stdout)
            (root / 'probe.cpp').write_text('#include "View.generated.h"\nauto factory=&lvui_generated::FactoryCaptureContract::build;\n')
            msvc = Path(args.cxx).name.lower() in ('cl', 'cl.exe')
            command = [args.cxx, '/std:c++latest', '/Zs', '/utf-8'] if msvc else [args.cxx, '-std=c++23', '-fsyntax-only']
            for include in includes:
                command.extend(['/I' if msvc else '-I', str(include)])
            compiled = subprocess.run([*command, str(root / 'probe.cpp')], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            report[name] = {'compiled': compiled.returncode == 0, 'valid': valid}
            if (compiled.returncode == 0) != valid:
                unexpected.append(name)
                print(f'Unexpected factory capture contract: {name}\n{compiled.stdout[:6500]}', flush=True)
    destination = args.build / 'lvui-completion/native-factory-capture-type-validation.json'
    destination.parent.mkdir(exist_ok=True)
    destination.write_text(json.dumps(report, indent=2) + '\n')
    if unexpected:
        raise RuntimeError('Factory capture mismatches: ' + ', '.join(unexpected))
    print('Factory captures: 14 valid native contracts, 6 rejected')


if __name__ == '__main__':
    main()
