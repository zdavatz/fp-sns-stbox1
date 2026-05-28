# PumpLogger Case — Design Specification

Last updated: 2026-05-26

This document is the SOURCE OF TRUTH for the case design.
**Before every geometry change, walk the verification checklist at the bottom.**

---

## 1. Purpose

Waterproof enclosure for the PumpLogger electronics:
- SensorTile.box PRO PCB (sensors + BLE + SD storage)
- LiPo battery + Qi wireless charging coil
- u-blox MAX-M10S GPS module + external GPS antenna
- Hall-sensor mini-PCB (magnetic on/off switch via external magnet-cap)

Operated on a pumpfoil board → exposed to splash water, occasional shallow
submersion (≤ 1 m, ≤ 5 min), UV, salt water possible.

---

## 2. HARD CONSTRAINTS (must satisfy — never relaxed)

### 2.1 Sealing
- Waterproof against splash + short shallow submersion
- O-ring face seal: groove in body rim, lid presses on it
- **Seal path is a SMOOTH closed loop — no sharp kinks (≤ 5° direction change at any transition)**
- Compression 25–30 % of cord diameter
- Fill ratio ≤ 90 % of groove cross-section
- O-ring is a **standard catalogue size** (EPDM, metric)
- Corner radius of seal path ≥ 3 × cord diameter

### 2.2 Mechanical
- **Lid must be lowerable OVER the assembled ST PCB** (PCB stays on body standoffs)
  → `rim_opening` (cavity at the parting plane) ≥ ST PCB outline + 1 mm clearance, in BOTH axes
- 4 corner screws clamp the lid to the body, even compression
- Wall thickness ≥ 3 mm in sealed region

### 2.3 Geometric
- Parting plane sits just below the ST PCB underside
  → removing the lid exposes the SD-card slot + USB connector
- All outer EDGES rounded (no sharp 90° edges — Verletzungsschutz)
- All housing geometry printable on FDM with minimal/no support

---

## 3. SOFT CONSTRAINTS (preferred, trade-offs OK if documented)
- So klein wie möglich, but only within the hard constraints
- Clean aesthetics: no unnecessary external bumps
- Components mit kleinstmöglichem Aufwand wechselbar
- Print time ≤ ~6 h per part

---

## 4. Components

| Bauteil | X | Y | Z (mm) | Befestigung |
|---|---|---|---|---|
| Qi-Ladespule | 49 | 32 | 1 | Auf Body-Boden geklebt |
| Batterie (LiPo) | 38 | 24 | 8 | Auf Qi-Spule geklebt |
| ST-PCB | 59 | 36.5 | 11 | Auf 4 Stützen im Body, scalloped corners (r=3) |
| GPS-Modul (flach, NEU 2026-05-28) | 28 | 16 | ~5 | Ans Lid-Dach geklebt, neben Antenne |
| GPS-Antenne | 25 | 26 | 8 | Ans Lid-Dach geklebt — überlappt PCB-Säule |
| Hall-PCB | 12 | 8 | 1 | An Lid-Innendach (für Magnet-Cap-Schalter) |

---

## 5. Layout

### Grundriss (3 Y-Bänder)
```
┌─────────────────────────────────┐
│  Reserve-Strip Y=0  11 mm        │  Y-Band 1: Hall-PCB + flaches GPS-Modul + Reserve
├─────────────────────────────────┤
│                                  │
│  ST-Platine  59.5 × 36.6         │  Y-Band 2: mittig (Footprint inkl. USB-/SD-Overhang)
│  (auf 4 1/4-Kreis-Pfosten)       │
│                                  │
└─────────────────────────────────┘
   (Hall-PCB + GPS-Modul + Antenne am Lid-Dach)
```

### Vertikaler Stack (Body)
- Boden → Qi 1 mm → Batterie 8 mm → Stützen → ST-PCB 11 mm
- ST-PCB-Unterseite bei z ≈ 13.5 mm absolut (= 2.5 mm über Trennebene)
- Trennebene bei z = 11 mm absolut (= 2.5 mm UNTER PCB-Unterseite)
- PCB-Top mit Bauteilen reicht 13.5 mm ins Lid hinein

