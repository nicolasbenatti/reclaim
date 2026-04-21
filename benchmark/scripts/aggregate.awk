# Aggregate raw benchmark CSV: compute mean/stddev throughput, max RSS.
#
# Input CSV columns (positional):
#   1: benchmark
#   2: n_threads
#   3: iterations
#   4: is_glibc
#   5: time_us          (aggregated: mean)
#   6: throughput_alloc_per_s (aggregated: mean)
#   7: peak_rss_kb      (aggregated: max)
#   8+: benchmark-specific params (carried through)
#
# Output: same columns with throughput_stddev inserted after column 6.

BEGIN { FS = OFS = "," }

NR == 1 {
    for (i = 1; i <= 6; i++) { if (i > 1) printf OFS; printf "%s", $i }
    printf OFS "throughput_stddev"
    for (i = 7; i <= NF; i++) printf OFS "%s", $i
    print ""
    next
}

{
    # Key = all columns except the 3 metrics (5, 6, 7)
    key = $1 SUBSEP $2 SUBSEP $3 SUBSEP $4
    for (i = 8; i <= NF; i++) key = key SUBSEP $i

    if (!(key in cnt)) {
        ord[++nk] = key
        nc[key] = NF
        for (i = 1; i <= 4; i++) kv[key, i] = $i
        for (i = 8; i <= NF; i++) kv[key, i] = $i
    }
    cnt[key]++
    sum5[key]   += $5
    sum6[key]   += $6
    sum6sq[key] += $6 * $6
    if (!(key in max7) || $7 + 0 > max7[key] + 0) max7[key] = $7
}

END {
    for (k = 1; k <= nk; k++) {
        key = ord[k]
        n  = cnt[key]
        m5 = sum5[key] / n
        m6 = sum6[key] / n
        sd6 = (n > 1) ? sqrt((sum6sq[key] - n * m6 * m6) / (n - 1)) : 0
        printf "%s,%s,%s,%s,%.3f,%.3f,%.3f,%s", \
            kv[key, 1], kv[key, 2], kv[key, 3], kv[key, 4], \
            m5, m6, sd6, max7[key]
        for (i = 8; i <= nc[key]; i++) printf ",%s", kv[key, i]
        print ""
    }
}
