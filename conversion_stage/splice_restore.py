"""Splice the restored RXN/propwrite block into dllmain.cpp before InstallActionHook."""
import io

SRC = r"E:\MHW-arts\ValorGauge\src\dllmain.cpp"
BLOCK = r"E:\MHW-arts\conversion_stage\restore_block.cpp.txt"

t = io.open(SRC, encoding="utf-8").read()
block = io.open(BLOCK, encoding="utf-8").read()

assert "void PollReactionWatch()" not in t, "already restored"
marker = "bool InstallActionHook() {"
assert marker in t
t = t.replace(marker, block, 1)

io.open(SRC, "w", encoding="utf-8", newline="\n").write(t)
print("spliced ok")
