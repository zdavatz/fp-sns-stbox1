# Hall-Sensor Mini-PCB — Power-Switch via Magnet-Cap

Tiny auxiliary board that gates the ST PCB's VCC based on a magnetic
field detected through the case lid. Replaces the on/off switch on the
ST PCB without any case-perforation.

UX logic:
- **Magnet present (cap on lid)** → Hall output HIGH → P-FET OFF → box OFF
- **Magnet absent (cap removed)** → Hall output LOW → P-FET ON → box ON

User-visible: pulling the bright-red magnet cap off the lid turns the box
on (green LED begins blinking). Putting the cap back on turns it off.
The cap covers the whole lid in signal red, so forgetting it is unlikely.

## Schematic

```
       BAT+ ──────────────┬──────────────────[ S of P-FET ]
                          │                        │
                          │                       [D of P-FET]──── VCC_OUT
                       (Vcc U1)                    │                   to ST PCB
                          │                       [G of P-FET]──┐
                          │                                     │
                       ┌──┴──┐                                  │
                       │ U1  │                                  │
                       │DRV5032FB                                │
                       │ FB  │ ── VOUT ────────────────────────┘
                       │     │                          (push-pull)
                       └──┬──┘
                          │
                          ├──[C1 100nF]──┐
                          │              │
                         GND ────────────┴──── GND  to ST PCB
```

Three external connections (3 wires to ST PCB):
- **BAT+** — direct from battery
- **VCC_OUT** — to ST PCB's main power input (replaces what the on-board
  switch used to gate)
- **GND** — common ground

## Bill of Materials

| Ref | Part                                          | Package | Qty | Price (Mouser) |
|-----|-----------------------------------------------|---------|-----|----------------|
| U1  | TI **DRV5032FB**  (Hall switch, push-pull)    | SOT-23-3 | 1 | ~0.60 € |
| Q1  | Vishay **Si2333DDS** (P-MOSFET, –20 V, 50 mΩ) | SOT-23  | 1 | ~0.30 € |
| C1  | 100 nF ceramic 0402                           | 0402    | 1 | ~0.01 € |

Total component cost: ~1 € per board.

DRV5032 sensitivity variants — pick **FB** (≈ 5 mT operate threshold).
The FB variant requires a substantial magnet (well over Earth's field at
50 µT, well under our magnet's field at ~30-50 mT through 2 mm PETG) so
random stray fields don't false-trigger.

## PCB Layout

```
  ┌──────────────────────────┐
  │ ┌────┐         ┌────────┐ │   ← top side
  │ │ U1 │         │ pad1   │ │     U1 sensor here, faces magnet (up)
  │ │ Hall ────────┤VCC_OUT │ │
  │ └────┘         │ pad2   │ │     3 solder pads for wires
  │      ┌────┐    │ GND    │ │
  │      │ Q1 │    │ pad3   │ │
  │      └────┘    │ BAT+   │ │
  │  C1            └────────┘ │
  └──────────────────────────┘
        ← 12 mm →

  Thickness: 1.0 mm 2-layer
  Hole-free design — bonds to case mount with cyanoacrylate
```

The mini-PCB mounts in the case body on a printed shelf at the right end
(opposite the GPS antenna). The Hall sensor face points **up** towards
the lid; the magnet in the cap above the lid sits ~6 mm away. Through
2 mm of PETG + air gap, a 8×3 mm N42 magnet delivers ~30-50 mT to the
sensor — well above the FB variant's threshold.

## Wire routing

Three wires from mini-PCB to ST PCB, ~50 mm each:
- 28 AWG silicone-insulated, color-coded (red/black/yellow recommended)
- Soldered on both ends (no connector — saves height + cost)
- Routed across the gap above the ST PCB (between PCB top and lid bottom)
- Strain relief: dab of hot glue or cyanoacrylate at both PCB ends

Routing acceptable because opening the case (= removing lid) does NOT
disturb the wiring (mini-PCB stays in body, ST PCB stays in body, wires
between them stay put).

## Quiescent current

With magnet present (= box "OFF"):
- DRV5032: ~1.3 µA typical
- P-FET (no Vgs): ~0 µA
- **Total quiescent ≈ 1.3 µA**

With 1500 mAh battery → **~130 years self-discharge time** ignoring
battery's own self-discharge. The battery's chemistry will fail first.

## Manufacturing

Smallest economical PCB run:
- **JLCPCB**: 5 pieces, 2-layer, 12×8 mm, ~8-12 € + shipping
- **JLCPCB Assembly**: $30-40 USD for 5 boards including parts
- **Manual assembly**: feasible with a hot-air station and steady hand

Either way ~50 € for 5 fully-built boards.

## Integration with case

The case.scad file's body model includes a printed mounting shelf at the
right end of the interior, at the correct Z height to put the Hall sensor
~6 mm below the underside of the magnet cap. The magnet_cap.scad cap
positions the magnet pocket above the same XY coordinates.

No firmware changes are needed: the ST PCB sees a clean power-on /
power-off transition exactly like the original physical switch did.

## Build-and-test sequence

1. Order 5× PCBs from JLCPCB
2. Reflow 3 SMT components (or hand-solder)
3. Solder 3 wires to ST PCB's battery input + GND + main VCC rail
4. Glue mini-PCB to case shelf with cyanoacrylate
5. Print magnet cap, glue 8×3 N42 magnet into pocket
6. Cap on case lid → box should be OFF (no LED activity)
7. Cap off → box should boot (green LED blink starts within ~3 sec)
8. Cap on again → box off cleanly within 100 ms

If step 7 doesn't trigger: check magnet orientation (one face activates,
the other doesn't — flip the magnet in the cap).
