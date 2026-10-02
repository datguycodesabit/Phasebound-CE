# Campaign and endless content

The campaign has ten original levels. The first five teach Core, Glider, Blink,
Tumbler, and Beam in that order. The final five combine forms through horizontal
form portals, with clear space around each transition so a player can settle
into the new movement style.

| # | Level | Opening form | Main lesson | Approx. time at base speed |
|---:|---|---|---|---:|
| 1 | Pulse Start | Core | Single spikes, occasional pairs, and one launch pad | 45.3 s |
| 2 | Open Skies | Glider | Start in the middle and stay in a broad flight lane | 47.4 s |
| 3 | Mirror Steps | Blink | Teleport between alternating floor and ceiling hazards | 49.5 s |
| 4 | Polarity Run | Tumbler | Reverse gravity before alternating surface spikes | 51.7 s |
| 5 | Signal Flow | Beam | Enter and follow a clear middle lane between spike gates | 53.8 s |
| 6 | Switchback | Core | Core, Glider, then Core again | 55.9 s |
| 7 | Airlock | Glider | Glider, Beam, then Core | 58.0 s |
| 8 | Twin Orbit | Blink | Blink, Tumbler, then Beam | 60.2 s |
| 9 | Phase Circuit | Tumbler | Tumbler, Core, then Glider | 62.3 s |
| 10 | Last Signal | Beam | Beam, Blink, Core, then Glider | 67.1 s |

Core, Blink, and Tumbler begin at `(32, 212)`; Glider and Beam begin at `(32, 112)`
to allow reaction time before reaching a surface. All start with gravity down and base speed
2 pixels per tick. Widths range from 344 to 508 columns. At 16 pixels per
column, the 512-column storage limit caps a base-speed level at about 67.7
seconds to its finish trigger; a 90-second level would need about 675 columns.
The campaign therefore uses the feasible 45-to-68-second portion of the
requested range.

Core hazards stay on the active surface and leave long recovery runways. The
Blink and Tumbler introductions alternate surface spikes, with enough horizontal
space to complete each teleport or gravity reversal before the next event.
Glider and Beam gates use spikes near the floor and ceiling while keeping a broad
middle lane open. Mixed levels place form portals on the tile grid and leave
room to regain a safe height or grounded state after each transition. Every
campaign level has one finish object.

Endless play builds grounded 96-column Core sections. The floor remains solid
through every entrance and exit. A seed shifts low-difficulty obstacle
positions and selects cluster shapes at higher difficulty; the same seed and
difficulty always produce the same section. Difficulty is clamped to 0–5 and
adds hazards gradually, from four spaced clusters up to six clusters of at
most three tiles. The clusters remain separated by enough distance for a Core
to land and prepare for the next jump.
