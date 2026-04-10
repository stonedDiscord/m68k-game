#!/usr/bin/env python3
import mido
import sys
import argparse

def midi_to_note_name(midi_note):
    notes = ["C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-"]
    note = notes[midi_note % 12]
    octave = midi_note // 12 - 1 # VT2 notes start at octave 0 (C-0 = MIDI 12)
    if octave < 0: octave = 0
    if octave > 8: octave = 8
    return f"{note}{octave}"

def convert(midi_file, output_file, track_mapping, speed=8):
    mid = mido.MidiFile(midi_file)
    
    # Calculate ticks per row
    # Standard VT2 is 50Hz. Speed=8 means 50/8 = 6.25 rows per second.
    # MIDI ticks are relative to bpm.
    # Let's use a simpler approach: 
    # Determine the shortest note or a reasonable quantization.
    # Usually 16th notes are 1 row. 
    # mid.ticks_per_beat is usually 480. 16th note = 120 ticks.
    
    ticks_per_row = mid.ticks_per_beat // 4 # Assume 16th notes
    
    # Collect all messages and their absolute times for each track
    track_data = []
    for i, track in enumerate(mid.tracks):
        abs_time = 0
        msgs = []
        for msg in track:
            abs_time += msg.time
            if msg.type == 'note_on' or msg.type == 'note_off':
                msgs.append({'time': abs_time, 'msg': msg})
        track_data.append(msgs)

    # Channels A, B, C for VT2
    channels = [[], [], []]
    
    # track_mapping is a list of lists of track indices to merge into each channel
    # e.g. [[4], [1, 3], [2]]
    
    for ch_idx, midi_track_indices in enumerate(track_mapping):
        merged_msgs = []
        for mt_idx in midi_track_indices:
            if mt_idx < len(track_data):
                merged_msgs.extend(track_data[mt_idx])
        
        # Sort by time
        merged_msgs.sort(key=lambda x: x['time'])
        channels[ch_idx] = merged_msgs

    # Determine total rows
    max_time = 0
    for ch in channels:
        if ch:
            max_time = max(max_time, ch[-1]['time'])
    
    total_rows = (max_time // ticks_per_row) + 1
    # Round up to multiple of 64
    total_rows = ((total_rows + 63) // 64) * 64
    
    patterns = []
    num_patterns = total_rows // 64
    
    for p in range(num_patterns):
        pattern = []
        for r in range(64):
            pattern.append(["--- .... ....", "--- .... ....", "--- .... ...."])
        patterns.append(pattern)

    # Fill patterns
    for ch_idx, msgs in enumerate(channels):
        # Sample mapping: Track 4 (Bass) uses sample 2, others use 1
        # Track 4 index might be different in track_mapping
        # Let's check which original track this msg came from
        # Actually let's just use a simple heuristic: if it's channel 0 (Bass), use Sample 2.
        sample = "2" if ch_idx == 0 else "1"
        
        active_notes = {}
        for item in msgs:
            msg = item['msg']
            row = item['time'] // ticks_per_row
            p_idx = row // 64
            r_idx = row % 64
            
            if p_idx >= num_patterns: continue
            
            if msg.type == 'note_on' and msg.velocity > 0:
                note_name = midi_to_note_name(msg.note)
                vol = hex(msg.velocity // 8)[2:].upper() # 0-127 to 0-F
                patterns[p_idx][r_idx][ch_idx] = f"{note_name} {sample}{vol}.. ...."
            elif msg.type == 'note_off' or (msg.type == 'note_on' and msg.velocity == 0):
                patterns[p_idx][r_idx][ch_idx] = f"R-- .... ...."

    # Write VT2 file
    with open(output_file, 'w') as f:
        f.write("[Module]\n")
        f.write("VortexTrackerII=0\n")
        f.write("Version=3.5\n")
        f.write(f"Title={midi_file}\n")
        f.write("Author=MIDI2VT2\n")
        f.write("NoteTable=0\n")
        f.write(f"Speed={speed}\n")
        
        order = ",".join([str(i) for i in range(num_patterns)])
        f.write(f"PlayOrder=L{order}\n\n")

        # Ornaments (empty)
        for i in range(1, 16):
            f.write(f"[Ornament{i}]\nL0\n\n")

        # Samples (placeholders or copies from original)
        # For simplicity, I'll provide basic samples if they are not defined
        f.write("[Sample1]\nTne +000_ +00_ F_\nTne +000_ +00_ F_ L\n\n")
        f.write("[Sample2]\nTne +001_ +00_ F_\nTne +001_ +00_ F_ L\n\n")
        for i in range(3, 32):
            f.write(f"[Sample{i}]\ntne +000_ +00_ 0_ L\n\n")

        # Patterns
        for i, p in enumerate(patterns):
            f.write(f"[Pattern{i}]\n")
            for r in p:
                f.write(f"....|..|{r[0]}|{r[1]}|{r[2]}\n")
            f.write("\n")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Convert MIDI to VT2 (Vortex Tracker II) format.')
    parser.add_argument('input', help='Input MIDI file')
    parser.add_argument('output', help='Output TXT file')
    parser.add_argument('--speed', type=int, default=8, help='Tracker speed (default: 8)')
    
    args = parser.parse_args()
    
    # Specific mapping requested by user:
    # Track 4 (Bass) -> Channel A (0)
    # Track 1 (EP) + Track 3 (Lead) -> Channel B (1)
    # Track 2 (Guitar) -> Channel C (2)
    # MIDI Track indices are 0-based.
    # From previous check:
    # 0: Conductor
    # 1: Electric Piano 2
    # 2: Electric Guitar (jazz)
    # 3: Lead 1 (square)
    # 4: Bass
    
    mapping = [[4], [1, 3], [2]]
    
    convert(args.input, args.output, mapping, args.speed)
    print(f"Converted {args.input} to {args.output}")
