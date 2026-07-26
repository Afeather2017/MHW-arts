from io import BytesIO

from mhw_lmt_loader.lmt.Lmt import Events, LMT

staging_path = (
    "E:/MHW-arts/research/mhw/sakura_test/w03_00_freehk_staging.lmt"
)
valor_path = "E:/MHW-arts/research/mhw/skeleton/w03_00_valor.lmt"
output_path = (
    "E:/MHW-arts/research/mhw/sakura_test/w03_00_slot338_roundtrip.lmt"
)

with open(staging_path, "rb") as stream:
    staging = LMT(stream)
with open(valor_path, "rb") as stream:
    valor = LMT(stream)

animation = staging.get_animation(338)
if animation is None:
    raise RuntimeError("FreeHK staging LMT has no slot 338")

# FreeHK emitted this motion without a TIML/event block. The older slot
# injector expects an Events object even when it contains no events.
if not hasattr(animation, "events"):
    animation.events = Events()
    animation.events.events = []
    animation.events.unkn = [0] * 8

valor.override_animation(338, animation)
with open(output_path, "wb") as stream:
    stream.write(valor.serialize())

print("WROTE", output_path)
