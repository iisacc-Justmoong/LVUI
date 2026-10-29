"""Compile real generated factories for native inferred value contracts."""
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
        'method': ('struct Backend {std::string format(std::int64_t n){return std::to_string(n);}};', 'let format = vm.format\nWindow { Label(text: format(2)) }', True),
        'factory': ('struct Backend {auto make(){return [n=std::make_unique<int>(2)](std::int64_t x){return std::to_string(*n+x);};}};', 'let format = vm.make()\nWindow { Label(text: format(2)) }', True),
        'field': ('struct Backend {std::function<void()> run=[]{ };};', 'let run = vm.run\nWindow { PushButton(run) }', True),
        'observable_field': ('struct Backend {ii::core::Observable<std::function<std::string(std::int64_t)>> format{[](auto n){return std::to_string(n);}};};', 'let render = vm.format\nWindow { Label(text: render(2)) }', True),
        'alias': ('struct Backend {auto make(){return []{return std::string("Native");};}};', 'let source = vm.make()\nlet alias = source\nWindow { Label(text: alias()) }', True),
        'fixed': ('struct Backend {auto make(){return []{return std::string("Native");};}};', 'let source = vm.make()\n@Static let frozen = source\nWindow { Label(text: frozen()) }', True),
        'variable': ('struct Backend {auto make(){return []{return std::string("Native");};}};', 'var format = vm.make()\nWindow { Label(text: format()) }', True),
        'typed_input': ('struct Backend {auto make(){return [](std::string){};}};', 'let run = vm.make()\nlet Editor(edit: (String) -> Void): View = InputField(onEdited: edit)\nWindow { Editor(edit: run) }', True),
        'scalar': ('struct Backend {ii::core::Observable<int> count{2};};', 'let count = vm.count\nvar number = vm.count\nWindow { Label(text: count + number) }', True),
        'const_method': ('struct Backend {std::string format(std::int64_t n)const& noexcept{return std::to_string(n);}};', 'let format = vm.format\nWindow { Label(text: format(2)) }', True),
        'function_pointer': ('struct Backend {static std::string invoke(std::int64_t n){return std::to_string(n);} decltype(&invoke) format=&invoke;};', 'let format = vm.format\nWindow { Label(text: format(2)) }', True),
        'referenced_field': ('struct Handler {std::string operator()(std::int64_t n){return std::to_string(n);}}; struct Backend {Handler handler;std::reference_wrapper<Handler> format{handler};};', 'let format = vm.format\nWindow { Label(text: format(2)) }', True),
        'typed_alias': ('struct Backend {auto make(){return []{return std::string("Native");};}};', 'let source: () -> String = vm.make()\nlet alias = source\nWindow { Label(text: alias()) }', True),
        'explicit_overload': ('struct Backend {std::string format(std::int64_t n){return std::to_string(n);} std::string format(std::string n){return n;}};', 'let format: (Int) -> String = vm.format\nWindow { Label(text: format(2)) }', True),
        'wrong_arity': ('struct Backend {auto make(){return [](int){return std::string("Native");};}};', 'let format = vm.make()\nWindow { Label(text: format()) }', False),
        'void_display': ('struct Backend {auto make(){return []{};}};', 'let run = vm.make()\nWindow { Label(text: run()) }', False),
        'scalar_call': ('struct Backend {int count=2;};', 'let count = vm.count\nWindow { Label(text: count()) }', False),
        'ambiguous_overload': ('struct Backend {std::string format(int){return "Int";}std::string format(std::string){return "String";}};', 'let format = vm.format\nWindow { Label(text: format(2)) }', False),
        'wrong_payload': ('struct Backend {auto make(){return [](bool){return std::string("Native");};}};', 'let format = vm.make()\nWindow { Label(text: format(2)) }', False),
    }
    report = {}
    unexpected = []
    with tempfile.TemporaryDirectory(prefix='native-inferred-value-types-', dir=args.build) as temporary:
        root = Path(temporary)
        includes = [root, source / 'src', (args.core / '../../../include').resolve(), (args.color / '../../../include').resolve(), args.gui / 'src', args.gui / 'build/install/include']
        for name, (members, body, valid) in cases.items():
            (root / 'Fixture.h').write_text('#pragma once\n#include <iiSDKCore/Core/Property/Observable.h>\n#include <memory>\n#include <functional>\n#include <string>\n#include <cstdint>\n' + members + '\n')
            (root / 'View.lvui').write_text('import vm Backend\n' + body + '\n')
            generated = subprocess.run([str(args.compiler), '--name', 'InferredValueContract', '--include', 'Fixture.h', str(root / 'View.lvui')], text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if generated.returncode:
                raise RuntimeError(f'Inferred value generation failed for {name}: {generated.stderr}')
            (root / 'View.generated.h').write_text(generated.stdout)
            setter = 'void replace(lvui_generated::InferredValueContract& view){(void)view.set_format_native([]{return []{return std::string("Replaced");};});}\n' if name == 'variable' else ''
            (root / 'probe.cpp').write_text('#include "View.generated.h"\nauto factory=&lvui_generated::InferredValueContract::build;\n' + setter)
            msvc = Path(args.cxx).name.lower() in ('cl', 'cl.exe')
            command = [args.cxx, '/std:c++latest', '/Zs', '/utf-8'] if msvc else [args.cxx, '-std=c++23', '-fsyntax-only']
            for include in includes:
                command.extend(['/I' if msvc else '-I', str(include)])
            compiled = subprocess.run([*command, str(root / 'probe.cpp')], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            report[name] = {'compiled': compiled.returncode == 0, 'valid': valid}
            if (compiled.returncode == 0) != valid:
                unexpected.append(name)
                print(f'Unexpected inferred value contract: {name}\n{compiled.stdout[:6500]}', flush=True)
    destination = args.build / 'lvui-completion/native-inferred-value-type-validation.json'
    destination.parent.mkdir(exist_ok=True)
    destination.write_text(json.dumps(report, indent=2) + '\n')
    if unexpected:
        raise RuntimeError('Inferred value mismatches: ' + ', '.join(unexpected))
    print(f'Inferred values: {sum(item[2] for item in cases.values())} valid native contracts, {sum(not item[2] for item in cases.values())} rejected')


if __name__ == '__main__':
    main()
