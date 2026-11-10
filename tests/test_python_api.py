"""Headless contract tests for the installed native LVUI compiler bridge."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'src' / 'Python'))
import ii_lvui


class PythonCompilerTests(unittest.TestCase):
    def test_default_state_and_binding(self):
        code = ii_lvui.compile_native('''const heading: String = "Ready"
var draft: String = heading
Window { InputField(text: draft) }''')
        self.assertIn('automatic_binding<std::string>', code)
        with self.assertRaises(ii_lvui.CompileError):
            ii_lvui.compile_native('@Static @Bindable const draft = "Ready"\nWindow { Label(text: draft) }')
        with self.assertRaises(ii_lvui.CompileError):
            ii_lvui.compile_native('const value: Int = 0\nWindow { PushButton(onClick: { value = 1 }) }')

    def test_reactive_counter_uses_cpp_view_model(self):
        source = (Path(__file__).resolve().parents[1] / 'docs' / 'examples' /
                  'ReactiveCounter.lvui').read_text(encoding='utf-8')
        code = ii_lvui.compile_native(source)
        self.assertIn('widgets.vstack(', code)
        self.assertIn('.increment(', code)
        self.assertIn('.reset(', code)

    def test_counter_example_uses_supported_native_inputs(self):
        source = (Path(__file__).resolve().parents[1] / 'docs' / 'examples' /
                  'Counter.lvui').read_text(encoding='utf-8')
        code = ii_lvui.compile_native(source)
        self.assertIn('.decrement(', code)
        self.assertIn('.increment(', code)

    def test_plain_text_editor(self):
        generated = ii_lvui.compile_native('import vm Backend\nWindow { TextEditor(text: vm.text, onEdited: vm.edit, onContentHeightChanged: vm.measure) }')
        self.assertIn('NativeTextEditor::create', generated)
        self.assertIn('content_height_changed', generated)
        with self.assertRaises(ii_lvui.CompileError):
            ii_lvui.compile_native('Window { TextEditor(filePath: "note.txt") }')

    def test_mobile_navigation(self):
        for name in ('MobileNavigationBar', 'MobileTabBar'):
            code = ii_lvui.compile_native('import vm Backend\nWindow { ' + name + '(model: vm.items, onActivated: vm.activate) }')
            self.assertIn('NativeMobileNavigation::create', code)

    def test_composed_account_button(self):
        code = ii_lvui.compile_native('Window { AbstractButton(accessibilityName: "Account") { Avatar() } }')
        self.assertIn('lvui::native::ButtonBinding<', code)
        self.assertIn('#include <iiGUI/NativeButtonView.h>', code)
        self.assertIn('NativeAvatar::create', code)

    def test_label_presentation(self):
        code = ii_lvui.compile_native('import vm Backend\nWindow { Label(text: vm.text, alignment: vm.alignment, elideRight: vm.elide) }')
        self.assertIn('set_label_options(value)', code)

    def test_standalone_view_and_selector(self):
        counter = ii_lvui.compile_native('''VStack {
    var count: Int = 0
    Label(text: count)
    PushButton(text: "+", onClick: { count = count + 1 })
}''')
        self.assertIn('assign_value(', counter)
        self.assertIn('static ii::core::Result<void> build', counter)
        self.assertIn('NativeListItemSelector::create',
                      ii_lvui.compile_native('ListItemSelector(options: ["One", "Two"])'))
        self.assertIn('Embedded', ii_lvui.compile_native('Label(text: "Embedded")'))
        with self.assertRaises(ii_lvui.CompileError):
            ii_lvui.compile_native('<Label/><Label/>')

    def test_scenes(self):
        source = '''Application {
    Scene(id: "main") { PushButton(onClick: openScene("tools")) }
    Scene(id: "tools", initiallyOpen: false) { Label(text: "Tools") }
}'''
        generated = ii_lvui.compile_native(source)
        self.assertIn('LVUI/Scene.h', generated)
        self.assertIn('SceneDescriptor', generated)
        self.assertIn('scene_registry', generated)
        markup = ii_lvui.compile_native('Application { Scene(id: "main") { Label() } }')
        self.assertIn('SceneDescriptor', markup)
        direct = ii_lvui.compile_native('''Application {
    Scene { PushButton(onClick: { openScene("tools") }) }
    Scene(id: "tools", initiallyOpen: false) { Label() }
}''')
        self.assertIn('scene_registry', direct)
        with self.assertRaises(ii_lvui.CompileError):
            ii_lvui.compile_native('Window { Scene { Label() } }')

    def test_view_session_features(self):
        example = (Path(__file__).resolve().parents[1] / 'example' / 'GettingStarted' /
                   'ViewSessionCounter.lvui').read_text(encoding='utf-8')
        self.assertIn('assign_value(', ii_lvui.compile_native(example))
        code = ii_lvui.compile_native('''import model Backend
Window {
    var count: Int = 0
    var content: View { Label(text: model.title) }
    VStack {
        content()
        PushButton(onClick: { count = count + 1 })
        Label(text: query(model.readPort, model.request()))
    }
}''')
        self.assertIn('assign_value(', code)
        self.assertIn('send_query(', code)
        markup = ii_lvui.compile_native('''var count: Int = 0
Window { PushButton(text: "+", onClick: { count = count + 1 }) }''')
        self.assertIn('assign_value(', markup)
        with self.assertRaises(ii_lvui.CompileError):
            ii_lvui.compile_native('Window { let n = 0\nPushButton(onClick: { n = 1 }) }')

    def test_query_connection(self):
        example = (Path(__file__).resolve().parents[1] / 'tests' / 'fixtures' /
                   'NativeQueryConnections.lvui').read_text(encoding='utf-8')
        generated = ii_lvui.compile_native(example)
        self.assertIn('NativeQueryConnections.h', generated)
        self.assertIn('query_connections()', generated)
        self.assertIn('OverflowPolicy::drop_oldest', generated)
        markup = ii_lvui.compile_native('''import vm Backend
Window { VStack { QueryConnection(source: vm.events, query: vm.selection,
onValue: vm.changed), Label() } }''')
        self.assertIn('query_connections()', markup)
        with self.assertRaises(ii_lvui.CompileError):
            ii_lvui.compile_native('import vm Backend\nWindow { VStack { '
                                   'QueryConnection(source: vm.events, query: vm.selection, '
                                   'onValue: vm.changed, capacity: 0) } }')

    def test_query_peer_view(self):
        example = (Path(__file__).resolve().parents[1] / 'tests' / 'fixtures' /
                   'NativeQueryPeer.lvui').read_text(encoding='utf-8')
        generated = ii_lvui.compile_native(example)
        self.assertIn('send_query(', generated)
        self.assertIn('assign_value(', generated)

    def test_localized_strings(self):
        example = (Path(__file__).resolve().parents[1] / 'example' / 'GettingStarted' /
                   'LocalizedCounter.lvui').read_text(encoding='utf-8')
        generated = ii_lvui.compile_native(example)
        self.assertIn('NativeAppStrings.h', generated)
        self.assertIn('set_locale(', generated)
        self.assertIn('set_string(', generated)
        markup = ii_lvui.compile_native('Window { Label(text: localized("title")) }')
        self.assertIn('NativeAppStrings.h', markup)
        with self.assertRaises(ii_lvui.CompileError):
            ii_lvui.compile_native('Window { Label(text: localized(3)) }')

    def test_compile_native_and_options(self):
        result = ii_lvui.compile_native('Window { Label(text: "안녕") }', name='PythonView',
                                        includes=('Native/Backend.h',))
        self.assertIn('namespace lvui_generated', result)
        self.assertIn('struct PythonView final', result)
        self.assertIn('Native/Backend.h', result)
        self.assertNotIn('<LVRS/', result)
        self.assertNotIn('<Qt', result)
        icon = ii_lvui.compile_native('Window { Image(iconName: "brush") }', name='IconView')
        self.assertIn('ii::gui::IconView::create', icon)
        html = ii_lvui.compile_native('Window { Image(iconName: "brush") }')
        self.assertIn('ii::gui::IconView::create', html)
        segments = ii_lvui.compile_native('Window { IconSegmentedControl() }')
        self.assertIn('lvui::icon_segmented_configuration()', segments)

    def test_style_profile_and_header(self):
        profile = 'let inspectorWidth: Metric = 352\n'
        code = ii_lvui.compile_native('Window { VStack(width: Metric.inspectorWidth) {} }',
                                      style_profile=profile)
        self.assertIn('.extent = {352, 0}', code)
        header = ii_lvui.compile_style_header(profile, namespace='Example.Theme')
        self.assertIn('namespace Example::Theme', header)
        self.assertIn('inspectorWidth = 352', header)

    def test_application_file_events(self):
        for source in (
            'Application(onOpenFiles: vm.open_files) { import vm Backend\n Window { Label() } }',
            'import vm Backend\nApplication(onOpenFiles: vm.open_files) { Window { Label() } }',
        ):
            generated = ii_lvui.compile_native(source)
            self.assertIn('callbacks.files_opened = file_open_handler(application)', generated)
            self.assertIn('LVUI/ApplicationFileEvents.h', generated)
            self.assertNotIn('<LVRS/', generated)
            self.assertNotIn('<Qt', generated)

    def test_application_launch_arguments(self):
        for source in (
            'Application(onLaunchArguments: vm.launch) { import vm Backend\n Window { Label() } }',
            'import vm Backend\nApplication(onLaunchArguments: vm.launch) { Window { Label() } }',
        ):
            generated = ii_lvui.compile_native(source)
            self.assertIn('launch_arguments_attachment', generated)
            self.assertNotIn('<LVRS/', generated)
            self.assertNotIn('<Qt', generated)

    def test_application_close_request(self):
        for source in (
            'Application(onCloseRequested: vm.can_close) { import vm Backend\n Window { PushButton(onClick: quitApplication()) } }',
            'import vm Backend\nApplication(onCloseRequested: vm.can_close) { Window { PushButton(onClick: quitApplication()) } }',
        ):
            generated = ii_lvui.compile_native(source)
            self.assertIn('close_request_handler', generated)
            self.assertIn('ApplicationCommands.h', generated)
            self.assertNotIn('<LVRS/', generated)
            self.assertNotIn('<Qt', generated)

    def test_application_dynamic_window_target(self):
        for source in (
            'Application { import vm Backend\n Window { PushButton(onClick: openWindow(vm.target)) }\n Window(id: "tools", initiallyOpen: false) { Label() } }',
            'import vm Backend\nApplication(onReady: openWindow(vm.target)) { Window { Label() }, Window(id: "tools", initiallyOpen: false) { Label() } }',
        ):
            generated = ii_lvui.compile_native(source)
            self.assertIn('selected_window_id', generated)
            self.assertIn('LVUI/SceneRegistry.h', generated)
            self.assertNotIn('<LVRS/', generated)
            self.assertNotIn('<Qt', generated)

    def test_avatar_uses_the_selected_shared_surface(self):
        code = ii_lvui.compile_native('Window { Avatar() }',
                                      style_profile='let surfaceRaised: Color = Color(0.1, 0.2, 0.3, 0.4)')
        self.assertIn('value.configuration.background = ii::gui::avatar_color('
                      'ii::gui::WidgetColor{0.1, 0.2, 0.3, 0.4})', code)
        self.assertNotIn('lvui::native::avatar_color(', code)

    def test_immediate_view_declaration(self):
        source = '''import vm Backend
let RandomString: () -> String = vm.randomString
let random = vm.icon
Window { HStack(id: toolbar) { gap: Metric.space8, IconButton(label: RandomString(), icon: random) { onClick: vm.activate() } } }'''
        generated = ii_lvui.compile_native(source)
        self.assertIn('scope->backend_0.activate()', generated)
        self.assertIn('widgets.hstack(', generated)
        with self.assertRaises(ii_lvui.CompileError):
            ii_lvui.compile_native('<Window><Label/></Window>')

    def test_tagless_event_script(self):
        generated = ii_lvui.compile_native('''import vm PanelModel
@State var expanded: Bool = false
Window { PushButton(onClick: {
    expanded = true
    vm.didOpen(expanded)
}) }''')
        self.assertIn('assign_value(', generated)
        self.assertIn('scope->backend_0.didOpen(', generated)
        with self.assertRaises(ii_lvui.CompileError):
            ii_lvui.compile_native('Window { PushButton(onClick: { missing.call() }) }')
        for source in ('var object: Object = { value: 1 }\nWindow {}',
                       'import vm Backend\nlet run: () -> Void = { vm.run() }\nWindow {}'):
            with self.subTest(source=source), self.assertRaises(ii_lvui.CompileError):
                ii_lvui.compile_native(source)

    def test_table_column_is_view_declaration(self):
        code = ii_lvui.compile_native('Window { Table(columnViews: [TableColumn() { key: "a", header: Label() }]) }')
        self.assertIn('NativeTableViews', code)
        with self.assertRaises(ii_lvui.CompileError):
            ii_lvui.compile_native('Window { Table(columnViews: [{key: "a", header: Label()}]) }')

    def test_explicit_layout_stacks(self):
        generated = ii_lvui.compile_native('Window { ZStack { VStack { Label() }\n HStack { Label() } } }')
        for stack in ('widgets.zstack(', 'widgets.vstack(', 'widgets.hstack('):
            self.assertIn(stack, generated)
        with self.assertRaises(ii_lvui.CompileError) as raised:
            ii_lvui.compile_native('Window { div { Label() } }')
        self.assertIn('HStack', str(raised.exception))
        self.assertIn('widgets.vstack(', ii_lvui.compile_native('VStack { Label(text: "A") }'))
        self.assertEqual(ii_lvui.compile_native('VStack { Label }'),
                         ii_lvui.compile_native('VStack { Label() }'))
        with self.assertRaises(ii_lvui.CompileError):
            ii_lvui.compile_native('Window { PageRouter }')

    def test_event_control_flow(self):
        generated = ii_lvui.compile_native('''import vm PanelModel
Window { PushButton(onClick: {
    if (vm.ready) {
        for (item in vm.items) {
            if (item.skip) { continue }
            vm.open(item.id)
        }
    } else { return }
}) }''')
        self.assertIn('script_snapshot(', generated)
        self.assertIn('continue;', generated)
        with self.assertRaises(ii_lvui.CompileError):
            ii_lvui.compile_native('Window { PushButton(onClick: { break }) }')

    def test_diagnostics_retain_source_position_and_stage(self):
        with self.assertRaises(ii_lvui.CompileError) as raised:
            ii_lvui.compile_native('Window {\n  UnknownWidget()\n}')
        self.assertEqual(raised.exception.stage, 'source')
        self.assertEqual((raised.exception.offset, raised.exception.line, raised.exception.column), (11, 2, 3))
        self.assertTrue(str(raised.exception))
        with self.assertRaises(ii_lvui.CompileError) as profile_error:
            ii_lvui.compile_native('Window { Label() }', style_profile='var width: Metric = 12')
        self.assertEqual(profile_error.exception.stage, 'style')
        with self.assertRaises(ii_lvui.CompileError) as header_error:
            ii_lvui.compile_style_header('', namespace='invalid;namespace')
        self.assertEqual(header_error.exception.stage, 'header')

    def test_invalid_arguments_and_include_are_rejected(self):
        with self.assertRaises(TypeError):
            ii_lvui.compile_native(3)
        with self.assertRaises(TypeError):
            ii_lvui.compile_native('Window { Label() }', includes='header.h')
        with self.assertRaises(ValueError):
            ii_lvui.compile_native('x' * (1024 * 1024 + 1))
        with self.assertRaises(ValueError):
            ii_lvui.compile_native('Window { Label() }', name='bad\0name')
        with self.assertRaises(ii_lvui.CompileError):
            ii_lvui.compile_native('Window { Label() }', includes=('../bad.h',))

    def test_concurrent_results_are_independent(self):
        def compile_one(index):
            name = f'ThreadView{index}'
            return name, ii_lvui.compile_native('Window { Label() }', name=name)
        with ThreadPoolExecutor(max_workers=4) as pool:
            outputs = list(pool.map(compile_one, range(8)))
        for name, result in outputs:
            self.assertIn(f'struct {name} final', result)
            for other, _ in outputs:
                if other != name:
                    self.assertNotIn(f'struct {other} final', result)


if __name__ == '__main__':
    unittest.main()
