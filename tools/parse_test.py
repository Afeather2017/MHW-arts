import sys
LOG = open(r"E:\MHW-arts\.tmp_parse.log", "w")
try:
    ADDON = r"C:\Users\Afeather\AppData\Roaming\Blender Foundation\Blender\4.3\scripts\addons\MHW-FreeKinetics"
    sys.path.insert(0, ADDON)
    sys.path.insert(0, ADDON + r"\struct")
    sys.path.insert(0, ADDON + r"\common")
    import Lmt
    for label, path in [
        ("clean-base", r"E:\MHW-arts\w03_00-clean-base.lmt"),
        ("vanilla", r"E:\SteamLibrary\steamapps\common\Monster Hunter World\nativePC\hm\wp\wp03\mot\w03_00\w03_00-origin.lmt"),
    ]:
        try:
            lmt = Lmt.LMT.parseFile(path)
            LOG.write(label + ": OK entries=" + str(len(lmt.ActionHeaders)) + "\n")
        except Exception as e:
            LOG.write(label + ": FAIL " + type(e).__name__ + " " + str(e)[:150] + "\n")
except Exception:
    import traceback
    LOG.write("TOP FAIL\n" + traceback.format_exc())
LOG.close()