### Body vs. Lid
- **Body**: flache Wanne ~11.5 mm aussen — Qi, Batterie, PCB-Pfosten
- **Lid**: tieferer Hohlkörper ~25.5 mm aussen (war 27 vor 2026-05-28) — GPS-Antenne, flaches GPS-Modul + Hall-PCB am Dach geklebt, ST-PCB-Komponenten ragen rein

---

## 6. Geometry (target)

| Mass | Wert (mm) | Anmerkung |
|---|---|---|
| Hauptbox external | 72 × 61 | Sauberes Rundrechteck |
| Box bounding (mit Lappen) | ~82 × 71 | Inkl. Eck-Lappen-Bulges |
| Cavity internal | 66 × 55 | = in_x × in_y |
| rim_opening (an Trennstelle) | ~63 × 52 | = cavity − 2× Rim-Flansch (2.3 mm) |
| Wandstärke | 3 | Wände, Boden, Dach |
| Eckenradius Hauptbox | 8 | Konturen-Verrundung |
| Trennebene | z = 11 | von Boden gemessen |
| Body-Höhe aussen | 11 | floor 3 + cavity 8 |
| Lid-Höhe aussen | 25.5 | cavity 22.5 + roof 3 (2026-05-28: war 27 mit altem stehendem GPS) |
| Gesamthöhe | 36.5 | (war 39) |

---

## 7. Sealing Specification

