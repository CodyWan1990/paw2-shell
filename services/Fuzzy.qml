pragma Singleton

import QtQuick

/// Tiny fuzzy matcher for the search panels — subsequence match with ranking:
/// prefix > word-start > contiguous substring > scattered subsequence.
QtObject {
    /// Score `query` against `text` (case-insensitive). -1 = no match, higher = better.
    function score(text, query) {
        let t = String(text).toLowerCase(), q = String(query).toLowerCase();
        if (q.length === 0) return 0;
        let idx = t.indexOf(q);
        if (idx === 0) return 1000 - t.length;                          // prefix
        if (idx > 0 && /[^a-z0-9]/.test(t[idx - 1])) return 800 - idx;  // word start
        if (idx > 0) return 600 - idx;                                  // contiguous
        let ti = 0;                                                     // scattered subsequence
        for (let ch of q) {
            ti = t.indexOf(ch, ti);
            if (ti < 0) return -1;
            ti++;
        }
        return 300 - (ti - q.length);   // tighter span = better
    }

    /// Filter + rank `items` by `query`; `textFn(item)` yields the searchable string.
    /// Empty query returns `items` unchanged; ties keep original order.
    function filter(items, query, textFn) {
        if (!items || items.length === 0) return [];
        if (!query || query.length === 0) return items;
        let scored = [];
        for (let i = 0; i < items.length; i++) {
            let s = score(textFn(items[i]), query);
            if (s >= 0) scored.push({ s: s, i: i, item: items[i] });
        }
        scored.sort((a, b) => (b.s - a.s) || (a.i - b.i));
        return scored.map(e => e.item);
    }
}
