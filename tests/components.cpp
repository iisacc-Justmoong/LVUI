#include <LVUI/Compiler.h>
#include <LVUI/Language.h>
#include <iostream>
#include <stdexcept>
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
std::string compile(std::string_view source, std::string framework = "iiGUI") {
    auto result = lvui::compile_native(source, {.framework = std::move(framework)});
    if (!result) throw std::runtime_error(result.error().message);
    return *result;
}
void syntax() {
    auto parsed = lvui::parse_view("import class Domain.Model\nlet Heading(title: String, count: Int = 2): View = Label(text: title)\nWindow { Heading(title: \"Ready\") }");
    check(bool(parsed), "A typed reusable View signature was rejected");
    check(parsed->bindings()[0].type == "View", "A component result lost its View type");
    check(parsed->bindings()[0].parameters && parsed->bindings()[0].parameters->size() == 2 &&
          parsed->bindings()[0].parameters->at(1).default_value->text == "2",
          "A component signature lost its typed parameters or default value");
    auto code = compile("let Heading(title: String): View = Label(text: title)\nWindow { Heading(title: \"Ready\") }");
    check(code.find("Ready") != code.npos, "A scalar parameter did not reach its View");
}
void borrowing() {
    const auto code = compile("import class Domain.Model\nimport document Domain.Model\nlet Body(backend: Model): View = VStack { Label(text: backend.title)\nPushButton(backend.increment) }\nWindow { VStack { Body(backend: document)\nBody(backend: document) } }");
    check(code.find("scope->backend_0.title") != code.npos && code.find("object).increment(std::forward") != code.npos,
          "A borrowed backend lost its property or deferred method");
    check(code.find(" backend_1;") == code.npos, "Passing a backend constructed another owner");
    const auto nested = compile("import class Domain.Model\nimport document Domain.Model\nlet Inner(backend: Model): View = Label(text: backend.title)\nlet Outer(backend: Model): View = VStack { import document Other\nInner(backend: backend) }\nWindow { Outer(backend: document) }");
    check(nested.find("scope->parent->backend_0.title") != nested.npos,
          "A forwarded backend was captured by a shadowing private import");
    const auto produced=compile("import class Domain.Model\nimport vm Domain.Model\nlet Box(backend: Model): View = Label()\nWindow { Box(backend: vm.make()) }");
    check(produced.find("object_input<::Domain::Model>")!=produced.npos,
          "A returned C++ value did not retain its declared payload check even when unused");
}
void slots() {
    auto code = compile("let Box(content: View): View = VStack { import vm Private\ncontent()\ncontent() }\nWindow { import vm Caller\nlet body: View = Label(text: vm.title)\nBox(content: body) }");
    check(code.find("scope->parent->backend_0.title") != code.npos,
          "A named View slot lost its caller environment");
    auto typed = compile("let Box(content: View): View = TabBar { content() }\nWindow { Box(content: Tab(iconName: \"generaladd\", text: \"One\")) }", "iiGUI");
    check(typed.find("lvui::native::NativeTab::create(") != typed.npos, "An inline named View slot lost its concrete child type");
    check(!lvui::compile_native("let Box(content: View): View = TabBar { content() }\nWindow { Box(content: Label()) }", {.framework="iiGUI"}),
          "A named View slot bypassed the receiving widget's child type");
}
void defaults() {
    auto code = compile("import vm Backend\nlet fallback: View = Label(text: \"Default body\")\nlet Box(title: String = vm.title, content: View = fallback): View = VStack { Label(text: title)\ncontent() }\nWindow { import vm Other\nBox() }");
    check(code.find("scope->parent->backend_0.title") != code.npos && code.find("Default body") != code.npos,
          "An omitted argument did not use the definition's default environment");
    auto dependent = compile("let Box(title: String, subtitle: String = title): View = Label(text: subtitle)\nWindow { Box(title: \"Shared\") }");
    check(dependent.find("Shared") != dependent.npos, "An earlier parameter was unavailable to a default");
    auto root = compile("let App(title: String, width: Int = 800): View = Window(title: title, width: width) { Label(text: title) }\nApp(title: \"Native\")");
    check(root.find(".size = {800, 420}") != root.npos && root.find("Native") != root.npos,
          "A parameterized Window root did not retain its static creation inputs");
    const auto slot_root = compile("let Pass(content: View): View = content()\nlet page: View = Window { Label(text: \"Passed root\") }\nPass(content: page)");
    check(slot_root.find("Passed root") != slot_root.npos, "A View parameter could not supply the root construction");
}
void callbacks() {
    const auto code = compile("import vm Backend\nlet Action(run: () -> Void): View = PushButton(run)\nWindow { Action(run: vm.save) }");
    check(code.find("invoke_callback<void()>") != code.npos && code.find(".save(") != code.npos,
          "A declared callback did not preserve its native signature and receiver");
    const auto forwarded = compile("import vm Backend\nlet Inner(run: () -> Void, edit: (String) -> Void): View = VStack { PushButton(run)\nInputField(onEdited: edit) }\nlet Outer(run: () -> Void, edit: (String) -> Void = vm.record): View = VStack { import vm Other\nInner(run: run, edit: edit) }\nWindow { Outer(run: vm.make(vm.amount)) }");
    check(forwarded.find("invoke_callback<void(std::string)>") != forwarded.npos &&
          forwarded.find("object).record(std::forward") != forwarded.npos &&
          forwarded.find("scope->parent->backend_0.make(") != forwarded.npos,
          "Forwarded callbacks or definition-site defaults lost their native owner");
    const auto parsed = lvui::parse_view("let Entry(edit: (String, Bool) -> Void): View = Label()\nWindow { Label() }");
    check(bool(parsed), "An explicit native callback signature was rejected");
    const auto& signature=parsed->bindings()[0].parameters->at(0).callable;
    check(signature&&signature->arguments==std::vector<std::string>{"String","Bool"}&&signature->result=="Void",
          "The front end lost a callback's declared argument or result types");
    check(bool(lvui::parse_view("let Entry(edit: (\nString,\nBool,\n)\n->\nVoid): View = Label()\nWindow { Label() }")),
          "A multiline callback signature was rejected");
    check(!compile("import class Domain.Event\nimport vm Backend\nlet Action(run: (Event) -> Void): View = AlertButton(method: run)\nWindow { Action(run: vm.accept) }","iiGUI").empty(),
          "An imported C++ event type was rejected");
    const auto shared=compile("import vm Backend\nlet Leaf(run: () -> Void): View = PushButton(run)\nlet Pair(run: () -> Void, again: () -> Void = run): View = VStack { Leaf(run: run)\nLeaf(run: again) }\nWindow { Pair(run: vm.make()) }");
    const auto factory=shared.find("make_callback_value<void()>");
    check(factory!=shared.npos&&shared.find("make_callback_value<void()>",factory+1)==shared.npos,
          "A component callback input was materialized separately for each consumer");
    for(const auto source:{
        "import vm Backend\nlet Body(run: () -> Void): View = Window { PushButton(run) }\nlet App(run: () -> Void): View = Body(run: run)\nApp(run: vm.make())",
        "import vm Backend\nlet Maybe(run: () -> Void): View = if (vm.visible) { PushButton(run) }\nWindow { Maybe(run: vm.make()) }",
        "import vm Backend\nlet Leaf(run: () -> Void): View = PushButton(run)\nlet Forward(run: () -> Void): View = Leaf(run: run)\nWindow { Forward(run: vm.make()) }"})
        check(!compile(source).empty(),"A root, conditional or forwarding view lost its shared callback activation");
}
void callback_diagnostics() {
    for (const auto source : {
        "import vm Backend\nlet Run(action: () -> Void): View = PushButton(action)\nWindow { Run(action: 1) }",
        "import vm Backend\nlet Run(action: () -> Void): View = PushButton(action)\nWindow { Run(action: missing.save) }",
        "import vm Backend\nlet Run(action: () -> Bool): View = PushButton(action)\nWindow { Run(action: vm.save) }",
        "import vm Backend\nlet Run(action: (Void) -> Void): View = PushButton(action)\nWindow { Run(action: vm.save) }",
        "import class Domain.Void\nimport vm Backend\nlet Run(action: (Void) -> Void): View = PushButton(action)\nWindow { Run(action: vm.save) }",
        "import vm Backend\nlet Run(action: (Unknown) -> Void): View = PushButton(action)\nWindow { Run(action: vm.save) }",
        "import vm Backend\nlet Inner(action: (String) -> Void): View = InputField(onEdited: action)\nlet Outer(action: () -> Void): View = Inner(action: action)\nWindow { Outer(action: vm.save) }",
        "let Run(action: () Void): View = Label()\nWindow { Label() }",
        "let Run(action: (String -> Void): View = Label()\nWindow { Label() }"}) {
        const auto result = lvui::compile_native(source);
        check(!result && result.error().line > 0 && result.error().column > 0,
              "An invalid native callback contract was accepted without a source diagnostic");
    }
}
void callback_limits() {
    std::string signature = "let Many(action: (";
    for (int n=0;n<16;++n) {if(n)signature+=", ";signature+="String";}
    check(bool(lvui::parse_view(signature+") -> Void): View = Label()\nWindow { Label() }")),
          "A bounded callback signature was rejected");
    const auto rejected=lvui::parse_view(signature+", String) -> Void): View = Label()\nWindow { Label() }");
    check(!rejected && rejected.error().message.find("16")!=rejected.error().message.npos,
          "An unbounded callback signature was accepted");
}
void function_values(){
    const auto generated=compile("import vm Backend\nlet Info(format: (Int) -> String, accepts: (Int) -> Bool): View = VStack { Label(text: format(vm.count))\nPushButton(enabled: accepts(vm.count)) }\nWindow { Info(format: vm.format, accepts: vm.accepts) }");
    check(generated.find("invoke_callback<std::string(std::int64_t)>")!=generated.npos&&generated.find("invoke_callback<bool(std::int64_t)>")!=generated.npos,
          "Value-returning native function signatures were not generated");
    const auto factory=compile("import vm Backend\nlet Info(format: (Int) -> String): View = Label(text: format(vm.count))\nWindow { Info(format: vm.makeFormatter(vm.amount)) }");
    check(factory.find("make_callback_value<std::string(std::int64_t)>")!=factory.npos&&factory.find("read_function(context")!=factory.npos,
          "A native function factory did not expose reactive value invocation");
    const auto declarations=compile("import vm Backend\nlet format: (Int) -> String = vm.format\nlet dynamic: (Int) -> String = vm.makeFormatter(vm.amount)\n@Static let frozen: (Int) -> String = vm.makeFormatter(vm.amount)\nlet Info(render: (Int) -> String = dynamic): View = Label(text: render(vm.count))\nWindow { VStack { Label(text: format(2))\nLabel(text: frozen(vm.count))\nInfo() } }");
    check(declarations.find("make_callback_value<std::string(std::int64_t)>")!=declarations.npos,
          "Readonly or Static native function declarations were rejected");
    auto parsed=lvui::parse_view("let format: (Int) -> String = vm.format\nWindow { Label() }");
    check(parsed&&parsed->bindings().front().callable&&parsed->bindings().front().callable->result=="String",
          "A function declaration lost its signature in the public AST");
    const auto snapshot=compile("import vm Backend\nlet dynamic: (Int) -> String = vm.makeFormatter(vm.amount)\n@Static let pinned: (Int) -> String = dynamic\nWindow { Label(text: pinned(vm.count)) }");
    check(snapshot.find("snapshot_callback_value")!=snapshot.npos,"A Static function alias did not select a native function version");
    for(const auto source:{
        "import vm Backend\nlet Info(format: (Int) -> String): View = Label(text: format())\nWindow { Info(format: vm.format) }",
        "import vm Backend\nlet Info(action: () -> Void): View = Label(text: action())\nWindow { Info(action: vm.save) }",
        "import vm Backend\nlet Info(test: () -> Bool): View = PushButton(test)\nWindow { Info(test: vm.accept) }"})
        check(!lvui::compile_native(source),"An invalid native function invocation was accepted");
    check(bool(lvui::parse_view("var format: (Int) -> String = vm.format\nWindow { Label() }")),"A typed function variable was rejected");
}
void function_variables(){
    const auto direct=compile("import vm Backend\nvar format: (Int) -> String = vm.format\nlet alias: (Int) -> String = format\n@Static let pinned: (Int) -> String = format\nWindow { VStack { Label(text: format(vm.count))\nLabel(text: alias(vm.count))\nLabel(text: pinned(vm.count)) } }");
    check(direct.find("FunctionVariable<std::string(std::int64_t)>")!=direct.npos&&direct.find("set_format(")!=direct.npos&&direct.find("set_format_native(")!=direct.npos,
          "A function variable did not receive native storage and typed write interfaces");
    check(direct.find("assign_value(")!=direct.npos&&direct.find("snapshot_callback_value(")!=direct.npos,"A function variable bypassed native assignment or Static version selection");
    const auto shared=compile("import vm Backend\nlet dynamic: (Int) -> String = vm.makeFormatter(vm.amount)\nvar format: (Int) -> String = dynamic\nWindow { Label(text: format(vm.count)) }");
    check(shared.find("::from_value(")!=shared.npos,"A function variable did not select an existing function version");
    const auto native=compile("import vm Backend\nvar format: (Int) -> String = vm.makeFormatter(vm.amount)\nWindow { Label(text: format(vm.count)) }");
    check(native.find("::from_native(")!=native.npos&&native.find("Methods::callable([scope]")!=native.npos&&native.find("Methods::argument_value<retained[0]>")!=native.npos,
          "A mutable native factory did not select owned arguments in an untracked initialization context");
    const auto nested=compile("import vm Backend\nvar format: (Int) -> String = vm.format\nlet Body(render: (Int) -> String): View = VStack { var local: (Int) -> String = render\nLabel(text: local(vm.count)) }\nWindow { Body(render: format) }");
    check(nested.find("parent->function_var_format")!=nested.npos,"A local function variable lost its borrowed parent value");
    auto parsed=lvui::parse_view("@Observable var format: (Int) -> String = vm.format\nWindow { Label() }");
    check(parsed&&parsed->bindings().front().mutable_value&&parsed->bindings().front().callable,"The public AST lost mutable function declaration metadata");
    check(!lvui::parse_view("@Static var format: (Int) -> String = vm.format\nWindow { Label() }"),"A Static function variable was accepted");
    check(!lvui::compile_native("import vm Backend\nlet initial: () -> Void = vm.run\nvar format: (Int) -> String = initial\nWindow { Label() }"),"An incompatible function initializer was accepted");
}
void diagnostics() {
    for (const auto source : {
        "let Box(title: String): View = Label(text: title)\nWindow { Box() }",
        "let Box(title: String): View = Label(text: title)\nWindow { Box(title: 42) }",
        "let Box(count: Int): View = Label(text: count)\nWindow { Box(count: 1.5) }",
        "let Box(ready: Bool): View = Label(text: ready)\nWindow { Box(ready: 1) }",
        "let Box(title: String): View = Label(text: title)\nWindow { Box(other: \"bad\") }",
        "let Box(title: String): View = Label(text: title)\nWindow { Box(\"bad\") }",
        "let Box(title: String, title: String): View = Label()\nWindow { Box() }",
        "let Box(title): View = Label()\nWindow { Box() }",
        "var Box(title: String): View = Label()\nWindow { Box() }",
        "let Box(title: String): String = \"bad\"\nWindow { Label() }",
        "let Box(content: View): View = VStack { content() }\nWindow { Box(content: 2) }",
        "let Box(): View = Label()\nWindow { Box() { Label() } }",
        "import class Domain.Model\nimport vm Other\nlet Box(backend: Model): View = Label(text: backend.title)\nWindow { Box(backend: vm) }",
        "import class Domain.Model\nlet Box(backend: Model): View = Label()\nWindow { Box(backend: Model) }",
        "import class Domain.Model\nimport document Domain.Model\nlet Box(backend: Model): View = Label()\nWindow { import class Domain.document\nBox(backend: document) }",
        "let Box(backend: Unknown): View = Label()\nWindow { Box(backend: missing) }",
        "let Box(title: String = 2): View = Label(text: title)\nWindow { Box() }"}) {
        auto result = lvui::compile_native(source);
        check(!result && result.error().line > 0 && result.error().column > 0,
              "An invalid component signature or invocation was accepted without a source diagnostic");
    }
    check(!lvui::compile_native("var value: String = \"Bad\"\nlet Box(count: Int): View = Label(text: count)\nWindow { Box(count: value) }"),
          "A known reactive scalar mismatch was deferred to the C++ compiler");
}
void limits() {
    std::string definition = "let Many(";
    for (int n = 0; n < 64; ++n) {
        if (n) definition += ", ";
        definition += "p" + std::to_string(n) + ": Int = 1";
    }
    check(!compile(definition + "): View = VStack(spacing: p63) { Label(text: \"Bounded\") }\nWindow { Many() }").empty(),
          "A bounded component signature was rejected");
    auto rejected = lvui::parse_view(definition + ", extra: Int = 1): View = Label()\nWindow { Many() }");
    check(!rejected && rejected.error().message.find("64") != rejected.error().message.npos,
          "An unbounded component signature was accepted");
}
}
int main(int argc, char** argv) {
    try {
        const std::string name = argc > 1 ? argv[1] : "syntax";
        if (name == "syntax") syntax(); else if (name == "borrowing") borrowing();
        else if (name == "slots") slots(); else if (name == "defaults") defaults();
        else if (name == "callbacks") callbacks(); else if (name == "callback_diagnostics") callback_diagnostics();
        else if (name == "callback_limits") callback_limits();
        else if (name == "function_values") function_values();
        else if (name == "function_variables") function_variables();
        else if (name == "diagnostics") diagnostics(); else if (name == "limits") limits();
        else throw std::runtime_error("Unknown component test");
        std::cout << "LVUI components " << name << " passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
