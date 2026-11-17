"""Keep live navigation and editor entry points aligned with the real tree.

Historical reports and immutable archives are deliberately outside this check.
"""
import json
from pathlib import Path
import re
import unittest

REPO = next(p for p in Path(__file__).resolve().parents if (p / '.vscode/tasks.json').is_file())
CORE = REPO / 'telephony_core_service'
TEST = CORE / 'services/multisim/test'


def missing_links(path, text):
    return [target for target in re.findall(r'\]\(([^)]+)\)', text)
            if not target.startswith(('http:', 'https:', '#', 'mailto:'))
            and not (path.parent / target.split('#', 1)[0]).exists()]


def default_builds(tasks):
    return [task['label'] for task in tasks if isinstance(task.get('group'), dict)
            and task['group'].get('kind') == 'build' and task['group'].get('isDefault')]


class LiveNavigation(unittest.TestCase):
    def test_live_links(self):
        paths = [REPO/'README.md', CORE/'docs/README.md', CORE/'services/multisim/README.md', TEST/'README.md',
                 TEST/'cases/README.md', TEST/'docs/Host_Usage.md', CORE/'docs/SIM_Multisim_Source_Architecture_Detailed_Design.md',
                 CORE/'docs/knowledge/code_map/modules.md', CORE/'docs/knowledge/verify/build-test.md',
                 CORE/'docs/knowledge/expert/constraints.md']
        for path in paths:
            with self.subTest(path=path):
                self.assertEqual(missing_links(path, path.read_text()), [])

    def test_missing_link_is_detected(self):
        self.assertEqual(missing_links(TEST/'README.md', '[bad](retired-entry.py)'), ['retired-entry.py'])

    def test_source_inventory_matches_current_module(self):
        module = CORE/'services/multisim'
        files = [p for p in module.rglob('*') if p.is_file()
                 and p.relative_to(module).parts[0] not in ('test', 'tools')]
        text = (CORE/'docs/SIM_Multisim_Source_Architecture_Detailed_Design.md').read_text()
        self.assertIn(f'{len(files)}个模块文件', text)
        self.assertIn(f'{sum(p.suffix in (".h", ".cpp") for p in files)}个C++单元', text)
        self.assertNotIn('├── SIM 重构代码目录结构设计方案.md', text)

    def test_one_default_build_and_resolved_task_references(self):
        tasks = json.loads((REPO/'.vscode/tasks.json').read_text())['tasks']
        self.assertEqual(default_builds(tasks), ['Build Host'])
        labels = [task['label'] for task in tasks]
        self.assertEqual(len(labels), len(set(labels)))
        launch = json.loads((REPO/'.vscode/launch.json').read_text())
        for config in launch['configurations']:
            self.assertIn(config['preLaunchTask'], labels)
        settings = json.loads((REPO/'.vscode/settings.json').read_text())
        for entry in settings['testMate.cpp.test.advancedExecutables']:
            self.assertEqual(entry['maxTestsPerExecutable'], 1)
            self.assertEqual(entry['parallelizationLimit'], 1)
            self.assertIn('%p', entry['env']['LLVM_PROFILE_FILE'])
            for label in entry['runTask']['before']:
                self.assertIn(label, labels)

    def test_duplicate_default_is_detectable(self):
        group = {'kind': 'build', 'isDefault': True}
        self.assertEqual(len(default_builds([{'label': 'a', 'group': group}, {'label': 'b', 'group': group}])), 2)

    def test_selected_tasks_only_require_business_case_name(self):
        tasks = json.loads((REPO/'.vscode/tasks.json').read_text())['tasks']
        for task in tasks:
            if task['label'] in ('Run Selected Test', 'Debug Selected Test'):
                self.assertNotIn('--target', task['args'])
                self.assertIn('${input:gtestFilter}', task['args'])
            args = task.get('args', [])
            if len(args) > 1:
                script = args[1].replace('${workspaceFolder}/', '')
                self.assertTrue((REPO/script).is_file(), script)
        build = next(task for task in tasks if task['label'] == 'Build Host')
        self.assertNotIn('--target', build['args'])


if __name__ == '__main__':
    unittest.main()
