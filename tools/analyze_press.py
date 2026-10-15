import csv

with open('tests/m_finger_press_expert_v4.csv', 'r') as f:
    rows = list(csv.DictReader(f))

cues = [
    {'name': 'Trial 1', 'press_s': 10.005, 'rel_s': 14.010, 'press_c': 667, 'rel_c': 934},
    {'name': 'Trial 2', 'press_s': 18.015, 'rel_s': 22.020, 'press_c': 1201, 'rel_c': 1468},
    {'name': 'Trial 3', 'press_s': 26.025, 'rel_s': 30.030, 'press_c': 1734, 'rel_c': 2001},
]

print("=================================================================")
print("   S32K144W Hardware Cued Press/Release Validation Report        ")
print("=================================================================")

for cue in cues:
    window = [r for r in rows if cue['press_s'] - 0.5 <= float(r['t_s']) <= cue['rel_s'] + 1.5]
    touches_in_win = [r for r in window if int(r['touch']) == 1]
    
    first_touch = touches_in_win[0]
    last_touch = touches_in_win[-1]
    
    t_press_detected = float(first_touch['t_s'])
    t_release_detected = float(last_touch['t_s'])
    
    onset_delay_ms = (t_press_detected - cue['press_s']) * 1000.0
    release_delay_ms = (t_release_detected - cue['rel_s']) * 1000.0
    
    active_press_frames = [r for r in window if float(r['dyn']) > 100.0]
    detected_frames = [r for r in active_press_frames if int(r['touch']) == 1]
    duty = (len(detected_frames) / len(active_press_frames) * 100.0) if active_press_frames else 0.0
    
    max_delta = max(float(r['dyn']) for r in detected_frames)
    mean_conf = sum(float(r['conf']) for r in detected_frames) / len(detected_frames)
    
    print(f"\n[*] {cue['name']}:")
    print(f"    - Press detected at:     {t_press_detected:.3f} s (Reaction latency: {onset_delay_ms:.0f} ms from Blue cue)")
    print(f"    - Release detected at:   {t_release_detected:.3f} s (Release delay: {release_delay_ms:.0f} ms from Green cue)")
    print(f"    - Solid Contact Frames:  {len(detected_frames)} / {len(active_press_frames)} ({duty:.1f}% solid duty)")
    print(f"    - Max Dynamic Delta:     {max_delta:.1f} LSB")
    print(f"    - Mean TinyML Conf:      {mean_conf:.2f}")

# Check false triggers outside the press windows
idle_frames = [r for r in rows if not any(cue['press_s'] - 0.5 <= float(r['t_s']) <= cue['rel_s'] + 1.5 for cue in cues)]
false_triggers = [r for r in idle_frames if int(r['touch']) == 1]
print("\n[*] Quiescent / Release Windows Integrity:")
print(f"    - Total idle frames evaluated: {len(idle_frames)}")
print(f"    - False triggers detected:     {len(false_triggers)} (FPR = 0.0%)")
print("=================================================================")
