import os
import shutil
import sys
import subprocess
import pathlib

# Check if file's relative location is as expected
script_dir = pathlib.Path(os.path.abspath(__file__)).parent
assert script_dir.match("*/source_tree/Integrations/AfterEffects")

sys.path.append(str(script_dir / "../../SDK/Python"))
from Libs.pk_tools_locate import get_qt_bin_path

#SETUP
#============
QT_BIN_PATH_OVERRIDE="" # e.g. "E:\\SDK\\Qt\\Qt5.15.2\\5.15.2\\msvc2019_64\\bin"

assembly_folder = "External/popcornfx.qt"
debug_dlls = False #Set this to True to add debug dlls in the private assembly
qt_dll_override_manifest = "qtoverride.dll.manifest"
win_kit_bin_path = "C:\\Program Files (x86)\\Windows Kits\\10\\bin"

# [0]   path: str
# [1]   addInAssembly: bool
# [2]   patchWithManifest: bool
dlls_to_copy = [
                ("Qt5Core.dll", True, False), #The first one must be Qt5Core
                ("Qt5Gui.dll", True, True),
                ("Qt5Widgets.dll", True, True),
                ("../plugins/platforms/qwindows.dll", False, True),
               ]
#============

def FindMtExe():
    versions_list = [name for name in os.listdir(win_kit_bin_path) if os.path.isdir(os.path.join(win_kit_bin_path, name)) and name.startswith("10.")]
    versions_list.sort(reverse=True)
    for version in versions_list:
        mt_exe_path = os.path.join(win_kit_bin_path, version + "\\x64\\mt.exe")
        if (os.path.exists(mt_exe_path)):
            return mt_exe_path
    return ""

def CopyDll(dll, debug, path_qt_folder, mt_exe_path, f):
    dll_path = dll[0] #path
    if debug:
        split_path = os.path.splitext(dll_path)
        dll_path = split_path[0] + "d" + split_path[1]
    src_fpath = os.path.join(path_qt_folder, dll_path)
    file = os.path.basename(dll_path)
    dest_fpath = os.path.join(assembly_folder, file)
    print("Copying: " + file)
    shutil.copy(src_fpath, dest_fpath)
    if dll[1]: #addInAssembly
        f.write("  <file name=\"" + file + "\" />\n")
    if dll[2]: #patchWithManifest
        cmd = [mt_exe_path, "-manifest", "qtoverride.dll.manifest", "-outputresource:" + assembly_folder + "/" + file + ";#2"]
        process = subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)
        process.check_returncode()

def main():
    mt_exe_path = FindMtExe()
    if not mt_exe_path:
        print("Error: Could not found mt.exe")
        sys.exit(1)
    
    if not os.path.exists(qt_dll_override_manifest):
        print("Error: Could not found:" + qt_dll_override_manifest)
        sys.exit(1)
    
    path_qt_folder = QT_BIN_PATH_OVERRIDE if QT_BIN_PATH_OVERRIDE else str(get_qt_bin_path(True))
    if not os.path.exists(path_qt_folder):
        print("Error: Could not found Qt folder: " + path_qt_folder)
        sys.exit(1)
    print("Found Qt directory at: " + path_qt_folder)
    
    if not path_qt_folder:
        print("Error: Could not found Qt directory in PATH")
        sys.exit(1)
    
    if os.path.isdir(assembly_folder):
        shutil.rmtree(assembly_folder)
    os.mkdir(assembly_folder)
    f = open(assembly_folder + "/popcornfx.qt.manifest", "w")
    if not f:
        print("Error: Could not open to write:" + assembly_folder + "/popcornfx.qt.manifest")
        sys.exit(1)
    
    f.write("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n")
    f.write("<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">\"\n")
    f.write("\n")
    f.write("  <assemblyIdentity type=\"Win32\" name=\"popcornfx.qt\" version=\"1.0.0.0\" processorArchitecture=\"amd64\" />\n")
    
    for dll in dlls_to_copy:
        CopyDll(dll, False, path_qt_folder, mt_exe_path, f)
        if debug_dlls:
            CopyDll(dll, True, path_qt_folder, mt_exe_path, f)
    
    f.write("\n")
    f.write("</assembly>")
    f.close()
    
    sys.exit(0)

if __name__ == "__main__":
    main()