### 7.1 O-Ring
- **Material**: EPDM (Wasser/UV/Seewasser-fest, Standard für Outdoor-Dichtungen)
- **Grösse: 70 × 2.0 mm** (ID × Schnurdicke, ISO 3601 / metrisch)
- **Lieferanten**:
  - [DichtungenShop24.de](https://www.dichtungen-shop24.de/) — günstig, breit, ~1-2 €/Stk
  - [ERIKS](https://shop.eriks.ch/) — industriell, sehr breit
  - Würth, Conrad/Distrelec — Sortiment
  - McMaster-Carr (US) — sehr breit, weltweiter Versand

### 7.2 Nut (Groove)
| Parameter | Wert | Begründung |
|---|---|---|
| Schnurdicke (cord) | 2.0 mm | Standard, robust |
| Nut-Tiefe (groove_d) | **1.4 mm** | → Quetschung 30 % (für raue FDM-Oberflächen) |
| Nut-Breite (groove_w) | **2.8 mm** | → Füllgrad 80 % |
| Eckenradius (min) | 5 mm | Hauptbox corner_r − seal_edge_inset = 8 − 3 |
| Centerline-Inset | 3 mm | von Aussenkante |

### 7.3 Pfad
- **Sauberes Rundrechteck — KEINE Detours**
- Möglich, weil Schrauben in **Eck-Lappen aussen** sitzen (siehe §8.1), nicht innerhalb der Seal-Loop
- Seal-Bounding: 66 × 55 mm, Eckenradius 5 mm
- **Perimeter berechnet**: 2·66 + 2·55 + (2π − 8)·5 = 233.6 mm
- O-Ring 70×2 Mittellinien-Umfang: π·72 = 226.2 mm
- **Vordehnung: 3.3 %** ✓ (innerhalb der EPDM-Toleranz, akzeptabel)

### 7.4 Füllgrad-Check
- O-Ring-Querschnitt: π·2²/4 = 3.14 mm²
- Nut-Querschnitt: 2.8 × 1.4 = 3.92 mm²
- Füllgrad: 3.14 / 3.92 = **80 %** ✓ (≤ 90 %)

---

## 8. Mounting Features

### 8.1 Eck-Lappen (Schrauben aussen)
- 4 runde Lappen aussen am Body+Lid an den Box-Ecken
- Jeder Lappen ~10 mm Durchmesser, sticht ~5 mm diagonal aus der Hauptbox-Kontur
- Lappen-Form: Halbzylinder + voll verrundete Übergänge zum Hauptkörper
- **M3 Brass heat-set Insert** (Ruthex M3×5) im Body-Lappen
- **M3 DIN 7991 Senkschraube**, 90° konische Senkung im Lid-Lappen
- Insert-Bohrung durchgehend → Schraube von unten möglich für Board-Montage

### 8.2 Federblech-Schächte (Board-Klemmung)
- 2 geschlossene Rechteckkanäle auf **einer** Längsseite
- Kanal-Querschnitt: **20.6 × 1.2 mm** (für 20 × 0.6 mm Federstahl, 0.5-0.7 mm CAD-Slack)
- Body-Teil: geschlossen unten (Boden = z=2), offen oben
- Lid-Teil: offen oben UND unten, Bump **5 mm kürzer** als Lid
- Strip eingeschoben von oben mit 180°-Bogen, äusserer Schenkel hängt aussen am Gehäuse runter und hakt unter das Foilboard
- Material rundum Kanal: ≥ 2 mm jede Seite (Outer-Bump 3 mm + Wand 3 mm − 1 Kanal)

### 8.3 Lanyard-Öse
- D-Form Lug an einer kurzen Stirnseite (X=0), auf halber Höhe
- 8 mm OD × 4 mm Dicke, 3 mm Loch (Paracord 2.5 mm)
- Voll verrundet, ragt 4 mm aus dem Body

### 8.4 Hall-Schalter (Magnet-Cap-System)
- Hall-Sensor (DRV5032FB) auf Mini-PCB (12 × 8 mm)
- PCB an der Lid-Innenseite geklebt, Sensor zum Lid-Dach gerichtet
- 3 Drähte vom Hall-PCB zum ST-PCB (BAT+, VCC_OUT, GND)
- Externes Magnet-Cap aus rotem PETG/PLA deckt das ganze Lid → unübersehbar
- N42-Magnet 8 × 3 mm im Cap, aktiviert Hall-Sensor durch Lid-Material

---

## 9. Print Orientation
- **Body**: Boden aufs Druckbett, offene Seite nach oben
  - O-Ring-Nut = nach oben offener Graben → KEIN Überhang
- **Lid**: Dach aufs Druckbett, offene Seite nach oben
  - Rim-Absatz hat 3 mm horizontalen Überhang → leichte Slicer-Stütze ODER 45°-Fase
- Eck-Lappen: keine Stützen nötig (rundum verrundet, kleiner Bulge)
- Federblech-Bumps: vertikal gedruckt, keine Stützen nötig

---

## 10. VERIFICATION CHECKLIST
*Run AFTER EVERY geometry change. Even small changes can break sealing.*

### Seal integrity
- [ ] Seal centerline path is smooth (no kinks > 5° at any transition)
- [ ] Compression in [25 %, 30 %] (= groove_d in [1.4, 1.5] for cord 2.0)
- [ ] Fill ratio ≤ 90 % (= π·cord² / 4 ≤ 0.9 × groove_w × groove_d)
- [ ] Seal corner radius ≥ 3 × cord (= ≥ 6 mm with cord 2)
- [ ] Re-compute seal perimeter, verify standard O-ring size still applicable (stretch 1-5 %)

### Fit / clearances
- [ ] rim_opening_x ≥ st_x + 1 mm (PCB passes during lid descent)
- [ ] rim_opening_y ≥ st_y + 1 mm
- [ ] rim_opening_x ≥ gps_w + 1 mm (GPS module passes too)
- [ ] rim_opening_y ≥ gps_d + 1 mm
- [ ] All inner mounts (standoffs, GPS holder, Hall console) clear from each other

### Structural
- [ ] Wall ≥ 3 mm in sealed area
- [ ] Material around M3 insert ≥ 2 mm (= boss radius ≥ 4.5)
- [ ] Spring-channel walls ≥ 2 mm on outer AND inner side

### Printability
- [ ] No overhang > 45° without support / chamfer strategy
- [ ] All features connect to a base (no floating geometry)
- [ ] Layer adhesion direction OK for stress paths

---

## 11. Open / Pending
- [x] ~~OpenSCAD implementation Option C (corner lobes)~~ → `case.scad` v4 done 2026-05-26
- [x] ~~Stage 2b ST standoffs (1/4-Pfosten mit Absatz, Merge-Brackets)~~ done 2026-05-26
- [x] ~~Federblech-Schächte + Dovetail-Schiene mittig~~ done 2026-05-26
- [ ] **FIRST PRINT** mit aktuellem Stand → Peter improvisiert GPS-Halter + Hall-Konsole am echten Bauteil
- [ ] First print + leak test with real 72 × 1.5 EPDM O-ring
- [ ] **Boden-Spalt**: 2 kleine sichtbare Rechtecke im Body-Boden bei den Federblech-Schächten — auch nach `spring_slot_floor=3.5` noch da. Wahrscheinlich Projection-Artefakt oder CGAL-Boundary-Edge. Untersuchen + fixen für nächste Iteration.
- [ ] Stage 2b GPS-Modul-Halter (deferred — Peter improvisiert erstmal)
- [ ] Stage 2b Hall-PCB-Konsole am Lid (deferred — idem)
- [ ] Eck-Lappen Hull-Blend (Schmutzfänger-Ecken glätten via hull(lobe + body_arc_center))
- [ ] **Magnet-Cap-Lösung** offen — alte Vollabdeckung passt nicht mehr wegen
      Eck-Lappen + Federblech-Bumps. Optionen: Voll-Cap mit Aussparungen,
      Mini-Button-Cap, Streifen-Cap, Top-only-Cap. Entscheidung pending.
      → `magnet_cap.scad` braucht entsprechend Rewrite sobald entschieden.
- [ ] Aussenkanten gerundet (horizontal edges: top/bottom of body and lid)
- [ ] Seal corner_r erhöhen von 5 → 6 mm? (knapp unter Empfehlung 3×cord)

---

## 12. History / Decision Log
| Date | Decision | Reason |
|---|---|---|
| 2026-05-21 | Architecture: low parting plane, hollow lid | SD/USB access, structural stiffness |
| 2026-05-22 | Wall 2.5 → 3 mm, no chamfer (sharp lid step) | Robustness, CGAL stability |
| 2026-05-24 | Federblech-Schächte (geschlossene Rohre) | User-spec |
| 2026-05-25 | Wavy O-ring detour (screws within outline) | First attempt to keep box small |
| 2026-05-26 | **REVERT** to clean rounded seal + corner lobes (Option C) | Wavy seal had 67° kinks → not reliable |
| 2026-05-26 | Compression 25 → 30 % (groove_d 1.5 → 1.4) | Better for FDM-rough surfaces |
| 2026-05-26 | O-ring 70 × 2 EPDM (3.3 % stretch) | Standard available size |
| 2026-05-26 | PCB outline → bare Gerber values 57.5 × 35.6 (was 59 × 36.5) | Exakte ST-Werte aus GKO |
| 2026-05-26 | usb_overhang=2, sd_overhang=1 als explizite Konnektor-Maße | User-spec |
| 2026-05-26 | SD-Slot auf FERN-Y-Seite (gegenüber GPS-Modul) | SD bleibt zugänglich, GPS nicht im Weg |
| 2026-05-26 | `standoff_h` als primärer Parameter, body_in_z davon abgeleitet | Qi/Bat-Dicken sind nur Constraints, nicht Treiber |
| 2026-05-28 | GPS-Modul: stehend (11×31×51) → **flach (16×28×5)**, ans Lid-Dach geklebt | Neues SMD-Modul gefunden; alter Höhentreiber weg |
| 2026-05-28 | Lid-Höhe 27 → **25.5 mm aussen** (lid_in_z 25 → 22.5) | Höhentreiber jetzt PCB-Top (13.5) + Antenne (8) + 1 mm Reserve |
| 2026-05-28 | Cavity XY unverändert; 11 mm Y=0-Strip → Reserve (Hall + GPS-Modul) | User-Vorgabe: Breite gleich, Strip beherbergt jetzt Hall + flaches GPS |
