#!/usr/bin/env bash
# CPU stats as JSON: { usage, freqGhz, temp, model, cores }
set -euo pipefail

read -r _ u n s i w irq sirq _ < /proc/stat
t1=$((u + n + s + i + w + irq + sirq)); idle1=$i
sleep 0.25
read -r _ u n s i w irq sirq _ < /proc/stat
t2=$((u + n + s + i + w + irq + sirq)); idle2=$i
dt=$((t2 - t1)); di=$((idle2 - idle1))
usage=0; (( dt > 0 )) && usage=$(( (100 * (dt - di)) / dt ))

cores=$(nproc)
model=$(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2- | sed 's/^ *//; s/"//g')
freq=$(awk -F: '/cpu MHz/{s+=$2;n++} END{if(n>0)printf "%.2f",(s/n)/1000; else print "0.00"}' /proc/cpuinfo)

temp=0
# Prefer the CPU package sensor from hwmon (k10temp/zenpower/coretemp). AMD's
# k10temp lives in hwmon, not thermal_zone (which is often just acpitz).
for h in /sys/class/hwmon/hwmon*; do
    [[ -r "$h/name" ]] || continue
    case "$(cat "$h/name")" in
        k10temp|zenpower|coretemp) ;;
        *) continue ;;
    esac
    # Pick the Tctl / Tdie / Package label if present, else temp1_input.
    pick=""
    for l in "$h"/temp*_label; do
        [[ -r "$l" ]] || continue
        case "$(cat "$l")" in
            Tctl|Tdie|*Package*) pick="${l%_label}_input"; break ;;
        esac
    done
    [[ -z "$pick" && -r "$h/temp1_input" ]] && pick="$h/temp1_input"
    [[ -n "$pick" && -r "$pick" ]] && { temp=$(( $(cat "$pick") / 1000 )); break; }
done
# Fall back to a CPU-ish thermal zone, then zone0.
if (( temp == 0 )); then
    for z in /sys/class/thermal/thermal_zone*; do
        [[ -r "$z/type" && -r "$z/temp" ]] || continue
        case "$(cat "$z/type")" in
            *x86_pkg_temp*|*coretemp*|*k10temp*|*cpu*|*CPU*)
                temp=$(( $(cat "$z/temp") / 1000 )); break ;;
        esac
    done
fi

printf '{"usage":%d,"freqGhz":"%s","temp":%d,"model":"%s","cores":%d}\n' \
    "$usage" "$freq" "$temp" "$model" "$cores"
