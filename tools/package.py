"""打包当前所有独立工程、文档和默认固件，排除中间对象和个人Keil设置。"""
from pathlib import Path
import zipfile
from make_projects import ROOT, PROJECTS

def package():
    paths = [ROOT / name for name in ('README.md', '.gitignore', '.gitattributes')]
    for folder, _, _ in PROJECTS:
        project = ROOT / folder
        for path in project.rglob('*'):
            if not path.is_file():
                continue
            if any(part in ('Objects', 'Listings', 'DebugConfig', '__pycache__') for part in path.relative_to(project).parts):
                continue
            if '.uvguix.' in path.name or path.suffix == '.uvoptx':
                continue
            paths.append(path)
        paths.extend(project / 'Objects' / ('Project' + ext) for ext in ('.hex', '.bin', '.axf'))
        paths.extend([project / 'Objects/Build.log', project / 'Listings/Project.map'])
    for folder in ('tools', 'tests', 'docs', 'ros1'):
        paths.extend(path for path in (ROOT / folder).rglob('*') if path.is_file() and
                     not any(part in ('build', '__pycache__') for part in path.relative_to(ROOT / folder).parts))
    output = ROOT / 'dist/STM32F103C8_进阶例程_中文注释版.zip'
    output.parent.mkdir(exist_ok=True)
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in sorted(set(paths)):
            archive.write(path, path.relative_to(ROOT).as_posix())
    with zipfile.ZipFile(output) as archive:
        assert archive.testzip() is None
        assert sum(name.endswith('Project.uvprojx') for name in archive.namelist()) == len(PROJECTS)
        assert sum(name.endswith('.hex') for name in archive.namelist()) == len(PROJECTS)
    print(f'{output} | {len(PROJECTS)} projects | {output.stat().st_size} bytes | CRC PASS')

if __name__ == '__main__':
    package()
