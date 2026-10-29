"""Compile real generated factories for native callable field contracts."""
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
        'observable_root': ('struct Backend {ii::core::Observable<std::function<void()>> run{[]{}};};', 'Window { PushButton(vm.run) }', True),
        'observable_nested': ('struct Node {ii::core::Observable<std::function<void()>> run{[]{}};};struct Backend {Node node;};', 'Window { PushButton(vm.node.run) }', True),
        'observable_result': ('struct Backend {ii::core::Observable<std::function<std::string(std::int64_t)>> format{[](auto n){return std::to_string(n);}};};', 'let render: (Int) -> String = vm.format\nWindow { Label(text: render(2)) }', True),
        'shared_noncopyable': ('struct Handler {Handler()=default;Handler(const Handler&)=delete;void operator()(){}};struct Backend {ii::core::Observable<std::shared_ptr<Handler>> run{std::make_shared<Handler>()};};', 'Window { PushButton(vm.run) }', True),
        'borrowed_noncopyable': ('struct Handler {Handler()=default;Handler(const Handler&)=delete;void operator()()&{}};struct Backend {Handler run;};', 'Window { PushButton(vm.run) }', True),
        'const_reference': ('struct Handler {void operator()()const{}};struct Backend {const Handler owned;std::reference_wrapper<const Handler> run{owned};};', 'Window { PushButton(vm.run) }', True),
        'function_pointer': ('inline void action(){};struct Backend {void(*run)()=&action;};', 'Window { PushButton(vm.run) }', True),
        'unique_handle': ('struct Handler {void operator()(){}};struct Backend {std::unique_ptr<Handler> run=std::make_unique<Handler>();};', 'Window { PushButton(vm.run) }', True),
        'temporary_parent': ('struct Parent {std::function<void()> run=[]{ };Parent()=default;Parent(const Parent&)=delete;Parent(Parent&&)=delete;};struct Backend {Parent parent(){return {};}};', 'Window { PushButton(vm.parent().run) }', True),
        'typed_default': ('struct Backend {ii::core::Observable<std::function<void(std::string)>> run{[](auto){}};};', 'let Actions(run: (String) -> Void = vm.run): View = InputField(onEdited: run)\nWindow { Actions() }', True),
        'static_and_variable': ('struct Backend {ii::core::Observable<std::function<void()>> run{[]{}};};', '@Static let frozen: () -> Void = vm.run\nvar manual: () -> Void = vm.run\nWindow { VStack { PushButton(frozen)\nPushButton(manual) } }', True),
        'private_field': ('class Backend {std::function<void()> run=[]{ };};', 'Window { PushButton(vm.run) }', False),
        'noncallable_field': ('struct Backend {int run=0;};', 'Window { PushButton(vm.run) }', False),
        'wrong_arguments': ('struct Backend {ii::core::Observable<std::function<void(int)>> run{[](int){}};};', 'Window { PushButton(vm.run) }', False),
        'wrong_result': ('struct Backend {ii::core::Observable<std::function<int()>> run{[]{return 1;}};};', 'let run: () -> Void = vm.run\nWindow { PushButton(run) }', False),
        'borrowed_const_mutable': ('struct Handler {void operator()(){}};struct Backend {const Handler run;};', 'Window { PushButton(vm.run) }', False),
    }
    report = {}
    unexpected = []
    with tempfile.TemporaryDirectory(prefix='native-callable-field-types-', dir=args.build) as temporary:
        root = Path(temporary)
        includes = [root, source / 'src', (args.core / '../../../include').resolve(), (args.color / '../../../include').resolve(), args.gui / 'src', args.gui / 'build/install/include']
        for name, (members, body, valid) in cases.items():
            (root / 'Fixture.h').write_text('#pragma once\n#include <iiSDKCore/Core/Property/Observable.h>\n#include <memory>\n#include <functional>\n#include <string>\n#include <cstdint>\n' + members + '\n')
            (root / 'View.lvui').write_text('import vm Backend\n' + body + '\n')
            generated = subprocess.run([str(args.compiler), '--name', 'CallableFieldContract', '--include', 'Fixture.h', str(root / 'View.lvui')], text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            if generated.returncode:
                raise RuntimeError(f'Field generation failed for {name}: {generated.stderr}')
            (root / 'View.generated.h').write_text(generated.stdout)
            (root / 'probe.cpp').write_text('#include "View.generated.h"\nauto factory=&lvui_generated::CallableFieldContract::build;\n')
            msvc = Path(args.cxx).name.lower() in ('cl', 'cl.exe')
            command = [args.cxx, '/std:c++latest', '/Zs', '/utf-8'] if msvc else [args.cxx, '-std=c++23', '-fsyntax-only']
            for include in includes:
                command.extend(['/I' if msvc else '-I', str(include)])
            compiled = subprocess.run([*command, str(root / 'probe.cpp')], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            report[name] = {'compiled': compiled.returncode == 0, 'valid': valid}
            if (compiled.returncode == 0) != valid:
                unexpected.append(name)
                print(f'Unexpected callable field contract: {name}\n{compiled.stdout[:6500]}', flush=True)
    destination = args.build / 'lvui-completion/native-callable-field-type-validation.json'
    destination.parent.mkdir(exist_ok=True)
    destination.write_text(json.dumps(report, indent=2) + '\n')
    if unexpected:
        raise RuntimeError('Callable field mismatches: ' + ', '.join(unexpected))
    print('Callable fields: 11 valid native contracts, 5 rejected')


if __name__ == '__main__':
    main()
